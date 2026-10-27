#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise portable/explicit wallpaper paths and their console-wide settings (#263)."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
services = (root / 'headless/prosperoeden/eden_services.cpp').read_text()
assert 'ListEntries(Eden::LauncherWallpapersDir(), false, ok)' in services
assert 'EdenServices::wallpapers_folder() { return Eden::LauncherWallpapersDir(); }' in services
code = r'''
#include "wallpaper_paths.h"
#include <cassert>
#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;
int main(int, char** argv) {
    const fs::path base{argv[1]};
    const std::string assets = (base / "assets").string();
    const std::string portable = assets + "/wallpapers";
    const std::string explicit_folder = (base / "other-drive" / "pictures").string();
    using Eden::ResolveLauncherWallpapersDir;
    // Backwards-compatible default, including a file misleadingly named wallpapers.
    assert(ResolveLauncherWallpapersDir(assets, "", true) == Eden::WallpapersDir());
    fs::create_directories(assets);
    std::ofstream(portable) << "not a directory";
    assert(ResolveLauncherWallpapersDir(assets, "", true) == Eden::WallpapersDir());
    fs::remove(portable);
    fs::create_directories(portable);
    assert(ResolveLauncherWallpapersDir(assets, "", true) == portable);
    // An explicitly chosen, missing USB drive does not silently select another folder.
    assert(ResolveLauncherWallpapersDir(assets, explicit_folder, true) == explicit_folder);
    assert(ResolveLauncherWallpapersDir(assets, "../pictures", true) == portable);
    assert(ResolveLauncherWallpapersDir(assets, "/a/../pictures", true) == portable);
    assert(ResolveLauncherWallpapersDir(assets, explicit_folder, false).empty());
    assert(ResolveLauncherWallpapersDir(assets, "", false).empty());

    const std::string file = (base / "settings.json").string();
    assert(Eden::LoadSavedWallpapersDir(file).empty());
    Eden::Settings::Json document{{"wallpapers_folder", explicit_folder}};
    assert(Eden::Settings::WriteWhole(document, file));
    assert(Eden::LoadSavedWallpapersDir(file) == explicit_folder);
    assert(Eden::SavePreferences(Eden::Preferences{}, file));
    assert(Eden::LoadSavedWallpapersDir(file) == explicit_folder);
    // Shared across profiles, unlike the selected image itself.
    document = Eden::Settings::LoadWhole(file);
    document["profiles"] = {{"first", std::string(32, 'a')}, {"current", std::string(32, 'b')}};
    assert(Eden::Settings::WriteWhole(document, file));
    assert(Eden::LoadSavedWallpapersDir(file) == explicit_folder);
    auto preferences = Eden::LoadPreferences(file);
    preferences.wallpaper = "Beach.png";
    assert(Eden::SavePreferences(preferences, file));
    document = Eden::Settings::LoadWhole(file);
    assert(document["wallpapers_folder"] == explicit_folder);
    assert(!document["profile_settings"][std::string(32, 'b')].contains("wallpapers_folder"));
    document["profiles"]["current"] = std::string(32, 'a');
    assert(Eden::Settings::WriteWhole(document, file));
    assert(Eden::LoadSavedWallpapersDir(file) == explicit_folder);
    assert(Eden::LoadPreferences(file).wallpaper.empty());
    for (const auto& invalid : {Eden::Settings::Json("relative"), Eden::Settings::Json("/a/../b"),
                               Eden::Settings::Json(42), Eden::Settings::Json(nullptr)}) {
        document["wallpapers_folder"] = invalid;
        assert(Eden::Settings::WriteWhole(document, file));
        assert(Eden::LoadSavedWallpapersDir(file).empty());
    }
}
'''
with tempfile.TemporaryDirectory() as tmp:
    exe = Path(tmp) / 'wallpaper-paths'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
                    '-I', str(root / 'headless'), '-x', 'c++', '-', '-o', str(exe)],
                   input=code, text=True, check=True)
    subprocess.run([str(exe), tmp], check=True)
print('Wallpaper paths: PASS portable folders, explicit USB paths, access checks and shared settings')
