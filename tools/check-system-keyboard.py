#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check native keyboard parameters without opening a system dialog."""
from pathlib import Path
import subprocess
import tempfile
import os

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="eden-keyboard-") as directory:
    for mode in ("normal", "sanitized"):
        executable = Path(directory) / ("keyboard-check-" + mode)
        sanitizers = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if mode == "sanitized" else []
        subprocess.run(["c++", "-std=c++20", "-pthread", "-Wall", "-Wextra", "-Werror", *sanitizers,
                        str(root / "headless/system_keyboard_check.cpp"), "-o", str(executable)],
                       check=True)
        env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=1:halt_on_error=1",
               "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"}
        subprocess.run([str(executable)], check=True, timeout=20, env=env)
print("Native keyboard: ordinary/numeric/password types, cancellation, timed-out system ownership, cleanup/reopen PASS (normal + ASan/UBSan)")
