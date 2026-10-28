// SPDX-License-Identifier: GPL-3.0-or-later
// Actual service bodies + settings/profile/mod/pending files and actual service-owned threads.
// Console metadata, preset renderer and backend/kit transport are deliberately synthetic.
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>
using namespace std::chrono_literals;
namespace fs = std::filesystem;
struct Gate {
    std::atomic<bool> entered{}, release{};
    void Wait() { entered = true; while (!release) std::this_thread::sleep_for(1ms); }
};
thread_local Gate* acquisition_gate = nullptr;
thread_local const void* acquisition_target = nullptr;
thread_local Gate* usb_check_gate = nullptr;
struct ObservedAtomic : std::atomic<bool> {
    using std::atomic<bool>::atomic;
    using std::atomic<bool>::operator=;
    operator bool() const {
        const bool checked = load();
        if (auto* gate = usb_check_gate) { usb_check_gate = nullptr; gate->Wait(); }
        return checked; // pause AFTER the exact production atomic load, not before its check
    }
};
struct ObservedMutex : std::mutex {
    void lock() {
        if (this == acquisition_target && acquisition_gate) acquisition_gate->Wait();
        std::mutex::lock();
    }
};
#include "usb_writer_class.h"
#include "admission_fixture_contract.inc"
#include "assets_dir.h"
#include "diagnostics.h"
#include "profiles.h"
#include "save_sync_config.h"
#include "remote/backends.h"
#include "remote/save_sync.h"
#include "remote/http.h"
#include "metadata_bridge.h"
#include "reshade_folder.h"
#include "update_notice.h"
#include "update_check/self_update.h"
#include "update_check/update_check.h"
#include "pe/core/strings.hpp"
namespace {
template<class F> void Until(F ready) {
    const auto end = std::chrono::steady_clock::now() + 8s;
    while (!ready() && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(1ms);
    assert(ready());
}
void Write(const std::string& path, const std::string& data) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream out(path); out << data; assert(out.good());
}
std::string Read(const std::string& path) {
    std::ifstream in(path); return {std::istreambuf_iterator<char>(in), {}};
}
std::map<std::string, std::string> Snapshot() {
    std::map<std::string, std::string> result;
    for (auto& entry : fs::recursive_directory_iterator(Eden::kDataDir))
        if (entry.is_regular_file()) result[entry.path().string()] = Read(entry.path().string());
    return result;
}
Gate *kit_gate = nullptr, *sync_gate = nullptr, *pair_gate = nullptr;
std::atomic<unsigned> kit_starts{}, kit_applies{}, sync_calls{}, pair_calls{}, remote_calls{};
std::atomic<bool> syncing{}, pairing{}, transport_saw_stop{};
std::string GameTitle(const std::string&) { return "Synthetic local game"; }
std::string GameFolderOf(const std::string&) { return {}; }
std::string SizeLabel(double size) { return std::to_string(size); }
std::vector<std::string> ListEntries(const std::string& folder, bool directories, bool& ok) {
    std::vector<std::string> names;
    std::error_code error;
    for (auto& entry : fs::directory_iterator(folder, error))
        if (entry.is_directory() == directories) names.push_back(entry.path().filename().string());
    ok = !error; return names;
}
using pe::fill;
using pe::tr;
std::string TextOf(const nlohmann::json&, const char*);
std::string TextOf(const Eden::SaveSync::Json&, const char*);
#include "usb_writer_helpers.inc"
bool WriteCover(const std::string&, const std::string&) { assert(false); return false; }
std::vector<std::string> ListFolder(const std::string&) { assert(false); return {}; }
}
namespace VideoCore {
struct Preset { std::string name; };
std::vector<Preset> GetFxPresetCatalog() { return {{"Synthetic preset"}}; }
void ReloadFxPresetCatalog() { Write(Eden::ConfigFile("preset-test"), "synthetic default installation"); }
}
namespace Eden {
namespace Multiplayer {
struct RoomClient::Impl {};
RoomClient::RoomClient() = default;
RoomClient::~RoomClient() = default;
bool RoomClient::Connect(Connection) { ++remote_calls; return true; }
void SetDetailedLogging(bool) {}
}
namespace Remote {
void Stop() {}
bool StopForSnapshot(const std::atomic<bool>& stop) { return !stop; } // real drain covered by worker suite
void Start(const Paths&, CoverWriter, FolderLister) {}
void Refresh() { ++remote_calls; }
Status Current() { Status status; status.generation = 1; return status; }
void Changed() { ++remote_calls; }
std::vector<std::string> Files(const std::string&, const std::string&) { return {}; }
std::vector<Title> Titles() {
    Game game; game.source = "synthetic"; game.id = "local";
    return {{"synthetic-key", {game}}};
}
bool Enqueue(const std::string&, const std::string&, bool) {
    ++remote_calls; Write(Eden::ConfigFile("remote/queue.json"), "enqueued"); return true;
}
bool Cancel(const std::string&, const std::string&) {
    ++remote_calls; Write(Eden::ConfigFile("remote/queue.json"), "cancelled"); return true;
}
SyncResult SyncSaveData(const std::string&, const nlohmann::json&, const SyncGame&, const SyncPlaces& places,
                        const SyncChooser&, const Stopped& stop) {
    ++sync_calls; syncing = true;
    assert(sync_gate); sync_gate->entered = true;
    while (!sync_gate->release) {
        if (stop()) transport_saw_stop = true;
        std::this_thread::sleep_for(1ms);
    }
    // A transport may finish a publication while cancellation is arriving. The preparer must join it.
    Write(places.store + "/synthetic-receipt", "completed before USB snapshot");
    syncing = false;
    SyncResult result; result.outcome = SyncOutcome::same; return result;
}
std::string TitleFolder(std::uint64_t id) { return std::to_string(id); }
bool PairStart(const nlohmann::json&, const std::string& folder, PairingStart*, std::string*, const Stopped& stop) {
    ++pair_calls; pairing = true;
    assert(pair_gate); pair_gate->entered = true;
    while (!pair_gate->release) {
        if (stop()) transport_saw_stop = true;
        std::this_thread::sleep_for(1ms);
    }
    Write(folder + "/synthetic-pair-receipt", "finished before USB snapshot");
    pairing = false; return false;
}
PairingResult PairPoll(const nlohmann::json&, const std::string&, const PairingStart&, const Stopped&) { assert(false); return {}; }
const Backend* FindBackend(const std::string& type) {
    static const Pairing pairing{PairStart, PairPoll};
    static const Backend backend{"synthetic", nullptr,
        [](const nlohmann::json&, const std::string&, std::string*) -> std::unique_ptr<SaveStore> { assert(false); return {}; }, &pairing};
    return type == "synthetic" ? &backend : nullptr;
}
}
}
extern "C" int remote_http_get(const remote_http_request*, remote_http_result*) { assert(false); return -1; }
extern "C" std::uint64_t eden_game_title_id(const char*) { return 0x0100000000001234; }
extern "C" int eden_game_addon_files(std::uint64_t, const char*, const char*, const char*,
                                     eden_found_file, void*) { return 0; }
extern "C" int eden_save_import(std::uint64_t, char*, std::size_t) {
    Write(Eden::ConfigFile("import-receipt"), "imported"); return EDEN_SAVE_DONE;
}
extern "C" int eden_save_export(std::uint64_t, char*, std::size_t) {
    Write(Eden::ConfigFile("export-receipt"), "exported"); return EDEN_SAVE_DONE;
}
extern "C" self_update_check_result self_update_check_self(self_update_offer* offer) {
    std::strcpy(offer->available, "99.999.999"); return SELF_UPDATE_AVAILABLE;
}
extern "C" int update_check_version_compare(const char*, const char*, int* comparable) { *comparable = 1; return 1; }
extern "C" const self_update_platform* self_update_console() { return nullptr; }
extern "C" int self_update_start(self_update_job*, const self_update_platform*, const self_update_offer*) {
    ++kit_starts; if (kit_gate) kit_gate->Wait(); Write(Eden::ConfigFile("update-stage"), "started"); return 1;
}
extern "C" void self_update_cancel(self_update_job*) {}
extern "C" void self_update_finish(self_update_job*) {}
extern "C" void self_update_poll(self_update_job*, self_update_status*) {}
extern "C" int self_update_apply(self_update_job*) {
    ++kit_applies; if (kit_gate) kit_gate->Wait(); Write(Eden::ConfigFile("update-go"), "applied"); return 1;
}
EdenServices::EdenServices(std::string, Eden::Multiplayer::RoomClient& rooms)
    : rooms_(rooms), cheat_manager_(bridge_, [](const auto&) { assert(false); return Eden::CheatManager::Build{}; }),
      amiibo_sync_([](const std::string&, std::size_t, const std::atomic<bool>&, Eden::AmiiboSyncProgress) -> std::string {
          assert(false); return {};
      }) {}
EdenServices::~EdenServices() {
    usb_stop_ = true; if (usb_thread_.joinable()) usb_thread_.join();
    stop_save_sync(); if (sync_thread_.joinable()) sync_thread_.join();
    pair_stop_ = true; if (pair_thread_.joinable()) pair_thread_.join();
}
#include "usb_writer_methods.inc"
#include "admission_usb_services.inc"
namespace {
Eden::UsbBackup::Status Prepared(EdenServices& service) {
    Until([&] { return !service.usb_backup_status().busy; });
    auto status = service.usb_backup_status();
    assert(status.active && status.drives == std::vector<std::string>{"/mnt/usb0"});
    return status;
}
using Action = std::function<bool(EdenServices&)>;
const std::uint64_t title = 0x0100000000001234;
void BridgeWriter(const char* name, const Action& action, bool pause_after_check = false, bool asynchronous = false) {
    Eden::Multiplayer::RoomClient rooms;
    EdenServices service({}, rooms);
    const auto before = Snapshot();
    Gate gate;
    bool accepted = false;
    std::thread writer([&] {
        if (pause_after_check) usb_check_gate = &gate;
        else { acquisition_target = &service.bridge_; acquisition_gate = &gate; }
        accepted = action(service);
    });
    Until([&] { return gate.entered.load(); }); // actual method passed its old USB check
#ifdef EXPECT_BASE_GAP
    (void)asynchronous;
    assert(service.begin_usb_backup()); Prepared(service);
    assert(Snapshot() == before);
    gate.release = true; writer.join();
    if (service.sync_thread_.joinable()) service.sync_thread_.join();
    if (service.pair_thread_.joinable()) service.pair_thread_.join();
    assert(accepted && Snapshot() != before && service.usb_active_);
    assert(!service.sync_thread_.joinable() && !service.pair_thread_.joinable());
    std::printf("PROVEN exact-base late writer after preparer drain: %s\n", name);
#else
    // Admitted writer owns admission even while queued on bridge. USB cannot overtake its drain.
    assert(!service.job_admission_mutex_.try_lock());
    std::atomic<bool> begun{};
    std::thread usb([&] { assert(service.begin_usb_backup()); begun = true; });
    assert(!service.usb_active_ && !begun && Snapshot() == before);
    gate.release = true; writer.join(); usb.join(); Prepared(service);
    // A legitimately admitted async job may be stopped before its first publication; its thread
    // must still be joined. Held in-flight callbacks are checked separately below.
    assert(accepted && (asynchronous || Snapshot() != before) && service.usb_active_);
    assert(!service.sync_thread_.joinable() && !service.pair_thread_.joinable());
    std::printf("Drained pre-admitted actual writer: %s\n", name);
#endif
    service.end_usb_backup(); assert(!service.usb_active_);
}
[[maybe_unused]] void LateWriter(const char* name, const Action& action) {
    Eden::Multiplayer::RoomClient rooms;
    EdenServices service({}, rooms);
    Gate gate;
    bool accepted = true;
    std::thread writer([&] {
        acquisition_target = &service.job_admission_mutex_; acquisition_gate = &gate;
        accepted = action(service);
    });
    Until([&] { return gate.entered.load(); });
    assert(service.begin_usb_backup()); Prepared(service);
    const auto before = Snapshot();
    const auto remotes = remote_calls.load(), starts = kit_starts.load(), pairs = pair_calls.load(), syncs = sync_calls.load();
    gate.release = true; writer.join();
    assert(!accepted && Snapshot() == before && remotes == remote_calls && starts == kit_starts);
    assert(pairs == pair_calls && syncs == sync_calls);
    service.end_usb_backup(); assert(!service.usb_active_);
    std::printf("Rejected queued actual writer before filesystem/backend activity: %s\n", name);
}
}
int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    // This fixture uses the production constant path, but will never delete/run on physical data.
    struct statfs data_mount{}, usb_mount{};
    assert(statfs("/data", &data_mount) == 0 && data_mount.f_type == 0x01021994);
    assert(statfs("/mnt/usb0", &usb_mount) == 0 && usb_mount.f_type == 0x01021994);
    fs::remove_all(Eden::kDataDir); // only the verified disposable tmpfs fixture above
    Eden::FilesystemAccessStatus() = 0;
    fs::create_directories(Eden::ConfigDir());
    const auto profile = Eden::Profiles::Make("Synthetic local profile");
    assert(Eden::Profiles::Write({profile}) && Eden::Profiles::Choose(profile, -1));
    KeepSaveSyncFile({profile}); // the real constructor's profile reconciliation precondition
    Write(Eden::ConfigFile("remote/sources.json"), R"({"sources":[{"type":"synthetic","url":"https://synthetic.invalid","name":"Local transport"}]})");
    Write(Eden::GamesPath("roms/local.nsp"), "synthetic ROM placeholder");
    Eden::UpdateNotice::Start(); Eden::UpdateNotice::WaitForCheck();
#if ADMISSION_THEME_CONTRACT
    {
        Eden::Multiplayer::RoomClient rooms;
        EdenServices service({}, rooms);
        auto credit = Eden::ThemeStorage::Reserve(service.theme_storage_, Eden::ThemeStorage::MinRequest,
            Eden::ThemeStorage::Headroom + Eden::ThemeStorage::MinRequest);
        assert(credit);
        const auto before = Snapshot();
        const auto remotes = remote_calls.load(), starts = kit_starts.load(), applies = kit_applies.load();
        const auto pairs = pair_calls.load(), syncs = sync_calls.load();
        assert(!service.begin_usb_backup() && !service.usb_active_);
        assert(!service.start_update() && !service.apply_update() && !service.start_pairing(0, 0));
        pe::ui::Game game; game.key = "synthetic-key";
        assert(!service.download(game, 0, false));
        service.refresh_sources(); service.start_save_sync("local.nsp", true);
        service.will_play("local.nsp"); assert(!service.launch_committed_);
        assert(!service.sync_thread_.joinable() && !service.pair_thread_.joinable());
        assert(before == Snapshot() && remotes == remote_calls && starts == kit_starts && applies == kit_applies);
        assert(pairs == pair_calls && syncs == sync_calls);
        auto last = credit; credit.reset(); assert(!service.start_update());
        last.reset(); assert(!service.theme_storage_->busy());
    }
#endif
    const std::vector<std::pair<const char*, Action>> writers{
        {"set_docked", [](auto& s) { return s.set_docked(title, false); }},
        {"set_game_settings", [](auto& s) { pe::ui::GameSettings value; value.volume = 35; return s.set_game_settings(title, value); }},
        {"set_preferences", [](auto& s) { pe::ui::Preferences value; value.volume = 40; return s.set_preferences(value); }},
        {"set_mod_enabled", [](auto& s) { return s.set_mod_enabled(title, "Synthetic mod", false); }},
        {"set_mods_enabled", [](auto& s) { return s.set_mods_enabled(title, false); }},
        {"set_cheats_enabled", [](auto& s) { return s.set_cheats_enabled(title, false); }}
    };
    for (const auto& [name, action] : writers) BridgeWriter(name, action);
    BridgeWriter("start_update", [](auto& s) { return s.start_update(); }, true);
    assert(Eden::SaveSync::SetEntry(profile.Key(), {{"type", "synthetic"}, {"url", "https://synthetic.invalid"}}));
    Gate ready; ready.release = true; pair_gate = sync_gate = &ready;
    BridgeWriter("start_pairing", [](auto& s) { return s.start_pairing(0, 0); }, true, true);
    BridgeWriter("start_save_sync", [](auto& s) { s.start_save_sync("local.nsp", true); return true; }, true, true);
    pair_gate = sync_gate = nullptr;
#ifdef EXPECT_BASE_GAP
    {
        // A retained-false Apply observation alone does NOT resurrect a job after Finish.
        Eden::Multiplayer::RoomClient rooms;
        EdenServices service({}, rooms);
        assert(service.start_update());
        Gate gate;
        bool applied = true;
        std::thread late([&] { usb_check_gate = &gate; applied = service.apply_update(); });
        Until([&] { return gate.entered.load(); });
        assert(service.begin_usb_backup()); Prepared(service);
        const auto before = Snapshot();
        gate.release = true; late.join();
        assert(!applied && before == Snapshot());
        service.end_usb_backup();
        std::puts("NO INDEPENDENT FINDING: exact-base late Apply cannot restart the job cleared by Finish");
    }
#endif
#ifndef EXPECT_BASE_GAP
    std::vector<std::pair<const char*, Action>> late = writers;
    const Eden::Multiplayer::Connection room{"synthetic.invalid", 24872, "Synthetic", {}};
    late.emplace_back("connect_room", [&](auto& s) { return s.connect_room(room); });
    late.emplace_back("connect_public_room", [&](auto& s) { return s.connect_public_room(room); });
    late.emplace_back("remember_room", [&](auto& s) { s.remember_room(room, true); return false; });
    late.emplace_back("make_mods_folder", [](auto& s) { return s.make_mods_folder(title); });
    late.emplace_back("set_cheat_enabled", [](auto& s) { return s.set_cheat_enabled(title, "Synthetic", "Synthetic", true); });
    late.emplace_back("delete_game", [](auto& s) {
        pe::ui::Game game; game.file = "local.nsp"; game.sources = {"synthetic"}; game.title_id = title;
        std::string text; return s.delete_game(game, &text);
    });
    late.emplace_back("unlink_profile", [](auto& s) { return s.unlink_profile(0); });
    late.emplace_back("start_update", [](auto& s) { return s.start_update(); });
    late.emplace_back("apply_update", [](auto& s) { return s.apply_update(); });
    late.emplace_back("start_pairing", [](auto& s) { return s.start_pairing(0, 0); });
    late.emplace_back("start_save_sync", [](auto& s) { s.start_save_sync("local.nsp", true); return s.sync_thread_.joinable(); });
    late.emplace_back("refresh_sources", [](auto& s) { s.refresh_sources(); return false; });
    late.emplace_back("download", [](auto& s) { pe::ui::Game game; game.key = "synthetic-key"; return s.download(game, 0, false); });
    late.emplace_back("cancel_download", [](auto& s) { return s.cancel_download("synthetic-key"); });
    late.emplace_back("save_import", [](auto& s) { std::string text; return s.save_import(title, &text); });
    late.emplace_back("save_export", [](auto& s) { std::string text; return s.save_export(title, &text); });
    for (const auto& [name, action] : late) LateWriter(name, action);
    Eden::Multiplayer::RoomClient rooms;
    {
        // Normal idle API behavior is real, not a permanent closed-gate default.
        EdenServices service({}, rooms);
        assert(service.connect_public_room(room)); // non-recursive admission
        assert(service.connect_room(room)); service.remember_room(room, false);
        assert(service.start_update() && service.apply_update());
        assert(service.make_mods_folder(title));
        assert(service.set_mod_enabled(title, "Synthetic mod", true));
        assert(service.set_mods_enabled(title, true) && service.set_cheats_enabled(title, true));
        Write(Eden::AssetsPath("cheats/0100000000001234/AB00000000000000.txt"), "[Synthetic cheat]\n04000000 00000000 00000001\n");
        const auto cheats = service.cheats(title); assert(!cheats.empty());
        assert(service.set_cheat_enabled(title, cheats[0].mod, cheats[0].name, true));
        service.refresh_sources(); pe::ui::Game game; game.key = "synthetic-key";
        assert(service.download(game, 0, false) && service.cancel_download(game.key));
        std::string text; assert(service.save_import(title, &text) && service.save_export(title, &text));
        // Missing settings could migrate, and ReadPostPresets may install default preset files.
        assert(service.begin_usb_backup()); Prepared(service);
        fs::remove(Eden::SettingsFile());
        Write(Eden::ConfigFile("settings.txt"), "1 1 55 0 0\n");
        const auto before = Snapshot();
        service.preferences(); service.game_settings(title); service.docked(title);
        service.mods(title); service.cheats(title); service.mods_enabled(title); service.cheats_enabled(title);
        service.room_connection(); service.saved_files_folder(); service.saved_games_folder();
        service.save_sync_setup(); service.save_sync_wanted("local.nsp");
        service.post_processing_labels(); service.post_processing_shared();
        assert(Snapshot() == before);
        service.end_usb_backup();
        service.preferences(); assert(fs::exists(Eden::SettingsFile()) && fs::exists(Eden::ConfigFile("preset-test")));
        assert(service.unlink_profile(0));
        game.file = "local.nsp"; game.sources = {"synthetic"}; game.title_id = title;
        assert(service.delete_game(game, &text) && !fs::exists(Eden::GamesPath("roms/local.nsp")));
        Write(Eden::GamesPath("roms/local.nsp"), "synthetic ROM placeholder");
    }
    {
        // Real StartSync thread, real pending JSON/profile binding; synthetic transport held AFTER stop.
        EdenServices service({}, rooms);
        assert(Eden::SaveSync::SetEntry(profile.Key(), {{"type", "synthetic"}, {"url", "https://synthetic.invalid"}}));
        Gate gate; sync_gate = &gate; transport_saw_stop = false;
        WritePending({{profile.Key(), "local.nsp"}});
        service.start_save_sync("local.nsp", false); Until([&] { return gate.entered.load(); });
        assert(service.begin_usb_backup()); Until([] { return transport_saw_stop.load(); });
        assert(service.usb_backup_status().busy && syncing);
        assert(service.bridge_.try_lock()); service.bridge_.unlock();
        gate.release = true; Prepared(service);
        assert(!syncing && ReadPending().size() == 1); // cancelled work must retain retry binding
        assert(fs::exists(SyncFolder() + "/" + profile.Key() + "/synthetic-receipt"));
        const auto before = Snapshot(); service.start_save_sync("local.nsp", false); assert(before == Snapshot());
        // Do not resume the intentionally pending fixture on close.
        WritePending({}); service.end_usb_backup();
        service.will_play("local.nsp"); // actual nested wanted path must not recurse admission
        assert(service.launch_committed_ && ReadPending().size() == 1);
        service.start_save_sync("local.nsp", true); assert(!service.sync_thread_.joinable());
        WritePending({}); sync_gate = nullptr;
    }
    {
        EdenServices service({}, rooms);
        Gate gate; pair_gate = &gate; transport_saw_stop = false;
        assert(service.start_pairing(0, 0)); Until([&] { return gate.entered.load(); });
        assert(service.begin_usb_backup()); Until([] { return transport_saw_stop.load(); });
        assert(service.usb_backup_status().busy && pairing);
        assert(service.bridge_.try_lock()); service.bridge_.unlock();
        gate.release = true; Prepared(service);
        assert(!pairing && fs::exists(SyncFolder() + "/" + profile.Key() + "/synthetic-pair-receipt"));
        assert(!service.start_pairing(0, 0)); service.end_usb_backup(); pair_gate = nullptr;
    }
    std::puts("USB writers PASS: exact guards, real settings/mod/profile/pending I/O, queued and grandfathered writers; real SaveSync/pair/update lifecycles with held synthetic backend callbacks; no network/physical USB/core");
#endif
}
