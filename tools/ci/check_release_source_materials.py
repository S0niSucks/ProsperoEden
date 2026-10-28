#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline byte/provenance and unchanged collector-policy audit of explicit gathered inputs.

It diagnoses policy refusals on pinned public source ARCHIVE METADATA only; it never
suppresses them or constructs a source_bundle override. No native objects are imported.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import tarfile


def canonical(value):
    return (json.dumps(value, indent=2, sort_keys=True) + '\n').encode()


def digest(path, algorithm='sha256'):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, algorithm).hexdigest()


def run(root, collector, manifest='material-hashes.json', output='material-verification.json'):
    spec = importlib.util.spec_from_file_location('reviewed_source_bundle', collector)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    records = json.loads((root / manifest).read_bytes())
    inventory = json.loads((root / 'inputs-integration.json').read_bytes())
    results = {}
    captured = []
    original = module.safe_name

    def diagnose(name):
        captured[:] = [name]
        try:
            return original(name)
        except module.Refused:
            raise  # observe, NEVER bypass the unchanged policy

    module.safe_name = diagnose
    nested = {}
    for name, record in sorted(records.items()):
        captured.clear()
        path = module.local_file(root / 'materials', record['path'], directory=name.startswith('git/'))
        if name.startswith('git/'):
            commit = record['commit']
            tree = module.git(path, 'rev-parse', commit + '^{tree}').decode().strip()
            module.require(tree == record['tree'], 'private Git tree mismatch')
            module.git(path, 'fsck', '--full', '--no-reflogs', '--no-dangling')
            module.git(path, 'diff', '--quiet', commit, '--')
            entries = module.git(path, 'ls-tree', '-rz', commit).split(b'\0')
            for entry in entries:
                if not entry:
                    continue
                meta, filename = entry.split(b'\t', 1)
                if filename.rsplit(b'/', 1)[-1] == b'cpmfile.json':
                    data = module.git(path, 'show', commit + ':' + filename.decode())
                    nested[name + '/' + filename.decode()] = json.loads(data)
            try:
                module.tree_entries(path, commit)
                policy = 'PASS'
            except module.Refused as error:
                policy = str(error)
            results[name] = {'git_commit': commit, 'git_tree': tree, 'hash_verification': 'PASS',
                             'collector_policy': policy, 'offending_path': captured[:1] if policy != 'PASS' else [],
                             'special_entries': record['special_entries']}
        else:
            module.require(digest(path) == record['sha256'], 'material SHA-256 mismatch: ' + name)
            module.require(all(digest(path, algorithm) == expected for algorithm, expected in record['expected'].items()),
                           'material pin mismatch: ' + name)
            policy = 'NOT_AN_ARCHIVE_INPUT'
            if name.startswith(('archives/', 'cpm/', 'closure/')):
                try:
                    definitions = module.archive_index(path)
                    for member, definition in definitions:
                        nested[name + '/' + member] = definition
                    policy = 'PASS'
                except (module.Refused, tarfile.TarError) as error:
                    policy = str(error)
            results[name] = {'sha256': record['sha256'], 'size': record['size'],
                             'hash_verification': 'PASS', 'collector_policy': policy,
                             'offending_path': captured[:1] if policy not in {'PASS', 'NOT_AN_ARCHIVE_INPUT'} else []}
        print(name, results[name]['hash_verification'], results[name]['collector_policy'],
              results[name]['offending_path'], flush=True)
    result = {'snapshot': inventory['snapshot'], 'relink': inventory['relink'], 'materials': results,
              'collector_sha256': digest(collector), 'nested_cpm_definitions': nested,
              'scope': 'Real source inputs only; no final-build relink receipt or legal/completeness certification.'}
    (root / output).write_bytes(canonical(result))
    print('VERIFIED_MATERIALS', len(results), 'POLICY_REFUSALS',
          sum(r['collector_policy'] not in {'PASS', 'NOT_AN_ARCHIVE_INPUT'} for r in results.values()))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', required=True)
    parser.add_argument('--collector', default=str(Path(__file__).with_name('source_bundle.py')))
    parser.add_argument('--manifest', default='material-hashes.json')
    parser.add_argument('--output', default='material-verification.json')
    args = parser.parse_args()
    run(Path(args.workspace), Path(args.collector), args.manifest, args.output)
