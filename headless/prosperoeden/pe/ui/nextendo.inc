// SPDX-License-Identifier: GPL-3.0-or-later
// Included by multiplayer.cpp to reuse the established row/focus/frame vocabulary.
void Launcher::press_nextendo(Key key) {
    if (nextendo_request_.valid()) return;
    if (key == Key::up || key == Key::down) {
        room_row_ = std::clamp(room_row_ + (key == Key::down ? 1 : -1), 0, 3);
        nextendo_unlink_confirm_ = false;
        nextendo_import_confirm_ = false;
        cue(Cue::focus);
        return;
    }
    if (key != Key::cross) return;
    using Eden::Nextendo::Action;
    Action action = Action::import_session;
    if (room_row_ == 0 && nextendo_status_.linked && !nextendo_import_confirm_) {
        nextendo_import_confirm_ = true;
        cue(Cue::open);
        return;
    }
    if (room_row_ == 1) action = nextendo_status_.enabled ? Action::disable : Action::enable;
    if (room_row_ == 2) action = Action::check_account;
    if (room_row_ == 3) {
        if (!nextendo_unlink_confirm_) { nextendo_unlink_confirm_ = true; cue(Cue::open); return; }
        action = Action::unlink;
    }
    nextendo_unlink_confirm_ = false;
    nextendo_import_confirm_ = false;
    nextendo_stop_ = false;
    nextendo_status_.checked = false;
    nextendo_status_.error.clear();
    try {
        // Bind the local player on the UI thread, before the worker can be delayed by scheduling.
        const auto profiles = services_.profiles();
        std::string profile;
        for (std::size_t i = 0; i < profiles.size(); ++i)
            if (profiles[i].playing) { profile = services_.profile_key(static_cast<int>(i)); break; }
        nextendo_request_ = std::async(std::launch::async, [this, action, profile] {
            return services_.nextendo_action(profile, action, nextendo_stop_);
        });
    } catch (const std::exception&) {
        nextendo_status_.error = "Could not start the Nextendo action. Retry.";
    }
}

void Launcher::draw_nextendo(Canvas& c) {
    const auto& t = th();
    const bool busy = nextendo_request_.valid();
    draw_frame(c, tr("Nextendo (experimental)"),
               tr(nextendo_status_.enabled ? TR("Service routing on") : TR("Service routing off")));
    const Rect pane = t.top_nav ? Rect{960.0f, 110.0f, 864.0f, 870.0f} : kPane;
    const float x = pane.x;
    const float w = pane.w;
    text_fit(c, tr(nextendo_status_.linked ? TR("Account linked to this profile") : TR("No account linked")),
             x, baseline(pane.y, 42, t.size_heading), t.size_heading, t.title, w);
    text_block(c, tr("Community services, not local rooms. Game sign-in and PS5 online play are not yet verified."),
               x, baseline(pane.y + 54, 28, 22), 22, 28, t.copy, w, 2, kShrink);
    const char* labels[]{nextendo_import_confirm_ ? TR("Confirm replace") :
        nextendo_status_.linked ? TR("Replace linked session") : TR("Import my session"),
        nextendo_status_.enabled ? TR("Turn routing off") : TR("Turn routing on"),
        TR("Check account"), nextendo_unlink_confirm_ ? TR("Confirm unlink") : TR("Unlink account")};
    for (int row = 0; row < 4; ++row) {
        const Rect r{x, pane.y + 142 + row * 86.0f, w, 74};
        const float focus = row == room_row_ && !rail_focus_ ? 1.0f : 0.0f;
        plate(c, row_plate(), r, focus);
        text_fit(c, tr(labels[row]), r.x + 28, baseline(r.y, r.h, t.size_text), t.size_text,
                 busy ? t.meta : t.title, r.w - 56);
    }
    const char* help = TR("Import your existing Citron-Nextendo session as config/nextendo/import.txt (private mode 0600). Import keeps routing off. Configure verified endpoints in nextendo/routes.json before enabling. See docs/features/NEXTENDO.md.");
    text_block(c, tr(help), x, baseline(pane.y + 510, 28, t.size_small), t.size_small, 28, t.copy, w, 5, kShrink);
    const std::string result = busy ? std::string(tr("Working... Circle cancels.")) :
        !nextendo_status_.error.empty() ? nextendo_status_.error :
        nextendo_status_.checked ? std::string(tr("Account accepted. This is not a game connection test.")) :
        std::string(tr("Native browser linking needs a registered Nextendo client. No phone-code API is assumed."));
    text_block(c, result, x, baseline(pane.y + 680, 28, t.size_small), t.size_small, 28,
               nextendo_status_.error.empty() ? t.copy : t.warning, w, 4, kShrink);
    static constexpr Hint hints[]{{Pad::updown, TR("Browse")}, {Pad::cross, TR("Select")}, {Pad::circle, TR("Back / cancel")}};
    draw_footer(c, hints, 3);
}
