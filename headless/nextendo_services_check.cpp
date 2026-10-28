// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the actual service adapter with synthetic profile/platform dependencies, no network.
#include "nextendo.h"
#include "nextendo_http.h"
#include "nextendo_oauth.h"
#include "remote/http.h"
#include "theme_storage.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sys/stat.h>

namespace {
const std::string first = "0123456789ABCDEF23456789ABCDEF01";
const std::string second = "1123456789ABCDEF23456789ABCDEF01";
std::string config;
bool storage_access = true;
int checks = 0;
int metadata_queries = 0;
std::string CurrentProfile(int user) { ++metadata_queries; return user == 0 ? first : second; }
}
namespace Eden {
bool FilesystemAccess() { ++metadata_queries; return storage_access; }
std::string ConfigDir() { ++metadata_queries; return config; }
namespace Multiplayer {
enum class Phase { Idle, Connected };
struct Snapshot { Phase phase = Phase::Idle; };
struct RoomClient {
    Snapshot state;
    Snapshot GetSnapshot() { return state; }
};
}
}
class EdenServices {
public:
    Eden::Nextendo::Status nextendo_status();
    std::string nextendo_link_url();
    Eden::Nextendo::Status nextendo_action(const std::string&, Eden::Nextendo::Action, const std::atomic<bool>&);
    Eden::Nextendo::Status nextendo_action_with_consent(const std::string&, Eden::Nextendo::Action, const std::atomic<bool>&, bool);
    std::mutex bridge_;
    std::mutex job_admission_mutex_;
    std::shared_ptr<Eden::ThemeStorage::Ledger> theme_storage_ = Eden::ThemeStorage::ProcessLedger();
    std::atomic<bool> network_mutation_active_{false};
    std::atomic<bool> usb_active_{false};
    bool launch_committed_ = false;
    std::atomic<bool> nextendo_busy_{false};
    std::string nextendo_browser_url_;
    int user_ = 0;
    Eden::Multiplayer::RoomClient rooms_;
};
#include "prosperoeden/nextendo_services.inc"
extern "C" int remote_http_run(const remote_http_request*, remote_http_result*) {
    ++checks;
    assert(false && "service guard tests must not make OAuth requests");
    return -1;
}

int main(int argc, char** argv) {
    assert(argc == 2);
    config = argv[1];
    EdenServices services;
    const std::atomic<bool> stop{false};
    using Eden::Nextendo::Action;
    const auto root = std::filesystem::path(config) / "nextendo";
    // Compile the unmodified production include against the actual process ledger and
    // atomic network flag. Refusal must precede profile/storage lookup, Store I/O and HTTP,
    // and must not acquire/clear a busy flag owned by a different request.
    const auto refused_without_io = [&] {
        const auto before_queries = metadata_queries, before_checks = checks;
        const auto before_busy = services.nextendo_busy_.load();
        const auto before_network = services.network_mutation_active_.load();
        const auto before_held = services.theme_storage_->held.load();
        const auto before_url = services.nextendo_browser_url_;
        for (const auto action : {Action::import_session, Action::enable, Action::disable,
                                  Action::check_account, Action::unlink, Action::link_browser}) {
            for (const auto& result : {services.nextendo_action(first, action, stop),
                    services.nextendo_action_with_consent(first, action, stop, true)}) {
                assert(result.error == "Nextendo action failed. Retry or unlink the session.");
                assert(!result.linked && !result.enabled && !result.checked && result.routes == 0);
            }
            assert(services.nextendo_busy_.load() == before_busy);
            assert(services.network_mutation_active_.load() == before_network);
            assert(services.theme_storage_->held.load() == before_held);
        }
        assert(metadata_queries == before_queries && checks == before_checks);
        assert(services.nextendo_browser_url_ == before_url && !std::filesystem::exists(root));
    };
    const auto consent_gate_reached = [&] {
        const auto before_queries = metadata_queries, before_checks = checks;
        assert(services.nextendo_action(first, Action::link_browser, stop).error ==
            Eden::Nextendo::GameLinkConsentRequired);
        assert(metadata_queries == before_queries && checks == before_checks);
        assert(!services.nextendo_busy_ && !std::filesystem::exists(root));
    };
    assert(services.theme_storage_ == Eden::ThemeStorage::ProcessLedger());
    assert(!services.theme_storage_->busy() && !services.network_mutation_active_);
    {
        auto credit = Eden::ThemeStorage::Reserve(services.theme_storage_, Eden::ThemeStorage::MinRequest,
            Eden::ThemeStorage::Headroom + Eden::ThemeStorage::MinRequest);
        assert(credit && services.theme_storage_->held == Eden::ThemeStorage::MinRequest);
        refused_without_io();
        auto last = credit;
        credit.reset();
        assert(services.theme_storage_->busy());
        refused_without_io();
        services.nextendo_busy_ = true;
        refused_without_io();
        assert(services.nextendo_busy_);
        services.nextendo_busy_ = false;
        last.reset();
        assert(!services.theme_storage_->busy() && services.theme_storage_->held == 0);
        consent_gate_reached();
    }
    services.network_mutation_active_ = true;
    refused_without_io();
    services.nextendo_busy_ = true;
    refused_without_io();
    assert(services.nextendo_busy_ && services.network_mutation_active_);
    services.nextendo_busy_ = false;
    services.network_mutation_active_ = false;
    consent_gate_reached();
    // The worker was scheduled for the first player, but another player was selected before it ran.
    services.user_ = 1;
    assert(!services.nextendo_action(first, Action::import_session, stop).error.empty());
    assert(!std::filesystem::exists(root) && checks == 0 && !services.nextendo_busy_);
    services.user_ = 0;
    services.usb_active_ = true;
    assert(!services.nextendo_action(first, Action::link_browser, stop).error.empty());
    assert(!services.nextendo_status().error.empty() && !services.nextendo_busy_ && !std::filesystem::exists(root));
    services.usb_active_ = false;
    services.launch_committed_ = true;
    assert(!services.nextendo_action(first, Action::import_session, stop).error.empty());
    assert(!services.nextendo_status().error.empty() && !services.nextendo_busy_ && !std::filesystem::exists(root));
    services.launch_committed_ = false;
    services.nextendo_busy_ = true;
    assert(!services.nextendo_action(first, Action::import_session, stop).error.empty());
    assert(services.nextendo_busy_); // a competing request must not clear somebody else's busy flag
    services.nextendo_busy_ = false;
    storage_access = false;
    assert(!services.nextendo_action(first, Action::import_session, stop).error.empty());
    assert(!services.nextendo_busy_ && !std::filesystem::exists(root));
    storage_access = true;
    services.rooms_.state.phase = Eden::Multiplayer::Phase::Connected;
    assert(!services.nextendo_action(first, Action::enable, stop).error.empty());
    assert(!std::filesystem::exists(root));
    services.rooms_.state.phase = Eden::Multiplayer::Phase::Idle;
    std::filesystem::create_directory(root);
    assert(chmod(root.c_str(), 0700) == 0);
    const auto import = root / "import.txt";
    { std::ofstream out(import); out << "pid=9\ntoken=synthetic-service-adapter-only\n"; }
    assert(chmod(import.c_str(), 0600) == 0);
    const auto linked = services.nextendo_action(first, Action::import_session, stop);
    assert(linked.linked && !linked.enabled && linked.error.empty());
    assert(services.nextendo_status().linked);
    const auto missing = services.nextendo_action(first, Action::link_browser, stop);
    assert(missing.error == Eden::Nextendo::GameLinkConsentRequired && services.nextendo_status().linked &&
        checks == 0 && services.nextendo_link_url().empty());
    const auto registration = root / "oauth.json";
    { std::ofstream out(registration); out << R"({"version":1,"client_id":"synthetic-registered-client","app":"synthetic-prosperoeden","redirect_uri":"http://127.0.0.1:{port}/callback"})"; }
    assert(chmod(registration.c_str(), 0600) == 0);
    const auto account_file = root / (first + ".account");
    const auto read = [&] { std::ifstream in(account_file); return std::string{std::istreambuf_iterator<char>(in), {}}; };
    const auto previous = read();
    // Neither absent nor malformed registration can gate a game-only check or provoke HTTP.
    std::filesystem::remove(registration);
    assert(services.nextendo_action(first, Action::check_account, stop).error == Eden::Nextendo::GameCheckUnavailable);
    { std::ofstream out(registration); out << "malformed-unused-registration"; }
    assert(services.nextendo_action(first, Action::check_account, stop).error == Eden::Nextendo::GameCheckUnavailable);
    assert(read() == previous && checks == 0 && services.nextendo_status().linked);
    assert(services.nextendo_action(first, Action::link_browser, stop).error == Eden::Nextendo::GameLinkConsentRequired);
    assert(services.nextendo_action_with_consent(first, Action::link_browser, stop, false).error == Eden::Nextendo::GameLinkConsentRequired);
    assert(!services.nextendo_action_with_consent(first, Action::link_browser, stop, true).error.empty());
    assert(services.nextendo_link_url().empty() && !services.nextendo_busy_ && checks == 0);
    { std::ofstream out(account_file); out << R"({"version":2,"credential_kind":"nex","pid":9,"token":"nx2.synthetic-expired-only","issued_at":1,"expires_at":2,"enabled":false})"; }
    const auto expired = read();
    assert(services.nextendo_action(first, Action::check_account, stop).error == Eden::Nextendo::GameSessionExpired);
    assert(read() == expired && services.nextendo_status().linked && checks == 0);
    const std::atomic<bool> cancelled{true};
    assert(services.nextendo_action(first, Action::check_account, cancelled).error == "Nextendo action cancelled.");
    assert(read() == expired && checks == 0);
    services.user_ = 1;
    assert(!services.nextendo_status().linked);
    assert(!services.nextendo_action(first, Action::check_account, stop).checked && checks == 0);
    std::puts("Nextendo service fixture PASS: real process theme credit/last-copy release; active network refusal before metadata/I/O/HTTP; competing busy flags preserved; all existing adapter cases retained.");
}
