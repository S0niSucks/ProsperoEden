#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Relink only focused network/metadata/VFS objects against read-only coordinator archives.
No emulator target is rebuilt or executed; assertions exercise actual loaders/providers.
"""
import json
import hashlib
import os
from pathlib import Path
import shlex
import socket
import subprocess
import time
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
SAVED = Path(os.environ.get('NETWORK_SAVED_CACHE', '/coordinator/ps5-eden-headless.2YUJmY'))
WORK = Path(os.environ.get('NETWORK_WORK', '/work/network-check')) / 'loader'
SANITIZE = os.environ.get('NETWORK_SANITIZE') == '1'


def main():
    if not Path('/.dockerenv').exists(): raise SystemExit('Docker only')
    WORK.mkdir(parents=True, exist_ok=True)
    Path('/run/samba/ncalrpc').mkdir(parents=True, exist_ok=True)
    build = WORK.parent / ('build-sanitized' if SANITIZE else 'build')
    subprocess.run(['cmake', '-S', str(ROOT / 'headless/network'), '-B', str(build),
                    '-DEDEN_NETWORK_DEPS=/work/deps', '-DEDEN_NETWORK_CHECK=ON',
                    *(['-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer',
                       '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'] if SANITIZE else [])], check=True)
    subprocess.run(['cmake', '--build', str(build), '--target', 'eden-network', '-j2'], check=True)
    entries = json.loads((SAVED / 'build/compile_commands.json').read_text())
    headers = hashlib.sha256()
    for header in sorted((ROOT / 'headless').rglob('*')):
        if header.suffix in ('.h', '.hpp', '.inc'): headers.update(header.read_bytes())
    def mapped(text):
        return text.replace('/root/.cache/ps5-eden-headless.2YUJmY', str(SAVED)).replace('/work/ProsperoEden', str(ROOT))
    def compile(source, template):
        entry = next(e for e in entries if e['file'].endswith('/' + template))
        args = shlex.split(mapped(entry['command']))
        out = WORK / (Path(source).name + '.o')
        at = args.index('-o'); args[at+1] = str(out)
        args[-1] = str(source)
        args += ['-UNDEBUG', '-O0', '-g', '-ffunction-sections', '-fdata-sections', '-I' + str(ROOT / 'headless'),
                 '-I' + str(ROOT / 'headless/prosperoeden'), '-I' + str(WORK)]
        args = [a for a in args if a != '-Wno-error-all']
        if Path(source).name == 'image.cpp':
            args += ['-I' + str(SAVED / 'source/externals'), '-I' + str(ROOT / 'third_party')]
        if SANITIZE: args += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        fingerprint = hashlib.sha256((shlex.join(args) + headers.hexdigest()).encode() + Path(source).read_bytes() +
                                     (WORK / 'network-fixtures.h').read_bytes()).hexdigest()
        stamp = out.with_suffix('.sha256')
        if not out.exists() or not stamp.exists() or stamp.read_text() != fingerprint:
            if subprocess.run(args, cwd=WORK).returncode:
                raise SystemExit('Focused compile failed: ' + str(source))
            stamp.write_text(fingerprint)
        return str(out)
    # Reuse the incumbent synthetic (no commercial keys/data) NCA writer, not a parser double.
    fixture = (ROOT / 'headless/cheat_identity_check.cpp').read_text()
    fixture = fixture[:fixture.index('\nint main()')]
    (WORK / 'network-fixtures.h').write_text(fixture)
    subprocess.run(['python3', '-B', str(ROOT / 'tools/check-network-integration.py')], check=True)
    derived = WORK.parent / 'integration/OFF/derived/vfs_real.cpp'
    objects = [compile(derived, 'vfs_real.cpp'),
               compile(ROOT / 'headless/network/vfs.cpp', 'nro_metadata_check.cpp'),
               compile(ROOT / 'headless/network/metadata.cpp', 'metadata_bridge.cpp'),
               compile(ROOT / 'headless/network/loader_check.cpp', 'nro_metadata_check.cpp')]
    ui = ROOT / 'headless/prosperoeden'
    ui_sources = [ROOT / 'headless/network/launcher_check.cpp', ui / 'host/fake_services.cpp',
                  ui / 'host/usb_backup_fake.cpp', ui / 'pe/gfx/image.cpp',
                  *sorted((ui / 'pe/core').glob('*.cpp')),
                  *[p for p in sorted((ui / 'pe/ui').glob('*.cpp'))
                    if p.name not in ('theme_kit.cpp', 'textures.cpp', 'runtime_launcher.cpp', 'runtime_loading.cpp', 'launcher_updates.cpp', 'qr_code.cpp')]]
    for source in ui_sources:
        objects.append(compile(source, 'nro_metadata_check.cpp'))
    # Query Ninja's saved link recipe without running Ninja's build graph or writing that tree.
    commands = subprocess.check_output(['ninja', '-C', str(SAVED / 'build'), '-t', 'commands', 'eden-nro-metadata-check'], text=True)
    link = commands.splitlines()[-1].split(' && ')[1]
    args = shlex.split(mapped(link)); output_at = args.index('-o')
    args[output_at+1] = str(WORK / 'check')
    args = [a for a in args if not a.endswith('nro_metadata_check.cpp.o') and not a.startswith('--dependency-file=')]
    args = [a for a in args if a != '-Xlinker']
    args[1:1] = objects + ['-Wl,--gc-sections', str(build / 'libeden-network.a'),
                         str(build / 'smb2/lib/libsmb2.a'), str(build / 'nfs/lib/libnfs.a')]
    if SANITIZE: args += ['-fsanitize=address,undefined']
    subprocess.run(args, cwd=SAVED / 'build', check=True)
    # Independent tiny PNG fixture, not an asset dependency or downloaded cover.
    png = WORK / 'icon.png'
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    png.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 2, 0, 0, 0)) +
                    chunk(b'IDAT', zlib.compress((b'\0' + b'\x40\x80\xc0' * 2) * 2)) + chunk(b'IEND', b''))
    subprocess.run([str(WORK / 'check'), 'fixtures', str(WORK), str(png)], check=True)
    for d in ('lock', 'state', 'cache', 'private', 'logs'): (WORK / d).mkdir(exist_ok=True)
    config = WORK / 'smb.conf'
    config.write_text(f'''[global]
server role = standalone server
interfaces = 127.0.0.1
bind interfaces only = yes
smb ports = 1448
disable netbios = yes
map to guest = Bad User
server min protocol = SMB2
load printers = no
printing = bsd
printcap name = /dev/null
pid directory = {WORK}
lock directory = {WORK / 'lock'}
state directory = {WORK / 'state'}
cache directory = {WORK / 'cache'}
private dir = {WORK / 'private'}
log file = {WORK / 'logs/log.%m'}
[games]
path = {WORK / 'share'}
read only = yes
guest ok = yes
wide links = no
follow symlinks = no
''')
    shares = WORK / 'shares.json'
    shares.write_text(json.dumps({'shares': [dict(id='test', protocol='smb', server='127.0.0.1', share='games',
                                               port=1448, cache_mib=1, updates_dir='addons')]}))
    shares.chmod(0o600)
    server = subprocess.Popen(['smbd', '--foreground', '--no-process-group', '-s', str(config)],
                              stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    try:
        for _ in range(100):
            try:
                with socket.create_connection(('127.0.0.1', 1448), timeout=.1): break
            except OSError: time.sleep(.02)
        subprocess.run([str(WORK / 'check'), 'check', str(WORK), str(shares)], check=True, timeout=90)
    finally:
        server.terminate(); server.wait(timeout=5)


if __name__ == '__main__': main()
