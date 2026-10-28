// SPDX-License-Identifier: GPL-3.0-or-later
// Verbatim service/controller bodies, real Editor/writer/keyboard/USB; synthetic NAS/IME only.
#include "prosperoeden/eden_services.h" // runner exposes actual members, removes unrelated vtable only
#include "assets_dir.h"
#include "profiles.h"
#include "system_keyboard.h"
#include "pe/core/strings.hpp"
#include "pe/audio/sounds.hpp"
#include "update_notice.h"
#include "diagnostics.h"
#include "nextendo.h"
#include "remote/http.h"
#include <barrier>
#include <cassert>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <pthread.h>
#include <set>
#include <sys/vfs.h>
using namespace std::chrono_literals;
namespace fs = std::filesystem;
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define SANITIZED_THREADS 1
#endif
#endif
#ifdef SANITIZED_THREADS
extern "C" int __interceptor_pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
#endif
static std::atomic<bool> fail_next_thread{false};
extern "C" int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*fn)(void*), void* arg) {
    if (fail_next_thread.exchange(false)) return EAGAIN;
#ifdef SANITIZED_THREADS
    return __interceptor_pthread_create(thread, attr, fn, arg);
#else
    static const auto real = reinterpret_cast<decltype(&pthread_create)>(dlsym(RTLD_NEXT, "pthread_create"));
    assert(real); return real(thread, attr, fn, arg);
#endif
}
namespace {
struct Gate { std::atomic<bool> entered{}, release{}; void Wait() { entered = true; while (!release) std::this_thread::sleep_for(1ms); } };
template<class F> void Until(F ready) {
    const auto end = std::chrono::steady_clock::now() + 6s;
    while (!ready() && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(1ms);
    assert(ready());
}
void Put(const std::string& file, const std::string& bytes) {
    fs::create_directories(fs::path(file).parent_path()); std::ofstream out(file); out << bytes; assert(out.good());
}
std::string Bytes(const std::string& file) { std::ifstream in(file); return {std::istreambuf_iterator<char>(in), {}}; }
std::atomic<Gate*> ime_gate{}, callback_gate{}, tail_gate{}, disconnect_gate{};
std::atomic<bool> discovery_busy{}, discovery_stop{}, throw_configure{};
std::atomic<bool> ime_saw_stop{};
std::atomic<unsigned> configurations{}, callbacks{}, connections{}, joins{}, asks{};
std::thread discovery;
Eden::TextRequest last_request;
std::u16string next_text;
Eden::TextOutcome next_outcome = Eden::TextOutcome::accepted;
std::string configured_file;
bool WriteCover(const std::string&, const std::string&) { assert(false); return false; }
std::vector<std::string> ListFolder(const std::string&) { assert(false); return {}; }
std::vector<std::pair<std::string, std::string>> ReadPending() { return {}; }
void WritePending(const std::vector<std::pair<std::string, std::string>>&) { assert(false); }
std::string CurrentProfile(int user) { const auto who = Eden::Profiles::Resolve(user); return who.profiles.at(who.current).Key(); }
std::vector<std::string> ProfileNames() { return {"Synthetic second profile"}; }
void KeepSaveSyncFile(const std::vector<Eden::Profiles::Profile>&) { assert(false); }
}
namespace Eden {
namespace Multiplayer {
struct RoomClient::Impl {};
RoomClient::RoomClient() = default;
RoomClient::~RoomClient() = default;
Snapshot RoomClient::GetSnapshot() const { return {}; }
}
namespace UpdateNotice { void Cancel() {} void Finish() {} void WaitForCheck() {} }
namespace Remote {
void Start(const Paths&, CoverWriter, FolderLister) {}
bool StopForSnapshot(const std::atomic<bool>& stop) { return !stop; }
}
TextAnswer AskSystemKeyboard(const TextRequest& request, const std::atomic<bool>& stop) {
    ++asks; last_request = request;
    if (auto* gate = ime_gate.load()) {
        gate->entered = true;
        while (!gate->release) { if (stop) ime_saw_stop = true; std::this_thread::sleep_for(1ms); }
    }
    return {stop ? TextOutcome::cancelled : next_outcome, next_text};
}
namespace Network {
bool ChargeTransfer(std::size_t) { assert(false); return false; } // no NAS I/O is permitted in this admission fixture
void StopDiscovery() {
    discovery_stop = true;
    if (discovery.joinable()) { discovery.join(); ++joins; }
}
void Disconnect() {
    StopDiscovery();
    if (auto* gate = disconnect_gate.exchange(nullptr)) gate->Wait();
    discovery_busy = false;
}
void Configure(const std::string& file, bool reconnect) {
    Disconnect(); ++configurations; configured_file = file;
    if (!reconnect) return;
    ++connections; discovery_busy = true; discovery_stop = false;
    discovery = std::thread([] {
        if (auto* gate = callback_gate.load()) gate->Wait();
        Put(Eden::ConfigFile("network-callback-fixture"), "synthetic final storage callback"); ++callbacks;
        discovery_busy = false;
        // Public busy=false need not mean callback/stack teardown is finished. The join is required.
        if (auto* gate = tail_gate.load()) gate->Wait();
    });
    if (throw_configure.exchange(false)) throw std::runtime_error("Synthetic-private-detail-never-published");
}
Status Current() { Status status; status.busy = discovery_busy; return status; }
}
}
extern "C" int remote_http_get(const remote_http_request*, remote_http_result*) { assert(false); return -1; }
extern "C" int remote_http_run(const remote_http_request*, remote_http_result*) { assert(false); return -1; }
EdenServices::EdenServices(std::string, Eden::Multiplayer::RoomClient& rooms)
    : rooms_(rooms), cheat_manager_(bridge_, [](const auto&) { assert(false); return Eden::CheatManager::Build{}; }),
      amiibo_sync_([](const auto&, auto, const auto&, auto) -> std::string { assert(false); return {}; }) {}
EdenServices::~EdenServices() { StopNetworkJobs(); usb_stop_ = true; if (usb_thread_.joinable()) usb_thread_.join(); }
void EdenServices::stop_save_sync() {}
void EdenServices::StartSync(std::vector<SyncJob>, bool) { assert(false); }
bool EdenServices::SaveSyncWanted(const std::string&) { return false; }
#include "network/services.cpp"
#include "prosperoeden/usb_backup_services.inc"
#include "compose_service_methods.inc"

namespace pe::ui {
// Only the shell is simulated; methods below are extracted verbatim from network_shares.cpp.
class Launcher {
public:
    explicit Launcher(EdenServices& service) : services_(service) {}
    ~Launcher() { network_stop_ = true; if (network_job_.valid()) network_job_.wait(); services_.network_editor_cancel(); }
    void press_network_shares(Key);
    void open_network_shares();
    void network_result(Eden::Network::EditorResult);
    void update_network_shares();
    void start_network_job(std::function<Eden::Network::EditorResult(NetworkReservation)>, bool, bool = false);
    enum class NetworkPage { locations, editor, discard, remove };
    EdenServices& services_;
    Eden::Network::EditorView network_view_;
    NetworkPage network_page_ = NetworkPage::locations;
    int network_row_ = 0;
    std::string network_remove_id_, message_;
    bool network_warning_ = false, network_commit_job_ = false, network_connection_job_ = false, rescan_ = false, closed = false;
    std::atomic<bool> network_stop_{false};
    std::future<Eden::Network::EditorResult> network_job_;
    void cue(audio::Cue) {}
    void close_modal() { closed = true; }
};
using Eden::Network::EditorResult;
using Eden::Network::Field;
namespace { constexpr int kRows = 8;
#include "compose_ui_saved.inc"
}
#include "compose_ui_methods.inc"
}
namespace {
using namespace Eden::Network;
using pe::ui::Key;
void Pump(pe::ui::Launcher& menu) {
    Until([&] { menu.update_network_shares(); return !menu.network_job_.valid(); });
}
void Focus(pe::ui::Launcher& menu, Field field) {
    const auto& rows = menu.network_view_.rows;
    const auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.field == field; });
    assert(found != rows.end()); menu.network_row_ = static_cast<int>(found - rows.begin());
}
void Enter(pe::ui::Launcher& menu, Field field, std::u16string value) {
    Focus(menu, field); next_text = std::move(value); menu.press_network_shares(Key::cross); Pump(menu);
}
void Catalog(const std::string& prefix) {
    std::set<std::string> keys;
    assert(PrepareKeyboardPrompts([&](std::string_view key) { keys.emplace(key); return std::string{key}; }));
    keys.emplace(DefaultNetworkLocationName());
    std::string po;
    for (const auto& key : keys) po += "msgid \"" + key + "\"\nmsgstr \"" + prefix + key + "\"\n\n";
    assert(pe::catalog().load(po) == 17);
}
void Excluded(EdenServices& service) {
    assert(service.network_job_busy() && service.network_shares().busy);
    assert(service.job_admission_mutex_.try_lock()); service.job_admission_mutex_.unlock();
    assert(service.bridge_.try_lock()); service.bridge_.unlock();
    assert(!service.begin_usb_backup() && !service.usb_active_);
    assert(!service.can_launch_game()); service.will_play("synthetic.nsp"); assert(!service.launch_committed_);
    const auto settings = Bytes(Eden::SettingsFile()), profiles = Bytes(Eden::Profiles::File());
    assert(!service.set_games_folder("/data/rejected-games") && !service.set_files_folder("/data/rejected-assets"));
    assert(service.profiles().empty() && service.profile_key(0).empty() && !service.choose_profile(0));
    assert(service.add_profile() == -1 && !service.rename_profile(0, 1) && !service.remove_profile(0));
    assert(settings == Bytes(Eden::SettingsFile()) && profiles == Bytes(Eden::Profiles::File()));
    assert(!service.reserve_network_job());
}
void Idle(EdenServices& service) { assert(!service.network_job_busy() && !discovery.joinable()); }
}
int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    struct statfs data{}, usb{};
    assert(statfs("/data", &data) == 0 && data.f_type == 0x01021994);
    assert(statfs("/mnt/usb0", &usb) == 0 && usb.f_type == 0x01021994);
    Eden::FilesystemAccessStatus() = 0;
    fs::create_directories(Eden::ConfigDir());
    const auto profile = Eden::Profiles::Make("Synthetic profile");
    assert(Eden::Profiles::Write({profile}) && Eden::Profiles::Choose(profile, -1));
    Eden::Multiplayer::RoomClient rooms;
    EdenServices service({}, rooms);
    // The gap BEFORE std::async is covered, and dropping an unstarted lease performs no I/O.
    auto lease = service.reserve_network_job(); assert(lease); Excluded(service);
    assert(configurations == 0 && asks == 0); lease.reset(); Idle(service);
    Gate admission;
    std::thread holder([&] { const std::lock_guard lock(service.job_admission_mutex_); admission.Wait(); });
    Until([&] { return admission.entered.load(); });
    assert(!service.reserve_network_job()); admission.release = true; holder.join();
    service.usb_active_ = true; assert(!service.reserve_network_job()); service.usb_active_ = false;
    service.nextendo_busy_ = true; assert(!service.reserve_network_job()); service.nextendo_busy_ = false;
    std::atomic<bool> early_stop{true};
    assert(service.network_connection_reserved(service.reserve_network_job(), true, early_stop) == EditorResult::cancelled);
    assert(configurations == 0 && asks == 0); Idle(service);
    // Failed native thread creation releases the lease, including the startup/legacy dispatch.
    fail_next_thread = true;
    assert(service.network_editor_connection(true) == EditorResult::unavailable && !fail_next_thread);
    Idle(service);
    {
        pe::ui::Launcher menu(service); Catalog("A: "); menu.open_network_shares(); menu.press_network_shares(Key::cross);
        assert(menu.network_view_.editing && menu.network_view_.rows[0].value == "A: Network location");
        Enter(menu, Field::server, u"127.0.0.1"); Enter(menu, Field::share, u"games"); Enter(menu, Field::user, u"fixture-user");
        Gate ime; ime_gate = &ime; next_text = u"Synthetic-private-secret";
        Focus(menu, Field::password); menu.press_network_shares(Key::cross);
        Until([&] { return ime.entered.load(); }); Excluded(service);
        assert(last_request.title == u"A: SMB password (private replacement)" && last_request.password && last_request.initial.empty());
        Catalog("B: "); // real mutable catalog reload while the worker owns only copied UTF-16
        ime.release = true; Pump(menu); ime_gate = nullptr;
        assert(last_request.placeholder == u"A: Enter a replacement; Cancel keeps the current password");
        for (const auto& row : service.network_editor_view().rows) assert(row.value.find("Synthetic-private-secret") == row.value.npos);
        Focus(menu, Field::name); fail_next_thread = true; menu.press_network_shares(Key::cross);
        assert(!menu.network_job_.valid() && !service.network_job_busy() && !fail_next_thread);
        Enter(menu, Field::name, u"User label");
        // Save+connect: retain through discovery and its tail even after public discovery busy=false.
        Gate callback, tail; callback_gate = &callback; tail_gate = &tail;
        menu.network_row_ = static_cast<int>(menu.network_view_.rows.size()) + 1; menu.press_network_shares(Key::cross);
        Until([&] { return callback.entered.load(); }); Excluded(service);
        callback.release = true; Until([&] { return tail.entered.load(); });
        assert(!discovery_busy); Excluded(service); tail.release = true; Pump(menu);
        callback_gate = tail_gate = nullptr; Idle(service);
        auto saved = ReadConfiguration(Eden::ConfigFile("network-shares.json"));
        assert(saved.shares.at(0).name == "User label" && saved.shares[0].password == "Synthetic-private-secret");
        const auto id = saved.shares[0].id;
        menu.network_row_ = 0; menu.press_network_shares(Key::cross);
        assert(menu.network_view_.id == id && menu.network_view_.rows[0].value == "User label"); // not B: default
        // Cancel in IME does not mutate the draft or stored secret, including teardown.
        Gate cancelled; ime_gate = &cancelled; next_text = u"Never-publish";
        Focus(menu, Field::password); menu.press_network_shares(Key::cross); Until([&] { return cancelled.entered.load(); });
        menu.press_network_shares(Key::circle); Excluded(service); cancelled.release = true; Pump(menu); ime_gate = nullptr;
        assert(ReadConfiguration(Eden::ConfigFile("network-shares.json")).bytes == saved.bytes);
        // Save without reconnect performs no new endpoint probes, and consumes its pre-dispatch token.
        auto count = connections.load(); menu.network_row_ = static_cast<int>(menu.network_view_.rows.size()); menu.press_network_shares(Key::cross); Pump(menu);
        assert(connections == count); Idle(service);
        // Disconnect and remove hold admission until the actual final drain returns.
        Gate disconnect; disconnect_gate = &disconnect;
        menu.network_row_ = static_cast<int>(menu.network_view_.locations.size()) + 2; menu.press_network_shares(Key::cross);
        Until([&] { return disconnect.entered.load(); }); Excluded(service); disconnect.release = true; Pump(menu); Idle(service);
        menu.network_row_ = 0; menu.press_network_shares(Key::square); menu.press_network_shares(Key::cross); Pump(menu);
        assert(ReadConfiguration(Eden::ConfigFile("network-shares.json")).shares.empty()); Idle(service);
        // Foreign tokens cannot enter this service. No file/IME/connection work is admitted.
        EdenServices other({}, rooms); const auto old = configurations.load();
        assert(service.network_connection_reserved(other.reserve_network_job(), false, menu.network_stop_) == EditorResult::blocked);
        assert(!other.network_job_busy() && configurations == old);
    }
    // Actual launcher network teardown contract: signal cancellation, wait, THEN cancel draft.
    // Even the cancelled native callback must finish before admission can open again.
    {
        auto menu = std::make_unique<pe::ui::Launcher>(service);
        menu->open_network_shares(); menu->press_network_shares(Key::cross);
        Gate final_ime; ime_gate = &final_ime; ime_saw_stop = false; next_text = u"Never-publish-on-teardown";
        Focus(*menu, Field::password); menu->press_network_shares(Key::cross);
        Until([&] { return final_ime.entered.load(); });
        auto destroying = std::async(std::launch::async, [menu = std::move(menu)]() mutable { menu.reset(); });
        Until([&] { return ime_saw_stop.load(); }); Excluded(service);
        final_ime.release = true; destroying.get(); ime_gate = nullptr; Idle(service);
        assert(ReadConfiguration(Eden::ConfigFile("network-shares.json")).shares.empty());
    }
    // Exception after a discovery starts must still join its held final callback before release.
    Gate thrown; callback_gate = &thrown; throw_configure = true; std::atomic<bool> stop{false};
    auto pending = std::async(std::launch::async, [&, job = service.reserve_network_job()]() mutable {
        return service.network_connection_reserved(std::move(job), true, stop);
    });
    Until([&] { return thrown.entered.load(); }); Excluded(service); thrown.release = true;
    assert(pending.get() == EditorResult::write); callback_gate = nullptr; Idle(service);
    // Startup cancellation/teardown: never wait with either launcher mutex, and do not leak lease.
    Gate startup; callback_gate = &startup;
    assert(service.network_editor_connection(true) == EditorResult::ok);
    Until([&] { return startup.entered.load(); }); Excluded(service);
    auto closing = std::async(std::launch::async, [&] { service.StopNetworkJobs(); });
    Until([&] { return discovery_stop.load(); }); Excluded(service); startup.release = true; closing.get(); callback_gate = nullptr; Idle(service);
    assert(!service.reserve_network_job());
    // Atomic USB/network claims: exactly one wins; no writer can slip in after the USB drain.
    EdenServices races({}, rooms);
    for (int i = 0; i < 12; ++i) {
        std::barrier start(2); pe::ui::NetworkReservation network; bool usb_started = false;
        std::thread n([&] { start.arrive_and_wait(); network = races.reserve_network_job(); });
        std::thread u([&] { start.arrive_and_wait(); usb_started = races.begin_usb_backup(); });
        n.join(); u.join(); assert(bool(network) != usb_started);
        if (usb_started) { Until([&] { return !races.usb_backup_status().busy; }); assert(!races.reserve_network_job()); races.end_usb_backup(); }
        network.reset(); assert(!races.network_job_busy() && !races.usb_active_);
    }
    assert(races.can_launch_game()); races.will_play("synthetic.nsp");
    assert(races.launch_committed_ && !races.reserve_network_job() && !races.begin_usb_backup());
    std::puts("Network/UI/USB composition PASS: pre-dispatch leases, actual controller/catalog/Editor/writer, masked private IME, final callback/Stop/Disconnect joins, failure/cancel/foreign-token release, launch/path/profile exclusion, atomic USB races. HOST ONLY; IME review NOT ACCEPTED.");
}
