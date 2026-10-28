#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline restore trial: real pinned direct sources and Eden patch application, no build.

Run in Docker --network none with a private writable volume. Uses the SNAPSHOT's
unchanged tools/deps.py CLI only for source archives/files, never Git setup commands,
SDK executables, binary packages, CMake configuration or a native/emulator rebuild.
The selected sources cannot establish full transitive/offline-build closure.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

from gather_release_sources import canonical, check, git, sha


SOURCE_ARCHIVES = {'libsmb2', 'libnfs', 'eden', 'ffmpeg', 'fmt', 'harfbuzz'}


def trial(workspace, destination, collector):
    inventory = json.loads((workspace / 'inputs-integration.json').read_bytes())
    records = json.loads((workspace / 'material-hashes.json').read_bytes())
    if destination.exists() or destination.is_symlink():
        raise ValueError('restore destination must be NEW; nothing is replaced')
    for parent in [*reversed(destination.parents), destination]:
        if parent.is_symlink():
            raise ValueError('restore destination symlink refused')
    destination.mkdir(parents=True)
    spec = importlib.util.spec_from_file_location('reviewed_source_bundle', collector)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    snapshot = inventory['snapshot']
    repo = workspace / 'snapshot.git'
    module.snapshot(repo, snapshot['commit'], snapshot['tree'])
    exported = module.Collector(destination, workspace / 'materials', {})
    exported.export_git(repo, snapshot['commit'], 'app', 'app')
    app = destination / 'app'
    cache = destination / 'download-cache'
    cache.mkdir()
    deps = json.loads((app / 'tools/deps.json').read_bytes())['items']
    selected = []
    for dep in deps:
        name = dep['name']
        if dep['kind'] == 'archive' and name in SOURCE_ARCHIVES:
            record = records['archives/' + name + '.archive']
            source = workspace / 'materials' / record['path']
            if not check(source, record['expected']):
                raise ValueError('corrupt source archive: ' + name)
            module.archive_index(source)  # NO override of the reviewed policy
            expected = next(dep[k] for k in ('sha256', 'sha512') if k in dep)
            cached = Path(dep.get('save', f'{name}-{expected[:12]}.tar.gz')).name
            shutil.copyfile(source, cache / cached)
            selected.append(name)
        elif dep['kind'] == 'files':
            for entry in dep['files']:
                relative = 'files/' + name + '/' + entry['name']
                source = workspace / 'materials' / relative
                if not check(source, {'sha256': entry['sha256']}):
                    raise ValueError('corrupt source file: ' + relative)
                shutil.copyfile(source, cache / (name + '-' + entry['name'].replace('/', '-')))
            selected.append(name)
    env = {**os.environ, 'PROSPEROEDEN_DEPS_CACHE': str(cache), 'PYTHONDONTWRITEBYTECODE': '1'}
    restored = subprocess.run(['python3', '-B', str(app / 'tools/deps.py'), 'fetch', *selected],
                              cwd=app, env=env, check=True, capture_output=True, timeout=240)
    print(restored.stdout.decode(), end='')
    creates = {dep['name']: dep['creates'] for dep in deps if dep['name'] in selected}
    if not all((app / path).is_file() for path in creates.values()):
        raise ValueError('original deps.py failed to restore expected source layout')
    eden = app / next(dep['creates'] for dep in deps if dep['name'] == 'eden')
    eden = eden.parent
    applied = {}
    for patch in sorted((app / 'tools/eden-patches').glob('*.diff')):
        data = patch.read_bytes()
        for arguments in (['--dry-run', '-N'], ['-N'], ['--dry-run', '-R']):
            result = subprocess.run(['patch', '-d', str(eden), '-p1', '-s', '-f', *arguments],
                                    input=data, check=True, capture_output=True, timeout=30)
        applied[patch.relative_to(app).as_posix()] = sha(patch)
    # Confirm every standalone input at its real build-consumed destination.
    for dep in deps:
        if dep['kind'] == 'files':
            for entry in dep['files']:
                if not check(app / dep['dest'] / entry['name'], {'sha256': entry['sha256']}):
                    raise ValueError('restored source file mismatch')
    result = {'status': 'PASS_SOURCE_RESTORE_AND_EDEN_PATCHES_ONLY', 'snapshot': snapshot,
              'restored_inputs': creates, 'applied_patches_sha256': applied,
              'restoration_tool_sha256': sha(app / 'tools/deps.py'),
              'relink': {'status': 'PENDING'},
              'not_run': ['binary-package extraction', 'CPM configure/fetch/cache reconstruction',
                          'full recursive source closure', 'SDK/runtime rebuild', 'native/emulator rebuild',
                          'final-build object receipt', 'modified-library relink trial', 'final binary comparison']}
    (destination / 'restore-result.json').write_bytes(canonical(result))
    print('RESTORE PASS', len(creates), 'direct dependency groups;', len(applied), 'Eden patches; NO BUILD')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', required=True)
    parser.add_argument('--destination', required=True)
    parser.add_argument('--collector', default=str(Path(__file__).with_name('source_bundle.py')))
    args = parser.parse_args()
    trial(Path(args.workspace), Path(args.destination), Path(args.collector))
