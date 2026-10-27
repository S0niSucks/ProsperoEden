// SPDX-License-Identifier: GPL-3.0-or-later
#include "pe/ui/launcher.hpp"

namespace pe::ui {
void Launcher::press_network_shares(Key key) {
    if (key == Key::circle) { close_modal(); return; }
    if (key == Key::square) {
        services_.disconnect_network_shares(); rescan_ = true; cue(audio::Cue::back);
    } else if (key == Key::cross) {
        services_.reconnect_network_shares(); rescan_ = true; cue(audio::Cue::select);
    }
}
void Launcher::draw_network_shares(Canvas& c, float open) {
    const auto status = services_.network_shares();
    const Rect panel{480.0f, 146.0f, 960.0f, 788.0f};
    c.list.push_opacity(open);
    glass(c, panel, th().radius_panel, th().panel.with_alpha(0.97f), th().panel_edge);
    text(c, tr("Network shares"), 522.0f, 210.0f, th().size_heading, th().title);
    text_block(c, tr("SMB / NFS · read-only · on-demand reads"), 522.0f, 260.0f,
        th().size_small, 30.0f, th().copy, 876.0f, 1);
    text_block(c, status.file, 522.0f, 304.0f, th().size_small, 28.0f, th().meta, 876.0f, 2);
    if (status.shares.empty()) {
        notice_block(c, status.error.empty() ?
            tr("Add your server, share, folder and private credentials to network-shares.json, then reconnect. See docs/NETWORK_SHARES.md. Only configured servers are contacted.") : status.error,
            522.0f, 406.0f, th().size_text, 38.0f, status.error.empty() ? th().copy : th().warning,
            876.0f, 5, !status.error.empty());
    }
    for (std::size_t index = 0; index < status.shares.size() && index < 4; ++index) {
        const auto& share = status.shares[index];
        const float top = 374.0f + static_cast<float>(index) * 100.0f;
        const std::string state = share.scanning ? tr("Connecting and scanning…") :
            share.connected ? fill(tr("Connected · {0} games"), {std::to_string(share.games)}) : tr("Disconnected");
        text_fit(c, share.name + " / " + share.protocol, 522.0f, top, th().size_text, th().value, 500.0f);
        text_fit(c, state, 1398.0f, top, th().size_small,
            share.connected ? th().accent_pale : th().warning, 350.0f, Align::right);
        text_fit(c, share.error, 522.0f, top + 36.0f, th().size_small, th().warning, 876.0f);
    }
    text_block(c, tr("Reconnect reloads settings and starts a new session. After a connection failure, start the game again. Saves and caches stay on the console."),
        522.0f, 802.0f, th().size_small, 28.0f, th().copy, 876.0f, 2);
    static constexpr Hint hints[] = {{Pad::cross, TR("Reconnect")}, {Pad::square, TR("Disconnect / cancel")},
                                      {Pad::circle, TR("Back")}};
    draw_menu_hints(c, hints, 3, 522.0f, 892.0f, th().copy, 876.0f);
    c.list.pop_opacity();
}
} // namespace pe::ui
