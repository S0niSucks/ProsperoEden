// SPDX-License-Identifier: GPL-3.0-or-later
// Focused production service-method check, not a console/core integration build.
#include "amiibo_sync.h"
#include "cheat_download.h"
#include "cheat_manager.h"
#include "usb_backup.h"
#include "nextendo.h"
#include "nextendo_http.h"
#include "nextendo_oauth.h"
#include "theme_storage.h"
#include "admission_fixture_contract.inc"
#include "remote/http.h"
#include <algorithm>
#include <barrier>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;
namespace fs = std::filesystem;
namespace {
fs::path root;
std::string assets;
const std::string first = "0123456789ABCDEF23456789ABCDEF01";
const std::string second = "1123456789ABCDEF23456789ABCDEF01";
std::atomic<unsigned> config_reads{}, path_writes{}, profile_reads{}, profile_writes{};
std::atomic<bool> hold_nextendo{}, nextendo_cancelled{};
std::atomic<unsigned> in_nextendo{}, account_requests{}, token_requests{};
std::mutex provider_lock;
std::string challenge, redirect;
std::atomic<bool> hold_cheat{}, hold_amiibo{}, cheat_cancelled{}, amiibo_cancelled{};
std::atomic<unsigned> in_cheat{}, in_amiibo{}, probes{}, requests{};
const std::string body = "{\"0100000000001234\":{\"ab00000000000000\":{\"" + std::string(64, 'a') +
    "\":{\"title\":\"[Local test]\",\"source\":\"04000000 00000000 00000001\"}}}}";
template <class F> void Until(F ready) {
    const auto until = std::chrono::steady_clock::now() + 5s;
    while (!ready() && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(1ms);
    assert(ready());
}
Eden::CheatManager::Build Build() { Eden::CheatManager::Build build{}; build[0] = 0xab; return build; }
std::string Fetch(const std::string&, std::size_t, const std::atomic<bool>& stop, Eden::AmiiboSyncProgress) {
    ++in_amiibo;
    while (hold_amiibo && !stop) std::this_thread::sleep_for(1ms);
    if (stop) {
        amiibo_cancelled = true;
        while (hold_amiibo) std::this_thread::sleep_for(1ms);
    }
    --in_amiibo;
    return R"({"amiibo":[{"head":"00000000","tail":"00000001","name":"Synthetic"}]})";
}
}
namespace Eden {
std::string kDataDir;
bool FilesystemAccess() { return true; }
std::string AssetsPath(const std::string& path) { return assets + "/" + path; }
std::string GamesDir() { return assets; }
std::string AssetsDir() { return assets; }
bool DirectoryExists(const std::string& path) { return fs::is_directory(path); }
std::string ConfigFile(const std::string& path) { return assets + "/config/" + path; }
std::string ConfigDir() { ++config_reads; return assets + "/config"; }
std::string CoversDir() { return assets + "/covers"; }
std::string GamesPath(const std::string& path) { return assets + "/" + path; }
bool SaveAssetsDir(const std::string& path) { ++path_writes; assets = path; return true; }
bool SaveGamesDir(const std::string&) { ++path_writes; return true; }
void Report(const char* tag, const char*) { assert(std::string(tag) == "profile"); }
// Only console profile metadata is simulated. The service guards and account unlink/write are real.
namespace Profiles {
constexpr unsigned kMax = 8;
struct Profile { std::string key, name; std::string Key() const { return key; } };
struct State { std::vector<Profile> profiles; int current; };
std::vector<Profile> list{{first, "First"}, {second, "Second"}};
int selected = 0;
State Resolve(int) { ++profile_reads; return {list, selected}; }
std::vector<Profile> Read() { ++profile_reads; return list; }
bool Choose(const Profile& profile, int) {
    ++profile_writes;
    for (unsigned n = 0; n < list.size(); ++n) if (list[n].key == profile.key) { selected = static_cast<int>(n); return true; }
    return false;
}
bool Write(const std::vector<Profile>& profiles) { ++profile_writes; list = profiles; return true; }
std::string FreeName(const std::vector<Profile>&) { return "Third"; }
Profile Make(const std::string& name) { return {"2123456789ABCDEF23456789ABCDEF01", name}; }
bool Seed(const Profile&) { ++profile_writes; return true; }
bool Forget(const Profile&) { ++profile_writes; return true; }
}
namespace UpdateNotice {
void Cancel() {}
void Finish() {}
void WaitForCheck() {}
}
namespace Remote {
struct Paths { std::string config, covers, roms, updates, downloads; };
std::atomic<unsigned> stops{}, starts{};
void Stop() { ++stops; }
bool StopForSnapshot(const std::atomic<bool>& stop) { Stop(); return !stop; }
void Start(const Paths&, bool(*)(const std::string&, const std::string&), std::vector<std::string>(*)(const std::string&)) { ++starts; }
}
namespace Multiplayer {
enum class Phase { Idle, Connected };
struct RoomClient {
    struct Snapshot { Phase phase = Phase::Idle; };
    Snapshot GetSnapshot() const { return {}; }
};
}
}
namespace pe::ui {
struct Game { std::string file = "local.nsp"; bool remote = false; bool network = false; };
struct Profile { std::string name; bool playing; };
}
namespace {
bool WriteCover(const std::string&, const std::string&) { assert(false); return false; }
std::vector<std::string> ListFolder(const std::string&) { assert(false); return {}; }
std::vector<std::pair<std::string, std::string>> ReadPending() { return {}; }
void WritePending(const std::vector<std::pair<std::string, std::string>>&) { assert(false); }
std::string CurrentProfile(int user) {
    const auto who = Eden::Profiles::Resolve(user);
    return who.profiles.at(static_cast<std::size_t>(who.current)).Key();
}
std::vector<std::string> ProfileNames() { return {"First", "Second", "Third", "Fourth"}; }
void KeepSaveSyncFile(const std::vector<Eden::Profiles::Profile>&) { ++profile_writes; }
}
class EdenServices {
public:
    EdenServices() : cheat_manager_(bridge_, [](const auto&) { ++probes; return Build(); }), amiibo_sync_(Fetch) {}
    ~EdenServices() { usb_stop_ = true; if (usb_thread_.joinable()) usb_thread_.join(); amiibo_sync_.Stop(); cheat_manager_.Stop(); }
    bool can_launch_game();
    bool set_files_folder(const std::string&);
    bool set_games_folder(const std::string&);
    bool check_cheat_database(const pe::ui::Game&, bool);
    bool install_cheat_database(const pe::ui::Game&);
    bool start_amiibo_sync(const std::vector<std::string>&, bool, bool);
    std::vector<pe::ui::Profile> profiles();
    std::string profile_key(int);
    bool choose_profile(int);
    int add_profile();
    bool rename_profile(int, int);
    bool remove_profile(int);
    Eden::Nextendo::Status nextendo_status();
    std::string nextendo_link_url();
    Eden::Nextendo::Status nextendo_action(const std::string&, Eden::Nextendo::Action, const std::atomic<bool>&);
    Eden::Nextendo::Status nextendo_action_with_consent(const std::string&, Eden::Nextendo::Action, const std::atomic<bool>&, bool);
    void will_play(const std::string&);
    bool begin_usb_backup();
    Eden::UsbBackup::Status usb_backup_status();
    bool usb_backup_action(const std::string&, const std::string&);
    void cancel_usb_backup();
    void end_usb_backup();
    bool game_exists(const std::string& file) { return file == "local.nsp"; }
    bool SaveSyncWanted(const std::string&) { return false; }
    Eden::CheatManagerRequest CheatRequest(const pe::ui::Game& game) {
        return {game.file, (root / game.file).string(), {}, std::string{Eden::kCheatCatalogSource},
            (root / "cache").string(), (root / "mods/0100000000001234").string(), 0x0100000000001234};
    }
    Eden::Multiplayer::RoomClient rooms_;
    int user_ = -1;
    std::thread sync_thread_, pair_thread_;
    std::atomic<bool> pair_stop_{false};
    struct SyncJob { std::string profile, file; };
    void stop_save_sync() {}
    void StartSync(std::vector<SyncJob>, bool) { assert(false); }
#include "launcher_admission_members.inc"
};
#include "launcher_admission_methods.inc"
#include "admission_usb_services.inc"
#include "admission_nextendo_services.inc"

extern "C" int remote_http_get(const remote_http_request* request, remote_http_result* response) {
    ++requests; ++in_cheat;
    while (hold_cheat && !request->stop(request->user)) std::this_thread::sleep_for(1ms);
    if (request->stop(request->user)) {
        cheat_cancelled = true;
        while (hold_cheat) std::this_thread::sleep_for(1ms);
        --in_cheat; return -1;
    }
    response->status = 200;
    const bool ok = request->begin(request->user, 200, body.size()) && request->sink(request->user, body.data(), body.size());
    --in_cheat; return ok ? 0 : -1;
}
namespace {
std::string Field(std::string_view query, std::string_view key) {
    while (!query.empty()) {
        const auto end = query.find('&');
        const auto item = query.substr(0, end);
        if (item.starts_with(std::string(key) + "=")) {
            std::string value;
            const auto encoded = item.substr(key.size() + 1);
            for (std::size_t n = 0; n < encoded.size(); ++n) {
                if (encoded[n] == '%') {
                    assert(n + 2 < encoded.size());
                    value += static_cast<char>(std::stoi(std::string(encoded.substr(n + 1, 2)), nullptr, 16));
                    n += 2;
                } else value += encoded[n] == '+' ? ' ' : encoded[n];
            }
            return value;
        }
        if (end == query.npos) break;
        query.remove_prefix(end + 1);
    }
    return {};
}
std::string Read(const fs::path& path) {
    std::ifstream in(path);
    assert(in.good());
    return {std::istreambuf_iterator<char>(in), {}};
}
void Write(const fs::path& path, const std::string& bytes) {
    { std::ofstream out(path); out << bytes; assert(out.good()); }
    assert(chmod(path.c_str(), 0600) == 0);
}
std::string LocalGet(const std::string& url) {
    constexpr std::string_view prefix = "http://127.0.0.1:";
    assert(url.starts_with(prefix));
    const auto slash = url.find('/', prefix.size());
    const std::string host = url.substr(7, slash - 7);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<std::uint16_t>(std::stoi(url.substr(prefix.size(), slash - prefix.size()))));
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    const timeval timeout{3, 0};
    assert(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
    assert(connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    const std::string request = "GET " + url.substr(slash) + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
    std::size_t sent = 0;
    while (sent < request.size()) {
        const auto size = send(fd, request.data() + sent, request.size() - sent, MSG_NOSIGNAL);
        assert(size > 0); sent += static_cast<std::size_t>(size);
    }
    std::string reply;
    char buffer[1024];
    for (;;) {
        const auto size = recv(fd, buffer, sizeof(buffer), 0);
        assert(size >= 0);
        if (!size) break;
        reply.append(buffer, size);
        assert(reply.size() <= 8192);
    }
    close(fd);
    return reply;
}
void Authorize(EdenServices& service) {
    const auto reply = LocalGet(service.nextendo_link_url());
    assert(reply.starts_with("HTTP/1.1 303"));
    const auto at = reply.find("Location: ");
    assert(at != reply.npos);
    const auto start = at + 10;
    const auto auth = reply.substr(start, reply.find("\r\n", start) - start);
    assert(auth.starts_with("https://nextendo.network/api/oauth/authorize?"));
    const auto query = std::string_view(auth).substr(auth.find('?') + 1);
    const std::string state = Field(query, "state"), callback = Field(query, "redirect_uri");
    assert(state.size() == 43 && Field(query, "code_challenge_method") == "S256");
    assert(Field(query, "client_id") == "synthetic-admission-client");
    assert(Field(query, "scope") == "identity game.matchmaking");
    {
        const std::lock_guard lock(provider_lock);
        challenge = Field(query, "code_challenge");
        redirect = callback;
    }
    assert(LocalGet(callback + "?code=synthetic-admission-code&state=" + state).starts_with("HTTP/1.1 200"));
}
void Seed(const fs::path& directory) {
    fs::create_directories(directory / "config");
    const fs::path accounts = directory / "config/nextendo";
    Eden::Nextendo::Store store(accounts.string());
    const std::atomic<bool> stop{false};
    for (const auto& key : {first, second}) {
        assert(store.Link(key, {9, "nx2.synthetic-local-" + key, false}, stop).linked);
    }
    Write(accounts / "routes.json", R"({"auth.lp1.nintendo.net":"synthetic.nextendo.network"})");
    for (const auto& key : {first, second}) assert(store.Apply(key, Eden::Nextendo::Action::enable, stop).enabled);
    Write(accounts / "oauth.json", R"({"version":1,"client_id":"synthetic-admission-client","app":"synthetic-prosperoeden","redirect_uri":"http://127.0.0.1:{port}/callback"})");
    Write(accounts / "import.txt", "pid=9\ntoken=nx2.synthetic-import-not-consumed\n");
}
void MutationsRefused(EdenServices& service, const std::string& other) {
    const auto paths = path_writes.load(), writes = profile_writes.load(), reads = profile_reads.load();
    assert(!service.can_launch_game());
    assert(!service.set_files_folder(other) && !service.set_games_folder(other));
    assert(!service.choose_profile(1) && service.add_profile() == -1);
    assert(!service.rename_profile(1, 1) && !service.remove_profile(1));
    assert(service.profiles().empty() && service.profile_key(0).empty());
    assert(!service.nextendo_status().error.empty()); // must not wait for the Store's HTTP-held mutex
    assert(paths == path_writes && writes == profile_writes && reads == profile_reads);
}
void RefusedWithoutIo(EdenServices& service) {
    const auto paths = config_reads.load(), reads = profile_reads.load();
    const auto accounts = account_requests.load(), tokens = token_requests.load();
    const std::atomic<bool> stop{false};
    for (const auto action : {Eden::Nextendo::Action::link_browser, Eden::Nextendo::Action::check_account,
                             Eden::Nextendo::Action::import_session, Eden::Nextendo::Action::unlink}) {
        // Actually queue the UI-owned work and release it only after USB/terminal commitment.
        std::barrier scheduled(2);
        Eden::Nextendo::Status result;
        std::thread delayed([&] { scheduled.arrive_and_wait(); result = service.nextendo_action_with_consent(first, action, stop, true); });
        scheduled.arrive_and_wait(); delayed.join();
        assert(!result.error.empty() && !service.nextendo_busy_ && service.nextendo_link_url().empty());
    }
    assert(paths == config_reads && reads == profile_reads && accounts == account_requests && tokens == token_requests);
}
void QueuedRefusal(EdenServices& service, bool usb) {
    const auto paths = config_reads.load(), reads = profile_reads.load();
    const auto accounts = account_requests.load(), tokens = token_requests.load();
    const std::atomic<bool> stop{false};
    std::atomic<bool> queued{false};
    std::barrier scheduled(2);
    Eden::Nextendo::Status result;
    std::thread delayed([&] {
        queued = true; scheduled.arrive_and_wait();
        result = service.nextendo_action_with_consent(first, Eden::Nextendo::Action::link_browser, stop, true);
    });
    Until([&] { return queued.load(); });
    // The UI captured the request first; only execution is delayed until storage/launch commits.
    if (usb) assert(service.begin_usb_backup());
    else service.will_play("local.nsp");
    scheduled.arrive_and_wait(); delayed.join();
    assert(!result.error.empty() && !service.nextendo_busy_ && service.nextendo_link_url().empty());
    assert(paths == config_reads && reads == profile_reads && accounts == account_requests && tokens == token_requests);
}
}
extern "C" int remote_http_run(const remote_http_request* request, remote_http_result* response) {
    // Synthetic public provider transport only. Store, PKCE/state and all three parsers are production.
    assert(request->https_only && request->no_redirect && request->nextendo_client_id);
    assert(std::string(request->nextendo_client_id) == "synthetic-admission-client");
    const bool token = std::string(request->url) == "https://nextendo.network/api/oauth/token";
    std::string reply;
    if (token) {
        ++token_requests;
        assert(std::string(request->method) == "POST" && !request->authorization);
        assert(std::string(request->content_type) == "application/x-www-form-urlencoded");
        const std::string_view form(request->body, request->body_size);
        const std::lock_guard lock(provider_lock);
        assert(Field(form, "code") == "synthetic-admission-code" && Field(form, "grant_type") == "authorization_code");
        assert(Field(form, "client_id") == "synthetic-admission-client" && Field(form, "redirect_uri") == redirect);
        assert(Eden::Nextendo::PkceChallenge(Field(form, "code_verifier")) == challenge);
        reply = R"({"access_token":"synthetic-web-admission-access-only","token_type":"Bearer","expires_in":3600,"scope":"identity game.matchmaking"})";
    } else {
        ++account_requests;
        assert(std::string(request->authorization) == "Bearer synthetic-web-admission-access-only");
        const auto url = std::string(request->url);
        assert(url == "https://nextendo.network/api/oauth/userinfo" || url == "https://nextendo.network/api/nex-token");
        reply = url.ends_with("userinfo") ? R"({"pid":17,"username":"synthetic-unused"})" :
            R"({"pid":17,"nex_token":"nx2.synthetic-validated-admission-proof","expires_in":86400})";
    }
    ++in_nextendo;
    while (hold_nextendo && !request->stop(request->user)) std::this_thread::sleep_for(1ms);
    if (request->stop(request->user)) {
        nextendo_cancelled = true;
        while (hold_nextendo) std::this_thread::sleep_for(1ms); // cancellation is not completion
        --in_nextendo;
        return -1;
    }
    response->status = 200;
    const bool ok = request->sink(request->user, reply.data(), reply.size()) == 1;
    --in_nextendo;
    return ok ? 0 : -1;
}
namespace {
void CheckNextendoAdmission() {
    using Eden::Nextendo::Action;
    const auto primary = root / "nextendo-primary", alternate = root / "nextendo-alternate";
    Seed(primary); Seed(alternate);
    assets = primary.string();
    const auto accounts = primary / "config/nextendo", others = alternate / "config/nextendo";
    const auto old_first = Read(accounts / (first + ".account")), old_second = Read(accounts / (second + ".account"));
    const auto other_first = Read(others / (first + ".account"));
    {
        EdenServices service;
        const std::atomic<bool> stop{false};
        const auto configs = config_reads.load(), tokens = token_requests.load(), reads = account_requests.load();
        for (unsigned n = 0; n < 2; ++n) {
            assert(service.nextendo_action(first, Action::link_browser, stop).error == Eden::Nextendo::GameLinkConsentRequired);
            assert(service.nextendo_action_with_consent(first, Action::link_browser, stop, false).error == Eden::Nextendo::GameLinkConsentRequired);
            assert(!service.nextendo_busy_ && service.nextendo_link_url().empty());
        }
        assert(config_reads == configs && token_requests == tokens && account_requests == reads);
        assert(service.nextendo_action(first, Action::check_account, stop).error == Eden::Nextendo::GameCheckUnavailable);
        assert(token_requests == tokens && account_requests == reads && Read(accounts / (first + ".account")) == old_first);
    }
    {
        EdenServices service;
        std::atomic<bool> stop{false};
        Eden::Nextendo::Status result;
        hold_nextendo = true; nextendo_cancelled = false;
        const auto configs = config_reads.load();
        std::thread worker([&] { result = service.nextendo_action_with_consent(first, Action::link_browser, stop, true); });
        Until([&] { return !service.nextendo_link_url().empty(); });
        Authorize(service);
        Until([] { return in_nextendo != 0; });
        assert(service.nextendo_busy_ && config_reads == configs + 1);
        MutationsRefused(service, alternate.string());
        assert(!service.begin_usb_backup() && !service.usb_active_);
        service.will_play("local.nsp"); assert(!service.launch_committed_);
        // Network wait owns neither lock. USB/mutations refuse, not wait for cancellation or I/O.
        assert(service.bridge_.try_lock()); service.bridge_.unlock();
        assert(service.job_admission_mutex_.try_lock()); service.job_admission_mutex_.unlock();
        assert(!service.nextendo_action(second, Action::unlink, stop).error.empty() && service.nextendo_busy_);
        stop = true; Until([] { return nextendo_cancelled.load(); });
        assert(!service.begin_usb_backup() && service.nextendo_busy_);
        hold_nextendo = false; worker.join();
        assert(!result.error.empty() && !result.checked && !service.nextendo_busy_ && config_reads == configs + 1);
        assert(Read(accounts / (first + ".account")) == old_first && Read(accounts / (second + ".account")) == old_second);
        assert(Read(others / (first + ".account")) == other_first && assets == primary.string());
        assert(service.can_launch_game());
        QueuedRefusal(service, true);
        RefusedWithoutIo(service); MutationsRefused(service, alternate.string());
        Until([&] { return !service.usb_backup_status().busy; });
        service.end_usb_backup(); assert(!service.usb_active_ && service.can_launch_game());
        QueuedRefusal(service, false); assert(service.launch_committed_);
        RefusedWithoutIo(service); MutationsRefused(service, alternate.string());
        assert(Read(accounts / (first + ".account")) == old_first && Read(accounts / "import.txt").find("not-consumed") != std::string::npos);
    }
    {
        // Real loopback receiver publishes a URL, checks state, and exchanges a PKCE-bound code.
        EdenServices service;
        std::atomic<bool> stop{false};
        Eden::Nextendo::Status result;
        hold_nextendo = true; nextendo_cancelled = false;
        const auto configs = config_reads.load();
        std::thread worker([&] { result = service.nextendo_action_with_consent(first, Action::link_browser, stop, true); });
        Until([&] { return !service.nextendo_link_url().empty(); });
        MutationsRefused(service, alternate.string());
        assert(!service.begin_usb_backup());
        service.will_play("local.nsp"); assert(!service.launch_committed_);
        Authorize(service); Until([] { return in_nextendo != 0; });
        stop = true; Until([] { return nextendo_cancelled.load(); });
        std::atomic<bool> bridge_held{false}, release_bridge{false};
        std::thread blocker([&] {
            const std::lock_guard bridge(service.bridge_);
            bridge_held = true;
            while (!release_bridge) std::this_thread::sleep_for(1ms);
        });
        Until([&] { return bridge_held.load(); });
        hold_nextendo = false; Until([] { return in_nextendo == 0; });
        // The actual final URL-clear callback is now blocked on another thread's bridge. Busy must
        // outlive it; admission remains free and refusals must precede acquiring that busy bridge.
        assert(service.job_admission_mutex_.try_lock()); service.job_admission_mutex_.unlock();
        assert(service.nextendo_busy_ && !service.begin_usb_backup());
        MutationsRefused(service, alternate.string());
        release_bridge = true; blocker.join();
        worker.join();
        assert(!result.error.empty() && !service.nextendo_busy_ && service.nextendo_link_url().empty());
        assert(config_reads == configs + 1 && Read(accounts / (first + ".account")) == old_first);
        assert(Read(accounts / (second + ".account")) == old_second && Read(others / (first + ".account")) == other_first);
        assert(service.begin_usb_backup()); Until([&] { return !service.usb_backup_status().busy; });
        service.end_usb_backup();
    }
    {
        EdenServices service;
        // A delayed, formerly captured profile is rejected after selection changes, before ConfigDir/I/O.
        const std::atomic<bool> stop{false};
        assert(service.choose_profile(1));
        const auto configs = config_reads.load(), request_count = account_requests.load();
        Eden::Nextendo::Status result;
        std::thread delayed([&] { result = service.nextendo_action(first, Action::check_account, stop); }); delayed.join();
        assert(!result.error.empty() && config_reads == configs && account_requests == request_count);
        assert(!service.nextendo_busy_ && service.choose_profile(0));
        // Select a different root BEFORE admission. Every subsequent store operation uses that snapshot.
        assert(service.set_files_folder(alternate.string()));
        hold_nextendo = true;
        const auto before = config_reads.load();
        const auto tokens = token_requests.load(), reads = account_requests.load();
        std::thread oauth([&] { result = service.nextendo_action_with_consent(first, Action::link_browser, stop, true); });
        Until([&] { return !service.nextendo_link_url().empty(); });
        Authorize(service); Until([] { return in_nextendo != 0; });
        MutationsRefused(service, primary.string());
        assert(!service.begin_usb_backup());
        hold_nextendo = false; oauth.join();
        assert(result.linked && !result.enabled && result.error.empty() && !service.nextendo_busy_);
        assert(config_reads == before + 1 && service.nextendo_link_url().empty());
        std::string error;
        const auto linked = Eden::Nextendo::Store(others.string()).Load(first, error);
        assert(linked && error.empty() && !linked->account.enabled && linked->account.pid == 17);
        assert(linked->account.token == "nx2.synthetic-validated-admission-proof");
        assert(token_requests == tokens + 1 && account_requests == reads + 2);
        assert(linked->account.expires_at > linked->account.issued_at);
        assert(Read(accounts / (first + ".account")) == old_first && Read(accounts / (second + ".account")) == old_second);
        assert(service.rename_profile(1, 1) && service.add_profile() == 2);
        assert(service.remove_profile(1) && !fs::exists(others / (second + ".account")));
        assert(Read(accounts / (second + ".account")) == old_second); // unlink uses the admitted root too
        assert(service.begin_usb_backup()); Until([&] { return !service.usb_backup_status().busy; });
        service.end_usb_backup();
    }
    assets = primary.string();
    Eden::Profiles::list = {{first, "First"}, {second, "Second"}}; Eden::Profiles::selected = 0;
    for (unsigned n = 0; n < 20; ++n) {
        EdenServices service;
        hold_nextendo = true;
        std::atomic<bool> stop{false}, finished{false};
        std::barrier start(4);
        std::thread nextendo([&] { start.arrive_and_wait(); service.nextendo_action(first, Action::check_account, stop); finished = true; });
        std::thread usb([&] { start.arrive_and_wait(); service.begin_usb_backup(); });
        std::thread commit([&] { start.arrive_and_wait(); service.will_play("local.nsp"); });
        start.arrive_and_wait(); usb.join(); commit.join();
        Until([&] { return finished || in_nextendo != 0; });
        if (in_nextendo) {
            assert(service.nextendo_busy_ && !service.usb_active_ && !service.launch_committed_);
            assert(!service.begin_usb_backup());
            service.will_play("local.nsp"); assert(!service.launch_committed_);
        }
        stop = true; hold_nextendo = false; nextendo.join();
        assert(!service.nextendo_busy_ && Read(accounts / (first + ".account")) == old_first);
        if (service.usb_active_) {
            assert(!service.launch_committed_);
            RefusedWithoutIo(service);
            Until([&] { return !service.usb_backup_status().busy; }); service.end_usb_backup();
        }
        service.will_play("local.nsp"); assert(service.launch_committed_);
        RefusedWithoutIo(service);
    }
    assert(in_nextendo == 0);
}
}
int main() {
    char temporary[] = "/tmp/eden-launch-admission-XXXXXX"; assert(mkdtemp(temporary)); root = temporary;
    assets = (root / "assets").string(); fs::create_directory(assets);
    Eden::kDataDir = assets;
    const pe::ui::Game game;
#if ADMISSION_THEME_CONTRACT
    {
        EdenServices service;
        auto credit = Eden::ThemeStorage::Reserve(service.theme_storage_, Eden::ThemeStorage::MinRequest,
            Eden::ThemeStorage::Headroom + Eden::ThemeStorage::MinRequest);
        assert(credit && service.theme_storage_->busy());
        const auto paths = path_writes.load(), reads = profile_reads.load(), writes = profile_writes.load();
        const auto before_requests = requests.load(), before_probes = probes.load();
        assert(!service.can_launch_game() && !service.begin_usb_backup() && !service.usb_active_);
        assert(!service.set_files_folder(assets) && !service.set_games_folder(assets));
        assert(!service.choose_profile(1) && service.add_profile() == -1);
        assert(!service.rename_profile(1, 1) && !service.remove_profile(1));
        assert(!service.check_cheat_database(game, false) && !service.install_cheat_database(game));
        assert(!service.start_amiibo_sync({}, false, true));
        service.will_play(game.file); assert(!service.launch_committed_);
        RefusedWithoutIo(service);
        assert(paths == path_writes && reads == profile_reads && writes == profile_writes);
        assert(before_requests == requests && before_probes == probes);
        auto last = credit; credit.reset(); assert(!service.begin_usb_backup());
        last.reset(); assert(!service.theme_storage_->busy() && service.can_launch_game());
    }
#endif
#if ADMISSION_NEXTENDO_NETWORK_CONTRACT
    {
        EdenServices service; service.network_mutation_active_ = true;
        RefusedWithoutIo(service);
        assert(!service.begin_usb_backup() && !service.usb_active_);
        service.network_mutation_active_ = false;
    }
#endif
    {
        EdenServices service;
        assert(service.can_launch_game());
        assert(service.set_files_folder(assets) && service.set_games_folder(root.string()));
        hold_cheat = true; hold_amiibo = true;
        assert(service.check_cheat_database(game, false)); Until([] { return in_cheat != 0; });
        assert(!service.can_launch_game() && !service.set_files_folder(assets) && !service.set_games_folder(root.string()));
        assert(!service.check_cheat_database(game, false) && !service.install_cheat_database(game));
        assert(service.start_amiibo_sync({}, false, true)); Until([] { return in_amiibo != 0; });
        std::thread commit([&] { service.will_play(game.file); });
        Until([] { return amiibo_cancelled.load(); });
        // will_play is definitely inside Stop/join. A bridge held here would deadlock real probes.
        assert(service.bridge_.try_lock()); service.bridge_.unlock();
        std::atomic<bool> attempted{}, admitted{};
        std::thread contender([&] { attempted = true; admitted = service.check_cheat_database(game, false); });
        Until([&] { return attempted.load(); });
        hold_amiibo = false; Until([] { return cheat_cancelled.load(); });
        assert(service.bridge_.try_lock()); service.bridge_.unlock();
        hold_cheat = false; commit.join(); contender.join();
        assert(!admitted && !service.can_launch_game());
        assert(!service.start_amiibo_sync({}, true, false) && !service.install_cheat_database(game));
        assert(!service.set_files_folder(assets) && !service.set_games_folder(root.string()));
        assert(!service.amiibo_sync_.Status().busy && !service.cheat_manager_.Status().busy);
    }
    // Race real Start/Install lifecycle calls against terminal commitment. The shared guard must
    // serialize all old CheatManager owner-only APIs, not merely make the flag's reads atomic.
    for (unsigned n = 0; n < 40; ++n) {
        EdenServices service;
        assert(service.check_cheat_database(game, false));
        Until([&] { return !service.cheat_manager_.Status().busy; });
        assert(service.cheat_manager_.Status().phase == "ready");
        std::barrier start(4);
        std::thread check([&] { start.arrive_and_wait(); service.check_cheat_database(game, true); });
        std::thread install([&] { start.arrive_and_wait(); service.install_cheat_database(game); });
        std::thread commit([&] { start.arrive_and_wait(); service.will_play(game.file); });
        start.arrive_and_wait(); check.join(); install.join(); commit.join();
        const auto before_requests = requests.load(), before_probes = probes.load();
        assert(!service.check_cheat_database(game, false) && !service.install_cheat_database(game));
        assert(!service.start_amiibo_sync({}, false, true) && !service.can_launch_game());
        assert(requests == before_requests && probes == before_probes);
        assert(!service.amiibo_sync_.Status().busy && !service.cheat_manager_.Status().busy);
    }
    {
        // Stop/join real cache writers before declaring a USB session ready. The fake
        // transports acknowledge cancellation but hold their return to expose the barrier.
        EdenServices service;
        hold_cheat = true; hold_amiibo = true; cheat_cancelled = false; amiibo_cancelled = false;
        assert(service.check_cheat_database(game, false)); Until([] { return in_cheat != 0; });
        assert(service.start_amiibo_sync({}, false, true)); Until([] { return in_amiibo != 0; });
        assert(service.begin_usb_backup()); Until([] { return amiibo_cancelled.load(); });
        const auto request_count = requests.load();
        assert(service.usb_backup_status().busy && service.usb_active_);
        assert(!service.can_launch_game() && !service.set_files_folder(assets) && !service.set_games_folder(assets));
        assert(!service.start_amiibo_sync({}, true, false) && !service.start_amiibo_sync({}, false, true));
        assert(!service.check_cheat_database(game, false) && !service.install_cheat_database(game));
        service.will_play(game.file); assert(!service.launch_committed_);
        service.cancel_usb_backup(); service.end_usb_backup();
        assert(service.usb_active_ && service.usb_backup_status().busy && requests == request_count);
        assert(service.bridge_.try_lock()); service.bridge_.unlock();
        hold_amiibo = false; Until([] { return cheat_cancelled.load(); });
        assert(service.usb_backup_status().busy);
        assert(service.bridge_.try_lock()); service.bridge_.unlock();
        hold_cheat = false; Until([&] { return !service.usb_backup_status().busy; });
        assert(in_cheat == 0 && in_amiibo == 0 && service.usb_active_);
        // A cancelled prepare may have no safely accessible USB drive. It remains
        // protected until explicitly closed; readiness never reopened either worker.
        assert(!service.can_launch_game() && !service.start_amiibo_sync({}, true, false));
        service.end_usb_backup(); assert(!service.usb_active_ && service.can_launch_game());
        assert(service.start_amiibo_sync({}, false, true));
        Until([&] { return !service.amiibo_sync_.Status().busy; });
        service.will_play(game.file);
        assert(!service.begin_usb_backup() && !service.can_launch_game());
    }
    for (unsigned n = 0; n < 20; ++n) {
        EdenServices service;
        std::barrier start(4);
        std::thread amiibo([&] { start.arrive_and_wait(); service.start_amiibo_sync({}, false, true); });
        std::thread usb([&] { start.arrive_and_wait(); service.begin_usb_backup(); });
        std::thread commit([&] { start.arrive_and_wait(); service.will_play(game.file); });
        start.arrive_and_wait(); amiibo.join(); usb.join(); commit.join();
        if (service.usb_active_) {
            Until([&] { return !service.usb_backup_status().busy; });
            assert(!service.launch_committed_ && !service.start_amiibo_sync({}, true, false));
            service.end_usb_backup();
            service.will_play(game.file);
        }
        assert(service.launch_committed_ && !service.can_launch_game() && !service.begin_usb_backup());
        assert(!service.start_amiibo_sync({}, false, true));
    }
    CheckNextendoAdmission();
    assert(in_cheat == 0 && in_amiibo == 0); fs::remove_all(root);
    std::puts("Launcher admission PASS: actual USB/Amiibo/cheat/Nextendo guards and members; real workers, Store/account parser/PKCE loopback; USB/launch races, delayed refusal before I/O, root/profile gates, callback cancellation lifetime, unchanged accounts (transport and console metadata simulated)");
}
