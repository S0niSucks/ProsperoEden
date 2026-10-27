// SPDX-License-Identifier: GPL-3.0-or-later
#include "pe/ui/launcher.hpp"
#include <algorithm>
#include <cstdio>

namespace pe::ui {
namespace {
std::string UsbSize(std::uint64_t bytes) {
    char text[48]; std::snprintf(text, sizeof(text), "%.1f MiB", static_cast<double>(bytes) / 1048576.0); return text;
}
std::string UsbShort(const std::string& path) {
    return path.size() <= 68 ? path : "..." + path.substr(path.size() - 65);
}
}
void Launcher::open_usb_backup() {
    if (!services_.begin_usb_backup()) {
        say(tr("USB backup needs storage access and no game starting."), true); cue(audio::Cue::error); return;
    }
    usb_page_ = "menu"; usb_choice_ = 0; usb_restore_ = false;
    rescan_ = true; // a just-scheduled service scan may be suppressed by the protected session
    usb_rows_.visible = 4; usb_rows_.pitch = 80.0f; usb_rows_.wrap = false;
    usb_rows_.reset(0, 0); usb_backup_ = services_.usb_backup_status();
    open_modal(Modal::usb_backup);
}
void Launcher::poll_usb_backup() {
    if (modal_ != Modal::usb_backup) return;
    usb_backup_ = services_.usb_backup_status();
    if (usb_page_ != "waiting" || usb_backup_.busy) return;
    if (usb_backup_.phase == "estimate" || usb_backup_.phase == "restore_estimate") {
        usb_page_ = "confirm"; usb_choice_ = 0; // safe default: Cancel
    } else if (usb_backup_.phase == "backups") {
        usb_page_ = "backups"; usb_rows_.reset(static_cast<int>(usb_backup_.backups.size()), 0);
    } else usb_page_ = "result";
}
void Launcher::press_usb_backup(Key key) {
    if (usb_backup_.restart) return; // deliberately locked: close via PS5, then cold-start
    if (usb_backup_.busy) {
        if (key == Key::circle) services_.cancel_usb_backup();
        return; // never leave a job or a writer running behind another screen
    }
    if (key == Key::circle) {
        services_.end_usb_backup(); close_modal(); return;
    }
    if (key == Key::up || key == Key::down || key == Key::left || key == Key::right) {
        if (usb_page_ == "drives" || usb_page_ == "backups") usb_rows_.move(key == Key::down ? 1 : -1);
        else usb_choice_ = 1 - usb_choice_;
        cue(audio::Cue::focus); return;
    }
    if (key != Key::cross) return;
    if (usb_page_ == "menu") {
        usb_restore_ = usb_choice_ == 1;
        usb_page_ = "drives"; usb_rows_.reset(static_cast<int>(usb_backup_.drives.size()), 0);
    } else if (usb_page_ == "drives" || usb_page_ == "backups") {
        const auto& paths = usb_page_ == "drives" ? usb_backup_.drives : usb_backup_.backups;
        auto row = usb_rows_.selected;
        if (row < 0 || row >= static_cast<int>(paths.size())) return;
        const auto action = usb_page_ == "backups" ? "inspect_restore" : usb_restore_ ? "list" : "inspect";
        if (services_.usb_backup_action(action, paths[static_cast<std::size_t>(row)])) usb_page_ = "waiting";
    } else if (usb_page_ == "confirm") {
        if (!usb_choice_) { services_.end_usb_backup(); close_modal(); return; }
        if (usb_backup_.estimate.available < usb_backup_.estimate.required) return;
        if (services_.usb_backup_action(usb_restore_ ? "restore" : "backup", usb_backup_.result)) usb_page_ = "waiting";
    } else if (usb_page_ == "result") { services_.end_usb_backup(); close_modal(); return; }
    usb_backup_ = services_.usb_backup_status(); cue(audio::Cue::select);
}
void Launcher::draw_usb_backup(Canvas& c, float open) {
    const auto& t = th(); auto& list = c.list;
    const Rect panel{430.0f, 160.0f, 1060.0f, 760.0f};
    list.push_opacity(open);
    glass(c, panel, t.radius_panel, t.panel.with_alpha(0.97f), t.panel_edge.with_alpha(0.66f), 1.6f);
    text_shrink(c, tr("USB backup"), 478.0f, 235.0f, t.size_display, t.title, 964.0f);
    const char* about = tr("Settings, profiles and all saves. ROMs, logs and caches are excluded.");
    text_block(c, about, 478.0f, 284.0f, t.size_small, 32.0f, t.copy, 964.0f, 2);
    const auto line = [&](const std::string& value, float y, Color color) {
        text_shrink(c, value, 478.0f, y, t.size_small, color, 964.0f);
    };
    const auto row = [&](const std::string& value, int index, bool focused, bool enabled = true) {
        const Rect r{478.0f, 390.0f + index * 80.0f, 964.0f, 70.0f};
        plate(c, row_plate(), r, focused ? 1.0f : 0.0f);
        text_shrink(c, value, r.x + 28.0f, baseline(r.y, r.h, t.size_text), t.size_text,
                    enabled ? t.value : t.faint, r.w - 56.0f);
    };
    if (usb_backup_.restart) {
        line(tr("Restore staged and verified. Close ProsperoEden, then reopen it."), 403.0f, t.accent_pale);
        text_block(c, tr("Do not change app data over FTP. Restore applies before the next startup. Existing files will be kept here:"),
                   478.0f, 458.0f, t.size_text, 38.0f, t.copy, 964.0f, 3);
        text_block(c, usb_backup_.result, 478.0f, 605.0f, t.size_small, 32.0f, t.body, 964.0f, 3);
    } else if (usb_backup_.busy) {
        const auto& p = usb_backup_.progress;
        const char* phase = p.phase == "validating" ? tr("Validating copied data...") :
                            p.phase == "copying" ? tr("Copying app data...") : tr("Preparing and checking data...");
        line(phase, 418.0f, t.accent_pale);
        line(UsbShort(p.path), 478.0f, t.body);
        line(fill(tr("{0} of {1}"), {UsbSize(p.done), UsbSize(p.total)}), 540.0f, t.body);
        const float fraction = p.total ? std::clamp(static_cast<float>(p.done) / p.total, 0.0f, 1.0f) : 0.0f;
        list.rounded_rect({478.0f, 580.0f, 964.0f, 8.0f}, 4.0f, t.row);
        if (fraction > 0.0f) list.rounded_rect({478.0f, 580.0f, 964.0f * fraction, 8.0f}, 4.0f, t.accent);
        line(tr("Cancel stops this job. Keep the USB drive connected until this finishes."), 674.0f, t.copy);
    } else if (usb_page_ == "menu") {
        row(tr("Back up app data"), 0, usb_choice_ == 0);
        row(tr("Restore from a backup"), 1, usb_choice_ == 1);
        line(tr("Storage jobs pause here. Do not change app data over FTP."), 636.0f, t.copy);
    } else if (usb_page_ == "drives" || usb_page_ == "backups") {
        const auto& paths = usb_page_ == "drives" ? usb_backup_.drives : usb_backup_.backups;
        line(usb_page_ == "drives" ? tr("Choose a USB drive") : tr("Choose a completed backup"), 365.0f, t.copy);
        for (int i = usb_rows_.first_row(); i <= usb_rows_.last_row(); ++i)
            row(UsbShort(paths[static_cast<std::size_t>(i)]), i - usb_rows_.first_row(), i == usb_rows_.selected);
        if (paths.empty()) line(tr("No completed backups or accessible drives found. Cancel goes back."), 440.0f, t.copy);
        if (usb_rows_.count > usb_rows_.visible) scrollbar(c, usb_rows_, 1458.0f, 390.0f, 310.0f);
    } else if (usb_page_ == "confirm") {
        const auto& e = usb_backup_.estimate;
        line(UsbShort(usb_backup_.result), 365.0f, t.body);
        line(fill(tr("Data: {0} in {1} files"), {UsbSize(e.bytes), std::to_string(e.files)}), 410.0f, t.body);
        line(fill(tr("Required (with reserve): {0} / Available: {1}"), {UsbSize(e.required), UsbSize(e.available)}), 455.0f, t.body);
        text_block(c, usb_restore_ ? tr("Restore merges backup files at the next startup. Existing files are preserved in a recovery folder. Files absent from the backup stay untouched.") :
                   tr("A new dated folder is created. Existing backups are never overwritten. Keep this drive private: backups may contain personal data and service tokens."),
                   478.0f, 505.0f, t.size_small, 32.0f, t.copy, 964.0f, 3);
        const bool enough = e.available >= e.required;
        // Two deliberate choices below the full estimate, Cancel focused by default.
        for (int i = 0; i < 2; ++i) {
            Rect r{478.0f + i * 494.0f, 666.0f, 470.0f, 72.0f};
            plate(c, row_plate(), r, usb_choice_ == i ? 1.0f : 0.0f);
            text_shrink(c, i == 0 ? tr("Cancel") : !enough ? tr("Not enough space") : usb_restore_ ? tr("Stage restore") : tr("Create backup"),
                        r.x + 24.0f, baseline(r.y, r.h, t.size_text), t.size_text, i == 0 || enough ? t.value : t.faint, r.w - 48.0f);
        }
    } else {
        line(usb_backup_.phase == "complete" ? tr("Backup complete and verified.") : tr("Operation stopped. Existing data was kept."), 410.0f, t.accent_pale);
        text_block(c, usb_backup_.result, 478.0f, 468.0f, t.size_small, 32.0f, t.body, 964.0f, 3);
    }
    if (!usb_backup_.error.empty()) text_block(c, usb_backup_.error, 478.0f, 754.0f, t.size_small, 32.0f, t.copy, 964.0f, 3);
    if (!usb_backup_.restart) {
        const Hint hints[] = {{Pad::circle, TR("Cancel")}, {Pad::cross, TR("Select")}};
        draw_menu_hints(c, hints, usb_backup_.busy ? 1 : 2, 478.0f, 866.0f, t.meta, 964.0f);
    }
    list.pop_opacity();
}
} // namespace pe::ui
