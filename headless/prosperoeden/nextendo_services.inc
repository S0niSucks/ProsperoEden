// SPDX-License-Identifier: GPL-3.0-or-later
// Included in eden_services.cpp: the same bridge/profile selection as saves and launch settings.
Eden::Nextendo::Status EdenServices::nextendo_status() {
    const std::lock_guard lock(bridge_);
    if (!Eden::FilesystemAccess()) return {false, false, false, 0, "Storage access is required for Nextendo."};
    return Eden::Nextendo::Store(Eden::ConfigDir() + "/nextendo").Get(CurrentProfile(user_));
}
Eden::Nextendo::Status EdenServices::nextendo_action(const std::string& profile, Eden::Nextendo::Action action,
                                                   const std::atomic<bool>& stop) {
    if (nextendo_busy_.exchange(true)) return {false, false, false, 0, "A Nextendo action is already running."};
    struct Busy {
        std::atomic<bool>& value;
        ~Busy() { value = false; }
    } busy{nextendo_busy_};
    {
        const std::lock_guard lock(bridge_);
        if (!Eden::FilesystemAccess()) return {false, false, false, 0, "Storage access is required for Nextendo."};
        if (profile != CurrentProfile(user_))
            return {false, false, false, 0, "The player profile changed. Open Nextendo again before retrying."};
        const auto phase = rooms_.GetSnapshot().phase;
        if (action == Eden::Nextendo::Action::enable && phase != Eden::Multiplayer::Phase::Idle)
            return {false, false, false, 0, "Leave the local room before enabling Nextendo."};
    }
    return Eden::Nextendo::Store(Eden::ConfigDir() + "/nextendo", Eden::Nextendo::CheckAccountHttp).Apply(profile, action, stop);
}
