#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused real native editor + controller/Mesa preview; Docker/private volume only, no app build."""
import concurrent.futures
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(os.environ.get('NETWORK_WORK', '/work/network-check')) / 'editor'
SANITIZE = os.environ.get('NETWORK_SANITIZE') == '1'


def main():
    if not Path('/.dockerenv').exists(): raise SystemExit('Docker only')
    WORK.mkdir(parents=True, exist_ok=True)
    subprocess.run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / 'headless'), '-fsyntax-only',
                    *[str(ROOT / 'headless/network' / name) for name in ('config.cpp', 'editor.cpp', 'keyboard.cpp')]], check=True)
    build = WORK / ('sanitized' if SANITIZE else 'build')
    sanitizers = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if SANITIZE else []
    subprocess.run(['cmake', '-S', str(ROOT / 'headless/network'), '-B', str(build),
                    '-DEDEN_NETWORK_DEPS=/work/deps', '-DEDEN_NETWORK_CHECK=ON',
                    '-DCMAKE_BUILD_TYPE=Debug', *(['-DCMAKE_C_FLAGS=' + ' '.join(sanitizers),
                    '-DCMAKE_CXX_FLAGS=' + ' '.join(sanitizers)] if SANITIZE else [])], check=True)
    subprocess.run(['cmake', '--build', str(build), '--target', 'eden-network-editor-check', '-j2'], check=True)
    subprocess.run([str(build / 'eden-network-editor-check'), tempfile.mkdtemp(prefix='config-', dir=WORK)], check=True)
    ui = ROOT / 'headless/prosperoeden'
    sources = [ROOT / 'headless/network/editor_ui_check.cpp', ui / 'host/fake_services.cpp',
               *sorted((ui / 'pe/core').glob('*.cpp')),
               *[p for p in sorted((ui / 'pe/ui').glob('*.cpp')) if p.name not in
                 ('theme_kit.cpp', 'runtime_launcher.cpp', 'runtime_loading.cpp', 'launcher_updates.cpp')],
               *[p for p in sorted((ui / 'pe/gfx').glob('*.cpp')) if p.name not in ('harfbuzz.cpp', 'system_fonts.cpp')]]
    includes = [ui, ui / 'host', ROOT / 'third_party', ROOT / 'third_party/lua', ROOT / 'headless', ROOT / 'tools/launcher/stb',
                Path(os.environ.get('NETWORK_SAVED_CACHE', '/coordinator/ps5-eden-headless.2YUJmY')) / 'source/externals']
    flags = ['g++', '-std=c++20', '-O0', '-g', '-Wall', '-Wextra', '-UNDEBUG', '-DGL_GLEXT_PROTOTYPES=1',
             '-ffunction-sections', '-fdata-sections', *sanitizers, *['-I' + str(p) for p in includes]]
    headers = hashlib.sha256()
    for header in sorted((ROOT / 'headless').rglob('*')):
        if header.suffix in ('.h', '.hpp', '.inc'): headers.update(header.read_bytes())
    objects = WORK / ('objects-sanitized' if SANITIZE else 'objects'); objects.mkdir(exist_ok=True)
    def compile(source):
        obj = objects / (str(source.relative_to(ROOT)).replace('/', '_') + '.o')
        stamp = obj.with_suffix('.sha256')
        digest = hashlib.sha256((str(flags) + headers.hexdigest()).encode() + source.read_bytes()).hexdigest()
        if not obj.exists() or not stamp.exists() or stamp.read_text() != digest:
            subprocess.run([*flags, '-c', str(source), '-o', str(obj)], check=True)
            stamp.write_text(digest)
        return str(obj)
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as workers: compiled = list(workers.map(compile, sources))
    binary = WORK / ('editor-ui-sanitized' if SANITIZE else 'editor-ui')
    subprocess.run(['g++', *sanitizers, '-Wl,--gc-sections', *compiled, str(build / 'libeden-network.a'),
                    str(build / 'smb2/lib/libsmb2.a'), str(build / 'nfs/lib/libnfs.a'), '-lEGL', '-lGL', '-pthread', '-o', str(binary)], check=True)
    env = {**os.environ, 'EGL_PLATFORM': 'surfaceless', 'LIBGL_ALWAYS_SOFTWARE': '1', 'GALLIUM_DRIVER': 'llvmpipe', 'LP_NUM_THREADS': '2'}
    # Mesa's inherited driver allocations are outside this focused leak test; pure config/editor
    # above keeps the caller's ASAN leak policy. All controller/renderer invalid-access checks stay on.
    if SANITIZE: env['ASAN_OPTIONS'] = env.get('ASAN_OPTIONS', '') + ':detect_leaks=0'
    subprocess.run([str(binary), str(ui / 'ui'), tempfile.mkdtemp(prefix='ui-config-', dir=WORK),
                    str(WORK / ('previews-sanitized' if SANITIZE else 'previews'))], check=True, env=env, timeout=90)
    print('Focused editor checks PASS (synthetic input only; no LAN access, full app build or console run)')


if __name__ == '__main__': main()
