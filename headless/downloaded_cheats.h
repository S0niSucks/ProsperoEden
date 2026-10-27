// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string_view>

namespace Eden {
// A reserved, immutable namespace. Each changed download gets a new content-derived name,
// so a previously chosen cheat cannot implicitly opt into new code (including master code).
inline constexpr std::string_view kDownloadedCheatPrefix = "Downloaded cheats - ";
inline bool IsDownloadedCheatMod(std::string_view name) {
    if (!name.starts_with(kDownloadedCheatPrefix)) return false;
    name.remove_prefix(kDownloadedCheatPrefix.size());
    if (name.size() != 16 + 3 + 64 || name.substr(16, 3) != " - ") return false;
    for (size_t i = 0; i < name.size(); ++i) {
        if (i >= 16 && i < 19) continue;
        if (!((name[i] >= '0' && name[i] <= '9') || (name[i] >= 'a' && name[i] <= 'f'))) return false;
    }
    return true;
}
} // namespace Eden
