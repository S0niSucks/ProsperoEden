// SPDX-License-Identifier: GPL-3.0-or-later
// Included inside RuntimeLauncher::Impl. A Lua boolean cannot authorize a network request.
// The pack may request this host-owned view only during real input, then a separate physical
// confirm is consumed by the host (not forwarded to Lua). Init/update/draw cannot start sync.
bool amiibo_input_context = false, amiibo_confirmation = false;
std::vector<std::string> amiibo_confirmation_ids;
void press_amiibo_confirmation(Key key) {
    if (key == Key::circle) { amiibo_confirmation = false; amiibo_confirmation_ids.clear(); }
    else if (key == Key::cross) {
        if (!services.start_amiibo_sync(amiibo_confirmation_ids, false, true))
            last_error = "Amiibo synchronization could not start";
        amiibo_confirmation = false; amiibo_confirmation_ids.clear(); query_cache.clear();
    }
}
void draw_amiibo_confirmation(gfx::DrawList& list) {
    list.reset_state();
    const auto& t = th();
    list.rounded_rect({0,0,1920,1080},0,t.base.with_alpha(0.98f));
    const Backdrop backdrop;
    Canvas c{list, fonts, covers, backdrop, time};
    text_shrink(c, tr("Amiibo download confirmation"), 300, 230, t.size_display, t.title, 1320);
    text_block(c, tr("This launcher requests public names and optional photos. No tags or keys are read, sent or downloaded."),
        300, 330, t.size_text, 40, t.copy, 1320, 3, kShrink);
    const auto selection = amiibo_confirmation_ids.empty() ? std::string(tr("Public catalog only")) :
        fill(tr("{0} explicitly selected model photos"), {std::to_string(amiibo_confirmation_ids.size())});
    text_shrink(c, selection, 300, 480, t.size_text, t.accent_pale, 1320);
    if (!amiibo_confirmation_ids.empty()) text_shrink(c, amiibo_confirmation_ids.front(), 300, 530, t.size_small, t.copy, 1320);
    text_block(c, tr("Connects to amiiboapi.org; selected photos come from raw.githubusercontent.com. Artwork may have third-party rights. Existing local names, dumps and pictures are kept."),
        300, 610, t.size_small, 36, t.copy, 1320, 4, kShrink);
    const auto binding = cached_preferences.menu_bind;
    constexpr Pad marks[]{Pad::cross, Pad::circle, Pad::square, Pad::triangle};
    const Hint hints[]{{marks[binding[0]], TR("Confirm download")}, {marks[binding[1]], TR("Back")}};
    draw_hints(c, hints, 2, 300, 930, t.copy, 1320);
}
