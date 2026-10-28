#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check changed native-gated frontend paths with saved host headers/flags; not a PS5 build."""
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SAVED = Path(os.environ.get('NETWORK_SAVED_CACHE', '/coordinator/ps5-eden-headless.2YUJmY'))


def main():
    if not Path('/.dockerenv').exists(): raise SystemExit('Docker only')
    entries = json.loads((SAVED / 'build/compile_commands.json').read_text())
    entry = next(e for e in entries if e['file'].endswith('/ProsperoEden/headless/main.cpp'))
    args = shlex.split(entry['command'].replace('/root/.cache/ps5-eden-headless.2YUJmY', str(SAVED))
                      .replace('/work/ProsperoEden', str(ROOT)))
    at = args.index('-o'); del args[at:at + 2]; args.remove('-c'); args.pop()
    args += ['-fsyntax-only', '-I' + str(ROOT / 'headless/prosperoeden'), '-I' + str(ROOT / 'headless'),
             '-DPS5_NATIVE=1', '-DEDEN_DEVICE_FRONTEND=0', '-Wno-unknown-warning-option']
    work = Path('/work/network-check'); work.mkdir(exist_ok=True)
    for source in ('headless/prosperoeden/eden_services.cpp', 'headless/main.cpp', 'headless/prosperoeden/pe/ui/library.cpp'):
        if subprocess.run([*args, str(ROOT / source)], cwd=work).returncode:
            raise SystemExit('Frontend syntax check failed: ' + source)
    print('Changed native-gated services/main/Library host syntax PASS (no emulator rebuild or console qualification)')


if __name__ == '__main__': main()
