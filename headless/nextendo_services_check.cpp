// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the actual service adapter with synthetic profile/platform dependencies, no network.
#include "nextendo.h"
#include "nextendo_http.h"
#include <cassert>
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
std::string CurrentProfile(int user) { return user == 0 ? first : second; }
}
namespace Eden {
bool FilesystemAccess() { return storage_access; }
std::string ConfigDir() { return config; }
namespace Multiplayer {
enum class Phase { Idle, Connected };
struct Snapshot { Phase phase = Phase::Idle; };
struct RoomClient {
    Snapshot state;
    Snapshot GetSnapshot() { return state; }
};
}
namespace Nextendo {
AccountResult CheckAccountHttp(const Account&, const std::atomic<bool>&) {
    ++checks;
    return AccountResult::accepted;
}
}
}
class EdenServices {
public:
    Eden::Nextendo::Status nextendo_status();
    Eden::Nextendo::Status nextendo_action(const std::string&, Eden::Nextendo::Action, const std::atomic<bool>&);
    std::mutex bridge_;
    std::atomic<bool> nextendo_busy_{false};
    int user_ = 0;
    Eden::Multiplayer::RoomClient rooms_;
};
#include "prosperoeden/nextendo_services.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    config = argv[1];
    EdenServices services;
    const std::atomic<bool> stop{false};
    using Eden::Nextendo::Action;
    const auto root = std::filesystem::path(config) / "nextendo";
    // The worker was scheduled for the first player, but another player was selected before it ran.
    services.user_ = 1;
    assert(!services.nextendo_action(first, Action::import_session, stop).error.empty());
    assert(!std::filesystem::exists(root) && checks == 0 && !services.nextendo_busy_);
    services.user_ = 0;
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
    assert(services.nextendo_action(first, Action::check_account, stop).checked && checks == 1);
    services.user_ = 1;
    assert(!services.nextendo_status().linked);
    assert(!services.nextendo_action(first, Action::check_account, stop).checked && checks == 1);
}
