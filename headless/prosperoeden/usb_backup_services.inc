// SPDX-License-Identifier: GPL-3.0-or-later
// Included in eden_services.cpp: uses its existing remote cover/lister callbacks.
bool EdenServices::begin_usb_backup() {
    if (!Eden::FilesystemAccess() || launch_committed_ || usb_active_.exchange(true)) return false;
    if (usb_thread_.joinable()) usb_thread_.join();
    usb_stop_ = false;
    { const std::lock_guard lock(usb_lock_); usb_ = {}; usb_.active = usb_.busy = true; usb_.phase = "preparing"; }
    usb_thread_ = std::thread([this] {
        Eden::UsbBackup::Status result; result.active = true; result.phase = "drives";
        try {
            // Join without bridge_ held: the jobs may be waiting for the metadata bridge.
            Eden::UpdateNotice::Cancel();
            Eden::UpdateNotice::Finish();
            Eden::UpdateNotice::WaitForCheck(); // the checker persists its catalog sequence
            cheat_manager_.Stop();
            usb_remote_resume_ = Eden::GamesDir() == Eden::AssetsDir() || Eden::DirectoryExists(Eden::GamesDir());
            Eden::Remote::Stop();
            stop_save_sync();
            if (sync_thread_.joinable()) sync_thread_.join();
            pair_stop_ = true;
            if (pair_thread_.joinable()) pair_thread_.join();
            const std::lock_guard bridge(bridge_); // drain an outstanding library metadata scan
            result.drives = Eden::UsbBackup::Drives(Eden::kDataDir, usb_stop_);
            if (result.drives.empty()) result.error = "No safely accessible USB drive. Connect a writable USB drive and try again.";
        } catch (const std::exception& e) { result.phase = "failed"; result.error = e.what(); }
        const std::lock_guard lock(usb_lock_); usb_ = std::move(result);
    });
    return true;
}
Eden::UsbBackup::Status EdenServices::usb_backup_status() {
    const std::lock_guard lock(usb_lock_); return usb_;
}
bool EdenServices::usb_backup_action(const std::string& action, const std::string& path) {
    std::string expected_manifest;
    {
        const std::lock_guard lock(usb_lock_);
        if (!usb_active_ || usb_.busy || usb_.restart) return false;
        const bool drive = std::find(usb_.drives.begin(), usb_.drives.end(), path) != usb_.drives.end();
        const bool backup = std::find(usb_.backups.begin(), usb_.backups.end(), path) != usb_.backups.end();
        if (!((drive && (action == "inspect" || action == "list" || (action == "backup" && usb_.phase == "estimate"))) ||
              (backup && (action == "inspect_restore" || (action == "restore" && usb_.phase == "restore_estimate"))))) return false;
        // Confirmation is bound to the exact last inspected path, not just any drive/backup.
        if ((action == "backup" || action == "restore") && usb_.result != path) return false;
        expected_manifest = usb_.estimate.manifest_sha256;
        usb_.busy = true; usb_.phase = action; usb_.error.clear(); usb_.result.clear(); usb_.progress = {};
    }
    if (usb_thread_.joinable()) usb_thread_.join();
    usb_stop_ = false;
    usb_thread_ = std::thread([this, action, path, expected_manifest] {
        try {
            const std::lock_guard bridge(bridge_);
            auto report = [this](const Eden::UsbBackup::Progress& p) {
                const std::lock_guard lock(usb_lock_); usb_.progress = p;
            };
            Eden::UsbBackup::Estimate estimate; std::string result, phase; std::vector<std::string> backups;
            if (action == "inspect") { estimate = Eden::UsbBackup::Inspect(Eden::kDataDir, path, usb_stop_); result = path; phase = "estimate"; }
            else if (action == "list") { backups = Eden::UsbBackup::Backups(path, usb_stop_); phase = "backups"; }
            else if (action == "inspect_restore") { estimate = Eden::UsbBackup::InspectRestore(Eden::kDataDir, path, usb_stop_, report); result = path; phase = "restore_estimate"; }
            else if (action == "backup") { result = Eden::UsbBackup::Backup(Eden::kDataDir, path, usb_stop_, report); phase = "complete"; }
            else { result = Eden::UsbBackup::StageRestore(Eden::kDataDir, path, usb_stop_, report, expected_manifest); phase = "restart"; }
            const std::lock_guard lock(usb_lock_);
            usb_.busy = false; usb_.phase = phase; usb_.result = result; usb_.estimate = estimate;
            if (action == "list") usb_.backups = std::move(backups);
            usb_.restart = phase == "restart";
        } catch (const Eden::UsbBackup::RestorePending& e) {
            const std::lock_guard lock(usb_lock_); usb_.busy = false; usb_.restart = true;
            usb_.phase = "restart"; usb_.result = e.recovery; usb_.error = e.what();
        } catch (const std::exception& e) {
            const std::lock_guard lock(usb_lock_); usb_.busy = false; usb_.phase = "failed"; usb_.error = e.what();
        }
    });
    return true;
}
void EdenServices::cancel_usb_backup() { usb_stop_ = true; }
void EdenServices::ResumeUsbJobs() {
    if (usb_remote_resume_)
        Eden::Remote::Start({Eden::ConfigFile("remote"), Eden::CoversDir(), Eden::GamesPath("roms"),
                            Eden::GamesPath("updates"), Eden::GamesPath(".remote-downloads")}, WriteCover, ListFolder);
    std::vector<SyncJob> jobs;
    for (const auto& [profile, file] : ReadPending()) jobs.push_back({profile, file});
    if (!jobs.empty()) StartSync(std::move(jobs), false);
}
void EdenServices::end_usb_backup() {
    if (!usb_active_) return;
    { const std::lock_guard lock(usb_lock_); if (usb_.busy || usb_.restart) return; }
    if (usb_thread_.joinable()) usb_thread_.join();
    usb_active_ = false;
    { const std::lock_guard lock(usb_lock_); usb_ = {}; }
    ResumeUsbJobs();
}
