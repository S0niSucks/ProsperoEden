#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""NEW private canonical Git checkouts, without filters; never normalize received files.

Any received worktree difference other than CRLF-only checkout conversion is a
blocker, not a permission to discard a local modification. All original bytes stay RO.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess

import source_bundle as tool


def prepare(received, workspace):
    inventory = tool.load_json((workspace / 'policy-inputs.json').read_bytes())
    initial = tool.load_json((received / 'material-hashes.json').read_bytes())
    supplements = tool.load_json((received / 'closure-source-hashes.json').read_bytes())
    target = workspace / 'canonical-materials'
    transport = workspace / 'canonical-transport'
    tool.require(not target.exists() and not transport.exists(), 'new canonical input roots required')
    target.mkdir()
    transport.mkdir()
    collector = tool.Collector(transport, received / 'materials', inventory)
    git_inputs = {key: initial['git/' + key.removeprefix('deps/')] for key in inventory['materials']
                  if key.startswith('deps/') and 'git/' + key.removeprefix('deps/') in initial}
    git_inputs.update({'extra-git/' + row['id']: supplements['git/' + row['id']] for row in inventory['extra_git_sources']})
    differences = []
    for owner, record in sorted(git_inputs.items()):
        source = tool.local_file(received / 'materials', record['path'], directory=True)
        collector.export_git(source, record['commit'], 'source/' + owner, owner)
        proof = collector.git_sources[owner]
        for name, row in proof['entries'].items():
            tool.require(row['mode'] != '160000', 'canonical preparation requires explicit nested Git handling')
            path = source / name
            if row['mode'] == '120000':
                if '/' in name:
                    tool.local_file(source, name.rsplit('/', 1)[0], directory=True)
                tool.require(path.is_symlink(), 'received link type changed')
                actual = os.fsencode(os.readlink(path))
            else:
                if name != '.env.example':
                    tool.local_file(source, name)
                else:
                    tool.require(not path.is_symlink() and path.is_file(), 'received reviewed template type mismatch')
                tool.require(bool(path.stat().st_mode & 0o111) == (row['mode'] == '100755'), 'received executable mode changed')
                actual = path.read_bytes()
            expected = (transport / row['storage']).read_bytes()
            if actual != expected:
                crlf_only = actual.replace(b'\r\n', b'\n') == expected
                differences.append({'owner': owner, 'path': name, 'received_sha256': hashlib.sha256(actual).hexdigest(),
                    'canonical_sha256': row['sha256'], 'crlf_only': crlf_only})
                tool.require(crlf_only, 'genuine received local source modification requires review')
        tool.restore_git_sources(collector.git_sources, collector.policy, transport, {owner: target / record['path']})
        dest = target / record['path']
        subprocess.run(['git', 'init', '-q', str(dest)], check=True)
        tool.git(dest, '-c', 'credential.helper=', 'fetch', '-q', '--no-tags', '--depth=1', str(source), record['commit'])
        tool.git(dest, 'update-ref', 'HEAD', record['commit'])
        tool.git(dest, 'read-tree', record['commit'])  # index only: no checkout/smudge/filter execution
    for name, record in sorted({**initial, **supplements}.items()):
        if name.startswith('git/'):
            continue
        source = tool.local_file(received / 'materials', record['path'])
        dest = target / record['path']
        dest.parent.mkdir(parents=True, exist_ok=True)
        with source.open('rb') as incoming, dest.open('xb') as output:
            shutil.copyfileobj(incoming, output)
        tool.require(tool.digest(dest) == record['sha256'], 'canonical material copy hash mismatch')
    (workspace / 'canonical-source-report.json').write_bytes(tool.canonical({'snapshot': inventory['snapshot'],
        'received_roots_modified': False, 'checkout_filter_differences': differences,
        'scope': 'Exact immutable Git blobs/modes, without checkout filters; not final SDK/driver applied-source deltas.'}))
    print('CANONICAL_GIT_INPUTS', len(git_inputs), 'CRLF_ONLY_DIFFERENCES', len(differences), 'RECEIVED UNCHANGED')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--received', required=True)
    parser.add_argument('--workspace', required=True)
    args = parser.parse_args()
    prepare(Path(args.received), Path(args.workspace))
