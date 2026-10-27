// SPDX-License-Identifier: GPL-3.0-or-later
// Inside RuntimePreviewServices. Synthetic controller fixture: no account, file, or network.
std::mutex nextendo_lock;
Eden::Nextendo::Status nextendo;
std::string nextendo_mode;
int nextendo_calls = 0, nextendo_unlinks = 0;
Eden::Nextendo::Status nextendo_status() override {
    std::lock_guard lock(nextendo_lock);
    return nextendo;
}
Eden::Nextendo::Status nextendo_action(const std::string& profile, Eden::Nextendo::Action action, const std::atomic<bool>& stop) override {
    using Eden::Nextendo::Action;
    bool slow;
    {
        std::lock_guard lock(nextendo_lock);
        const auto list = profiles();
        bool selected = false;
        for (std::size_t i = 0; i < list.size(); ++i)
            if (list[i].playing && profile == profile_key(static_cast<int>(i))) selected = true;
        if (!selected) return {false, false, false, 0, "The player profile changed. Open Nextendo again before retrying."};
        ++nextendo_calls;
        slow = nextendo_mode == "slow";
    }
    while (slow && !stop.load()) std::this_thread::sleep_for(std::chrono::milliseconds{2});
    std::lock_guard lock(nextendo_lock);
    nextendo.error.clear();
    if (stop.load()) { nextendo.error = "Nextendo action cancelled."; return nextendo; }
    if (action == Action::import_session) nextendo = {true, false, false, 1, {}};
    if (action == Action::enable) {
        if (!nextendo.linked) nextendo.error = "Import a Nextendo session for this profile first.";
        else nextendo.enabled = true;
    }
    if (action == Action::disable) nextendo.enabled = false;
    if (action == Action::check_account) {
        if (nextendo.linked) nextendo.checked = true;
        else nextendo.error = "Import a Nextendo session for this profile first.";
    }
    if (action == Action::unlink) { nextendo = {}; ++nextendo_unlinks; }
    return nextendo;
}
