#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual release CLI: raw license inventory, extraction, reproducibility and refusal fixtures.
No native/ELF build, real app input, network, source offer or compliance certification.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import runpy
import shutil
import subprocess
import sys
import zipfile
from release_licenses import release_license_entries

ROOT = Path(__file__).resolve().parents[1]
fixtures = runpy.run_path(str(ROOT / 'tools/check-export-privacy.py'))
fixture, write, digest = (fixtures[name] for name in ('fixture', 'write', 'digest'))

# Unlike the privacy guard, this guard permits lstat: license nodes must be rejected by
# their actual type, not by an injected stat error. It independently refuses payload IO.
IO_GUARD = r'''
import json, os, sys
nodes = [os.path.normcase(os.path.abspath(p)) for p in json.loads(os.environ['LICENSE_DENIED_IO'])]
log = os.environ['LICENSE_IO_LOG']
def record(item):
    with open(log, 'a', encoding='utf-8') as stream: stream.write(json.dumps(item) + '\n')
record({'guard': 'active'})
def audit(event, args):
    if event not in ('open', 'os.scandir', 'os.listdir') or not args or not isinstance(args[0], (str, bytes, os.PathLike)): return
    path = os.path.normcase(os.path.abspath(os.fsdecode(args[0])))
    if any(path == node or path.startswith(node + os.sep) for node in nodes):
        record({'event': event, 'path': path})
        raise PermissionError('Synthetic license IO guard')
sys.addaudithook(audit)
'''


def cli(repo, app, logs, denied=()):
    logs.mkdir(parents=True)
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1')
    if denied:
        guard = logs / 'guard'; guard.mkdir()
        write(guard / 'sitecustomize.py', IO_GUARD.encode())
        env.update(PYTHONPATH=str(guard), LICENSE_DENIED_IO=json.dumps([str(p.absolute()) for p in denied]),
                   LICENSE_IO_LOG=str(logs / 'guard.jsonl'))
    command = [sys.executable, '-B', str(repo / 'tools/ci/make-dist.py'), str(app)]
    result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=20)
    (logs / 'stdout.log').write_text(result.stdout, encoding='utf-8')
    (logs / 'stderr.log').write_text(result.stderr, encoding='utf-8')
    events = [json.loads(line) for line in (logs / 'guard.jsonl').read_text().splitlines()] if denied else []
    if denied: assert events[0] == {'guard': 'active'}
    archive = repo / 'dist/ProsperoEden-v1.000.001.zip'
    record = {'command': command, 'returncode': result.returncode, 'forbidden_io_attempts': events[1:],
              'zip_sha256': digest(archive) if archive.exists() else None}
    (logs / 'cli.json').write_text(json.dumps(record, indent=2))
    return result, archive, record


def stage_real(repo, entries):
    # Remove only the known synthetic files we just created, not any user-owned directory.
    for name in fixtures['license_fixture'](): (repo / 'third_party/licenses' / name).unlink()
    (repo / 'third_party/licenses/example').rmdir()
    for name, data in entries.items(): write(repo / name, data)


def compare_delta(old_path, new_path, additions):
    with zipfile.ZipFile(old_path) as old, zipfile.ZipFile(new_path) as new:
        assert set(new.namelist()) == set(old.namelist()) | additions
        for info in old.infolist():
            peer = new.getinfo(info.filename)
            assert old.read(info) == new.read(peer)
            assert (info.date_time, info.external_attr, info.compress_type) == (peer.date_time, peer.external_attr, peer.compress_type)


def qualify(output, baseline, report):
    entries = release_license_entries(ROOT)
    manifest_bytes = entries['third_party/licenses/SOURCES.json']
    manifest = json.loads(manifest_bytes)
    expected_licenses = {'third_party/licenses/SOURCES.json',
                         *('third_party/licenses/' + row['destination'] for row in manifest['files'])}
    assert set(entries) == expected_licenses | {'LICENSE', 'THIRD_PARTY_NOTICES.md'}
    for row in manifest['files']:
        data = entries['third_party/licenses/' + row['destination']]
        assert len(data) == row['size'] and hashlib.sha256(data).hexdigest() == row['sha256']
    source_hashes = {name: digest(ROOT / name) for name in
                     (*fixtures['source_files'](ROOT), 'tools/check-release-licenses.py', 'tools/check-export-privacy.py')}
    report.update(python=sys.version, platform=sys.platform, source_files_sha256=source_hashes,
                  license_manifest_sha256=hashlib.sha256(manifest_bytes).hexdigest(),
                  declared_notice_files=len(manifest['files']), packaged_license_files=len(expected_licenses),
                  raw_entries_sha256={name: hashlib.sha256(data).hexdigest() for name, data in entries.items()})
    repo, app, public = fixture(ROOT, output / 'real', 'release')
    stage_real(repo, entries)
    result, archive, record = cli(repo, app, output / 'real/first')
    report['runs'].append(record)
    assert result.returncode == 0, result.stderr
    expected = {**{'PPSA99008/' + name: data for name, data in public.items()},
                'README.md': b'Synthetic fixture README.md\n', **entries}
    with zipfile.ZipFile(archive) as zipped:
        assert zipped.testzip() is None and len(zipped.namelist()) == len(expected)
        assert set(zipped.namelist()) == set(expected)
        for info in zipped.infolist():
            assert zipped.read(info) == expected[info.filename]
            assert info.date_time == (2026, 1, 1, 0, 0, 0) and (info.external_attr >> 16) & 0o777 == 0o777
        for target in re.findall(r'\]\((third_party/licenses/[^)#]+)\)', entries['THIRD_PARTY_NOTICES.md'].decode()):
            assert target in expected or any(name.startswith(target.rstrip('/') + '/') for name in expected), target
        zipped.extractall(output / 'real/extracted')
    for name, data in expected.items():
        assert (output / 'real/extracted' / name).read_bytes() == data
    fixtures['verify_integrity'](repo, app, archive, 'release', public)
    shutil.copy2(archive, output / 'real/first/release.zip')
    for name in entries:
        assert (repo / name).read_bytes() == entries[name]
        os.utime(repo / name, (1800000000, 1800000000))
    result, second, record = cli(repo, app, output / 'real/second')
    report['runs'].append(record)
    assert result.returncode == 0 and second.read_bytes() == (output / 'real/first/release.zip').read_bytes()
    report['reproducible_release_sha256'] = digest(second)
    if baseline:
        old_repo, old_app, _ = fixture(baseline, output / 'baseline-real', 'release')
        stage_real(old_repo, entries)
        result, old, record = cli(old_repo, old_app, output / 'baseline-real/cli')
        report['runs'].append(record)
        assert result.returncode == 0, result.stderr
        compare_delta(old, second, expected_licenses)
        report['intentional_delta'] = {'baseline_sha256': digest(old), 'new_sha256': digest(second),
                                       'added_members': sorted(expected_licenses),
                                       'all_old_member_bytes_and_metadata_unchanged': True}
    adverse(output, report)
    assert source_hashes == {name: digest(ROOT / name) for name in source_hashes}
    assert entries == release_license_entries(ROOT), 'Maintained notice bytes were modified'
    report['passed'] = True


def adverse(output, report):
    kinds = ['missing-dir', 'missing-manifest', 'missing-file', 'altered-size', 'altered-same-size',
             'malformed-json', 'duplicate-key', 'wrong-format', 'empty-files', 'bad-record',
             'bad-hash', 'bad-size', 'duplicate-destination', 'case-destination', 'reserved-manifest',
             'traversal', 'absolute', 'backslash', 'colon', 'trailing-alias', 'unlisted-file',
             'missing-project-license', 'missing-project-notices']
    if os.name == 'posix':
        kinds += ['third-party-link', 'license-dir-link', 'component-dir-link', 'license-link',
                  'manifest-link', 'project-license-link', 'readme-link', 'license-fifo', 'manifest-fifo',
                  'unlisted-fifo', 'license-dir-fifo']
    else: report['skipped'].append('Physical symlink/FIFO cases require the Linux Docker run')
    for kind in kinds:
        directory = output / 'adverse' / kind
        repo, app, _ = fixture(ROOT, directory, 'release')
        base = repo / 'third_party/licenses'
        manifest_path = base / 'SOURCES.json'
        manifest = json.loads(manifest_path.read_bytes())
        license_path = base / 'example/COPYING'
        denied = []
        edit_manifest = False
        if kind == 'missing-dir': base.rename(repo / 'removed-licenses')
        elif kind == 'missing-manifest': manifest_path.unlink()
        elif kind == 'missing-file': license_path.unlink()
        elif kind.startswith('altered-'):
            data = license_path.read_bytes()
            write(license_path, data + b'x' if kind == 'altered-size' else b'X' + data[1:])
        elif kind in ('malformed-json', 'duplicate-key'):
            write(manifest_path, b'{' if kind == 'malformed-json' else b'{"format_version":1,"format_version":1,"files":[]}')
        elif kind == 'unlisted-file': write(base / 'unlisted.txt', b'Synthetic unlisted notice')
        elif kind.startswith('missing-project-'):
            (repo / ('LICENSE' if kind == 'missing-project-license' else 'THIRD_PARTY_NOTICES.md')).unlink()
        elif kind.endswith('-link') or kind.endswith('-fifo'):
            target = {'third-party-link': repo / 'third_party', 'license-dir-link': base,
                      'component-dir-link': base / 'example', 'license-link': license_path,
                      'manifest-link': manifest_path, 'project-license-link': repo / 'LICENSE',
                      'readme-link': repo / 'README.md', 'license-fifo': license_path,
                      'manifest-fifo': manifest_path, 'unlisted-fifo': base / 'extra.fifo',
                      'license-dir-fifo': base}[kind]
            outside = repo / 'synthetic-outside'
            write(outside, b'Synthetic canary, never read by the CLI')
            if target.is_dir(): target.rename(repo / 'removed-directory')
            elif target.exists(): target.unlink()
            if kind.endswith('-fifo'): os.mkfifo(target)
            else: target.symlink_to(outside)
            denied = [target, outside]
        else:
            edit_manifest = True
            row = manifest['files'][0]
            if kind == 'wrong-format': manifest['format_version'] = 2
            elif kind == 'empty-files': manifest['files'] = []
            elif kind == 'bad-record': manifest['files'] = [None]
            elif kind == 'bad-hash': row['sha256'] = 'invalid'
            elif kind == 'bad-size': row['size'] = True
            elif kind in ('duplicate-destination', 'case-destination'):
                manifest['files'].append({**row, 'destination': row['destination'] if kind == 'duplicate-destination' else row['destination'].upper()})
            else:
                row['destination'] = {'reserved-manifest': 'SOURCES.json', 'traversal': '../outside',
                                      'absolute': '/outside', 'backslash': 'example\\COPYING',
                                      'colon': 'example/C:OPYING', 'trailing-alias': 'example./COPYING'}[kind]
        if edit_manifest: write(manifest_path, json.dumps(manifest).encode())
        result, archive, record = cli(repo, app, directory / 'cli', denied)
        record['case'] = kind; report['runs'].append(record)
        assert result.returncode != 0 and not archive.exists(), (kind, result.stderr)
        assert not record['forbidden_io_attempts'], (kind, record)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--baseline', type=Path)
    args = parser.parse_args()
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=False)
    report = {'passed': False, 'runs': [], 'skipped': [],
              'scope': 'Synthetic release app plus maintained notice bytes; no ELF/native build, source/relink delivery or hardware qualification'}
    try:
        qualify(output, args.baseline.resolve() if args.baseline else None, report)
    finally:
        (output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(f'Release licenses PASS: {len(report["runs"])} actual CLI invocations; report {digest(output / "report.json")}')


if __name__ == '__main__':
    main()
