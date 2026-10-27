#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""ASET bounds and the launcher's key-free NRO / encrypted-container separation."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
services = (root / 'headless/prosperoeden/pe/ui/services.hpp').read_text()
start = services.index('inline bool ready_for_game(')
ready = services[start:services.index('\n}\n', start) + 3]
code = r'''
#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include "nro_assets.h"
#include "game_files.h"
struct Home { bool setup_ready = false, homebrew_ready = false; };
''' + ready + r'''
using namespace Eden::NroAssets;
void Put(std::vector<unsigned char>& bytes, std::size_t at, std::uint64_t value, unsigned size) {
    for (unsigned n = 0; n < size; ++n) bytes.at(at + n) = static_cast<unsigned char>(value >> (n * 8));
}
std::vector<unsigned char> Fixture() {
    std::vector<unsigned char> bytes(0x80 + 0x38 + 16 + 0x4000 + 32);
    std::memcpy(bytes.data() + 0x10, "NRO0", 4);
    Put(bytes, 0x18, 0x80, 4);
    std::memcpy(bytes.data() + 0x80, "ASET", 4);
    Put(bytes, 0x88, 0x38, 8); Put(bytes, 0x90, 16, 8);
    Put(bytes, 0x98, 0x48, 8); Put(bytes, 0xa0, 0x4000, 8);
    Put(bytes, 0xa8, 0x4048, 8); Put(bytes, 0xb0, 32, 8);
    return bytes;
}
Layout Check(const std::vector<unsigned char>& bytes, bool short_read = false) {
    return Read(bytes.size(), [&](unsigned char* out, std::size_t count, std::uint64_t at) {
        assert(count <= 0x80); // metadata validation never requests icon- or file-sized reads
        if (short_read || at > bytes.size() || count > bytes.size() - at) return std::size_t{0};
        std::memcpy(out, bytes.data() + at, count); return count;
    });
}
int main() {
    const auto valid = Fixture();
    auto layout = Check(valid);
    assert(layout.valid && layout.icon.offset == 0xb8 && layout.nacp.size == 0x4000);
    assert(layout.romfs.offset == 0x40c8);
    assert(!Check(valid, true).valid);
    auto bytes = valid;
    bytes.resize(0x70); assert(!Check(bytes).valid);
    bytes = valid; bytes[0x10] = 'X'; assert(!Check(bytes).valid);
    bytes = valid; Put(bytes, 0x18, 0x70, 4); assert(!Check(bytes).valid);
    bytes = valid; Put(bytes, 0x18, 0xffffffffu, 4); assert(!Check(bytes).valid);
    bytes = valid; bytes[0x80] = 'X'; assert(!Check(bytes).valid);
    bytes = valid; Put(bytes, 0x84, 1, 4); assert(!Check(bytes).valid);
    for (auto at : {0x88, 0x98, 0xa8}) {
        bytes = valid; Put(bytes, at, 0, 8); assert(!Check(bytes).valid);
        bytes = valid; Put(bytes, at, std::numeric_limits<std::uint64_t>::max(), 8); assert(!Check(bytes).valid);
        bytes = valid; Put(bytes, at + 8, std::numeric_limits<std::uint64_t>::max(), 8); assert(!Check(bytes).valid);
    }
    bytes = valid; Put(bytes, 0xa0, 0x3fff, 8); assert(!Check(bytes).valid);
    bytes = valid; Put(bytes, 0x98, 0x38, 8); assert(!Check(bytes).valid); // overlapping assets
    bytes = valid; bytes.resize(0x80 + 0x38 + 1024 * 1024 + 1);
    Put(bytes, 0x90, 1024 * 1024 + 1, 8); assert(!Check(bytes).valid);
    bytes = valid; Put(bytes, 0x90, 0, 8); assert(Check(bytes).valid); // icon optional
    bytes = valid; Put(bytes, 0xa0, 0, 8); assert(Check(bytes).valid); // NACP optional
    bytes = valid; bytes.resize(0x80); assert(!Check(bytes).valid); // bare NRO still loads, no metadata
    Home home;
    assert(!ready_for_game(home, "Demo.NRO")); // no storage is not a key-free setup
    home.homebrew_ready = true;
    assert(ready_for_game(home, "Tools/Demo.NRO"));
    for (const auto* file : {"Game.nsp", "Game.xci", "Game.nsz", "Game.xcz", "Demo.nro.nsp", "Demo.nro/"})
        assert(!ready_for_game(home, file));
    home.setup_ready = true;
    assert(ready_for_game(home, "Game.nsp"));
}
'''
with tempfile.TemporaryDirectory() as tmp:
    exe = Path(tmp) / 'nro-assets'
    subprocess.run([os.environ.get('CXX', 'clang++-18'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I', str(root / 'headless'), '-x', 'c++', '-',
                    '-o', str(exe)], input=code, text=True, check=True)
    subprocess.run([str(exe)], check=True)
print('NRO assets: PASS bounded reads, truncated/overflowing/overlapping assets, unsupported '
      'versions, absent metadata and key-free homebrew without bypassing container setup (ASan/UBSan)')
