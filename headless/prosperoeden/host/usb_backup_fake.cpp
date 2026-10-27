// SPDX-License-Identifier: GPL-3.0-or-later
#include "fake_services.hpp"
namespace pe::host {
bool FakeServices::begin_usb_backup() {
    if (usb_job.active) return false;
    usb_job = {}; usb_job.active = true; usb_job.phase = "drives";
    if (usb_mode == "no_drive") usb_job.error = "Synthetic preview: no accessible USB drive.";
    else usb_job.drives = {"/mnt/usb0", "/mnt/usb1"};
    return true;
}
bool FakeServices::usb_backup_action(const std::string& action, const std::string& path) {
    if (!usb_job.active || usb_job.busy || usb_job.restart) return false;
    usb_job.error.clear(); usb_job.result = path; usb_job.phase = action; usb_action = action;
    usb_polls = 0; usb_job.busy = true; usb_job.progress = {"copying", "user/nand/user/save/profile/title/save.bin", 0, 16000000};
    return true;
}
Eden::UsbBackup::Status FakeServices::usb_backup_status() {
    if (usb_job.busy) {
        ++usb_polls;
        usb_job.progress.done = std::min<std::uint64_t>(16000000, usb_polls * 200000);
        if (usb_polls > (usb_action == "backup" || usb_action == "restore" ? 90 : 3) && usb_mode != "slow") {
            usb_job.busy = false;
            if (usb_mode == "failed" || (usb_mode == "bad_manifest" && usb_action == "inspect_restore")) { usb_job.phase = "failed"; usb_job.error = "Synthetic preview: content validation failed. Existing data was kept."; }
            else if (usb_action == "inspect" || usb_action == "inspect_restore") {
                usb_job.phase = usb_action == "inspect" ? "estimate" : "restore_estimate";
                usb_job.estimate = {16000000, 80000000, usb_mode == "low_space" ? 1000000ULL : 9000000000ULL, 26, {}};
            } else if (usb_action == "list") {
                usb_job.phase = "backups";
                usb_job.backups = {"/mnt/usb0/ProsperoEden-2026-10-27_10-22-00-0", "/mnt/usb0/ProsperoEden-2026-10-26_10-22-00-0"};
            } else if (usb_action == "backup") { ++usb_backups; usb_job.phase = "complete"; usb_job.result = "/mnt/usb0/ProsperoEden-2026-10-27_11-00-00-0"; }
            else { ++usb_restores; usb_job.phase = "restart"; usb_job.restart = true; usb_job.result = "/data/.prosperoeden-restores/restore-2026-10-27_11-00-00-0/previous"; }
        }
    }
    return usb_job;
}
void FakeServices::cancel_usb_backup() {
    if (!usb_job.busy) return;
    ++usb_cancels; usb_job.busy = false; usb_job.phase = "failed";
    usb_job.error = "Synthetic preview: cancelled. Existing data was not replaced.";
}
} // namespace pe::host
