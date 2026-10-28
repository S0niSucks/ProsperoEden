#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual CLI/ZIP privacy qualification in miniature synthetic repos, never real app data.

--output retains fixtures, ZIPs, commands, logs and hashes; otherwise use an owned temp folder.
--baseline points to an archived old source tree to demonstrate leaks (not an acceptance gate).
No native build: the optional root/build symbol input is absent in every miniature repository.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ('tools/network_privacy.py', 'tools/build_identity.py',
           'tools/ci/make-dist.py', 'tools/launcher/package-update.py')
PRIVATE = b'SYNTHETIC-PRIVATE-CANARY-NOT-REAL-USER-DATA'
STAMP = 1767225600

# This guard does not import the policy under test. Exact fixture nodes/descendants are
# supplied independently. An attempted denied stat, read, readlink or enumeration is a
# failure even if pathlib swallows PermissionError. The synthetic payload is never opened.
GUARD = r'''
import json, os, sys
nodes = [os.path.normcase(os.path.abspath(path)) for path in json.loads(os.environ['PE_EXPORT_DENIED'])]
log = os.environ['PE_EXPORT_GUARD_LOG']
def record(value):
    with open(log, 'a', encoding='utf-8') as stream:
        stream.write(json.dumps(value) + '\n')
def check(event, value):
    if not isinstance(value, (str, bytes, os.PathLike)): return
    path = os.path.normcase(os.path.abspath(os.fsdecode(value)))
    if any(path == node or path.startswith(node + os.sep) for node in nodes):
        record({'event': event, 'path': path})
        raise PermissionError('Synthetic privacy guard blocked ' + event)
record({'guard': 'active'})
def audit(event, args):
    if event in ('open', 'os.scandir', 'os.listdir') and args: check(event, args[0])
sys.addaudithook(audit)
for name in ('stat', 'lstat', 'readlink'):
    original = getattr(os, name)
    def guarded(path, *args, _name=name, _original=original, **kwargs):
        check('os.' + _name, path)
        return _original(path, *args, **kwargs)
    setattr(os, name, guarded)
'''


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    os.utime(path, (STAMP, STAMP))


def fixture(source, directory, collector):
    repo = directory / 'repo'
    for name in SOURCES:
        target = repo / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / name, target)
    for name in ('README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md'):
        write(repo / name, ('Synthetic fixture ' + name + '\n').encode())
    write(repo / 'docs/CHANGELOG.md', b'## v1.000.001\n\nSynthetic notes.\n')
    folder = repo / 'input' / ('PPSA99008' if collector == 'release' else 'pack')
    public = {'config/settings.json': b'{"synthetic":true}',
              'config/network-shares.txt': b'public near match',
              'config/nextendo.json': b'public near match',
              'config/nextendo-public/keep.txt': b'public near match',
              'assets/network-shares.json': b'public asset',
              'nested/config/settings.json': b'public nested config',
              'configuration/nextendo/account.txt': b'public near match'}
    if os.name == 'posix':
        public.update({'config. /settings-extra.json': b'public alias config',
                       'config./nextendo-public/keep.txt': b'public alias near match'})
    if collector == 'release':
        public.update({'sce_sys/param.json': b'{"contentVersion":"01.000.001"}',
                       'eboot.bin': b'Synthetic non-executable eboot identity fixture'})
    else:
        manifest = {'schema': 1, 'id': 'privacy-fixture', 'name': 'Synthetic fixture',
                    'version': '1.2.3', 'host_contract': 'launcher-v1.0',
                    'api': {'major': 1, 'min_minor': 0}, 'entry': 'main.lua',
                    'font': 'assets/font.pefont', 'assets': {},
                    'updates': {'feed': 'https://example.invalid/feed.json'}}
        public.update({'manifest.json': json.dumps(manifest).encode(),
                       'main.lua': b'return {}\n', 'assets/font.pefont': b'synthetic font bytes'})
    for name, data in public.items():
        write(folder / name, data)
    return repo, folder, public


def invoke(repo, folder, collector, directory, denied=(), extra=()):
    if collector == 'release':
        command = [sys.executable, '-B', str(repo / 'tools/ci/make-dist.py'), str(folder)]
        archive = repo / 'dist/ProsperoEden-v1.000.001.zip'
    else:
        command = [sys.executable, '-B', str(repo / 'tools/launcher/package-update.py'), str(folder),
                   '--output', str(repo / 'output'), '--url', 'https://example.invalid/fixture.zip', *extra]
        archive = repo / 'output/privacy-fixture-1.2.3.zip'
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1')
    guard_log = directory / 'guard.jsonl'
    if denied:
        guard = directory / 'guard'; guard.mkdir()
        write(guard / 'sitecustomize.py', GUARD.encode())
        env.update(PYTHONPATH=str(guard), PE_EXPORT_DENIED=json.dumps([str(p.absolute()) for p in denied]),
                   PE_EXPORT_GUARD_LOG=str(guard_log))
    result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=20)
    (directory / 'stdout.log').write_text(result.stdout, encoding='utf-8')
    (directory / 'stderr.log').write_text(result.stderr, encoding='utf-8')
    events = [json.loads(line) for line in guard_log.read_text().splitlines()] if guard_log.exists() else []
    if denied:
        assert events and events[0] == {'guard': 'active'}, 'CLI access guard was not installed'
    record = {'command': command, 'returncode': result.returncode, 'guard_attempts': events[1:],
              'zip_sha256': digest(archive) if archive.exists() else None}
    (directory / 'cli.json').write_text(json.dumps(record, indent=2))
    return result, archive, record


def inspect(archive, collector, public):
    prefix = 'PPSA99008/' if collector == 'release' else 'privacy-fixture/'
    expected = {prefix + name: data for name, data in public.items()}
    if collector == 'release':
        expected.update({name: ('Synthetic fixture ' + name + '\n').encode()
                         for name in ('README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md')})
    with zipfile.ZipFile(archive) as zipped:
        assert zipped.testzip() is None
        assert len(zipped.namelist()) == len(set(zipped.namelist())), 'Duplicate ZIP members'
        actual = {info.filename: zipped.read(info) for info in zipped.infolist()}
        for name, data in expected.items():
            assert actual[name] == data, 'Public bytes changed: ' + name
        if collector == 'release':
            assert all((info.external_attr >> 16) & 0o777 == 0o777 for info in zipped.infolist())
            assert all(info.date_time == (2026, 1, 1, 0, 0, 0) for info in zipped.infolist())
    return sorted(set(actual) - set(expected)), [name for name, data in actual.items() if PRIVATE in data]


def verify_integrity(repo, folder, archive, collector, public):
    # Hash/source-identity checks do not touch the independently specified private nodes.
    for name, data in public.items():
        assert (folder / name).read_bytes() == data, 'Source was modified: ' + name
    if collector == 'release':
        for line in (repo / 'dist/SHA256SUMS').read_text().splitlines():
            checksum, name = line.split('  ')
            assert digest(repo / 'dist' / name) == checksum
        assert not (repo / 'build').exists(), 'Optional real ELF/symbol inputs must be absent'
    else:
        feed = json.loads((repo / 'output/launcher-updates.json').read_text())
        release = feed['releases'][0]
        assert feed['id'] == 'privacy-fixture' and release['version'] == '1.2.3'
        assert release['sha256'] == digest(archive) and release['bytes'] == archive.stat().st_size
        assert release['host_contract'] == 'launcher-v1.0' and release['api']['major'] == 1


def cases():
    # Each case is separate so case-insensitive hosts never collapse distinct fixture names.
    return [
        ('canonical', ['config/network-shares.json'], False),
        ('staging-prefix', ['config/network-shares.json' + suffix
                            for suffix in ('.tmp', '.new', '.backup', 'anything')], False),
        ('mixed-case', ['CoNfIg/NETWORK-SHARES.JSON.new'], False),
        ('config-dot', ['config./network-shares.json'], True),
        ('config-space', ['config /network-shares.json.new'], True),
        ('component-alias', ['nested/CONFIG. /NeTwOrK-ShArEs.JsOn.pending'], True),
        ('backslash', [r'config\network-shares.json'], True),
        ('backslash-component', [r'prefix\CONFIG. /NETWORK-SHARES.JSON.tmp'], True),
        ('nextendo', ['config/nextendo/' + name for name in
                      ('account.json', 'oauth/refresh.json', 'routes.json', 'tokens/token.bin')], False),
        ('nextendo-case', ['CoNfIg/NeXtEnDo/routes.json'], False),
        ('nextendo-alias', ['config. /NEXTENDO. /oauth/refresh.json'], True),
        ('nextendo-backslash', [r'config\nextendo\tokens\refresh.json'], True),
    ]


def qualify(output, baseline, report):
    hashes = {name: digest(ROOT / name) for name in SOURCES}
    report.update(source_files_sha256=hashes, python=sys.version, platform=sys.platform,
                  test_files_sha256={name: digest(ROOT / name) for name in
                                     ('tools/check-network-privacy.py', 'tools/check-export-privacy.py')},
                  scope='Actual packaging CLIs, synthetic data only; no native build/real app data/ELF symbols')
    sources = [('fixed', ROOT)] + ([('baseline', baseline)] if baseline else [])
    clean_hashes = {}
    for label, source in sources:
        report[label + '_source_files_sha256'] = {name: digest(source / name) for name in SOURCES}
        for collector in ('release', 'launcher'):
            for case, names, posix in [('clean', [], False), *cases()]:
                if posix and os.name != 'posix':
                    report['skipped'].append(label + '/' + collector + '/' + case); continue
                directory = output / label / collector / case
                repo, folder, public = fixture(source, directory, collector)
                denied = []
                for name in names:
                    path = folder / name; write(path, PRIVATE); denied.append(path)
                result, archive, record = invoke(repo, folder, collector, directory,
                                                  denied if label == 'fixed' else ())
                record.update(source=label, collector=collector, case=case)
                report['runs'].append(record)
                if collector == 'launcher' and case == 'backslash-component':
                    assert result.returncode != 0 and not archive.exists() and not record['guard_attempts'], record
                    record['observation'] = 'Invalid backslash ancestor rejected before private child access; existing path validation retained'
                    continue
                if label == 'baseline' and result.returncode:
                    record['observation'] = 'Baseline rejected fixture; no leak claimed'
                    continue
                assert result.returncode == 0, (label, collector, case, result.stderr)
                extra, leaks = inspect(archive, collector, public)
                record.update(extra_members=extra, private_canary_members=leaks)
                if label == 'fixed':
                    assert not extra and not leaks and not record['guard_attempts'], record
                verify_integrity(repo, folder, archive, collector, public)
                assert all(digest(repo / name) == digest(source / name) for name in SOURCES)
                if case == 'clean': clean_hashes[label, collector] = digest(archive)
    if baseline:
        for collector in ('release', 'launcher'):
            assert clean_hashes['baseline', collector] == clean_hashes['fixed', collector], 'Clean ZIP identity changed'
            assert any(r['source'] == 'baseline' and r['collector'] == collector and
                       r['private_canary_members'] for r in report['runs'] if 'private_canary_members' in r), 'Baseline leak not reproduced'

    # Blocked nodes must not be inspected, regardless of their underlying file type. The
    # guard forbids even lstat, and directory canaries include a FIFO/link on Linux.
    kinds = ['directory'] + (['symlink', 'dangling', 'fifo'] if os.name == 'posix' else [])
    if os.name != 'posix': report['skipped'].append('POSIX symlink/FIFO physical fixtures (run in Docker)')
    for collector in ('release', 'launcher'):
        for kind in kinds:
            private_nodes = ['config/network-shares.jsonanything', 'config/nextendo']
            if os.name == 'posix': private_nodes += ['CONFIG. /NEXTENDO. ', r'CONFIG. \NETWORK-SHARES.JSON.stage']
            for index, private in enumerate(private_nodes):
                name = kind + '-' + str(index)
                for label, source in sources if kind == 'directory' else [('fixed', ROOT)]:
                    directory = output / label / collector / ('blocked-' + name)
                    repo, folder, public = fixture(source, directory, collector)
                    path = folder / private; path.parent.mkdir(parents=True, exist_ok=True)
                    outside = repo / 'synthetic-outside-canary'; write(outside, PRIVATE)
                    if kind == 'directory':
                        write(path / 'unreadable/never-open.txt', PRIVATE)
                        if os.name == 'posix':
                            os.mkfifo(path / 'never-open.fifo')
                            (path / 'never-follow.link').symlink_to(outside)
                            (path / 'unreadable').chmod(0)
                    elif kind in ('symlink', 'dangling'):
                        path.symlink_to(outside if kind == 'symlink' else repo / 'nonexistent')
                    else: os.mkfifo(path)
                    result, archive, record = invoke(repo, folder, collector, directory, [path, outside])
                    record.update(source=label, collector=collector, case='blocked-' + name)
                    report['runs'].append(record)
                    if label == 'baseline':
                        assert record['guard_attempts'], 'Baseline did not exercise the pre-deny guard'
                        record['observation'] = 'Guard records attempted pre-deny access; canary reads prevented'
                    else:
                        assert result.returncode == 0 and not record['guard_attempts'], (record, result.stderr)
                        assert inspect(archive, collector, public) == ([], [])
                        verify_integrity(repo, folder, archive, collector, public)
                    # Restore only our own fixture permissions so temporary CI cleanup works.
                    if kind == 'directory': (path / 'unreadable').chmod(0o700)

    negative_checks(output, report)
    assert hashes == {name: digest(ROOT / name) for name in SOURCES}, 'Source checkout was changed'
    report['passed'] = True


def negative_checks(output, report):
    for collector in ('release', 'launcher'):
        kinds = ['public-symlink', 'public-directory-link', 'public-fifo'] if os.name == 'posix' else []
        if collector == 'launcher':
            kinds += ['bad-id', 'bad-schema', 'bad-version', 'bad-feed-url', 'bad-contract', 'bad-api',
                      'missing-resource', 'private-entry', 'private-font', 'inside-output',
                      'notes-limit', 'bad-min-app', 'path-length', 'path-depth', 'size-limit', 'count-limit',
                      'expanded-limit', 'bad-zip-url', 'path-backslash', 'path-colon', 'path-control',
                      'previous-id', 'root-symlink'] if os.name == 'posix' else ['bad-id', 'private-entry', 'inside-output']
        for kind in kinds:
            directory = output / 'negative' / collector / kind
            repo, folder, public = fixture(ROOT, directory, collector)
            extra = []
            if kind.startswith('public-'):
                if kind == 'public-fifo': os.mkfifo(folder / 'forbidden.fifo')
                elif kind == 'public-directory-link': (folder / 'forbidden.link').symlink_to(folder / 'config', target_is_directory=True)
                else: (folder / 'forbidden.link').symlink_to(folder / 'main.lua' if collector == 'launcher' else folder / 'eboot.bin')
            elif kind in ('inside-output', 'bad-min-app', 'notes-limit', 'previous-id', 'bad-zip-url'):
                if kind == 'inside-output': extra = ['--output', str(folder / 'output')]
                if kind == 'bad-min-app': extra = ['--min-app', 'invalid']
                if kind == 'bad-zip-url': extra = ['--url', 'http://example.invalid/fixture.zip']
                if kind == 'notes-limit':
                    write(repo / 'notes.txt', b'x' * 8193); extra = ['--notes', str(repo / 'notes.txt')]
                if kind == 'previous-id':
                    write(repo / 'previous.json', b'{"schema":1,"id":"other","releases":[]}')
                    extra = ['--previous', str(repo / 'previous.json')]
            elif kind.startswith('path-'):
                name = {'path-length': 'x' * 241, 'path-depth': '/'.join(['dir'] * 21 + ['file']),
                        'path-backslash': 'public\\file', 'path-colon': 'public:file',
                        'path-control': 'public\x01file'}[kind]
                write(folder / name, b'public')
            elif kind == 'size-limit':
                with (folder / 'too-large').open('wb') as stream: stream.truncate(64 * 1024**2 + 1)
            elif kind == 'count-limit':
                for n in range(4097): write(folder / 'many' / str(n), b'public')
            elif kind == 'expanded-limit':
                for n in range(5):
                    with (folder / ('large-' + str(n))).open('wb') as stream: stream.truncate(60 * 1024**2)
            elif kind == 'root-symlink':
                linked = repo / 'linked'; linked.symlink_to(folder, target_is_directory=True); folder = linked
            else:
                manifest = json.loads(public['manifest.json'])
                for case, key, value in [('bad-id', 'id', 'builtin'), ('bad-schema', 'schema', 2),
                                         ('bad-version', 'version', 'invalid'), ('bad-contract', 'host_contract', 'invalid'),
                                         ('bad-api', 'api', {'major': 2}), ('missing-resource', 'font', 'missing'),
                                         ('private-entry', 'entry', 'config/nextendo/account.json'),
                                         ('private-font', 'font', 'config/network-shares.json')]:
                    if kind == case: manifest[key] = value
                if kind == 'bad-feed-url': manifest['updates']['feed'] = 'https://user:pass@example.invalid/feed'
                if kind in ('private-entry', 'private-font'): write(folder / manifest['entry' if kind == 'private-entry' else 'font'], PRIVATE)
                write(folder / 'manifest.json', json.dumps(manifest).encode())
            result, archive, record = invoke(repo, folder, collector, directory, extra=extra)
            record.update(source='fixed', collector=collector, case=kind)
            report['runs'].append(record)
            assert result.returncode != 0, 'Existing validation disabled: ' + kind
            assert not archive.exists() or kind == 'previous-id', 'Unexpected archive publication: ' + kind


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--baseline', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='pe-export-privacy-') as tmp:
        output = args.output.resolve() if args.output else Path(tmp) / 'results'
        output.mkdir(parents=True, exist_ok=False)
        report = {'passed': False, 'runs': [], 'skipped': []}
        try:
            qualify(output, args.baseline.resolve() if args.baseline else None, report)
        finally:
            (output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print(f'Packaging privacy PASS: {len(report["runs"])} actual CLI invocations; report {digest(output / "report.json")}')
        print(f'Evidence: {output}' if args.output else 'Owned temporary synthetic fixtures removed on exit')


if __name__ == '__main__':
    main()
