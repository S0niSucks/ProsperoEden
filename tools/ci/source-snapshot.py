#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Record a read-only checkout's Git state and verify its copied Docker build inputs.

source-snapshot.py CHECKOUT COPY OUTPUT
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

checkout, copy, output = map(Path, sys.argv[1:])


def git(*args):
    return subprocess.run(['git', '-c', f'safe.directory={checkout.as_posix()}', '-C', str(checkout), *args],
                          capture_output=True, check=True).stdout


names = git('ls-files', '-z', '--cached', '--others', '--exclude-standard').decode('utf-8').split('\0')
files = {}
for name in sorted(set(names) - {''}):
    relative = Path(name)
    if relative.is_absolute() or '..' in relative.parts:
        raise SystemExit('unsafe input path in Git listing')
    original, copied = checkout / relative, copy / relative
    if original.is_dir():
        continue  # a submodule; its pinned revision belongs to the dependency lock
    if not original.exists():
        if copied.exists():
            raise SystemExit(f'deleted input is still present in Docker copy: {name}')
        continue
    digest = hashlib.sha256(original.read_bytes()).hexdigest()
    if not copied.is_file() or hashlib.sha256(copied.read_bytes()).hexdigest() != digest:
        raise SystemExit(f'Docker copy differs from checkout: {name}')
    files[name] = digest
diff = git('diff', '--binary', 'HEAD')
status = git('status', '--porcelain=v1', '-z').decode('utf-8')
snapshot = {'schema': 1, 'head': git('rev-parse', 'HEAD').decode().strip(), 'dirty': bool(status),
            'diff_sha256': hashlib.sha256(diff).hexdigest(), 'files_sha256': files}
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(snapshot, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
print(f'Source snapshot: verified {len(files)} files, HEAD={snapshot["head"]}, dirty={snapshot["dirty"]}')
