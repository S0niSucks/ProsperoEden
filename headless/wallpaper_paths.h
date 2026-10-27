// SPDX-License-Identifier: GPL-3.0-or-later
// Portable wallpapers beside the keys/firmware/roms tree, or an explicitly chosen folder (#263).
#pragma once
#include <string>
#include <sys/stat.h>
#include "assets_dir.h"

namespace Eden {
inline std::string ResolveLauncherWallpapersDir(const std::string& assets, const std::string& saved, bool access) {
    if (!access) return {};
    if (ValidAssetsDir(saved)) return saved;
    const std::string portable = assets + "/wallpapers";
    struct stat info{};
    return stat(portable.c_str(), &info) == 0 && S_ISDIR(info.st_mode) ? portable : WallpapersDir();
}

// Read once, not on every rendered frame. Reopen the app after changing the folder setting.
inline const std::string& LauncherWallpapersDir() {
    static const std::string directory = ResolveLauncherWallpapersDir(
        AssetsDir(), LoadSavedWallpapersDir(), FilesystemAccess());
    return directory;
}
} // namespace Eden
