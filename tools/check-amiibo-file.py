#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the NFC descriptor helper against real files and injected partial system calls."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='eden-amiibo-io-') as directory:
    binary = Path(directory) / 'check'
    for flags in ([], ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']):
        subprocess.run([
            os.environ.get('CXX', 'clang++-18'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
            '-UNDEBUG', *flags, '-I', str(root / 'headless'),
            str(root / 'headless/amiibo_file_io_check.cpp'),
            '-Wl,--wrap=pread,--wrap=pwrite', '-o', str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True, timeout=15)
