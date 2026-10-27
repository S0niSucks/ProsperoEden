#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real job/cache/installer with synthetic transport and identity; no guest or network."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='eden-cheat-manager-build-') as tmp:
    binary = Path(tmp) / 'check'
    for sanitizer in ('', 'address,undefined', 'thread'):
        flags = ['-fsanitize=' + sanitizer, '-fno-omit-frame-pointer'] if sanitizer else []
        subprocess.run([os.environ.get('CXX', 'clang++-18'), '-std=c++20', '-O1', '-g', '-UNDEBUG',
                        '-Wall', '-Wextra', '-Werror', *flags, '-I', str(root / 'headless'),
                        *[str(root / 'headless' / name) for name in ('cheat_manager_check.cpp',
                          'cheat_manager.cpp', 'cheat_store.cpp', 'cheat_catalog.cpp', 'cheat_download.cpp')],
                        '-lcrypto', '-pthread', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=60)
