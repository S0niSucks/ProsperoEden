// SPDX-FileCopyrightText: Copyright 2026 Oliv3rmoon
// SPDX-License-Identifier: GPL-3.0-or-later
// The game files ProsperoEden plays: NSP and XCI, and NSZ and XCZ, the same compressed by nsz
// (headless/ncz.h reads what they hold back as it was), plus NRO homebrew loaded by Eden's NRO loader.
#pragma once
#include <cctype>
#include <string_view>

namespace Eden {
inline bool EndsWithNoCase(std::string_view name, std::string_view suffix) {
    if (name.size() < suffix.size()) return false;
    name.remove_prefix(name.size() - suffix.size());
    for (std::size_t i = 0; i < suffix.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(name[i])) != suffix[i]) return false;
    return true;
}
// Read as an NSP: a package of NCAs.
inline bool IsPackageFile(std::string_view name) {
    return EndsWithNoCase(name, ".nsp") || EndsWithNoCase(name, ".nsz");
}
// Read as an XCI: a game card's image.
inline bool IsCardImageFile(std::string_view name) {
    return EndsWithNoCase(name, ".xci") || EndsWithNoCase(name, ".xcz");
}
inline bool IsHomebrewFile(std::string_view name) { return EndsWithNoCase(name, ".nro"); }
// A copied Switch homebrew tree stays intact below roms/switch/. No flattening or moving files.
inline bool ValidHomebrewPath(std::string_view name) {
    if (!name.starts_with("switch/") || !IsHomebrewFile(name) || name.size() > 511) return false;
    unsigned depth = 0;
    for (std::size_t at = 0; at < name.size();) {
        const auto slash = name.find('/', at);
        const auto end = slash == std::string_view::npos ? name.size() : slash;
        const auto part = name.substr(at, end - at);
        if (part.empty() || part.front() == '.' || part.size() > 255 || ++depth > 10) return false;
        for (unsigned char c : part)
            if (c < 32 || c == 127 || c == '\\' || c == ':') return false;
        if (slash == std::string_view::npos) return true;
        at = slash + 1;
    }
    return false;
}
inline bool IsGameFile(std::string_view name) {
    return IsPackageFile(name) || IsCardImageFile(name) || IsHomebrewFile(name);
}
} // namespace Eden
