#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Oliv3rmoon
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual library.json serializer/writer; synthetic files, ASan/UBSan, no vendor reads."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
services = (root / 'headless/prosperoeden/eden_services.cpp').read_text()
scan = services[services.index('std::vector<pe::ui::Game> EdenServices::games(const'):]
scan = scan[:scan.index('\n}\n')]
assert scan.index('Eden::LibraryFile::Write(') > scan.index('games.push_back(std::move(game));')
assert scan.index('Eden::LibraryFile::Write(') < scan.index('lock.unlock();')
assert 'Eden::FilesystemAccess() && !(cancel && cancel->load(std::memory_order_acquire))' in scan
assert 'Eden::GamePath(game.file)' in scan
assert 'Eden::LibraryFile::Text(Eden::AssetsDir(), listed, Eden::GamesDir())' in scan
code = r'''
#include "library_file.h"
#include <cassert>
#include <filesystem>
#include <fstream>
using namespace Eden::LibraryFile;
int main() {
    namespace fs = std::filesystem;
    const std::vector<Game> games{{"Title/main", "/mnt/usb0/eden/roms/Title/main",
                                  "日本語 \"Title\"", 0x0100000000000001, "cover.tga"},
                                 {"Demo.NRO", "/mnt/usb0/eden/roms/Demo.NRO", "Demo", 0, ""},
                                 {"Unknown.nsp", "/roms/Unknown.nsp", std::string("Bad \xff title"), 0, ""}};
    const auto text = Text("/mnt/usb0/eden", games);
    const auto json = nlohmann::json::parse(text);
    assert(json["version"] == 1 && json["app"] == "PPSA99008");
    assert(!json.contains("games_folder")); // original API/schema remains compatible
    const auto independent = nlohmann::json::parse(Text("/mnt/usb0/eden", games, "/mnt/usb1/games"));
    assert(independent["game_files"] == json["game_files"] && independent["games"] == json["games"]);
    assert(independent["games_folder"] == "/mnt/usb1/games");
    assert(json["games"][0]["title_id"] == "0100000000000001");
    assert(json["games"][0]["file"] == "Title/main");
    assert(json["games"][0]["title"] == games[0].title);
    assert(json["games"][1]["title_id"].is_null() && json["games"][1]["cover"].is_null());
    assert(json["games"][2]["title"] == "Bad \xef\xbf\xbd title");
    assert(Write("library.json", text));
    struct stat before{}, after{};
    assert(stat("library.json", &before) == 0 && Write("library.json", text));
    assert(stat("library.json", &after) == 0 && before.st_ino == after.st_ino);
    assert(Write("library.json", Text("/new-root", {})));
    assert(stat("library.json", &after) == 0 && before.st_ino != after.st_ino);
    // Neither old-file reads nor new writes follow symlinks or special files.
    std::ofstream("victim") << "preserve";
    fs::create_symlink("victim", "linked.json");
    assert(!Write("linked.json", text) && fs::is_symlink("linked.json"));
    fs::create_hard_link("victim", "hardlinked.json");
    assert(!Write("hardlinked.json", text));
    assert(mkfifo("pipe.json", 0600) == 0 && !Write("pipe.json", text));
    fs::create_directory("directory.json");
    assert(!Write("directory.json", text));
    assert(!Write("absent/library.json", text));
    std::ifstream victim("victim"); std::string contents; victim >> contents;
    assert(contents == "preserve");
    // A pre-existing predictable legacy .tmp symlink is never opened.
    fs::create_symlink("victim", "library.json.tmp");
    assert(Write("library.json", text) && fs::is_symlink("library.json.tmp"));
    for (const auto& entry : fs::directory_iterator("."))
        assert(!entry.path().filename().string().starts_with("library.json.tmp."));
    assert(!Write("huge.json", std::string((16u << 20) + 1, 'x')));
    std::puts("LIBRARY_JSON_INTERCHANGE_PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='encore-library-file-') as tmp:
    exe = Path(tmp) / 'library-file'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra',
                    '-Werror', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-I', str(root / 'headless'), '-x', 'c++', '-', '-o', str(exe)],
                   input=code, text=True, check=True)
    subprocess.run([str(exe)], cwd=tmp, check=True, timeout=30)
