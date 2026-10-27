// SPDX-License-Identifier: GPL-3.0-or-later
// Focused production service-method check, not a console/core integration build.
#include "amiibo_sync.h"
#include "cheat_download.h"
#include "cheat_manager.h"
#include "remote/http.h"
#include <algorithm>
#include <barrier>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;
namespace fs = std::filesystem;
namespace {
fs::path root;
std::string assets;
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
bool FilesystemAccess() { return true; }
std::string AssetsPath(const std::string& path) { return assets + "/" + path; }
bool SaveAssetsDir(const std::string& path) { assets = path; return true; }
bool SaveGamesDir(const std::string&) { return true; }
void Report(const char*, const char*) { assert(false); }
namespace Multiplayer {
enum class Phase { Idle, Connected };
struct RoomClient {
    struct Snapshot { Phase phase = Phase::Idle; };
    Snapshot GetSnapshot() const { return {}; }
};
}
}
namespace pe::ui { struct Game { std::string file = "local.nsp"; bool remote = false; }; }
namespace {
std::vector<std::pair<std::string, std::string>> ReadPending() { assert(false); return {}; }
void WritePending(const std::vector<std::pair<std::string, std::string>>&) { assert(false); }
std::string CurrentProfile(int) { assert(false); return {}; }
}
class EdenServices {
public:
    EdenServices() : cheat_manager_(bridge_, [](const auto&) { ++probes; return Build(); }), amiibo_sync_(Fetch) {}
    ~EdenServices() { amiibo_sync_.Stop(); cheat_manager_.Stop(); }
    bool can_launch_game();
    bool set_files_folder(const std::string&);
    bool set_games_folder(const std::string&);
    bool check_cheat_database(const pe::ui::Game&, bool);
    bool install_cheat_database(const pe::ui::Game&);
    bool start_amiibo_sync(const std::vector<std::string>&, bool, bool);
    void will_play(const std::string&);
    bool game_exists(const std::string& file) { return file == "local.nsp"; }
    bool save_sync_wanted(const std::string&) { return false; }
    Eden::CheatManagerRequest CheatRequest(const pe::ui::Game& game) {
        return {game.file, (root / game.file).string(), {}, std::string{Eden::kCheatCatalogSource},
            (root / "cache").string(), (root / "mods/0100000000001234").string(), 0x0100000000001234};
    }
    Eden::Multiplayer::RoomClient rooms_;
    int user_ = -1;
#include "launcher_admission_members.inc"
};
#include "launcher_admission_methods.inc"

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
int main() {
    char temporary[] = "/tmp/eden-launch-admission-XXXXXX"; assert(mkdtemp(temporary)); root = temporary;
    assets = (root / "assets").string(); fs::create_directory(assets);
    const pe::ui::Game game;
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
    assert(in_cheat == 0 && in_amiibo == 0); fs::remove_all(root);
    std::puts("Launcher admission PASS: production service guards, real worker Start/Install/commit races, terminal refusal, busy path protection, no bridge held across either join");
}
