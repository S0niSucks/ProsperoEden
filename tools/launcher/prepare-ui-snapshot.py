#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Seed a private UI archive with already-retained renderer inputs. Never fetches/installs."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import sys
import tarfile

archive, provenance, cache_source, output = map(Path, sys.argv[1:])
output.mkdir(parents=True, exist_ok=False)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


metadata = json.loads(provenance.read_text())
assert digest(archive) == metadata['archive_sha256'], 'Input code archive hash mismatch'
with tarfile.open(archive) as source:
    manifest = json.load(source.extractfile('tools/deps.json'))
items = {i['name']: i for i in manifest['items'] if i['name'] in ('harfbuzz', 'ui-kit')}
kit = items['ui-kit']
for entry in kit['files']:
    path = cache_source / kit['dest'] / entry['name']
    assert not path.is_symlink() and digest(path) == entry['sha256'], f'UI-kit pin mismatch: {path}'
hb = items['harfbuzz']
hb_root = cache_source / hb['extract']
version = (hb_root / 'src/hb-version.h').read_text()
for field, value in (('MAJOR', 12), ('MINOR', 3), ('MICRO', 2)):
    assert re.search(rf'#define\s+HB_VERSION_{field}\s+{value}\b', version), 'Retained HarfBuzz version mismatch'
assert (cache_source / hb['creates']).is_file()
files = []
for folder in (hb['extract'], kit['dest']):
    for path in sorted((cache_source / folder).rglob('*')):
        assert not path.is_symlink(), f'Refusing a dependency symlink: {path}'
        if path.is_file():
            files.append((path, path.relative_to(cache_source).as_posix()))
        else:
            assert path.is_dir(), f'Refusing a nonregular dependency input: {path}'
dependencies = {
    'scope': 'Already-retained private renderer sources; no fetch or install',
    'ui_kit': 'All individual SHA-256 pins checked against archived tools/deps.json',
    'harfbuzz': 'Retained 12.3.2 version checked; exact file hashes recorded. Original download archive is absent, '
                'so its published archive pin was not reverified by this preparation.',
    'files': {name: digest(path) for path, name in files},
}
dep_file = output / 'dependencies.json'
dep_file.write_text(json.dumps(dependencies, indent=2))
snapshot = output / 'source.tar'
shutil.copyfile(archive, snapshot)
with tarfile.open(snapshot, 'a') as target:
    for path, name in files:
        target.add(path, arcname=name, recursive=False)
    target.add(dep_file, arcname='.ui-test-dependencies.json', recursive=False)
metadata['git_archive_sha256'] = metadata['archive_sha256']
metadata['archive_sha256'] = digest(snapshot)
metadata['dependencies_manifest_sha256'] = digest(dep_file)
metadata['dependency_validation'] = {k: v for k, v in dependencies.items() if k != 'files'}
(output / 'source.json').write_text(json.dumps(metadata, indent=2))
print(json.dumps({'archive_sha256': metadata['archive_sha256'],
                  'dependencies_manifest_sha256': metadata['dependencies_manifest_sha256'],
                  'retained_files': len(files)}, indent=2))
