#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused native Nextendo consent controller/Mesa seam. Docker only; no IME/provider/full app."""
import argparse
import concurrent.futures
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True, help='NEW private output directory')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--tsan', action='store_true', help='ThreadSanitizer instead of ASan/UBSan')
    args = parser.parse_args()
    assert Path('/.dockerenv').exists(), 'Docker only'
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=False)
    ui = root / 'headless/prosperoeden'
    spec = importlib.util.spec_from_file_location('launcher_strings', root / 'tools/launcher/strings.py')
    strings = importlib.util.module_from_spec(spec); spec.loader.exec_module(strings)
    extracted = strings.marked_text()  # real extractor, read-only: no POT/catalog edits
    expected = [
        'Link a Nextendo game session? Share your identity and allow game matchmaking (identity, game.matchmaking). No friends, presence or profile editing permission is requested. Game routing stays off until you enable it separately.',
        'Confirm identity and game matchmaking permission before linking a Nextendo game session.',
        'Remote checks are unavailable for game-only Nextendo sessions. Link again to obtain a fresh game session; the saved session has not been removed.',
        'The Nextendo game session has expired or the clock is incorrect. Link again before enabling or launching.',
        'Nextendo game linking failed. Check registration, consent, connectivity and trusted certificates.',
        'Invalid oauth.json. Supply your registered client_id and HTTP 127.0.0.1 redirect_uri (port >= 1024 or {port}); app is optional.',
    ]
    assert all('pe/ui/nextendo.inc' in extracted.get(text, []) for text in expected)
    (out / 'six-msgids.json').write_text(json.dumps({text: extracted[text] for text in expected}, indent=2) + '\n')
    sources = [root / 'headless/nextendo_consent_ui_check.cpp', ui / 'host/fake_services.cpp',
               ui / 'host/usb_backup_fake.cpp', root / 'headless/network/editor.cpp', root / 'headless/network/config.cpp',
               root / 'headless/network/keyboard.cpp',
               *sorted((ui / 'pe/core').glob('*.cpp')),
               *[p for p in sorted((ui / 'pe/ui').glob('*.cpp')) if p.name not in
                 ('theme_kit.cpp', 'runtime_launcher.cpp', 'runtime_loading.cpp', 'launcher_updates.cpp')],
               *[p for p in sorted((ui / 'pe/gfx').glob('*.cpp')) if p.name not in ('harfbuzz.cpp', 'system_fonts.cpp')]]
    includes = [ui, ui / 'host', root / 'third_party', root / 'third_party/lua', root / 'headless', root / 'tools/launcher/stb']
    sanitizers = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if args.sanitize else []
    assert not (args.sanitize and args.tsan), 'Select one sanitizer mode'
    if args.tsan: sanitizers = ['-fsanitize=thread', '-fno-omit-frame-pointer']
    cxx = os.environ.get('CXX', 'clang++-18')
    flags = [cxx, '-std=c++20', '-O0', '-g', '-Wall', '-Wextra', '-UNDEBUG', '-DGL_GLEXT_PROTOTYPES=1',
             '-ffunction-sections', '-fdata-sections', *sanitizers, *['-I' + str(p) for p in includes]]
    inputs = set(sources) | {Path(__file__), root / 'tools/launcher/strings.py'}
    inputs.update(p for p in (root / 'headless').rglob('*') if p.suffix in ('.h', '.hpp', '.inc') and p.is_file())
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(inputs)}
    receipt = {'commands': [], 'inputs': hashes, 'result': 'FAILED'}
    def run(command, name, **kw):
        result = subprocess.run(command, capture_output=True, text=True, **kw)
        (out / (name + '.log')).write_text(result.stdout + result.stderr)
        receipt['commands'].append({'argv': command, 'returncode': result.returncode})
        if result.returncode: print(result.stdout + result.stderr, flush=True)
        result.check_returncode()
        return result.stdout
    try:
        def compile(source):
            name = str(source.relative_to(root)).replace('/', '_')
            obj = out / (name + '.o')
            run([*flags, '-MMD', '-MF', str(obj.with_suffix('.d')), '-c', str(source), '-o', str(obj)], name)
            return str(obj)
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as workers:
            objects = list(workers.map(compile, sources))
        binary = out / 'nextendo-consent-ui'
        run([cxx, *sanitizers, '-Wl,--gc-sections', *objects, '-lEGL', '-lGL', '-pthread', '-o', str(binary)], 'link')
        env = {**os.environ, 'EGL_PLATFORM': 'surfaceless', 'LIBGL_ALWAYS_SOFTWARE': '1', 'GALLIUM_DRIVER': 'llvmpipe', 'LP_NUM_THREADS': '2'}
        # Existing Mesa host seam: driver lifetime allocations are not native controller leaks.
        # Invalid-access/UB checks remain enabled; pure service tests retain normal leak checks.
        if args.sanitize: env['ASAN_OPTIONS'] = env.get('ASAN_OPTIONS', '') + ':detect_leaks=0'
        for width, height in ((1280, 720), (1920, 1080)):
            data = out / f'data-{height}'; data.mkdir()
            print(run([str(binary), str(ui / 'ui'), str(data), str(out / f'screens-{height}'), str(width), str(height)],
                      f'ui-{height}', env=env, timeout=90), end='', flush=True)
        assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == value for p, value in hashes.items())
        receipt['result'] = 'PASS: native consent UI ' + ('TSan' if args.tsan else 'ASan/UBSan' if args.sanitize else 'normal') + '; six msgids extracted; no IME/provider'
    finally:
        (out / 'receipt.json').write_text(json.dumps(receipt, sort_keys=True, indent=2) + '\n')


if __name__ == '__main__':
    main()
