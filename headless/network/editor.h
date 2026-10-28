// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "config.h"
#include <mutex>

namespace Eden::Network {
enum class Field { name, protocol, server, share, folder, updates, enabled, port, user, password, domain,
                   encrypt, uid, gid, mount_port, timeout, cache };
enum class EditorResult { ok, cancelled, unavailable, invalid, required, duplicate, limit, blocked, no_edit,
                          unsafe, conflict, write, imported_invalid, saved_sync_warning };
enum class PasswordChoice { keep, clear };
struct EditorRow { Field field; std::string value; };
struct LocationSummary { std::string id, name, protocol; };
// Safe built-in view: password presence/state only. Never serialize this API into Lua.
struct EditorView {
    std::vector<LocationSummary> locations;
    std::vector<EditorRow> rows;
    std::string id;
    bool editing = false, dirty = false, existing = false;
    EditorResult error = EditorResult::ok;
};
// Private backend keyboard provider. Only Editor::Enter consumes its answer; UI futures return
// EditorResult, never the entered text. Password entry always starts empty, even when replacing.
struct PrivateText { Field field; std::string initial; std::size_t limit; bool password, numbers; };
struct PrivateAnswer { EditorResult result = EditorResult::unavailable; std::string text; };
using PrivateKeyboard = std::function<PrivateAnswer(const PrivateText&, const std::atomic<bool>&)>;
PrivateAnswer NativeNetworkKeyboard(const PrivateText&, const std::atomic<bool>&);
std::string EditorMessage(EditorResult);
class Editor {
public:
    EditorResult Open(const std::string& file);
    EditorResult Begin(const std::string& id); // empty ID: a new, stable generated identity
    EditorView View() const;
    EditorResult Change(Field field); // protocol/boolean choices only
    EditorResult Password(PasswordChoice choice);
    EditorResult Enter(Field field, const std::atomic<bool>& stop, const PrivateKeyboard& keyboard = NativeNetworkKeyboard);
    EditorResult Save(const std::atomic<bool>& stop);
    EditorResult Remove(const std::string& id, const std::atomic<bool>& stop);
    void Cancel();
private:
    mutable std::mutex mutex;
    std::string file, selected;
    Configuration source;
    Share draft;
    bool editing = false, dirty = false, existing = false, replaced = false;
    std::uint64_t generation = 0;
    EditorResult error = EditorResult::ok;
};
} // namespace Eden::Network
