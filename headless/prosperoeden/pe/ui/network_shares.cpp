// SPDX-License-Identifier: GPL-3.0-or-later
#include "pe/ui/launcher.hpp"
#include <algorithm>

namespace pe::ui {
namespace {
using Eden::Network::EditorResult;
using Eden::Network::Field;
constexpr int kRows = 8;
const char* Label(Field field) {
    switch (field) {
    case Field::name: return TR("Location name");
    case Field::protocol: return TR("Protocol");
    case Field::server: return TR("Server IPv4");
    case Field::share: return TR("Share / export");
    case Field::folder: return TR("Games folder");
    case Field::updates: return TR("Updates / DLC folder");
    case Field::enabled: return TR("Enabled");
    case Field::port: return TR("Server port");
    case Field::user: return TR("SMB user");
    case Field::password: return TR("SMB password");
    case Field::domain: return TR("SMB domain");
    case Field::encrypt: return TR("SMB encryption");
    case Field::uid: return TR("NFS UID");
    case Field::gid: return TR("NFS GID");
    case Field::mount_port: return TR("NFS mount port");
    case Field::timeout: return TR("Timeout (ms)");
    case Field::cache: return TR("Cache (MiB)");
    }
    return "";
}
const char* Help(Field field) {
    switch (field) {
    case Field::protocol: return TR("SMB2/3 or NFS v3 over TCP. Changing protocol clears the share and SMB credentials in this draft.");
    case Field::server: return TR("Numeric IPv4 only. No server is contacted while you edit.");
    case Field::share: return TR("SMB: share name only. NFS: absolute export path, for example /games.");
    case Field::folder: return TR("Relative to the share / export. Leave empty to use its root. No links or .. components.");
    case Field::updates: return TR("Optional, relative to Games folder. Scans this folder and one child level for matching updates / DLC.");
    case Field::password: return TR("Enter replaces the password in a masked system keyboard. Square clears it; Triangle restores the saved password.");
    case Field::user: return TR("Empty user selects SMB2 guest. Clear password, domain and encryption too. Signed sign-in requires a user.");
    case Field::encrypt: return TR("Required encryption needs SMB3 and a user. Unsupported servers fail; there is no silent downgrade.");
    case Field::uid: case Field::gid: return TR("NFS v3 AUTH_SYS numeric identity (0–2147483647). Configure read-only access on your server.");
    case Field::mount_port: return TR("0 asks the server portmapper for mountd. Otherwise use a fixed TCP mount port (1–65535).");
    case Field::timeout: return TR("Per-operation timeout, 250–15000 milliseconds. Scans also have their own bounded read budget.");
    case Field::cache: return TR("Read-only cache, 1–32 MiB per connected location. No whole-game downloads.");
    default: return TR("Changes stay in this draft until Save. Cancel leaves the saved configuration unchanged.");
    }
}
bool Saved(EditorResult result) { return result == EditorResult::ok || result == EditorResult::saved_sync_warning; }
}

void Launcher::network_result(EditorResult result) {
    network_view_ = services_.network_editor_view();
    message_ = Eden::Network::EditorMessage(result);
    network_warning_ = result != EditorResult::ok && result != EditorResult::cancelled;
    if (result != EditorResult::ok && result != EditorResult::cancelled) cue(audio::Cue::notify);
}
void Launcher::open_network_shares() {
    network_page_ = NetworkPage::locations; network_row_ = 0; network_remove_id_.clear();
    network_result(services_.network_editor_open());
}
void Launcher::start_network_job(std::function<EditorResult(NetworkReservation)> operation, bool commit, bool connection) {
    if (network_job_.valid()) return;
    try {
        auto reservation = services_.reserve_network_job(); // BEFORE dispatch, visible to USB/launch/path/profile
        if (!reservation) { network_result(EditorResult::blocked); return; }
        network_stop_ = false; network_commit_job_ = commit; network_connection_job_ = connection;
        network_job_ = std::async(std::launch::async,
            [reservation = std::move(reservation), operation = std::move(operation)]() mutable {
                return operation(std::move(reservation)); // worker consumes/releases only after its last join/callback
            });
    } catch (...) {
        // std::async can fail before there is a future. Capture destruction still releases admission.
        network_commit_job_ = network_connection_job_ = false;
        network_result(EditorResult::unavailable);
    }
}
void Launcher::update_network_shares() {
    if (!network_job_.valid() || network_job_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    EditorResult result;
    try { result = network_job_.get(); } catch (...) { result = EditorResult::unavailable; }
    network_result(result);
    if (network_connection_job_) rescan_ = true;
    if (network_commit_job_ && Saved(result)) {
        network_page_ = NetworkPage::locations; network_row_ = 0; rescan_ = true;
        if (result == EditorResult::ok) message_ = tr("Network settings saved.");
    }
    network_commit_job_ = network_connection_job_ = false;
}
void Launcher::press_network_shares(Key key) {
    if (network_job_.valid()) { if (key == Key::circle) network_stop_ = true; return; }
    if (network_page_ == NetworkPage::discard || network_page_ == NetworkPage::remove) {
        if (key == Key::circle) { network_page_ = network_view_.editing ? NetworkPage::editor : NetworkPage::locations; message_.clear(); }
        else if (key == Key::cross) {
            if (network_page_ == NetworkPage::discard) {
                services_.network_editor_cancel(); network_view_ = services_.network_editor_view();
                network_page_ = NetworkPage::locations; network_row_ = 0; message_.clear();
            } else {
                start_network_job([this, id = network_remove_id_](NetworkReservation job) {
                    return services_.network_remove_reserved(std::move(job), id, network_stop_);
                }, true);
            }
        }
        return;
    }
    if (key == Key::circle) {
        if (network_page_ == NetworkPage::editor) {
            if (network_view_.dirty) { network_page_ = NetworkPage::discard; message_.clear(); }
            else { services_.network_editor_cancel(); network_view_ = services_.network_editor_view(); network_page_ = NetworkPage::locations; network_row_ = 0; }
        } else { services_.network_editor_cancel(); close_modal(); }
        return;
    }
    const int fields = static_cast<int>(network_view_.rows.size());
    const int locations = static_cast<int>(network_view_.locations.size());
    const bool editing = network_page_ == NetworkPage::editor;
    const int count = editing ? fields + 3 : locations + 3;
    if (key == Key::up || key == Key::down || key == Key::l1 || key == Key::r1) {
        const int step = key == Key::up ? -1 : key == Key::down ? 1 : key == Key::l1 ? -kRows : kRows;
        network_row_ = std::clamp(network_row_ + step, 0, count - 1); cue(audio::Cue::focus); return;
    }
    if (!editing) {
        if (key == Key::square && network_row_ < locations) {
            network_remove_id_ = network_view_.locations[network_row_].id; network_page_ = NetworkPage::remove; message_.clear(); return;
        }
        if (key != Key::cross) return;
        if (network_row_ <= locations) {
            const std::string id = network_row_ < locations ? network_view_.locations[network_row_].id : "";
            // Resolve on the UI thread only, and only on creation. Existing/user names remain data.
            const auto name = id.empty() ? tr(std::string{Eden::Network::DefaultNetworkLocationName()}) : std::string{};
            const auto result = services_.network_editor_begin_named(id, name);
            network_result(result);
            if (result == EditorResult::ok) { network_page_ = NetworkPage::editor; network_row_ = 0; }
        } else {
            start_network_job([this, connect = network_row_ == locations + 1](NetworkReservation job) {
                return services_.network_connection_reserved(std::move(job), connect, network_stop_);
            }, false, true);
        }
        return;
    }
    if (network_row_ < fields) {
        const Field field = network_view_.rows[network_row_].field;
        if (field == Field::password && (key == Key::square || key == Key::triangle)) {
            network_result(services_.network_editor_password(key == Key::square ? Eden::Network::PasswordChoice::clear : Eden::Network::PasswordChoice::keep)); return;
        }
        if (key != Key::cross && key != Key::left && key != Key::right) return;
        if (field == Field::protocol || field == Field::enabled || field == Field::encrypt) {
            network_result(services_.network_editor_change(field));
            network_row_ = std::min(network_row_, static_cast<int>(network_view_.rows.size()) - 1);
        } else if (key == Key::cross) {
            // No catalog access is retained by the provider, snapshot, callback or worker.
            auto prompts = Eden::Network::PrepareKeyboardPrompts([](std::string_view key) { return tr(std::string{key}); });
            if (!prompts) { network_result(EditorResult::invalid); return; }
            start_network_job([this, field, prompts = *prompts](NetworkReservation job) {
                return services_.network_enter_reserved(std::move(job), field, network_stop_, prompts);
            }, false);
        }
    } else if (key == Key::cross) {
        if (network_row_ == fields + 2) { press_network_shares(Key::circle); return; }
        start_network_job([this, reconnect = network_row_ == fields + 1](NetworkReservation job) {
            return services_.network_save_reserved(std::move(job), reconnect, network_stop_);
        }, true);
    }
}

void Launcher::draw_network_shares(Canvas& c, float open) {
    const bool editing = network_page_ == NetworkPage::editor;
    c.list.push_opacity(open);
    glass(c, {340, 72, 1240, 936}, th().radius_panel, th().panel.with_alpha(0.98f), th().panel_edge);
    text(c, tr(editing ? (network_view_.existing ? TR("Edit network location") : TR("Add network location")) : TR("Network shares")),
         384, 136, th().size_heading, th().title);
    text_block(c, tr("Read-only SMB / NFS · Up to four locations · Built-in private keyboard"), 384, 181, th().size_small, 28, th().copy, 1152, 1);
    if (network_page_ == NetworkPage::discard || network_page_ == NetworkPage::remove) {
        const bool remove = network_page_ == NetworkPage::remove;
        text(c, tr(remove ? TR("Remove this network location?") : TR("Discard unsaved changes?")), 384, 340, th().size_heading, th().title);
        if (remove) {
            const auto found = std::find_if(network_view_.locations.begin(), network_view_.locations.end(), [&](const auto& location) { return location.id == network_remove_id_; });
            if (found != network_view_.locations.end()) text_fit(c, found->name, 384, 300, th().size_text, th().accent_pale, 1152);
        }
        text_block(c, tr(remove ? TR("Only its saved connection settings are removed. Files on the server and local saves are not deleted.") :
            TR("The saved network configuration will not change.")), 384, 400, th().size_text, 36, th().copy, 1152, 3);
        static constexpr Hint confirm[] = {{Pad::cross, TR("Confirm")}, {Pad::circle, TR("Keep editing / back")}};
        draw_menu_hints(c, confirm, 2, 384, 960, th().copy, 1152);
        if (network_job_.valid()) notice(c, tr("Working… Circle cancels before save is committed."), 384, 820, th().size_small, th().accent_pale, 1152, false);
        else if (!message_.empty()) notice_block(c, tr(message_), 384, 820, th().size_small, 28, th().warning, 1152, 2, true);
    } else {
        const auto status = services_.network_shares();
        const int fields = static_cast<int>(network_view_.rows.size());
        const int locations = static_cast<int>(network_view_.locations.size());
        const int count = editing ? fields + 3 : locations + 3;
        const int first = std::clamp(network_row_ - kRows + 1, 0, std::max(0, count - kRows));
        for (int i = first; i < std::min(count, first + kRows); ++i) {
            const float y = 222 + (i - first) * 66.0f;
            plate(c, row_plate(), {384, y, 1152, 58}, i == network_row_ ? 1.0f : 0.0f);
            std::string label, value;
            if (editing && i < fields) {
                const auto& row = network_view_.rows[i]; label = tr(Label(row.field));
                value = row.value.empty() ? tr("Not set") : row.value;
                if (row.field == Field::password || row.field == Field::protocol || row.field == Field::enabled || row.field == Field::encrypt) value = tr(value);
            } else if (!editing && i < locations) {
                const auto& location = network_view_.locations[i]; label = location.name;
                const auto found = std::find_if(status.shares.begin(), status.shares.end(), [&](const auto& s) { return s.id == location.id; });
                value = found != status.shares.end() && found->scanning ? tr("Connecting and scanning…") :
                    found != status.shares.end() && found->connected ? fill(tr("Connected · {0} games"), {std::to_string(found->games)}) : tr("Disconnected");
            } else label = tr(editing ? (i == fields ? TR("Save (no connection)") : i == fields + 1 ? TR("Save and reconnect") : TR("Cancel editing")) :
                (i == locations ? TR("Add network location") : i == locations + 1 ? TR("Reconnect saved locations") : TR("Disconnect / cancel")));
            text_fit(c, label, 402, y + 38, th().size_text, th().title, value.empty() ? 1090 : 430);
            text_fit(c, value, 1518, y + 38, th().size_text, th().value, 630, Align::right);
        }
        if (count > kRows) text_fit(c, fill(tr("{0} / {1} · L1 / R1 to page"), {std::to_string(network_row_ + 1), std::to_string(count)}),
            1536, 780, th().size_small, th().meta, 1152, Align::right);
        const char* help = editing && network_row_ < fields ? Help(network_view_.rows[network_row_].field) :
            editing ? TR("Save never contacts a server. Save and reconnect explicitly reloads and connects enabled locations.") :
            TR("Choose a location to edit, or Add network location. Square removes the selected location after confirmation.");
        std::string detail = message_;
        bool warning = network_warning_;
        if (detail.empty() && !editing && network_row_ < locations) {
            const auto& location = network_view_.locations[network_row_];
            const auto found = std::find_if(status.shares.begin(), status.shares.end(), [&](const auto& s) { return s.id == location.id; });
            if (found != status.shares.end()) { detail = found->error; warning = !detail.empty(); }
        }
        if (network_job_.valid()) { detail = tr("Working… Circle cancels before save is committed."); warning = false; }
        if (detail.empty()) text_block(c, tr(help), 384, 820, th().size_small, 28, th().copy, 1152, 2);
        else notice_block(c, tr(detail), 384, 820, th().size_small, 28, warning ? th().warning : th().accent_pale, 1152, 2, warning);
        text_block(c, tr("Credentials are private plaintext in config/network-shares.json (0600), not encrypted. Do not share this file or staged copies."),
            384, 883, th().size_small, 27, th().meta, 1152, 2);
        static constexpr Hint hints[] = {{Pad::updown, TR("Choose")}, {Pad::cross, TR("Edit / select")}, {Pad::circle, TR("Back / cancel")}};
        static constexpr Hint password[] = {{Pad::cross, TR("Replace")}, {Pad::square, TR("Clear")}, {Pad::triangle, TR("Keep saved")}, {Pad::circle, TR("Back")}};
        const bool secret = editing && network_row_ < fields && network_view_.rows[network_row_].field == Field::password;
        draw_menu_hints(c, secret ? password : hints, secret ? 4 : 3, 384, 972, th().copy, 1152);
    }
    c.list.pop_opacity();
}
} // namespace pe::ui
