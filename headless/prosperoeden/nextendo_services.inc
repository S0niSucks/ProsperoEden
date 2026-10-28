// SPDX-License-Identifier: GPL-3.0-or-later
// Included in eden_services.cpp: the same bridge/profile selection as saves and launch settings.
Eden::Nextendo::Status EdenServices::nextendo_status() {
    const std::lock_guard admission(job_admission_mutex_);
    if (usb_active_ || launch_committed_)
        return {false, false, false, 0, "Nextendo storage is unavailable during USB backup or after launch commitment."};
    if (nextendo_busy_) return {false, false, false, 0, "A Nextendo action is already running."};
    std::string profile, root;
    {
        const std::lock_guard lock(bridge_);
        if (!Eden::FilesystemAccess()) return {false, false, false, 0, "Storage access is required for Nextendo."};
        profile = CurrentProfile(user_);
        root = Eden::ConfigDir() + "/nextendo";
    }
    // Keep storage admission, but never wait for Store's mutex with the metadata bridge held.
    return Eden::Nextendo::Store(root).Get(profile);
}
std::string EdenServices::nextendo_link_url() {
    const std::lock_guard lock(bridge_);
    return nextendo_browser_url_;
}
Eden::Nextendo::Status EdenServices::nextendo_action(const std::string& profile, Eden::Nextendo::Action action,
                                                   const std::atomic<bool>& stop) {
    return nextendo_action_with_consent(profile, action, stop, false);
}
Eden::Nextendo::Status EdenServices::nextendo_action_with_consent(const std::string& profile,
    Eden::Nextendo::Action action, const std::atomic<bool>& stop, bool confirmed_game_permissions) {
    std::string bound_profile, root;
    {
        const std::lock_guard admission(job_admission_mutex_);
        if (usb_active_) return {false, false, false, 0, "Close USB backup before changing Nextendo."};
        if (launch_committed_) return {false, false, false, 0, "Nextendo actions are unavailable after launch commitment."};
        if (nextendo_busy_) return {false, false, false, 0, "A Nextendo action is already running."};
        if (stop.load()) return {false, false, false, 0, "Nextendo action cancelled."};
        if (action == Eden::Nextendo::Action::link_browser && !confirmed_game_permissions)
            return {false, false, false, 0, Eden::Nextendo::GameLinkConsentRequired};
        const std::lock_guard lock(bridge_);
        if (!Eden::FilesystemAccess()) return {false, false, false, 0, "Storage access is required for Nextendo."};
        if (profile != CurrentProfile(user_))
            return {false, false, false, 0, "The player profile changed. Open Nextendo again before retrying."};
        const auto phase = rooms_.GetSnapshot().phase;
        if (action == Eden::Nextendo::Action::enable && phase != Eden::Multiplayer::Phase::Idle)
            return {false, false, false, 0, "Leave the local room before enabling Nextendo."};
        bound_profile = profile;
        root = Eden::ConfigDir() + "/nextendo";
        nextendo_busy_ = true; // publication is atomic with USB/launch/path/profile admission
    }
    struct Busy {
        std::atomic<bool>& value;
        ~Busy() { value = false; }
    } busy{nextendo_busy_}; // outlives LinkBrowser's final callback and all account reads/writes
    using namespace Eden::Nextendo;
    Store store(root);
    Status result;
    if (action == Action::link_browser) {
        result = LinkBrowser(store, bound_profile, stop, [this](const std::string& url) {
            const std::lock_guard lock(bridge_);
            nextendo_browser_url_ = url;
        }, confirmed_game_permissions);
    } else if (action == Action::check_account) {
        result = Store(root, CheckAccountHttp).Apply(bound_profile, action, stop);
    } else result = store.Apply(bound_profile, action, stop);
    if (!result.error.empty()) {
        auto current = store.Get(bound_profile); // failed replacement must not hide an existing link/consent
        current.error = result.error;
        return current;
    }
    return result;
}
