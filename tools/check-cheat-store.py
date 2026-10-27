#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Cache/install tests with synthetic code and real filesystem operations; no network calls."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='eden-cheat-store-build-') as tmp:
    binary = Path(tmp) / 'check'
    for sanitizer in ('', 'address,undefined', 'thread'):
        flags = ['-fsanitize=' + sanitizer, '-fno-omit-frame-pointer'] if sanitizer else []
        subprocess.run([os.environ.get('CXX', 'clang++-18'), '-std=c++20', '-O1', '-g', '-UNDEBUG',
                        '-Wall', '-Wextra', '-Werror', *flags, '-I', str(root / 'headless'),
                        *[str(root / 'headless' / name) for name in
                          ('cheat_store_check.cpp', 'cheat_store.cpp', 'cheat_catalog.cpp', 'cheat_download.cpp')],
                        *['-Wl,--wrap=' + name for name in ('write', 'pread', 'fsync', 'linkat', 'renameat')],
                        '-lcrypto', '-pthread', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=60)
