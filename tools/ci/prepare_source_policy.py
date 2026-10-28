#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Revalidate the received 59/15 inventories and emit NEW, exact reviewed policy.

Received material/index/restore files are read-only. This bounded release-specific
recipe does not infer permission from filenames or turn savednative into a receipt.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path

import source_bundle as tool


RECEIVED_INDEX = '35f79c0398074667598bab13bb5b7ce163521ffb7cc53bb52b1f5dcc0c2c2277'
TEMPLATES = {'deps/boilerplate': 'dd44bbdc75437332ed22e3ba95126733419ef25a',
             'deps/ps5-vulkan-tools': '5b5e4fc2d80fdb67a4f61ba9e6a8a424026bb8a5'}
LINKS = {'deps/ps5-mesa': ('7b59ef27c1b09b9671bc4153c41940c3155c3af2', 10),
         'deps/ps5-payload-sdk-fork': ('b83202be73e930050e00de0f1b4d9ec46c0391bf', 15),
         'extra-git/official-sdk-v042': ('4eb701204fc3f8d31e84cf8ca272974e2be9c867', 15)}


def prepare(received, workspace, repo, commit):
    tool.real_root(received)
    tool.require(tool.digest(received / 'preparation-evidence-sha256.json') == RECEIVED_INDEX, 'received index anchor mismatch')
    index = tool.load_json((received / 'preparation-evidence-sha256.json').read_bytes())
    tool.require(len(index) == 12, 'unexpected received index count')
    for name, checksum in index.items():
        tool.require(tool.digest(tool.local_file(received, name)) == checksum, 'received evidence changed')
    records = tool.load_json((received / 'material-hashes.json').read_bytes())
    supplements = tool.load_json((received / 'closure-source-hashes.json').read_bytes())
    tool.require(len(records) == 59 and len(supplements) == 15, 'unexpected received material count')
    for name, record in {**records, **supplements}.items():
        if name.startswith('git/'):
            path = tool.local_file(received / 'materials', record['path'], directory=True)
            tool.require(tool.git(path, 'rev-parse', 'HEAD').decode().strip() == record['commit'] and
                         tool.git(path, 'rev-parse', 'HEAD^{tree}').decode().strip() == record['tree'], 'received Git pin mismatch')
        else:
            path = tool.local_file(received / 'materials', record['path'])
            tool.require(tool.digest(path) == record['sha256'], 'received material hash mismatch')
            for algorithm, expected in record['expected'].items():
                tool.require(tool.digest(path, algorithm) == expected, 'received pin mismatch')
    tree = tool.git(repo, 'rev-parse', commit + '^{tree}').decode().strip()
    snap = tool.snapshot(repo, commit, tree)
    old = tool.load_json((received / 'inputs-integration.json').read_bytes())
    recipe_hashes = {}
    for path in ('tools/deps.json', 'UPSTREAM.json', 'tools/build-headless-native.sh', 'tools/ps5.cmake', 'headless/ps5.cmake'):
        before = tool.git(received / 'snapshot.git', 'show', old['snapshot']['commit'] + ':' + path)
        after = tool.git(repo, 'show', commit + ':' + path)
        tool.require(before == after, 're-review changed source/omission recipe before preparing policy')
        recipe_hashes[path] = hashlib.sha256(after).hexdigest()
    inventory = copy.deepcopy(tool.load_json((received / 'inputs-integration-with-source-supplements.json').read_bytes()))
    inventory['snapshot'] = snap  # freshly checked app pins/recipes, not borrowed final-build evidence
    inventory['source_policy'] = {'git': {}, 'archives': {}}
    inventory['extra_git_sources'] = []
    git_inputs = {key: {'path': row['path'], **records['git/' + key.removeprefix('deps/')]} for key, row in old['materials'].items()
                  if key.startswith('deps/') and 'git/' + key.removeprefix('deps/') in records}
    for key, record in sorted(supplements.items()):
        if not key.startswith('git/'):
            continue
        name = key.removeprefix('git/')
        item = {'id': name, 'path': record['path'], 'commit': record['commit'], 'tree': record['tree'], 'origin': record['origin']}
        inventory['extra_git_sources'].append(item)
        git_inputs['extra-git/' + name] = record
    for owner, record in sorted(git_inputs.items()):
        path = received / 'materials' / record['path']
        special = {}
        for raw in tool.git(path, 'ls-tree', '-rz', record['commit']).split(b'\0'):
            if not raw:
                continue
            meta, filename = raw.split(b'\t', 1)
            mode, kind, oid = meta.decode().split()
            name = filename.decode()
            if mode == '120000':
                special[name] = oid
        tool.require(not special or owner in LINKS, 'unreviewed component links')
        if owner not in TEMPLATES and owner not in LINKS:
            continue
        review = {'commit': record['commit'], 'tree': record['tree'], 'templates': {}, 'symlinks': {},
                  'reason': 'Coordinator-reviewed immutable public source; exact template/link bytes, not a general privacy exception.'}
        if owner in TEMPLATES:
            tool.require(record['commit'] == TEMPLATES[owner], 'template component pin changed')
            blob = tool.git(path, 'show', record['commit'] + ':.env.example')
            review['templates']['.env.example'] = {'sha256': hashlib.sha256(blob).hexdigest(),
                'reason': 'Coordinator inspected this public defaults-only template (example IP 192.168.1.100 and FTP port 2121).'}
        if owner in LINKS:
            expected_commit, count = LINKS[owner]
            tool.require(record['commit'] == expected_commit and len(special) == count, 'reviewed link set changed')
            review['symlinks'] = {name: hashlib.sha256(tool.git(path, 'cat-file', 'blob', oid)).hexdigest()
                                  for name, oid in sorted(special.items())}
        inventory['source_policy']['git'][owner] = review
    pacbrew_hash = 'a85f65de418a8e6a898c6c3e3c870d50fff7618a200e4dd59ea9692af6ecec4d'
    tool.require(records['archives/pacbrew.archive']['sha256'] == pacbrew_hash, 'pacbrew reviewed pin changed')
    prefix = 'opt/ps5-payload-sdk/target/user/homebrew/bin/'
    inventory['source_policy']['archives']['deps/pacbrew'] = {
        'sha256': pacbrew_hash, 'reason': 'Coordinator inspected this exact pinned pacbrew archive and its sole root-relative hardlink.',
        'hardlinks': {prefix + 'libdeflate-gzip': {'target': prefix + 'libdeflate-gunzip',
                     'reason': 'Exact reviewed hardlink; target must be an existing regular member of this same archive.'}}}
    tool.require(not workspace.exists() and not workspace.is_symlink(), 'new evidence workspace required')
    tool.real_root(workspace.parent)
    workspace.mkdir(parents=True)
    (workspace / 'policy-inputs.json').write_bytes(tool.canonical(inventory))
    (workspace / 'policy-origin.json').write_bytes(tool.canonical({'received_index_sha256': RECEIVED_INDEX,
        'original_snapshot': old['snapshot'], 'integration_snapshot': snap, 'revalidated_recipes_sha256': recipe_hashes,
        'material_root_readonly': str(received / 'materials'), 'relink': 'PENDING',
        'scope': 'Revalidated source inputs and explicit coordinator review only. Not a final build or source-completeness receipt.'}))
    print('PREPARED_NEW_POLICY', len(inventory['source_policy']['git']), 'reviewed Git components; 7 Git and 8 archive supplements; RELINK PENDING')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('received', 'workspace', 'snapshot-repo', 'snapshot'):
        parser.add_argument('--' + option, required=True)
    args = parser.parse_args()
    prepare(Path(args.received), Path(args.workspace), Path(args.snapshot_repo), args.snapshot)
