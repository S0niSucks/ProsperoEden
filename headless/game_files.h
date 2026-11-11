// SPDX-FileCopyrightText: Copyright 2026 Oliv3rmoon
// SPDX-License-Identifier: GPL-3.0-or-later
// File selection for the nine extensions recognized by Encore's game picker:
// NSP/XCI, their NCZ-compressed NSZ/XCZ variants, and direct NRO/NSO/NCA/NAX/KIP.
// Direct inputs can require external program contents and are not guaranteed
// to boot. Internal .ncz, NSO LZ4 and ZBIC/zstd are not standalone games.
#pragma once
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

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
// Eden also has dedicated NRO, NSO, NCA, NAX and KIP loaders. These may be
// standalone homebrew, program partitions or system components, not necessarily
// complete game containers. Keep them selectable; let the native core loader
// determine whether enough content exists to boot the selection.
inline bool IsDirectLoaderFile(std::string_view name) {
    return EndsWithNoCase(name, ".nro") || EndsWithNoCase(name, ".nso") ||
           EndsWithNoCase(name, ".nca") || EndsWithNoCase(name, ".nax") ||
           EndsWithNoCase(name, ".kip");
}
inline bool IsGameFile(std::string_view name) {
    return IsPackageFile(name) || IsCardImageFile(name) || IsDirectLoaderFile(name);
}

// Adapted from Swordpdf/ProsperoEden PR #286 (Oliv3rmoon). Keep a selected
// ExeFS folder's identity instead of saving every such title as just "main".
// This is lexical naming only: it does not widen ValidRomFilename or the
// symlink-free admission checks below. Development files outside the ROM root
// retain the previous basename fallback.
inline std::string RememberedRomName(const std::filesystem::path& selected,
                                     const std::filesystem::path& roms) {
    const auto relative = selected.lexically_relative(roms);
    if (!relative.empty() && !relative.is_absolute()) {
        bool safe = true;
        for (const auto& part : relative)
            if (part == "." || part == "..") safe = false;
        if (safe) return relative.generic_string();
    }
    return selected.filename().generic_string();
}

// A deconstructed ExeFS title uses a directory containing both "main" and
// "main.npdm". The canonical launcher selection is ONLY <folder>/main:
// neither arbitrary nested paths nor '..' are valid persisted ROM entries.
inline bool IsDeconstructedGameSelection(std::string_view name) {
    constexpr std::string_view suffix{"/main"};
    if (!name.ends_with(suffix)) return false;
    const std::string_view folder = name.substr(0, name.size() - suffix.size());
    if (folder.empty() || folder == "." || folder == ".." ||
        folder.find_first_of("/\\") != std::string_view::npos) return false;
    return true;
}

// Symlink-free detection avoids treating arbitrary ROM directories or
// symlink targets outside the selected storage root as launchable titles.
inline bool IsDeconstructedGameDirectory(const std::filesystem::path& folder) {
    namespace fs = std::filesystem;
    std::error_code error;
    const auto directory_status = fs::symlink_status(folder, error);
    if (error || !fs::is_directory(directory_status) || fs::is_symlink(directory_status))
        return false;
    for (const char* name : {"main", "main.npdm"}) {
        error.clear();
        const auto status = fs::symlink_status(folder / name, error);
        if (error || !fs::is_regular_file(status) || fs::is_symlink(status))
            return false;
    }
    return true;
}
} // namespace Eden
