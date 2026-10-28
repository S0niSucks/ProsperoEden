// SPDX-License-Identifier: GPL-3.0-or-later
// Actual OAuth receiver/HTTPS/storage, synthetic provider only. Never use real credentials here.
#include "nextendo_oauth.h"
#include "nextendo_http.h"
#include "nextendo_json.h"
#include <cassert>
#include <cstring>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <thread>

namespace {
std::string ca, connection;
std::string bad_connection, bad_ca, fault_mode;
curl_slist* mapping = nullptr;
curl_slist* bad_mapping = nullptr;
int requests = 0;
const char* fail_header = nullptr;
int header_failures = 0;
const char* later_header = nullptr;
int fail_request = 0;
std::chrono::milliseconds delay_sync{0};
std::atomic<bool>* cancel_on_sync = nullptr;
const std::string profile = "0123456789ABCDEF23456789ABCDEF01";
std::string Read(const std::filesystem::path& path) {
    std::ifstream in(path);
    return {std::istreambuf_iterator<char>(in), {}};
}
void Write(const std::filesystem::path& path, const std::string& bytes) {
    { std::ofstream out(path); out << bytes; assert(out.good()); }
    assert(chmod(path.c_str(), 0600) == 0);
}
}
extern "C" curl_slist* __real_curl_slist_append(curl_slist*, const char*);
extern "C" curl_slist* __wrap_curl_slist_append(curl_slist* headers, const char* value) {
    if (fail_header && std::strcmp(fail_header, value) == 0) {
        fail_header = nullptr;
        ++header_failures;
        return nullptr;
    }
    return __real_curl_slist_append(headers, value);
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
    if (cancel_on_sync) { *cancel_on_sync = true; cancel_on_sync = nullptr; }
    if (delay_sync.count()) { const auto delay = delay_sync; delay_sync = std::chrono::milliseconds{0}; std::this_thread::sleep_for(delay); }
    return __real_fsync(fd);
}
extern "C" void console_curl_setup(CURL* easy) {
    ++requests;
    if (requests == fail_request && later_header) fail_header = later_header;
    // Test seam is ONLY the platform adapter: production URL, SNI, verification and HTTP logic stay real.
    // CONNECT_TO changes the destination socket, not the TLS peer hostname.
    assert(curl_easy_setopt(easy, CURLOPT_CONNECT_TO, mapping) == CURLE_OK);
    assert(curl_easy_setopt(easy, CURLOPT_CAINFO, ca.c_str()) == CURLE_OK);
    if ((requests == 2 && fault_mode.starts_with("identity-")) || (requests == 3 && fault_mode.starts_with("nex-"))) {
        if (fault_mode.ends_with("wrong-host") || fault_mode.ends_with("unknown-ca")) {
            assert(curl_easy_setopt(easy, CURLOPT_CONNECT_TO, bad_mapping) == CURLE_OK);
            if (fault_mode.ends_with("wrong-host")) assert(curl_easy_setopt(easy, CURLOPT_CAINFO, bad_ca.c_str()) == CURLE_OK);
        }
    }
    assert(curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1L) == CURLE_OK);
    assert(curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 2L) == CURLE_OK);
    assert(curl_easy_setopt(easy, CURLOPT_PROXY, "") == CURLE_OK);
    assert(curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L) == CURLE_OK);
}
int main(int argc, char** argv) {
    assert(argc == 6);
    const std::filesystem::path root(argv[1]);
    const std::string mode(argv[4]);
    fault_mode = mode;
    ca = argv[3];
    connection = "nextendo.network:443:127.0.0.1:" + std::string(argv[2]);
    mapping = curl_slist_append(nullptr, connection.c_str());
    bad_connection = "nextendo.network:443:127.0.0.1:" + std::string(argv[5]);
    bad_mapping = curl_slist_append(nullptr, bad_connection.c_str());
    bad_ca = (std::filesystem::path(ca).parent_path() / "bad.pem").string();
    assert(bad_mapping);
    assert(mapping);
    assert(Eden::Nextendo::PkceChallenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk") ==
           "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"); // RFC 7636 appendix B
    using namespace Eden::Nextendo;
    Registration registration;
    std::string error;
    const std::string json = R"({"version":1,"client_id":"synthetic+registered&client","app":"synthetic-prosperoeden","redirect_uri":"http://127.0.0.1:{port}/callback"})";
    assert(ParseRegistration(json, registration, error));
    Registration public_registration;
    assert(ParseRegistration(R"({"version":1,"client_id":"synthetic","redirect_uri":"http://127.0.0.1:47823/callback"})", public_registration, error));
    assert(public_registration.app.empty());
    for (const char* bad : {"{}", R"({"client_id":"","redirect_uri":"http://127.0.0.1:3333/callback"})",
         R"({"version":1,"client_id":"synthetic","app":"test","redirect_uri":"http://0.0.0.0:3333/callback"})",
         R"({"version":1,"client_id":"synthetic","app":"test","redirect_uri":"http://localhost:3333/callback"})",
         R"({"version":1,"client_id":"synthetic","app":"test","redirect_uri":"http://127.0.0.1:80/callback"})",
         R"({"version":1,"client_id":"synthetic","app":"test","redirect_uri":"http://127.0.0.1:{port}/start"})",
         R"({"version":1,"client_id":"synthetic","app":"","redirect_uri":"http://127.0.0.1:{port}/callback"})",
         R"({"version":1,"client_id":"synthetic","app":null,"redirect_uri":"http://127.0.0.1:{port}/callback"})",
         R"({"version":0,"version":1,"client_id":"synthetic","app":"test","redirect_uri":"http://127.0.0.1:{port}/callback"})",
         R"({"version":1,"client_id":"first","client_id":"second","app":"test","redirect_uri":"http://127.0.0.1:{port}/callback"})",
         R"({"version":1,"client_id":"synthetic","app":"first","app":"second","redirect_uri":"http://127.0.0.1:{port}/callback"})",
         R"({"version":1,"client_id":"synthetic","app":"test","redirect_uri":"http://127.0.0.1:{port}/first","redirect_\u0075ri":"http://127.0.0.1:{port}/callback"})",
         R"({"version":1,"client_id":"synthetic\r\nInjected: yes","app":"test","redirect_uri":"http://127.0.0.1:{port}/callback"})"}) {
        Registration invalid;
        assert(!ParseRegistration(bad, invalid, error));
    }
    Store store(root, AccountCheckFunction{[](const Account& account, const std::atomic<bool>& stop) {
        return CheckAccountHttpWithClient(account, "synthetic+registered&client", stop);
    }});
    std::atomic<bool> stop{false};
    assert(store.Link(profile, {9, "old-synthetic-local-session", true}, stop).linked);
    Write(root / "routes.json", R"({"auth.lp1.nintendo.net":"synthetic.nextendo.network"})");
    assert(store.Apply(profile, Action::enable, stop).enabled);
    const auto account_file = root / (profile + ".account");
    const auto before = Read(account_file);
    bool published = false;
    assert(LinkBrowser(store, profile, stop, [&](const auto&) { published = true; }).error == GameLinkConsentRequired);
    assert(!LinkBrowser(store, profile, stop, [&](const auto&) { published = true; }, true).error.empty());
    assert(!published && requests == 0 && Read(account_file) == before);
    Write(root / "oauth.json", json);
    if (mode.starts_with("profile-")) {
        std::cout << "PROFILE" << std::endl;
        const auto checked = store.Apply(profile, Action::check_account, stop);
        assert(!checked.checked && !checked.error.empty());
        assert(store.Get(profile).linked && Read(account_file) == before);
        assert(header_failures == 0 && requests == 0 && checked.error == GameCheckUnavailable);
        assert(checked.error.find("synthetic") == checked.error.npos);
        curl_slist_free_all(mapping);
        curl_slist_free_all(bad_mapping);
        std::cout << "DONE " << mode << " PASS\n";
        return 0;
    }
    if (mode == "token-header-oom") fail_header = "Content-Type: application/x-www-form-urlencoded";
    if (mode.ends_with("auth-oom")) later_header = "Authorization: Bearer synthetic-web-access-not-a-game-token";
    if (mode.ends_with("client-oom")) later_header = "X-Nextendo-Client-Id: synthetic+registered&client";
    if (mode.ends_with("accept-oom")) later_header = "Accept: application/json, */*";
    fail_request = mode.starts_with("identity-") ? 2 : mode.starts_with("nex-") ? 3 : mode.starts_with("token-") ? 1 : 0;
    if (mode == "storage-failure") Write(account_file.string() + ".new", "stale staging must survive");
    auto start = [&](const std::string& url) { if (!url.empty()) std::cout << "START " << url << std::endl; };
    std::thread cancel([&] {
        std::string command;
        std::getline(std::cin, command);
        if (command == "cancel") stop = true;
    });
    Status result;
    if (mode == "highlevel") {
        result = LinkBrowser(store, profile, stop, start, true);
    } else {
        if (mode == "fixed") {
            auto reserve = BrowserLink::Open(registration, error);
            assert(reserve);
            registration.redirect_uri = reserve->StartUrl().substr(0, reserve->StartUrl().rfind('/')) + "/callback";
        }
        if (mode == "no-app") registration.app.clear();
        auto flow = BrowserLink::Open(registration, error,
            mode == "expiry" ? std::chrono::milliseconds{250} :
            mode.starts_with("expiry-") ? std::chrono::milliseconds{700} :
            mode == "shared-deadline" ? std::chrono::milliseconds{800} : std::chrono::milliseconds{5000});
        assert(flow && error.empty());
        if (mode == "fixed") assert(registration.redirect_uri == flow->StartUrl().substr(0, flow->StartUrl().rfind('/')) + "/callback");
        auto occupied = registration;
        occupied.redirect_uri = flow->StartUrl().substr(0, flow->StartUrl().rfind('/')) + "/callback";
        assert(!BrowserLink::Open(occupied, error)); // fixed registered port collision fails, no fallback port
        start(flow->StartUrl());
        Account account;
        if (flow->Finish(account, stop, error)) {
            if (mode == "cancel-before-save") stop = true;
            if (mode == "cancel-in-save") cancel_on_sync = &stop;
            if (mode == "expiry-save") delay_sync = std::chrono::milliseconds{800};
            if (mode == "nex-expiry-save") delay_sync = std::chrono::milliseconds{2200};
            result = store.Link(profile, std::move(account), stop, flow->Deadline());
        } else result.error = error;
        assert(!flow->Finish(account, stop, error)); // a code cannot be exchanged twice
    }
    cancel.join();
    const bool success = mode == "success" || mode == "recover" || mode == "highlevel" || mode == "slow-client" || mode == "fixed" || mode == "no-app" || mode == "scope-omitted" || mode == "scope-reordered" || mode == "stored-expiry" || mode == "snapshot-v2" || mode == "access-lowercase-type";
    if (success) {
        const auto saved = store.Load(profile, error);
        assert(result.linked && result.error.empty() && saved && !saved->account.enabled);
        assert(saved->account.token == "nx2.synthetic-proof-not-a-real-session" && requests == 3);
        assert(saved->account.pid == 9 && saved->account.expires_at > saved->account.issued_at && GameSessionCurrent(saved->account));
        const auto stored = Read(account_file);
        assert(stored.find("access_token") == stored.npos && stored.find("refresh") == stored.npos && stored.find("scope") == stored.npos);
        assert(stored.find("synthetic-web") == stored.npos && stored.find("discard-me") == stored.npos);
        const auto check = store.Apply(profile, Action::check_account, stop);
        assert(check.error == GameCheckUnavailable && !check.checked && requests == 3 && Read(account_file) == stored);
        struct stat st{};
        assert(stat(account_file.c_str(), &st) == 0 && (st.st_mode & 077) == 0);
        assert(!std::filesystem::exists(account_file.string() + ".new"));
        if (mode == "stored-expiry") {
            auto expired = Detail::ParseUniqueJson(stored);
            expired["issued_at"] = 1u; expired["expires_at"] = 2u; expired["enabled"] = true;
            Write(account_file, expired.dump());
            assert(!store.Apply(profile, Action::enable, stop).error.empty());
            assert(!BeginGame(store, profile, false, error) && !GuestEnabled());
            assert(store.Apply(profile, Action::check_account, stop).error == GameSessionExpired);
            assert(store.Get(profile).linked && store.Apply(profile, Action::disable, stop).error.empty());
        }
        if (mode == "snapshot-v2") {
            assert(store.Apply(profile, Action::enable, stop).enabled);
            assert(!BeginGame(store, profile, true, error) && !GuestEnabled());
            assert(BeginGame(store, profile, false, error) && GuestPid() == 9);
            const auto frozen = GuestSession();
            auto replacement = saved->account;
            replacement.pid = 10;
            assert(store.Link(profile, replacement, stop).linked && !store.Get(profile).enabled);
            assert(GuestSession() == frozen && frozen->account.pid == 9 && frozen->account.expires_at == saved->account.expires_at);
            EndGame();
            assert(!GuestEnabled());
        }
    } else {
        assert(!result.error.empty() && Read(account_file) == before);
        assert(result.error.find("synthetic-proof") == result.error.npos);
        assert(result.error.find("synthetic-code") == result.error.npos);
        assert(result.error.find("synthetic-web") == result.error.npos);
        if (mode.starts_with("expiry") || mode == "shared-deadline") assert(result.error.find("expired") != result.error.npos);
        if (mode == "nex-expiry-save") assert(result.error == GameSessionExpired && !delay_sync.count() && requests == 3);
        if (mode.starts_with("cancel")) assert(result.error.find("cancelled") != result.error.npos);
    }
    if (mode.ends_with("-oom")) assert(header_failures == 1);
    curl_slist_free_all(mapping);
    curl_slist_free_all(bad_mapping);
    std::cout << "DONE " << mode << " PASS\n";
}
