// SPDX-License-Identifier: GPL-3.0-or-later
#include "metadata.h"
#include "shares.h"
#include "game_files.h"
#include <cstdio>
#include <map>
#include <mutex>

namespace Eden::Network {
namespace {
struct Saved { std::weak_ptr<Session> session; Metadata metadata; };
std::mutex saved_mutex;
std::map<std::string, Saved> saved;
}
Metadata CachedMetadata(const std::string& uri) {
    const std::lock_guard lock(saved_mutex);
    const auto found = saved.find(uri);
    if (found != saved.end())
        if (const auto session = found->second.session.lock(); session && session->Healthy()) return found->second.metadata;
    return {};
}
Metadata InspectGame(const std::string& uri, const std::string& keys, const std::string& cover) {
    Metadata result;
    auto opened = Open(uri);
    if (!opened.session) { result.error = "Network game unavailable. Reconnect in Settings."; return result; }
    const auto staged = cover + ".network-new";
    try {
        ReadBudget probe;
        result.content = Eden::IsHomebrewFile(uri) ? EDEN_CONTENT_GAME : eden_rom_content(uri.c_str(), keys.c_str());
        if (result.content == EDEN_CONTENT_UPDATE || result.content == EDEN_CONTENT_DLC) return result;
        char title[513]{};
        const int found = eden_extract_game_metadata(uri.c_str(), keys.c_str(), staged.c_str(), title, sizeof(title));
        result.title_id = eden_game_title_id(uri.c_str());
        if (probe.Exhausted() || !opened.session->Healthy()) {
            (void)std::remove(staged.c_str());
            result = {}; result.error = "Network metadata unavailable or probe budget exceeded. File-name fallback is in use.";
            return result;
        }
        if (found & EDEN_METADATA_TITLE) result.title = title;
        if ((found & EDEN_METADATA_COVER) && std::rename(staged.c_str(), cover.c_str()) == 0) result.cover = cover;
        else (void)std::remove(staged.c_str());
    } catch (...) {
        (void)std::remove(staged.c_str());
        result = {}; result.error = "Network metadata could not be read. File-name fallback is in use.";
        return result;
    }
    // A network game's add-ons come only from its own configured share root. This deliberately
    // clears the previous local/share summary before language/settings consume it.
    const auto roots = AddonRoots(uri);
    ReadBudget addons;
    eden_scan_addons(roots.empty() ? nullptr : roots.front().c_str(), nullptr, keys.c_str());
    if (!addons.Exhausted() && opened.session->Healthy()) {
        char update[64]{};
        eden_game_addons(result.title_id, update, sizeof(update), &result.dlc);
        result.update = update;
    } else {
        eden_scan_addons(nullptr, nullptr, keys.c_str());
        result.error = "Network add-on scan failed or exceeded its budget. Reconnect before starting.";
    }
    {
        const std::lock_guard lock(saved_mutex);
        if (saved.size() >= 16384) saved.clear();
        saved[uri] = {opened.session, result}; // metadata must not keep an old transport alive
    }
    return result;
}
} // namespace Eden::Network
