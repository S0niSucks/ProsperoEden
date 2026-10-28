#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise post-link identity, malformed inputs and release manifest packaging in isolation."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
from build_identity import identity, stamp

root = Path(__file__).resolve().parents[1]


def refused(data):
    try:
        identity(data)
    except (ValueError, struct.error):
        return
    raise AssertionError('invalid identity accepted')


with tempfile.TemporaryDirectory(prefix='eden-build-identity-') as tmp:
    work = Path(tmp)
    cpp = work / 'main.cpp'
    cpp.write_text('#include "build_identity.h"\n#include <cstdio>\n'
                   'int main() { char text[65]; Eden::ReadBuildIdentity(text); std::puts(text); }\n')
    elf = work / 'identity.elf'
    subprocess.run(['clang++-18', '-std=c++20', '-O2', '-flto', '-fuse-ld=lld',
                    '-I', str(root / 'headless'), str(cpp), str(root / 'headless/build_identity.cpp'),
                    '-o', str(elf)], check=True)
    refused(elf.read_bytes())
    digest = stamp(elf)
    data = elf.read_bytes()
    assert subprocess.check_output([str(elf)], text=True).strip() == digest
    assert stamp(elf) == digest and elf.read_bytes() == data
    for at in [16, 40, len(data) - 1]:
        changed = bytearray(data); changed[at] ^= 1; refused(changed)
    for data_invalid in [b'', b'not an ELF', data[:64]]:
        refused(data_invalid)
    changed = bytearray(data); struct.pack_into('<Q', changed, 32, len(data)); refused(changed)
    changed = bytearray(data); struct.pack_into('<H', changed, 54, 1); refused(changed)

    # The packaging script runs in a disposable miniature repository, not this checkout's dist.
    repo = work / 'repo'
    (repo / 'tools/ci').mkdir(parents=True)
    for name in ['ci/make-dist.py', 'build_identity.py', 'network_privacy.py', 'release_licenses.py']:
        shutil.copy2(root / 'tools' / name, repo / 'tools' / name)
    # Exercise the actual current exporter and its verified notice inputs, not stubs.
    for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
        shutil.copy2(root / name, repo / name)
    shutil.copytree(root / 'third_party/licenses', repo / 'third_party/licenses')
    (repo / 'docs').mkdir()
    (repo / 'docs/CHANGELOG.md').write_text('## v1.005.000\n\nFixture notes.\n')
    (repo / 'build/headless-native').mkdir(parents=True)
    shutil.copy2(elf, repo / 'build/headless-native/llvm-pie.elf')
    source = {'schema': 1, 'head': 'fixture', 'dirty': True, 'files_sha256': {'test': 'abc'}}
    (repo / 'build/source-snapshot.json').write_text(json.dumps(source))
    app = repo / 'build/app/PPSA99008'
    (app / 'sce_sys').mkdir(parents=True)
    (app / 'sce_sys/param.json').write_text('{"contentVersion":"01.005.000"}')
    (app / 'eboot.bin').write_bytes(b'fixture eboot, not a console executable')
    subprocess.run([sys.executable, '-B', str(repo / 'tools/ci/make-dist.py'), str(app)],
                   check=True, stdout=subprocess.DEVNULL)
    manifest = json.loads((repo / 'dist/build-identity.json').read_text())
    assert manifest['build_identity'] == digest and manifest['source'] == source
    for field, file in [('elf', repo / 'build/symbols/ProsperoEden-v1.005.000.elf'),
                        ('eboot', app / 'eboot.bin'), ('zip', repo / 'dist/ProsperoEden-v1.005.000.zip')]:
        assert manifest[field + '_sha256'] == hashlib.sha256(file.read_bytes()).hexdigest()
    for line in (repo / 'dist/SHA256SUMS').read_text().splitlines():
        checksum, file = line.split('  ')
        assert hashlib.sha256((repo / 'dist' / file).read_bytes()).hexdigest() == checksum
print('Build identity PASS: runtime field survives LTO/stamping, idempotence, modified/unstamped/'
      'malformed ELF refusal, release manifest/symbols/eboot/ZIP/source hashes')
