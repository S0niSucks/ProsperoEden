// SPDX-License-Identifier: GPL-3.0-or-later
// The storage root for game/user-supplied files. Internal storage is /data/prosperoeden by
// default; an external selection changes only this root, never the required subfolder layout.
// Resolved once per process; a new root applies after Encore is reopened.
#pragma once
#include <string>
#include <string_view>

#include "settings_store.h"
#include "storage_paths.h"

namespace Eden {
// One storage root, one schema. Internal storage defaults to /data/prosperoeden; an optional
// external root is saved as a single path and must contain the same keys/, firmware/, roms/,
// updates/, mods/, save-import/, save-export/ and ryujinx/ layout. Without filesystem access the
// app can only see its packaged/sandbox assets/ fallback.
inline std::string ResolveAssetsDir() {
    if (!FilesystemAccess()) return AppFile("assets");
    if (std::string saved = LoadSavedAssetsDir(); !saved.empty())
        return saved;
    return kDefaultAssetsDir;
}

inline const std::string& AssetsDir() {
    static const std::string directory = ResolveAssetsDir();
    return directory;
}
inline std::string AssetsPath(std::string_view child) { return AssetsDir() + "/" + std::string(child); }

// Independent content locations are frozen for the process. Missing USB/media
// is reported as unavailable; never silently switch a game reader to another root.
struct ContentLocations { std::string games, wallpapers, image; };
inline const ContentLocations& ContentDirs() {
    static const ContentLocations snapshot = [] {
        ContentLocations result{AssetsPath("roms"), AssetsPath("wallpapers"), {}};
        if (!FilesystemAccess() || !UserContent::kIoAvailable) return result;
        // One settings document and one initialization point for all fields.
        // Separate lazy statics could pick up a mid-process save on first use.
        const auto document = Settings::Load(SettingsFile());
        const auto games = Settings::String(document, Settings::Json::json_pointer("/storage/games_folder"));
        const auto wallpapers = Settings::String(document, Settings::Json::json_pointer("/storage/wallpapers_folder"));
        const auto image = Settings::String(document, Settings::Json::json_pointer("/storage/wallpaper"));
        if (ValidAssetsDir(games)) result.games = games;
        if (ValidAssetsDir(wallpapers)) result.wallpapers = wallpapers;
        if (UserContent::ImageName(image)) result.image = image;
        return result;
    }();
    return snapshot;
}
inline const std::string& GamesDir() { return ContentDirs().games; }
inline std::string GamePath(std::string_view name) { return GamesDir() + "/" + std::string(name); }
inline const std::string& WallpapersDir() { return ContentDirs().wallpapers; }
inline const std::string& WallpaperName() { return ContentDirs().image; }
} // namespace Eden
