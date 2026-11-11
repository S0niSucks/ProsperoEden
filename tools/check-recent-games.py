#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Oliv3rmoon
# SPDX-License-Identifier: GPL-3.0-or-later
"""Adapt PR #286 to Encore's strict ExeFS history contract; actual C++ + UBSan.

History may retain <folder>/main, but this does not admit arbitrary nested games,
traversal or network paths. No native or console execution is claimed.
"""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
main = (root / 'headless/main.cpp').read_text()
assert 'Eden::RememberedRomName(guest, Eden::GamesDir())' in main
for name in ('SaveLastGame', 'SaveRecentGame'):
    assert f'Eden::{name}(filename)' in main
settings = (root / 'headless/settings_store.h').read_text()
start = settings.index('inline bool ValidRomFilename(std::string_view name) {')
validator = settings[start:settings.index('\n}', start) + 2]
code = r'''
#include <cassert>
#include <cstdio>
#include "game_files.h"
namespace Eden {
VALIDATOR
}
int main() {
    const auto name = [](const char* selected) {
        return Eden::RememberedRomName(selected, "/data/eden/roms");
    };
    assert(name("/data/eden/roms/Game.nsp") == "Game.nsp");
    assert(name("/data/eden/roms/My Game/main") == "My Game/main");
    assert(name("/data/eden/roms/Other/main") == "Other/main");
    assert(name("/data/eden/roms/My Game/main") != name("/data/eden/roms/Other/main"));
    assert(Eden::ValidRomFilename(name("/data/eden/roms/My Game/main")));
    assert(Eden::ValidRomFilename(name("/data/eden/roms/Game.NSZ")));
    assert(name("/data/eden/roms-old/Other.nsp") == "Other.nsp");
    assert(name("/elsewhere/Game.nsp") == "Game.nsp");
    // Normalization must not smuggle a multi-component or traversal selection
    // through the existing persisted-name validator.
    assert(!Eden::ValidRomFilename(name("/data/eden/roms/A/B/main")));
    assert(!Eden::ValidRomFilename(name("/data/eden/roms/A/../main")));
    assert(!Eden::ValidRomFilename("../Game.nsp"));
    assert(!Eden::ValidRomFilename("A/../main"));
    assert(!Eden::ValidRomFilename("A\\main"));
    assert(!Eden::ValidRomFilename("pe-net://share/Game.nsp"));
    std::puts("RECENT_EXEFS_IDENTITY_PASS");
}
'''.replace('VALIDATOR', validator)
with tempfile.TemporaryDirectory(prefix='encore-history-') as tmp:
    exe = Path(tmp) / 'recent-games'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra',
                    '-Werror', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                    '-I', str(root / 'headless'), '-x', 'c++', '-', '-o', str(exe)],
                   input=code, text=True, check=True)
    subprocess.run([str(exe)], check=True, timeout=30)
