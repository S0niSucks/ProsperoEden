#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual notice-staging CLI qualification using synthetic app folders, never a native build.

Run on an unprivileged POSIX host. --output must be a fresh private directory. The maintained
notice bytes are copied, not edited. No network, console install, ELF generation or PKG claim.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import zipfile
from release_licenses import release_license_entries

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ('tools/package-license-notices.py', 'tools/package-headless-native.sh',
           'tools/check-native-license-notices.py', 'tools/release_licenses.py',
           'tools/network_privacy.py', 'tools/ci/make-dist.py', 'tools/build_identity.py')
PAYLOAD = {'eboot.bin': b'Synthetic non-executable app fixture, NOT ELF or SELF.\n',
           'sce_sys/param.json': b'{"contentVersion":"01.000.001"}\n',
           'licenses/Lapy-MIT.txt': b'Synthetic Lapy notice, preserve exact whitespace. \r\n\n',
           'config/public.json': b'{"synthetic_public_setting":true}\n',
           'config/nextendo-public/keep.txt': b'Public prefix neighbour, not an account.\n',
           'ui/public.txt': b'Public launcher fixture.\n'}

# Independent guard: knows only explicit synthetic nodes, not the production deny policy.
# Descriptor-relative operations are resolved via already-open /proc/self/fd descriptors.
# Type-refusal fixtures permit lstat, but private nodes forbid metadata as well as payload IO.
GUARD = r'''
import builtins, io, json, os
rules = json.loads(os.environ['NATIVE_NOTICE_GUARD'])
log = os.environ['NATIVE_NOTICE_LOG']
original_open, original_io, original_readlink = builtins.open, io.open, os.readlink
def record(value):
    with original_open(log, 'a', encoding='utf-8') as stream: stream.write(json.dumps(value) + '\n')
record({'guard': 'active'})
def absolute(path, dir_fd=None):
    if isinstance(path, int): return original_readlink('/proc/self/fd/' + str(path))
    path = os.fsdecode(path)
    if not os.path.isabs(path) and dir_fd is not None:
        path = os.path.join(original_readlink('/proc/self/fd/' + str(dir_fd)), path)
    return os.path.abspath(path)
def check(event, path, metadata=False, dir_fd=None):
    value = absolute(path, dir_fd)
    for rule in rules:
        node = rule['path']
        if (value == node or value.startswith(node + '/')) and (not metadata or rule.get('metadata')):
            record({'event': event, 'path': value})
            raise PermissionError('Synthetic native notice access guard')
def guarded_open(path, *args, **kwargs):
    check('open', path); return original_open(path, *args, **kwargs)
def guarded_io(path, *args, **kwargs):
    check('io.open', path); return original_io(path, *args, **kwargs)
builtins.open, io.open = guarded_open, guarded_io
def wrap(name, metadata=False):
    original = getattr(os, name)
    def guarded(path, *args, **kwargs):
        check(name, path, metadata, kwargs.get('dir_fd'))
        return original(path, *args, **kwargs)
    setattr(os, name, guarded)
    for supported in (os.supports_dir_fd, os.supports_fd, os.supports_follow_symlinks):
        if original in supported: supported.add(guarded)
for name in ('open', 'listdir', 'scandir'): wrap(name)
for name in ('stat', 'lstat', 'readlink'): wrap(name, True)
'''


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def snapshot(root):
    """Test-owned fixtures only; record unreadable nodes and never follow links/FIFOs."""
    result = {}
    def walk(path):
        info = path.lstat()
        name = str(path.relative_to(root))
        row = {'mode': info.st_mode, 'size': info.st_size, 'mtime_ns': info.st_mtime_ns}
        result[name] = row
        if stat.S_ISDIR(info.st_mode):
            try:
                for child in sorted(path.iterdir()): walk(child)
            except PermissionError:
                row['unreadable'] = True
        elif stat.S_ISREG(info.st_mode):
            try: row['sha256'] = sha(path.read_bytes())
            except PermissionError: row['unreadable'] = True
        elif stat.S_ISLNK(info.st_mode): row['link'] = os.readlink(path)
    walk(root)
    return result


def fixture(directory, entries, staged=False):
    repo = directory / 'repo'
    for name in SOURCES:
        write(repo / name, (ROOT / name).read_bytes())
    for name, data in entries.items(): write(repo / name, data)
    write(repo / 'README.md', b'Synthetic packaging fixture.\n')
    write(repo / 'docs/CHANGELOG.md', b'## v1.000.001\n\nSynthetic notes.\n')
    app = repo / 'build/release/PPSA99008'
    for name, data in {**PAYLOAD, **(entries if staged else {})}.items(): write(app / name, data)
    return repo, app


def invoke(repo, app, directory, report, check=False, guards=(), cwd=None, collector=False):
    directory.mkdir(parents=True)
    command = [sys.executable, '-B', str(repo / ('tools/ci/make-dist.py' if collector else 'tools/package-license-notices.py')), str(app)]
    if check: command.append('--check')
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1')
    if not collector:
        guard = directory / 'guard'; guard.mkdir()
        write(guard / 'sitecustomize.py', GUARD.encode())
        rules = [{'path': str((repo / 'build/release/PPSA99008' / name).absolute())} for name in PAYLOAD]
        rules.extend({'path': str(path.absolute()), 'metadata': metadata} for path, metadata in guards)
        env.update(PYTHONPATH=str(guard), NATIVE_NOTICE_GUARD=json.dumps(rules),
                   NATIVE_NOTICE_LOG=str(directory / 'guard.jsonl'))
    result = subprocess.run(command, cwd=cwd, env=env, capture_output=True, text=True, timeout=30)
    write(directory / 'stdout.log', result.stdout.encode())
    write(directory / 'stderr.log', result.stderr.encode())
    events = [json.loads(line) for line in (directory / 'guard.jsonl').read_text().splitlines()] if not collector else []
    if not collector: assert events and events[0] == {'guard': 'active'}, result.stderr
    record = {'case': str(directory.relative_to(report['_output'])), 'command': command,
              'returncode': result.returncode, 'forbidden_access_attempts': events[1:]}
    write(directory / 'cli.json', json.dumps(record, indent=2).encode())
    report['runs'].append(record)
    assert not record['forbidden_access_attempts'], record
    return result


def success(output, entries, report):
    repo, app = fixture(output / 'success', entries)
    initial = snapshot(app)
    for label, check in (('first', False), ('idempotent', False), ('verify', True)):
        before = snapshot(app)
        result = invoke(repo, app, output / 'success' / label, report, check)
        assert result.returncode == 0, result.stderr
        receipt = json.loads(result.stdout)
        assert set(receipt['files']) == set(entries)
        assert set(receipt['created']) == (set(entries) if label == 'first' else set())
        if label != 'first': assert snapshot(app) == before
    for name, data in {**PAYLOAD, **entries}.items():
        assert (app / name).read_bytes() == data
        if name in entries: assert stat.S_IMODE((app / name).stat().st_mode) == 0o777
    for name in PAYLOAD: assert snapshot(app)[name] == initial[name]
    external = output / 'success/external-app'
    external.mkdir()
    result = invoke(repo, external, output / 'success/external', report)
    assert result.returncode == 0, result.stderr
    result = invoke(repo, Path('external-app'), output / 'success/relative', report, True, cwd=external.parent)
    assert result.returncode == 0, result.stderr
    for name, data in entries.items(): assert (external / name).read_bytes() == data
    for target in re.findall(r'\]\((LICENSE|third_party/licenses/[^)#]+)\)', entries['THIRD_PARTY_NOTICES.md'].decode()):
        destination = app / target
        assert destination.exists() and destination.resolve().is_relative_to(app), target
    manifest = json.loads(entries['third_party/licenses/SOURCES.json'])
    assert len(manifest['files']) == 11
    for row in manifest['files']:
        data = (app / 'third_party/licenses' / row['destination']).read_bytes()
        assert len(data) == row['size'] and sha(data) == row['sha256']
    expected = {'PPSA99008/' + name: data for name, data in {**PAYLOAD, **entries}.items()}
    expected.update(entries); expected['README.md'] = (repo / 'README.md').read_bytes()
    assert not (repo / 'build/headless-native/llvm-pie.elf').exists()
    for label in ('zip-first', 'zip-repeat'):
        result = invoke(repo, app, output / 'success' / label, report, collector=True)
        assert result.returncode == 0, result.stderr
        archive = repo / 'dist/ProsperoEden-v1.000.001.zip'
        with zipfile.ZipFile(archive) as zipped:
            assert len(zipped.namelist()) == len(expected) and set(zipped.namelist()) == set(expected)
            for info in zipped.infolist():
                assert zipped.read(info) == expected[info.filename]
                assert info.date_time == (2026, 1, 1, 0, 0, 0) and (info.external_attr >> 16) & 0o777 == 0o777
                assert 'licenses/third_party/licenses' not in info.filename
            assert zipped.testzip() is None
            if label == 'zip-first': zipped.extractall(output / 'success/extracted')
        checksum = sha(archive.read_bytes())
        assert (repo / 'dist/SHA256SUMS').read_text() == f'{checksum}  {archive.name}\n'
        write(output / 'success' / label / archive.name, archive.read_bytes())
        if label == 'zip-first': first = archive.read_bytes()
        else: assert first == archive.read_bytes()
    report['release_zip_sha256'] = checksum
    for name, data in expected.items():
        assert (output / 'success/extracted' / name).read_bytes() == data
    report['app_notice_files_sha256'] = {name: sha(data) for name, data in entries.items()}
    report['lapy_preserved_sha256'] = sha(PAYLOAD['licenses/Lapy-MIT.txt'])
    report['license_manifest_sha256'] = sha(entries['third_party/licenses/SOURCES.json'])


def adverse(output, entries, report):
    cases = ['source-missing-tree', 'source-missing-manifest', 'source-missing-file',
             'source-missing-license', 'source-missing-notices', 'source-altered-size',
             'source-altered-hash', 'source-malformed-manifest', 'source-duplicate-key',
             'source-extra-file', 'source-extra-directory', 'source-manifest-traversal',
             'source-manifest-backslash', 'source-denied-read', 'source-denied-directory',
             'target-altered', 'target-missing-check', 'target-extra-file', 'target-extra-directory',
             'target-recursive-layer', 'target-parent-file', 'target-license-directory',
             'target-case-alias', 'target-tree-case-alias', 'target-trailing-alias', 'target-denied-read',
             'target-denied-write', 'target-denied-directory', 'target-deep-denied-write',
             'path-missing', 'path-root', 'path-source', 'path-source-descendant',
             'path-parent-traversal', 'path-backslash', 'path-trailing-alias', 'path-private',
             'path-relative-private', 'path-denied-directory']
    for scope in ('source', 'target'):
        for node in ('third-party', 'tree', 'component', 'manifest', 'file', 'project-license', 'project-notices'):
            for kind in ('link', 'fifo'): cases.append(f'{scope}-{node}-{kind}')
    cases += ['path-link', 'path-ancestor-link', 'target-payload-link', 'target-payload-fifo']
    cases += ['target-lapy-link', 'target-lapy-fifo', 'target-lapy-dir-link', 'target-lapy-dir-fifo']
    private_names = ['config/network-shares.json-future', 'CoNfIg./NeTwOrK-ShArEs.JsOn.bak ',
                     'nested/config/nextendo/account.bin', 'CONFIG /NEXTENDO./tokens',
                     'nested/config\\nextendo/account.bin']
    for scope in ('source', 'target'):
        for n in range(len(private_names)):
            for kind in ('file', 'directory', 'link', 'fifo'):
                cases.append(f'private-{scope}-{n}-{kind}')
    for case in cases:
        directory = output / 'adverse' / case
        repo, app = fixture(directory, entries, staged=True)
        license_name = 'third_party/licenses/' + json.loads(entries['third_party/licenses/SOURCES.json'])['files'][0]['destination']
        source_tree = repo / 'third_party/licenses'
        target_tree = app / 'third_party/licenses'
        manifest_path = source_tree / 'SOURCES.json'
        guards = []
        check, argument, cwd = False, app, None
        if case.startswith('private-'):
            _, scope, index, kind = case.split('-')
            path = (source_tree if scope == 'source' else app) / private_names[int(index)]
            path.parent.mkdir(parents=True, exist_ok=True)
            if kind == 'file': write(path, b'SYNTHETIC PRIVATE CANARY')
            elif kind == 'directory': write(path / 'child', b'SYNTHETIC PRIVATE CANARY')
            elif kind == 'link': path.symlink_to(directory / 'never-follow')
            else: os.mkfifo(path)
            # For a private subtree, protect its boundary, not just the canary leaf.
            node = path
            if 'nextendo' in private_names[int(index)].lower():
                node = path.parent
            guards.append((node, True))
        elif case.startswith(('source-', 'target-')) and case.endswith(('-link', '-fifo')):
            scope, node, kind = case.split('-', 1)[0], case.rsplit('-', 1)[0].split('-', 1)[1], case.rsplit('-', 1)[1]
            base = repo if scope == 'source' else app
            path = {'third-party': base / 'third_party', 'tree': base / 'third_party/licenses', 'component': base / Path(license_name).parent,
                    'manifest': base / 'third_party/licenses/SOURCES.json', 'file': base / license_name,
                    'project-license': base / 'LICENSE', 'project-notices': base / 'THIRD_PARTY_NOTICES.md',
                    'payload': app / 'extra-payload', 'lapy': app / 'licenses/Lapy-MIT.txt',
                    'lapy-dir': app / 'licenses'}[node]
            if path.is_dir(): path.rename(directory / 'removed-directory')
            elif path.exists(): path.unlink()
            outside = directory / 'never-follow'; write(outside, b'Synthetic outside canary')
            if kind == 'link': path.symlink_to(outside)
            else: os.mkfifo(path)
            guards.extend(((path, False), (outside, False)))
        elif case == 'source-missing-tree': source_tree.rename(directory / 'removed-source')
        elif case == 'source-missing-manifest': manifest_path.unlink()
        elif case == 'source-missing-file': (repo / license_name).unlink()
        elif case == 'source-missing-license': (repo / 'LICENSE').unlink()
        elif case == 'source-missing-notices': (repo / 'THIRD_PARTY_NOTICES.md').unlink()
        elif case.startswith('source-altered'):
            path = repo / license_name; data = path.read_bytes()
            write(path, data + b'x' if case.endswith('size') else bytes([data[0] ^ 1]) + data[1:])
        elif case == 'source-malformed-manifest': write(manifest_path, b'{')
        elif case == 'source-duplicate-key': write(manifest_path, b'{"format_version":1,"format_version":1}')
        elif case == 'source-extra-file': write(source_tree / 'extra', b'Synthetic extra')
        elif case == 'source-extra-directory': (source_tree / 'empty-extra').mkdir()
        elif case.startswith('source-manifest-'):
            manifest = json.loads(manifest_path.read_bytes())
            manifest['files'][0]['destination'] = '../outside' if case.endswith('traversal') else 'nested\\COPYING'
            write(manifest_path, json.dumps(manifest).encode())
        elif case == 'source-denied-read': (repo / license_name).chmod(0)
        elif case == 'source-denied-directory': source_tree.chmod(0)
        elif case == 'target-altered':
            path = app / license_name; data = path.read_bytes(); write(path, bytes([data[0] ^ 1]) + data[1:])
        elif case == 'target-missing-check': (app / license_name).unlink(); check = True
        elif case == 'target-extra-file': write(target_tree / 'extra', b'Synthetic extra')
        elif case == 'target-extra-directory': (target_tree / 'empty-extra').mkdir()
        elif case == 'target-recursive-layer': write(target_tree / license_name, b'Forbidden recursive copy')
        elif case == 'target-parent-file':
            target_tree.rename(directory / 'removed-target'); write(target_tree, b'Parent cannot be a file')
        elif case == 'target-license-directory': (app / 'LICENSE').unlink(); (app / 'LICENSE').mkdir()
        elif case == 'target-case-alias': (app / 'LICENSE').rename(app / 'license')
        elif case == 'target-tree-case-alias': target_tree.rename(target_tree.with_name('LiCeNsEs'))
        elif case == 'target-trailing-alias': (app / 'LICENSE').rename(app / 'LICENSE. ')
        elif case == 'target-denied-read': (app / license_name).chmod(0)
        elif case == 'target-denied-write':
            (app / 'third_party').rename(directory / 'removed-target'); app.chmod(0o555)
        elif case == 'target-denied-directory': target_tree.chmod(0)
        elif case == 'target-deep-denied-write':
            path = app / license_name; path.unlink(); path.parent.chmod(0o555)
        elif case == 'path-missing': argument = directory / 'missing'
        elif case == 'path-denied-directory': app.chmod(0)
        elif case == 'path-root': argument = Path('/')
        elif case == 'path-source': argument = repo
        elif case == 'path-source-descendant': argument = source_tree
        elif case == 'path-parent-traversal': argument = app / '../PPSA99008'
        elif case in ('path-backslash', 'path-trailing-alias', 'path-private'):
            argument = directory / {'path-backslash': 'app\\alias', 'path-trailing-alias': 'app. ',
                                    'path-private': 'config/nextendo/app'}[case]
            argument.mkdir(parents=True)
            guards.append((argument, True))
        elif case == 'path-relative-private':
            cwd = directory / 'config/nextendo'; cwd.mkdir(parents=True)
            argument = Path('app'); (cwd / argument).mkdir()
            guards.append((cwd / argument, True))
        elif case in ('path-link', 'path-ancestor-link'):
            path = directory / 'linked'; path.symlink_to(app if case == 'path-link' else app.parent)
            argument = path if case == 'path-link' else path / app.name
            # O_NOFOLLOW may attempt opening the link itself; never open its target.
            guards.append((app, False))
        else: raise AssertionError(case)
        before = snapshot(repo)
        result = invoke(repo, argument, directory / 'cli', report, check, guards, cwd)
        assert result.returncode != 0, case
        assert snapshot(repo) == before, f'Refusal modified existing/source fixture: {case}'
        if case.startswith(('path-root', 'path-source', 'path-parent', 'path-backslash', 'path-trailing', 'path-private', 'path-relative')):
            assert 'Notice staging refused:' in result.stderr, (case, result.stderr)
    report['adverse_cases'] = len(cases)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'posix' or os.geteuid() == 0:
        parser.error('Run on Linux as an unprivileged user so permission failures are real')
    output = args.output.absolute(); output.mkdir(parents=True, exist_ok=False)
    report = {'passed': False, 'runs': [], '_output': output,
              'scope': 'Synthetic app-folder and actual Python CLIs only; no ELF/native build, FTP/console, PKG or source/relink qualification',
              'python': sys.version, 'euid': os.geteuid(), 'platform': sys.platform}
    try:
        hashes = {name: sha((ROOT / name).read_bytes()) for name in SOURCES}
        report['source_files_sha256'] = hashes
        entries = release_license_entries(ROOT)
        shell = (ROOT / 'tools/package-headless-native.sh').read_text()
        invocation = 'python3 -B "$root/tools/package-license-notices.py" "$app"'
        assert shell.count(invocation) == 1
        assert shell.index('"$app/licenses/Lapy-MIT.txt"') < shell.index(invocation) < shell.index('llvm-readobj-18 --dyn-symbols')
        syntax = subprocess.run(['bash', '-n', str(ROOT / 'tools/package-headless-native.sh')], capture_output=True, text=True)
        assert syntax.returncode == 0, syntax.stderr
        report['native_script_checked'] = 'bash -n plus single post-Lapy staging call; native script NOT executed'
        success(output, entries, report)
        adverse(output, entries, report)
        assert entries == release_license_entries(ROOT)
        assert hashes == {name: sha((ROOT / name).read_bytes()) for name in SOURCES}
        report['passed'] = True
    finally:
        report.pop('_output')
        write(output / 'report.json', json.dumps(report, indent=2).encode())
    print(f'Native notice staging PASS: {len(report["runs"])} actual CLI invocations; report {sha((output / "report.json").read_bytes())}')


if __name__ == '__main__':
    main()
