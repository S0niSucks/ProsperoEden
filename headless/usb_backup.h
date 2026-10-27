// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Eden::UsbBackup {
struct Progress {
    std::string phase, path;
    std::uint64_t done = 0, total = 0;
};
using Report = std::function<void(const Progress&)>;
// Publication could not be safely withdrawn: the owner must keep jobs/launch locked until exit.
struct RestorePending : std::runtime_error {
    std::string recovery;
    RestorePending(const std::string& message, std::string recovery)
        : std::runtime_error(message), recovery(std::move(recovery)) {}
};
struct Estimate {
    std::uint64_t bytes = 0, required = 0, available = 0;
    std::size_t files = 0;
    std::string manifest_sha256; // binds a restore confirmation to the validated manifest
};
struct Status {
    bool active = false, busy = false, restart = false;
    std::string phase, error, result;
    Progress progress;
    Estimate estimate;
    std::vector<std::string> drives, backups;
};
// Blocking, bounded operations. The owner quiesces app writers and runs these on its worker.
// Paths are absolute; every component is descriptor-relative and opened without following links.
Estimate Inspect(const std::string& data, const std::string& usb, const std::atomic<bool>& stop);
std::vector<std::string> Drives(const std::string& data, const std::atomic<bool>& stop);
std::string Backup(const std::string& data, const std::string& usb, const std::atomic<bool>& stop, Report report = {});
std::vector<std::string> Backups(const std::string& usb, const std::atomic<bool>& stop);
Estimate InspectRestore(const std::string& data, const std::string& backup, const std::atomic<bool>& stop, Report report = {});
// Creates a durable pending transaction outside data. It does NOT replace active app state.
std::string StageRestore(const std::string& data, const std::string& backup,
                         const std::atomic<bool>& stop, Report report = {}, const std::string& expected_manifest = {});
// Cold-start gate: before settings, migrations, profiles, metadata, downloads or emulator start.
// Returns false on any unsafe/unsupported state; the caller must exit without opening app data.
bool ApplyPending(const std::string& data, std::string& error);
} // namespace Eden::UsbBackup
