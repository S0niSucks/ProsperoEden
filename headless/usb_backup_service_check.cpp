// SPDX-License-Identifier: GPL-3.0-or-later
// Exact feature service implementation with synthetic launcher/job collaborators, real files.
#include "usb_backup.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <cerrno>
#include <dlfcn.h>
#include <pthread.h>
// Fail the actual OS thread-creation call, not a replacement service implementation.
// Ordinary workers must retain sanitizer bookkeeping: Clang links its interceptor
// into the executable, so RTLD_NEXT alone would incorrectly bypass it for libc.
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define EDEN_CHECK_SANITIZED_THREADS 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define EDEN_CHECK_SANITIZED_THREADS 1
#endif
#if defined(EDEN_CHECK_SANITIZED_THREADS)
extern "C" int __interceptor_pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
#endif
static std::atomic<bool> fail_next_thread{false};
extern "C" int pthread_create(pthread_t* thread, const pthread_attr_t* attributes,
                             void* (*function)(void*), void* argument) {
    if (fail_next_thread.exchange(false)) return EAGAIN;
#if defined(EDEN_CHECK_SANITIZED_THREADS)
    return __interceptor_pthread_create(thread, attributes, function, argument);
#else
    static const auto real = reinterpret_cast<decltype(&pthread_create)>(dlsym(RTLD_NEXT, "pthread_create"));
    assert(real);
    return real(thread, attributes, function, argument);
#endif
}
namespace fs = std::filesystem;
namespace Eden {
constexpr const char* kDataDir = "/work/usb-service/data";
bool FilesystemAccess() { return true; }
std::string GamesDir() { return kDataDir; }
std::string AssetsDir() { return kDataDir; }
bool DirectoryExists(const std::string& path) { return fs::is_directory(path); }
std::string ConfigFile(const std::string& path) { return std::string(kDataDir) + "/config/" + path; }
std::string CoversDir() { return std::string(kDataDir) + "/covers"; }
std::string GamesPath(const std::string& path) { return std::string(kDataDir) + "/" + path; }
namespace UpdateNotice {
std::atomic<unsigned> cancels{}, finishes{}, waits{};
void Cancel() { ++cancels; }
void Finish() { ++finishes; }
void WaitForCheck() { ++waits; }
}
namespace Remote {
struct Paths { std::string config, covers, roms, updates, downloads; };
std::atomic<unsigned> stops{}, starts{};
void Stop() { ++stops; }
bool StopForSnapshot(const std::atomic<bool>& stop) { Stop(); return !stop; }
void Start(const Paths&, bool(*)(const std::string&, const std::string&), std::vector<std::string>(*)(const std::string&)) { ++starts; }
}
}
bool WriteCover(const std::string&, const std::string&) { return true; }
std::vector<std::string> ListFolder(const std::string&) { return {}; }
std::vector<std::pair<std::string, std::string>> ReadPending() { return {}; }
class EdenServices {
public:
    ~EdenServices() { usb_stop_ = true; if (usb_thread_.joinable()) usb_thread_.join(); }
    bool begin_usb_backup();
    Eden::UsbBackup::Status usb_backup_status();
    bool usb_backup_action(const std::string&, const std::string&);
    void cancel_usb_backup();
    void end_usb_backup();
    void ResumeUsbJobs();
    struct Cheat { std::atomic<unsigned> stops{}; void Stop() { ++stops; } } cheat_manager_;
    struct Amiibo { std::atomic<unsigned> stops{}, resumes{}; void Stop() { ++stops; } void AllowStart() { ++resumes; } } amiibo_sync_;
    std::atomic<bool> usb_active_{false}, usb_stop_{false}, pair_stop_{false}, nextendo_busy_{false}, network_mutation_active_{false};
    bool launch_committed_ = false, usb_remote_resume_ = false;
    std::thread usb_thread_, sync_thread_, pair_thread_;
    std::mutex job_admission_mutex_, usb_lock_, bridge_;
    Eden::UsbBackup::Status usb_;
    std::atomic<unsigned> sync_stops{};
    void stop_save_sync() { ++sync_stops; }
    struct SyncJob { std::string profile, file; };
    void StartSync(std::vector<SyncJob>, bool) { assert(!usb_active_); }
};
#include "prosperoeden/usb_backup_services.inc"
Eden::UsbBackup::Status Wait(EdenServices& services) {
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    for (;;) {
        auto status = services.usb_backup_status(); if (!status.busy) return status;
        assert(std::chrono::steady_clock::now() < until); std::this_thread::yield();
    }
}
int main() {
    fs::remove_all("/work/usb-service"); fs::create_directories(std::string(Eden::kDataDir) + "/config");
    std::ofstream(std::string(Eden::kDataDir) + "/config/prosperoeden.json") << "service synthetic settings";
    // The runner mounts this path as an independent tmpfs USB fixture.
    for (auto& entry : fs::directory_iterator("/mnt/usb0")) fs::remove_all(entry.path());
    {
        EdenServices failed;
        failed.nextendo_busy_ = true;
        assert(!failed.begin_usb_backup() && !failed.usb_active_ && !failed.usb_thread_.joinable());
        assert(failed.cheat_manager_.stops == 0 && failed.amiibo_sync_.stops == 0);
        failed.nextendo_busy_ = false;
        failed.network_mutation_active_ = true;
        assert(!failed.begin_usb_backup() && !failed.usb_active_ && !failed.usb_thread_.joinable());
        failed.network_mutation_active_ = false;
        fail_next_thread = true;
        assert(!failed.begin_usb_backup() && !fail_next_thread);
        auto status = failed.usb_backup_status();
        assert(!status.active && !status.busy && status.phase == "failed");
        assert(!failed.usb_active_ && !failed.usb_remote_resume_);
        failed.end_usb_backup();
        assert(Eden::Remote::stops == 0 && Eden::Remote::starts == 0);
        assert(failed.cheat_manager_.stops == 0 && failed.amiibo_sync_.stops == 0 && failed.sync_stops == 0);
        assert(Eden::UpdateNotice::cancels == 0 && Eden::UpdateNotice::finishes == 0);
    }
    EdenServices services;
    assert(services.begin_usb_backup()); assert(!services.begin_usb_backup());
    auto state = Wait(services);
    assert(state.active && state.drives == std::vector<std::string>{"/mnt/usb0"});
    assert(Eden::Remote::stops == 1 && services.sync_stops == 1 && services.cheat_manager_.stops == 1);
    assert(services.amiibo_sync_.stops == 1);
    assert(Eden::UpdateNotice::cancels == 1 && Eden::UpdateNotice::finishes == 1 && Eden::UpdateNotice::waits == 1);
    assert(!services.usb_backup_action("backup", "/mnt/usb0"));
    assert(!services.usb_backup_action("inspect", "/etc"));
    fail_next_thread = true;
    assert(!services.usb_backup_action("inspect", "/mnt/usb0") && !fail_next_thread);
    state = services.usb_backup_status();
    assert(state.active && !state.busy && !state.restart && state.phase == "failed");
    // Failed admission is not an irreversible publication; the same protected
    // session can inspect again without restarting any paused job.
    assert(services.usb_backup_action("inspect", "/mnt/usb0")); state = Wait(services);
    assert(state.phase == "estimate" && state.estimate.files == 1);
    assert(services.usb_backup_action("backup", "/mnt/usb0"));
    assert(!services.usb_backup_action("restore", "/mnt/usb0"));
    state = Wait(services); assert(state.phase == "complete" && fs::exists(state.result + "/COMPLETE"));
    auto backup = state.result;
    services.end_usb_backup(); assert(!services.usb_active_ && Eden::Remote::starts == 1);
    assert(services.amiibo_sync_.resumes == 1);
    assert(services.begin_usb_backup()); Wait(services);
    assert(services.usb_backup_action("inspect", "/mnt/usb0")); Wait(services);
    { const std::lock_guard lock(services.bridge_);
        assert(services.usb_backup_action("backup", "/mnt/usb0")); services.cancel_usb_backup(); }
    state = Wait(services); assert(state.phase == "failed" && state.error.find("Cancelled") != state.error.npos);
    services.end_usb_backup(); assert(Eden::Remote::starts == 2);
    assert(services.amiibo_sync_.resumes == 2);
    assert(services.begin_usb_backup()); Wait(services);
    assert(services.usb_backup_action("list", "/mnt/usb0")); state = Wait(services);
    assert(state.backups.size() == 1 && state.backups.front() == backup);
    assert(!services.usb_backup_action("restore", backup));
    assert(services.usb_backup_action("inspect_restore", backup)); state = Wait(services);
    assert(state.phase == "restore_estimate" && state.estimate.manifest_sha256.size() == 64);
    assert(services.usb_backup_action("restore", backup)); state = Wait(services);
    assert(state.restart && services.usb_active_);
    services.end_usb_backup(); assert(services.usb_active_ && Eden::Remote::starts == 2);
    assert(services.amiibo_sync_.resumes == 2); // never reopen admission for a staged restore
    assert(!services.usb_backup_action("inspect", "/mnt/usb0"));
    assert(fs::exists("/work/usb-service/.data-restores/pending.json"));
    std::cout << "USB service lifecycle/action gating tests passed (synthetic jobs, real filesystem)\n";
}
