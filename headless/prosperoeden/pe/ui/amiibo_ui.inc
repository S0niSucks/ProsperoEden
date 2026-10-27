// SPDX-License-Identifier: GPL-3.0-or-later
// Included in settings.cpp: retain the launcher's existing typography, panels and key remapping.
void Launcher::open_amiibo() {
    amiibo_confirm_ = false; amiibo_ids_.clear(); amiibo_row_ = 0;
    amiibo_ = services_.amiibo_sync();
    open_modal(Modal::amiibo);
    // An explicit visit may read local names, but never starts network activity.
    if (!amiibo_.busy) (void)services_.start_amiibo_sync({}, true, false);
}
void Launcher::press_amiibo(Key key) {
    amiibo_ = services_.amiibo_sync();
    if (amiibo_.busy) {
        if (key == Key::circle) { services_.cancel_amiibo_sync(); say(tr("Cancelling Amiibo request...")); }
        return;
    }
    if (amiibo_confirm_) {
        if (key == Key::circle) { amiibo_confirm_ = false; return; }
        if (key == Key::cross) {
            amiibo_confirm_ = false;
            if (!services_.start_amiibo_sync(amiibo_ids_, false, true))
                say(tr("Amiibo sync could not start. Check storage access and try again."), true);
        }
        return;
    }
    const int last = 1 + static_cast<int>(amiibo_.entries.size());
    amiibo_row_ = std::clamp(amiibo_row_, 0, last);
    if (key == Key::circle) { close_modal(); return; }
    if (key == Key::up || key == Key::down || key == Key::l1 || key == Key::r1) {
        const int step = key == Key::l1 ? -6 : key == Key::r1 ? 6 : key == Key::up ? -1 : 1;
        amiibo_row_ = std::clamp(amiibo_row_ + step, 0, last); cue(Cue::focus);
    }
    if (key != Key::cross) return;
    if (amiibo_row_ == 1) { (void)services_.start_amiibo_sync({}, true, false); return; }
    amiibo_ids_.clear();
    if (amiibo_row_ >= 2) amiibo_ids_.push_back(amiibo_.entries[static_cast<std::size_t>(amiibo_row_ - 2)].id);
    amiibo_confirm_ = true; // a second, separately labelled press is required for every online run
}
void Launcher::draw_amiibo(Canvas& c, float open) {
    amiibo_ = services_.amiibo_sync();
    const auto& t = th();
    auto& list = c.list;
    list.push_opacity(open);
    constexpr Rect panel{430.0f, 110.0f, 1060.0f, 860.0f};
    glass(c, panel, t.radius_panel, t.panel.with_alpha(0.97f), t.panel_edge.with_alpha(0.66f), 1.6f);
    text_shrink(c, tr("Amiibo"), 472.0f, baseline(140.0f, 64.0f, t.size_display), t.size_display, t.title, 976.0f);
    text_block(c, tr("Public names and optional photos. No tags or keys are read, sent or downloaded."),
               472.0f, 242.0f, t.size_small, 28.0f, t.copy, 976.0f, 2, kShrink);
    const float size = t.size_text;
    if (amiibo_confirm_) {
        text_shrink(c, amiibo_ids_.empty() ? tr("Download the public catalog?") : tr("Download this model's photo?"),
                    472.0f, 360.0f, size, t.title, 976.0f);
        if (!amiibo_ids_.empty()) text_fit(c, amiibo_ids_.front(), 472.0f, 408.0f, t.size_small, t.accent_pale, 976.0f);
        text_block(c, tr("Connects to amiiboapi.org; selected photos come from raw.githubusercontent.com. Artwork may have third-party rights. Existing local names, dumps and pictures are kept."),
                   472.0f, 464.0f, t.size_small, 32.0f, t.copy, 976.0f, 5, kShrink);
        const Hint hints[] = {{Pad::cross, TR("Confirm download")}, {Pad::circle, TR("Back")}};
        draw_menu_hints(c, hints, 2, 472.0f, 920.0f, t.copy, 976.0f);
    } else if (amiibo_.busy) {
        const char* phase = amiibo_.phase == "catalog" ? tr("Downloading public catalog...") :
            amiibo_.phase == "photo" ? tr("Downloading selected photo...") :
            amiibo_.phase == "saving" ? tr("Saving validated cache...") : tr("Checking local catalog...");
        text_shrink(c, phase, 472.0f, 374.0f, size, t.title, 976.0f);
        text_fit(c, std::to_string(amiibo_.done / 1024) + " / " +
            (amiibo_.total ? std::to_string(amiibo_.total / 1024) : "?") + " KiB",
            472.0f, 428.0f, t.size_small, t.copy, 976.0f);
        text_block(c, tr("Cancellation keeps local data. A file already saved stays available offline."),
                   472.0f, 506.0f, t.size_small, 32.0f, t.copy, 976.0f, 3, kShrink);
        const Hint hints[] = {{Pad::circle, TR("Cancel")}};
        draw_menu_hints(c, hints, 1, 472.0f, 920.0f, t.copy, 976.0f);
    } else {
        const int count = 2 + static_cast<int>(amiibo_.entries.size());
        amiibo_row_ = std::clamp(amiibo_row_, 0, count - 1);
        const int first = std::clamp(amiibo_row_ - 3, 0, std::max(0, count - 6));
        for (int row = first; row < std::min(count, first + 6); ++row) {
            const Rect r{472.0f, 302.0f + 69.0f * (row - first), 976.0f, 64.0f};
            plate(c, row_plate(), r, row == amiibo_row_ ? 1.0f : 0.0f);
            std::string label;
            if (row == 0) label = tr("Download / refresh public catalog...");
            else if (row == 1) label = tr("Use offline catalog");
            else {
                const auto& e = amiibo_.entries[static_cast<std::size_t>(row - 2)];
                label = e.name + (e.series.empty() ? "" : " / " + e.series);
            }
            text_shrink(c, label, r.x + 20.0f, baseline(r.y, r.h, size), size, t.title, r.w - 40.0f);
        }
        std::string detail;
        if (amiibo_row_ >= 2) detail = amiibo_.entries[static_cast<std::size_t>(amiibo_row_ - 2)].id + "  /  " + tr("Select to fetch photo; existing art is kept.");
        else detail = fill(tr("{0} public models available offline"), {std::to_string(amiibo_.entries.size())});
        text_shrink(c, detail, 472.0f, 756.0f, t.size_small, t.accent_pale, 976.0f);
        const char* result = amiibo_.phase == "failed" ? tr("Request failed. Use offline catalog or retry.") :
            amiibo_.phase == "cancelled" ? tr("Request cancelled. Local data was kept.") :
            amiibo_.phase == "ready" ? tr("Catalog ready. Photos are fetched only when selected.") : tr("No network activity until you confirm a download.");
        text_shrink(c, result, 472.0f, 800.0f, t.size_small, t.copy, 976.0f);
        if (!amiibo_.error.empty()) text_block(c, amiibo_.error, 472.0f, 834.0f, 18.0f, 24.0f, t.copy, 976.0f, 2, kShrink);
        const Hint hints[] = {{Pad::cross, TR("Select")}, {Pad::l1, TR("Page")}, {Pad::r1, TR("Page")}, {Pad::circle, TR("Back")}};
        draw_menu_hints(c, hints, 4, 472.0f, 920.0f, t.copy, 976.0f);
    }
    list.pop_opacity();
}
