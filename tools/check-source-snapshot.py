#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify Docker source-copy provenance, including local edits, new/deleted and ignored files."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

tool = Path(__file__).resolve().parent / 'ci/source-snapshot.py'
with tempfile.TemporaryDirectory(prefix='eden-source-snapshot-') as tmp:
    root = Path(tmp)
    repo, copy, manifest = root / 'repo', root / 'copy', root / 'manifest.json'
    repo.mkdir(); copy.mkdir()

    def git(*args):
        return subprocess.run(['git', '-C', str(repo), *args], capture_output=True, check=True).stdout

    git('init', '-q')
    (repo / 'keep.txt').write_text('before\n')
    (repo / 'deleted.txt').write_text('deleted\n')
    (repo / '.gitignore').write_text('ignored/\n')
    git('add', '.')
    git('-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'fixture')
    (repo / 'keep.txt').write_text('after\n')
    (repo / 'deleted.txt').unlink()
    (repo / 'new.txt').write_text('new\n')
    (repo / 'ignored').mkdir(); (repo / 'ignored/private.txt').write_text('not a build input\n')
    for name in ['keep.txt', 'new.txt', '.gitignore']:
        shutil.copy2(repo / name, copy / name)

    def run(ok):
        result = subprocess.run([sys.executable, '-B', str(tool), str(repo), str(copy), str(manifest)],
                                capture_output=True, text=True)
        assert (result.returncode == 0) == ok, result.stdout + result.stderr

    run(True)
    data = json.loads(manifest.read_text())
    assert data['dirty'] and data['head'] == git('rev-parse', 'HEAD').decode().strip()
    assert set(data['files_sha256']) == {'keep.txt', 'new.txt', '.gitignore'}
    assert data['files_sha256']['new.txt'] == hashlib.sha256((repo / 'new.txt').read_bytes()).hexdigest()
    assert data['diff_sha256'] == hashlib.sha256(git('diff', '--binary', 'HEAD')).hexdigest()
    (copy / 'deleted.txt').write_text('old stale input'); run(False)
    (copy / 'deleted.txt').unlink()
    (copy / 'keep.txt').write_text('wrong copy'); run(False)
    shutil.copy2(repo / 'keep.txt', copy / 'keep.txt')
    (copy / 'new.txt').unlink(); run(False)
print('Source snapshot PASS: verified local/new/deleted inputs, ignored-file exclusion; stale/'
      'different/missing copy rejection')
