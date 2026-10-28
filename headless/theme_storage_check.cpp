// SPDX-License-Identifier: GPL-3.0-or-later
// Actual EdenServices callback + actual NX installer/recovery. Synthetic worker status/capacity
// only; USB prefix is verbatim unless an explicitly reported private overlay was requested.
#include "theme_storage.h"
#include "admission_fixture_contract.inc"
#include "storage_paths.h"
#include "remote/remote.h"
#include "update_notice.h"
#include "prosperoeden/pe/ui/services.hpp"
#include "prosperoeden/pe/ui/scene/nx_install.hpp"
#include <barrier>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;
using namespace Eden::ThemeStorage;
namespace nx = pe::ui::scene::nx;
using pe::ui::scene::Json;
namespace {
enum class Probe { real, supplied, unknown, bad_block, overflow, readonly, block_fallback };
std::atomic<Probe> probe{Probe::supplied};
std::atomic<std::uint64_t> free_bytes{1024 * MiB};
std::atomic<unsigned> probes{0};
bool downloading = false, refreshing = false;
auto update_phase = Eden::UpdateNotice::Phase::idle;
struct Worker {
    bool busy = false;
    struct Snapshot { bool busy; };
    Snapshot Status() const { return {busy}; }
};
std::shared_ptr<Ledger> observed;
std::uint64_t expected_credit = 0;
unsigned writes = 0, syncs = 0;
bool observe = false, fail_manifest = false, fail_sync = false;
void Writing() {
    if (!observe) return;
    assert(observed && observed->held == expected_credit && observed->busy());
    ++writes;
}
std::map<std::string, std::string> Snapshot(const fs::path& root) {
    std::map<std::string, std::string> result;
    if (fs::exists(root)) for (const auto& item : fs::recursive_directory_iterator(root)) if (item.is_regular_file()) {
        std::ifstream in(item.path());
        result[item.path().lexically_relative(root).string()] = {std::istreambuf_iterator<char>(in), {}};
    }
    return result;
}
template<class F> void Rejects(F action) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}
nx::Conversion Plan(const std::string& id) {
    nx::Conversion plan; plan.id = id; plan.name = "Original storage-only fixture";
    plan.files["manifest.json"] = Json{{"schema",1},{"id",id},{"name",plan.name},
        {"api",{{"major",1},{"min_minor",1}}},{"host_contract","launcher-v1.0"},{"state_schema",1},
        {"required_capabilities",Json::array({"themes.scene.v1","actions.launch.v1"})},{"scene","theme.json"}}.dump();
    plan.files["theme.json"] = Json{{"schema",1},{"id",id},{"entry","home"},{"screens",{{"home","home.json"}}}}.dump();
    plan.files["home.json"] = Json{{"nodes",Json::array({{{"id","original"},{"type","rect"},{"w",1920},{"h",1080}}})}}.dump();
    plan.files["import-report.json"] = plan.report.dump();
    return plan;
}
}
namespace Eden::Remote {
std::vector<Download> Downloads() {
    return downloading ? std::vector<Download>{Download{}} : std::vector<Download>{};
}
Status Current() {
    Status result; SourceStatus source; source.refreshing = refreshing; result.sources.push_back(source); return result;
}
}
namespace Eden::UpdateNotice { Progress Poll() { Progress result; result.phase = update_phase; return result; } }

// Method/member declarations come verbatim from EdenServices.h. Other dependencies are narrow
// synthetic status providers. The real production method is included without a rewritten body.
class EdenServices {
public:
    #include "theme_storage_members.inc"
    std::mutex job_admission_mutex_;
    bool launch_committed_ = false;
    std::atomic<bool> usb_active_{false}, network_mutation_active_{false}, nextendo_busy_{false};
    Worker cheat_manager_, amiibo_sync_;
    std::thread sync_thread_, pair_thread_;
    bool begin_usb_backup_admission();
};
#include "prosperoeden/theme_storage_services.inc"
#include "theme_storage_usb_handoff.inc"
#include "theme_storage_default.inc"

extern "C" int __real_fstatvfs(int, struct statvfs*);
extern "C" int __wrap_fstatvfs(int fd, struct statvfs* status) {
    ++probes;
    struct stat info{}; assert(fstat(fd, &info) == 0 && S_ISDIR(info.st_mode));
    char path[256]{}; const auto n = readlink(("/proc/self/fd/" + std::to_string(fd)).c_str(), path, sizeof(path));
    assert(n > 0 && std::string(path, n) == Eden::kDataDir);
    if (probe == Probe::real) return __real_fstatvfs(fd, status);
    if (probe == Probe::unknown) { errno = EIO; return -1; }
    *status = {}; status->f_frsize = 1; status->f_bavail = free_bytes.load();
    if (probe == Probe::bad_block) status->f_frsize = 0;
    if (probe == Probe::overflow) { status->f_frsize = 4096; status->f_bavail = UINT64_MAX; }
    if (probe == Probe::readonly) status->f_flag = ST_RDONLY;
    if (probe == Probe::block_fallback) { status->f_frsize = 0; status->f_bsize = 1; }
    return 0;
}
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" ssize_t __wrap_write(int fd, const void* data, size_t size) { Writing(); return __real_write(fd, data, size); }
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
    Writing(); if (observe) ++syncs;
    if (observe && fail_sync) { fail_sync = false; errno = EIO; return -1; }
    return __real_fsync(fd);
}
extern "C" int __real_linkat(int, const char*, int, const char*, int);
extern "C" int __wrap_linkat(int from, const char* source, int to, const char* target, int flags) {
    Writing();
    // Allow staging the manifest, then interrupt publication with a real resumable journal.
    char path[512]{}; const auto n = readlink(("/proc/self/fd/" + std::to_string(to)).c_str(), path, sizeof(path));
    if (observe && fail_manifest && std::string_view(target) == "manifest.json" && n > 0 &&
        std::string(path, n).find("/launchers/") != std::string::npos) {
        fail_manifest = false; errno = ENOSPC; return -1;
    }
    return __real_linkat(from, source, to, target, flags);
}
extern "C" int __real_unlinkat(int, const char*, int);
extern "C" int __wrap_unlinkat(int fd, const char* name, int flags) { Writing(); return __real_unlinkat(fd, name, flags); }

int main() {
    assert(fs::exists("/.dockerenv"));
    const std::string root = Eden::kDataDir;
    assert(!fs::exists(root)); fs::create_directories(root);
    Eden::FilesystemAccessStatus() = 0;
    EdenServices service;
    auto ledger = service.theme_storage_; assert(!ledger->busy());
    const auto reserve = [&](std::uint64_t bytes = MinRequest) { return service.reserve_theme_import_storage(root, bytes); };
    // Exact-root/bound/access checks deny before any capacity query.
    const auto before = probes.load();
    for (const auto& bad : {root + "/", root + "/launchers", root + "/../prosperoeden", std::string("/tmp"),
                           root + std::string("\0suffix", 7)}) assert(!service.reserve_theme_import_storage(bad, MinRequest));
    for (const auto amount : {std::uint64_t(0), MinRequest - 1, MaxRequest + 1, UINT64_MAX}) assert(!reserve(amount));
    Eden::FilesystemAccessStatus() = -1; assert(!reserve()); Eden::FilesystemAccessStatus() = 0;
    assert(probes == before && !ledger->busy());
    for (auto* flag : {&service.usb_active_, &service.network_mutation_active_, &service.nextendo_busy_}) {
        *flag = true; assert(!reserve()); *flag = false;
    }
    service.launch_committed_ = true; assert(!reserve()); service.launch_committed_ = false;
    service.cheat_manager_.busy = true; assert(!reserve()); service.cheat_manager_.busy = false;
    service.amiibo_sync_.busy = true; assert(!reserve()); service.amiibo_sync_.busy = false;
    for (auto* worker : {&service.sync_thread_, &service.pair_thread_}) {
        *worker = std::thread([] {}); assert(!reserve()); worker->join();
    }
    downloading = true; assert(!reserve()); downloading = false;
    refreshing = true; assert(!reserve()); refreshing = false;
    update_phase = Eden::UpdateNotice::Phase::applying; assert(!reserve()); update_phase = Eden::UpdateNotice::Phase::idle;
    assert(probes == before);
    for (auto bad : {Probe::unknown, Probe::bad_block, Probe::overflow, Probe::readonly}) { probe = bad; assert(!reserve()); }
    probe = Probe::supplied; free_bytes = Headroom + MinRequest - 1; assert(!reserve());
    free_bytes = Headroom + MinRequest; { auto token = reserve(); assert(token && ledger->held == MinRequest); }
    assert(!ledger->busy());
    probe = Probe::block_fallback; { auto token = reserve(); assert(token); }
    probe = Probe::real; { auto token = reserve(); assert(token); }
    probe = Probe::supplied; free_bytes = 1024 * MiB;
    fs::rename(root, root + "-pinned"); fs::create_directory_symlink(root + "-pinned", root);
    assert(!reserve()); fs::remove(root); fs::rename(root + "-pinned", root);
    fs::rename(root, root + "-missing"); assert(!reserve()); fs::rename(root + "-missing", root);
    { auto token = reserve(MaxRequest); assert(token && ledger->held == MaxRequest); assert(!reserve()); }
    assert(!ledger->busy());
    // Cross-instance, concurrent app requests share ONE process ledger, not per-caller credits.
    EdenServices other; assert(other.theme_storage_ == ledger);
    free_bytes = Headroom + 4 * MinRequest;
    std::barrier ready(9), held(9), release(9); std::atomic<unsigned> accepted{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) threads.emplace_back([&, i] {
        ready.arrive_and_wait();
        auto token = (i % 2 ? service : other).reserve_theme_import_storage(root, MinRequest);
        if (token) ++accepted;
        held.arrive_and_wait(); release.arrive_and_wait();
    });
    ready.arrive_and_wait(); held.arrive_and_wait(); assert(accepted == 4 && ledger->held == 4 * MinRequest);
    release.arrive_and_wait(); for (auto& thread : threads) thread.join(); assert(!ledger->busy());
    free_bytes = 1024 * MiB;
    { auto token = reserve(); auto copy = token; token.reset(); assert(ledger->busy()); copy.reset(); assert(!ledger->busy()); }
    { auto instance = std::make_unique<EdenServices>(); auto token = instance->reserve_theme_import_storage(root, MinRequest);
      assert(token); instance.reset(); token.reset(); assert(!ledger->busy()); }
    Rejects([&] { auto token = reserve(); assert(token); throw std::runtime_error("synthetic owner throw"); });
    assert(!ledger->busy());
    // Existing USB session always excludes the app callback. Reverse exclusion is tested
    // only when the extracted production prefix (or explicitly labeled overlay) has the guard.
    assert(service.begin_usb_backup_admission()); assert(!reserve()); service.usb_active_ = false;
#if ADMISSION_THEME_CONTRACT
    { auto token = reserve(); assert(token); assert(!service.begin_usb_backup_admission() && !service.usb_active_); }
    assert(service.begin_usb_backup_admission()); service.usb_active_ = false;
    for (int i = 0; i < 32; ++i) {
        std::barrier start(3), admitted(3), finish(3);
        bool theme_won = false, usb_won = false;
        std::thread theme([&] {
            start.arrive_and_wait(); auto token = reserve(); theme_won = bool(token);
            admitted.arrive_and_wait(); finish.arrive_and_wait();
        });
        std::thread usb([&] {
            start.arrive_and_wait(); usb_won = service.begin_usb_backup_admission();
            admitted.arrive_and_wait(); finish.arrive_and_wait();
        });
        start.arrive_and_wait(); admitted.arrive_and_wait();
        assert(theme_won != usb_won && ledger->busy() == theme_won && service.usb_active_.load() == usb_won);
        finish.arrive_and_wait(); theme.join(); usb.join();
        service.usb_active_ = false; assert(!ledger->busy());
    }
#endif
    DefaultThemeServices defaults;
    const pe::ui::ThemeImportStorage denied = [&](const std::string& where, std::uint64_t bytes) {
        return defaults.reserve_theme_import_storage(where, bytes);
    };
    const auto original = Snapshot(root);
    Rejects([&] { nx::install(root, Plan("default-denied"), true, true, denied); });
    assert(!ledger->busy() && original == Snapshot(root)); // no local fallback token
    unsigned calls = 0;
    const pe::ui::ThemeImportStorage callback = [&](const std::string& where, std::uint64_t bytes) {
        ++calls; assert(where == root && bytes == expected_credit);
        auto token = service.reserve_theme_import_storage(where, bytes);
        if (token) assert(ledger->held == bytes);
        return token;
    };
    observed = ledger; observe = true;
    auto plan = Plan("original-success"); expected_credit = nx::storage_bytes(plan.files);
    assert(nx::install(root, plan, true, true, callback) == plan.id);
    assert(calls == 1 && writes > 0 && syncs > 0 && !ledger->busy());
    assert(fs::exists(root + "/launchers/" + plan.id + "/manifest.json"));
    const auto installed = Snapshot(root + "/launchers/" + plan.id);
    // Callback throw after a genuine token was acquired releases it before propagating.
    auto throwing = Plan("callback-throw"); expected_credit = nx::storage_bytes(throwing.files);
    Rejects([&] { nx::install(root, throwing, true, true, [&](const std::string& where, std::uint64_t bytes)->std::shared_ptr<void> {
        auto token = service.reserve_theme_import_storage(where, bytes); assert(token); throw std::runtime_error("synthetic callback throw");
    }); });
    assert(!ledger->busy() && !fs::exists(root + "/launchers/" + throwing.id));
    auto broken = Plan("write-throw"); expected_credit = nx::storage_bytes(broken.files); fail_sync = true;
    Rejects([&] { nx::install(root, broken, true, true, callback); }); assert(!ledger->busy() && !fail_sync);
    auto cold = Plan("cold-restore"); expected_credit = nx::storage_bytes(cold.files); fail_manifest = true;
    Rejects([&] { nx::install(root, cold, true, true, callback); }); assert(!ledger->busy() && !fail_manifest);
    assert(fs::exists(root + "/launchers/" + cold.id + "/import-pending.json"));
    const auto pending = Snapshot(root); service.usb_active_ = true;
    const auto denied_recovery = nx::recover(root, callback); assert(!denied_recovery.at(0).at("recovered").get<bool>());
    assert(pending == Snapshot(root) && !ledger->busy()); service.usb_active_ = false;
    free_bytes = 0;
    const auto low_recovery = nx::recover(root, callback); assert(!low_recovery.at(0).at("recovered").get<bool>());
    assert(pending == Snapshot(root) && !ledger->busy()); free_bytes = 1024 * MiB;
    const auto old_calls = calls, old_writes = writes;
    const auto recovered = nx::recover(root, callback); assert(recovered.at(0).at("recovered").get<bool>());
    assert(calls == old_calls + 1 && writes > old_writes && !ledger->busy());
    assert(!fs::exists(root + "/launchers/" + cold.id + "/import-pending.json"));
    assert(installed == Snapshot(root + "/launchers/" + plan.id));
    assert(fs::exists(root + "/theme-import/staged")); // release does not pretend bytes disappeared
    observe = false; observed.reset();
    std::cout << "Theme storage PASS: actual method/default-deny; exact root/bounds/access/capacity; concurrent process credits; USB reverse exclusion exercised=" << ADMISSION_THEME_CONTRACT << "; RAII copy/owner/throw/final-write; actual cold recovery and failed-save preservation\n";
}
