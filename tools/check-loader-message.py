#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Oliv3rmoon
# SPDX-License-Identifier: GPL-3.0-or-later
"""Own-frontend loader diagnostics, adapted from ProsperoEden dae49cf.

Exercises the actual formatter under UBSan and checks bounded call-site wiring.
Does not read vendor/sources/eden or claim its enum/API/native qualification.
"""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
main = (root / 'headless/main.cpp').read_text()
start = main.index('                if (loaded != Core::SystemResultStatus::Success) {')
branch = main[start:main.index('throw std::runtime_error(text);', start)]
assert 'Loader::GetResultStatusString(result)' in branch
assert 'status >= base && status - base <= static_cast<unsigned>(Loader::ResultStatus::ErrorIntegrityVerificationFailed)' in branch
assert 'revision < 0x20' in branch
assert 'keys.HasKey(Core::Crypto::S128KeyType::Master, revision)' in branch
assert 'Eden::LoadFailure::Text(status, base, message, key_problem, newest_master_key)' in branch
assert branch.index('Core::SystemResultStatus::ErrorVideoCore') < branch.index('const unsigned status')
code = r'''
#include <cassert>
#include <cstdio>
#include "load_failure.h"
using Eden::LoadFailure::Text;
int main() {
    const std::string area = "The key area key could not be found.";
    assert(Text(28, 7, area, true, 0x0f) == "Loader status 28 (21): " + area +
           " The keys file (prod.keys) lacks a key this game needs. The newest master key in it is master_key_0f;"
           " a game made for a newer system version needs keys from that version. See stderr.log.");
    assert(Text(20, 7, "No production keys", true, -1) ==
           "Loader status 20 (13): No production keys. The keys file (prod.keys) lacks a key this game needs."
           " It holds no master key. See stderr.log.");
    assert(Text(9, 7, "There is no program.", false, 3) ==
           "Loader status 9 (2): There is no program. Check this ROM, its keys and firmware; see stderr.log.");
    assert(Text(4, 7, "", false, -1) ==
           "Loader status 4. Check this ROM, its keys and firmware; see stderr.log.");
    assert(Text(999, 7, "", false, -1) ==
           "Loader status 999. Check this ROM, its keys and firmware; see stderr.log.");
    std::puts("LOADER_MESSAGE_FORMATTER_PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='encore-loader-message-') as tmp:
    exe = Path(tmp) / 'loader-message'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra',
                    '-Werror', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                    '-I', str(root / 'headless'), '-x', 'c++', '-', '-o', str(exe)],
                   input=code, text=True, check=True)
    subprocess.run([str(exe)], check=True, timeout=30)
print('Frontend formatter/wiring checked; vendor enum/API and PS5 execution not qualified.')
