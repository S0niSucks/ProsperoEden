// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor.h"
#include "prosperoeden/pe/core/strings.hpp" // No catalog access or UI link dependency in the backend.
#include <algorithm>
#include <charconv>
#include <limits>
#include <sstream>
#include <unistd.h>

namespace Eden::Network {
namespace {
EditorResult Map(ConfigError error) {
    switch (error) {
    case ConfigError::none: return EditorResult::ok;
    case ConfigError::invalid: return EditorResult::imported_invalid;
    case ConfigError::unsafe: return EditorResult::unsafe;
    case ConfigError::conflict: return EditorResult::conflict;
    case ConfigError::cancelled: return EditorResult::cancelled;
    case ConfigError::sync: return EditorResult::saved_sync_warning;
    default: return EditorResult::write;
    }
}
bool Clean(std::string_view value, std::size_t max) {
    return value.size() <= max && std::none_of(value.begin(), value.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
bool ValidName(std::string_view value) {
    if (value.empty() || !Clean(value, 256)) return false;
    // New default labels and custom keyboard providers must be bounded, scalar UTF-8 too.
    for (std::size_t i = 0; i < value.size();) {
        const auto first = static_cast<unsigned char>(value[i++]);
        unsigned code = first, count = 0, minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { code = first & 31; count = 1; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { code = first & 15; count = 2; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { code = first & 7; count = 3; minimum = 0x10000; }
        else if (first >= 0x80) return false;
        while (count > 0) {
            --count;
            if (i == value.size() || (static_cast<unsigned char>(value[i]) & 0xc0) != 0x80) return false;
            code = (code << 6) | (static_cast<unsigned char>(value[i++]) & 63);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
    }
    return true;
}
bool Duplicate(const std::vector<Share>& shares, const Share& draft, const std::string& old) {
    return std::any_of(shares.begin(), shares.end(), [&](const auto& s) {
        return s.id != old && (s.id == draft.id || (s.protocol == draft.protocol && s.server == draft.server &&
            s.port == draft.port && s.share == draft.share && s.folder == draft.folder));
    });
}
bool SmbField(Field field) { return field == Field::user || field == Field::password || field == Field::domain || field == Field::encrypt; }
bool NfsField(Field field) { return field == Field::uid || field == Field::gid || field == Field::mount_port; }
bool Applicable(Field field, const Share& s) { return !(s.protocol == "smb" ? NfsField(field) : SmbField(field)); }
std::string Text(const Share& s, Field field) {
    switch (field) {
    case Field::name: return s.name;
    case Field::server: return s.server;
    case Field::share: return s.share;
    case Field::folder: return s.folder;
    case Field::updates: return s.updates_dir;
    case Field::user: return s.user;
    case Field::domain: return s.domain;
    case Field::port: return std::to_string(s.port);
    case Field::mount_port: return std::to_string(s.mount_port);
    case Field::uid: return std::to_string(s.uid);
    case Field::gid: return std::to_string(s.gid);
    case Field::timeout: return std::to_string(s.timeout_ms);
    case Field::cache: return std::to_string(s.cache_bytes >> 20);
    default: return {}; // in particular: passwords have no text getter
    }
}
bool Number(Field f) { return f == Field::port || f == Field::mount_port || f == Field::uid || f == Field::gid || f == Field::timeout || f == Field::cache; }
bool Assign(Share& s, Field f, const std::string& value) {
    if (!Clean(value, f == Field::folder || f == Field::updates ? 2048 : f == Field::server ? 15 : 256)) return false;
    if (Number(f)) {
        unsigned number{};
        const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || number > 2147483647) return false;
        switch (f) {
        case Field::port: if (!number || number > 65535) return false; s.port = number; break;
        case Field::mount_port: if (number > 65535) return false; s.mount_port = number; break;
        case Field::uid: s.uid = number; break;
        case Field::gid: s.gid = number; break;
        case Field::timeout: if (number < 250 || number > 15000) return false; s.timeout_ms = number; break;
        case Field::cache: if (number < 1 || number > 32) return false; s.cache_bytes = static_cast<std::size_t>(number) << 20; break;
        default: return false;
        }
        return true;
    }
    switch (f) {
    case Field::name: if (!ValidName(value)) return false; s.name = value; break;
    case Field::server: { Share check = s; check.server = value; check.share = s.protocol == "smb" ? "games" : "/games";
        check.folder.clear(); check.updates_dir.clear(); check.user.clear(); check.domain.clear(); check.password.clear(); check.encrypt = false;
        if (!Valid(check)) return false;
        s.server = value; break; }
    case Field::share:
        if (s.protocol == "smb" ? (!SafePath(value) || value.find('/') != value.npos) :
            (value != "/" && (!value.starts_with('/') || !SafePath(std::string_view{value}.substr(1))))) return false;
        s.share = value; break;
    case Field::folder: if (!SafePath(value, true)) return false; s.folder = value; break;
    case Field::updates: if (!SafePath(value, true)) return false; s.updates_dir = value; break;
    case Field::user: s.user = value; break;
    case Field::domain: s.domain = value; break;
    case Field::password: s.password = value; break;
    default: return false;
    }
    return true;
}
}
std::string EditorMessage(EditorResult result) {
    switch (result) {
    case EditorResult::ok: return {};
    case EditorResult::cancelled: return TR("Cancelled. The saved network settings were not changed.");
    case EditorResult::unavailable: return TR("The network settings keyboard is unavailable.");
    case EditorResult::invalid: return TR("Invalid value. Use numeric IPv4, safe paths, valid ports, timeout 250–15000 ms and cache 1–32 MiB. SMB credentials require a user.");
    case EditorResult::required: return TR("Server IPv4 and share / export are required. Enter them before saving.");
    case EditorResult::duplicate: return TR("This network location already exists. Edit it instead.");
    case EditorResult::limit: return TR("At most four network locations can be saved.");
    case EditorResult::blocked: return TR("Network settings are unavailable while another protected job or a game is starting.");
    case EditorResult::no_edit: return TR("Reopen the network location before editing.");
    case EditorResult::unsafe: return ConfigurationError(ConfigError::unsafe);
    case EditorResult::conflict: return ConfigurationError(ConfigError::conflict);
    case EditorResult::imported_invalid: return ConfigurationError(ConfigError::invalid);
    case EditorResult::saved_sync_warning: return ConfigurationError(ConfigError::sync);
    default: return ConfigurationError(ConfigError::write);
    }
}
EditorResult Editor::Open(const std::string& path) {
    std::lock_guard lock(mutex); ++generation; draft = {}; editing = dirty = existing = replaced = false;
    selected.clear(); file = path; source = ReadConfiguration(file, true); return error = Map(source.error);
}
EditorResult Editor::Begin(const std::string& id) {
    return Begin(id, DefaultNetworkLocationName());
}
std::string_view DefaultNetworkLocationName() { return TR("Network location"); }
EditorResult Editor::Begin(const std::string& id, std::string_view initial_name) {
    std::lock_guard lock(mutex);
    if (source.error != ConfigError::none) return error = Map(source.error);
    if (id.empty() && !ValidName(initial_name)) return error = EditorResult::invalid;
    ++generation; draft = {}; selected = id; existing = !id.empty(); replaced = false;
    if (existing) {
        const auto found = std::find_if(source.shares.begin(), source.shares.end(), [&](const auto& s) { return s.id == id; });
        if (found == source.shares.end()) return error = EditorResult::no_edit;
        draft = *found;
    } else {
        if (source.shares.size() >= 4) return error = EditorResult::limit;
        static std::atomic<std::uint32_t> sequence{0};
        do {
            std::ostringstream id;
            id << "share-" << std::hex << static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count())
               << '-' << static_cast<unsigned>(getpid()) << '-' << ++sequence;
            draft.id = id.str(); // do not reuse a deleted location's URI identity for another NAS
        } while (std::any_of(source.shares.begin(), source.shares.end(), [&](const auto& s) { return s.id == draft.id; }));
        draft.name = initial_name;
    }
    editing = true; dirty = false; return error = EditorResult::ok;
}
EditorView Editor::View() const {
    std::lock_guard lock(mutex); EditorView v;
    v.id = draft.id; v.editing = editing; v.dirty = dirty; v.existing = existing; v.error = error;
    for (const auto& s : source.shares) v.locations.push_back({s.id, s.name, s.protocol});
    if (!editing) return v;
    for (auto f : {Field::name, Field::protocol, Field::server, Field::share, Field::folder, Field::updates, Field::enabled,
                   Field::port, Field::user, Field::password, Field::domain, Field::encrypt, Field::uid, Field::gid,
                   Field::mount_port, Field::timeout, Field::cache}) {
        if (!Applicable(f, draft)) continue;
        auto value = Text(draft, f);
        if (f == Field::protocol) value = draft.protocol == "smb" ? TR("SMB") : TR("NFS v3 / AUTH_SYS");
        if (f == Field::enabled) value = draft.enabled ? "On" : "Off";
        if (f == Field::encrypt) value = draft.encrypt ? TR("Required (SMB3)") : TR("Off (signing required with a user)");
        if (f == Field::password) value = draft.password.empty() ? "Not set" : replaced ? TR("Replacement set") : TR("Set / keep existing");
        v.rows.push_back({f, std::move(value)});
    }
    return v;
}
EditorResult Editor::Change(Field f) {
    std::lock_guard lock(mutex);
    if (!editing || !Applicable(f, draft)) return error = EditorResult::no_edit;
    if (f == Field::protocol) {
        draft.protocol = draft.protocol == "smb" ? "nfs" : "smb";
        draft.port = draft.protocol == "smb" ? 445 : 2049;
        draft.share.clear(); draft.user.clear(); draft.password.clear(); draft.domain.clear(); draft.encrypt = false; replaced = false;
    } else if (f == Field::enabled) draft.enabled = !draft.enabled;
    else if (f == Field::encrypt) draft.encrypt = !draft.encrypt;
    else return error = EditorResult::invalid;
    dirty = true; ++generation; return error = EditorResult::ok;
}
EditorResult Editor::Password(PasswordChoice choice) {
    std::lock_guard lock(mutex);
    if (!editing || draft.protocol != "smb") return error = EditorResult::no_edit;
    draft.password.clear(); replaced = false;
    if (choice == PasswordChoice::keep && existing) {
        const auto found = std::find_if(source.shares.begin(), source.shares.end(), [&](const auto& s) { return s.id == selected; });
        if (found != source.shares.end()) draft.password = found->password;
    }
    dirty = true; ++generation; return error = EditorResult::ok;
}
EditorResult Editor::Enter(Field f, const std::atomic<bool>& stop, const PrivateKeyboard& keyboard) {
    std::uint64_t ticket; PrivateText text{};
    {
        std::lock_guard lock(mutex);
        if (!editing || !Applicable(f, draft) || f == Field::protocol || f == Field::enabled || f == Field::encrypt)
            return error = EditorResult::no_edit;
        ticket = generation; text.field = f; text.initial = Text(draft, f);
        text.password = f == Field::password; text.numbers = Number(f);
        text.limit = text.numbers ? 10 : f == Field::folder || f == Field::updates ? 2048 : f == Field::server ? 15 : 256;
    }
    PrivateAnswer answer;
    try { if (!stop) answer = keyboard(text, stop); else answer.result = EditorResult::cancelled; }
    catch (...) { answer.result = EditorResult::unavailable; } // never propagate keyboard exception text
    std::lock_guard lock(mutex);
    if (stop || ticket != generation || !editing) return EditorResult::cancelled;
    if (answer.result != EditorResult::ok) return error = answer.result;
    if (!Assign(draft, f, answer.text)) return error = EditorResult::invalid;
    replaced |= f == Field::password; dirty = true; ++generation; return error = EditorResult::ok;
}
EditorResult Editor::Save(const std::atomic<bool>& stop) {
    std::lock_guard lock(mutex);
    if (!editing) return error = EditorResult::no_edit;
    if (draft.server.empty() || draft.share.empty()) return error = EditorResult::required;
    if (!Valid(draft) || !Clean(draft.name, 256)) return error = EditorResult::invalid;
    if (Duplicate(source.shares, draft, selected)) return error = EditorResult::duplicate;
    auto shares = source.shares;
    if (existing) *std::find_if(shares.begin(), shares.end(), [&](const auto& s) { return s.id == selected; }) = draft;
    else shares.push_back(draft);
    const auto result = Map(WriteConfiguration(file, source, shares, stop));
    if (result == EditorResult::ok || result == EditorResult::saved_sync_warning) {
        source = ReadConfiguration(file); draft = {}; editing = dirty = existing = replaced = false; ++generation;
    }
    return error = result;
}
EditorResult Editor::Remove(const std::string& id, const std::atomic<bool>& stop) {
    std::lock_guard lock(mutex);
    if (source.error != ConfigError::none) return error = Map(source.error);
    auto shares = source.shares;
    if (std::erase_if(shares, [&](const auto& s) { return s.id == id; }) != 1) return error = EditorResult::no_edit;
    const auto result = Map(WriteConfiguration(file, source, shares, stop));
    if (result == EditorResult::ok || result == EditorResult::saved_sync_warning) {
        source = ReadConfiguration(file); draft = {}; editing = dirty = existing = replaced = false; ++generation;
    }
    return error = result;
}
void Editor::Cancel() {
    std::lock_guard lock(mutex); ++generation; draft = {}; editing = dirty = existing = replaced = false; error = Map(source.error);
}
} // namespace Eden::Network
