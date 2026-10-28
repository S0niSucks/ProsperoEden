# SPDX-License-Identifier: GPL-3.0-or-later
"""Verified notice bytes for a release ZIP, not corresponding source or legal certification."""
import hashlib
import json
import re
import stat


def _object(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise ValueError(f'Duplicate license manifest key: {key}')
        value[key] = item
    return value


def _relative(name):
    if not isinstance(name, str) or not name or any(
            not part or part in ('.', '..') or part.rstrip(' .') != part or
            any(ord(c) < 32 or ord(c) == 127 or c in '\\:' for c in part)
            for part in name.split('/')):
        raise ValueError(f'Unsafe license path: {name!r}')
    return name


def release_license_entries(root):
    """Fail closed on absent/malformed/incomplete notice inputs; snapshot raw bytes once.

    Only top-level files[].destination/size/sha256 describe shipped copies. Nested excerpt
    hashes describe provenance, not complete source files to collect or a source offer.
    Inputs must remain quiescent; lstat checks are not a concurrent-mutation sandbox.
    """
    base = root / 'third_party/licenses'
    for directory in (root / 'third_party', base):
        if not stat.S_ISDIR(directory.lstat().st_mode):
            raise ValueError(f'License directory must not be a link/special node: {directory}')
    paths = {}
    folded = set()

    def walk(directory):
        for path in sorted(directory.iterdir()):
            name = _relative(path.relative_to(base).as_posix())
            if name.casefold() in folded:
                raise ValueError(f'Case-colliding license path: {name}')
            folded.add(name.casefold())
            mode = path.lstat().st_mode
            if stat.S_ISDIR(mode):
                walk(path)
            elif stat.S_ISREG(mode):
                paths[name] = path
            else:
                raise ValueError(f'License links/special files are forbidden: {name}')

    walk(base)
    if 'SOURCES.json' not in paths:
        raise ValueError('Missing regular license SOURCES.json')
    manifest_bytes = paths['SOURCES.json'].read_bytes()
    manifest = json.loads(manifest_bytes, object_pairs_hook=_object)
    if not isinstance(manifest, dict) or type(manifest.get('format_version')) is not int or manifest['format_version'] != 1:
        raise ValueError('Unsupported license manifest format_version')
    records = manifest.get('files')
    if not isinstance(records, list) or not records:
        raise ValueError('License manifest files must be a nonempty list')
    declared = {}
    for record in records:
        if not isinstance(record, dict):
            raise ValueError('Invalid license file record')
        name = _relative(record.get('destination'))
        if name.casefold() == 'sources.json' or name.casefold() in declared:
            raise ValueError(f'Duplicate/reserved license destination: {name}')
        size, sha = record.get('size'), record.get('sha256')
        if type(size) is not int or size < 0 or not isinstance(sha, str) or not re.fullmatch('[0-9a-f]{64}', sha):
            raise ValueError(f'Invalid license size/hash: {name}')
        declared[name.casefold()] = name
    if set(paths) != {'SOURCES.json', *declared.values()}:
        raise ValueError('License directory does not exactly match SOURCES.json destinations')
    entries = {}
    for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
        path = root / name
        if not stat.S_ISREG(path.lstat().st_mode):
            raise ValueError(f'Required notice must be a regular file: {name}')
        entries[name] = path.read_bytes()
    payloads = {'SOURCES.json': manifest_bytes}
    for record in records:
        name = record['destination']
        path = paths[name]
        if path.lstat().st_size != record['size']:
            raise ValueError(f'License size mismatch: {name}')
        data = path.read_bytes()
        if len(data) != record['size'] or hashlib.sha256(data).hexdigest() != record['sha256']:
            raise ValueError(f'License bytes/hash mismatch: {name}')
        payloads[name] = data
    entries.update(('third_party/licenses/' + name, payloads[name]) for name in sorted(payloads))
    return entries
