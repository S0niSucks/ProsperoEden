#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Controller-only helpers. Preview services are synthetic, never hardware/writer evidence."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def settings_navigation(category):
    """Fresh builtin Home, default grid theme: locate a category in this exact source snapshot."""
    source = (ROOT / 'headless/prosperoeden/pe/ui/settings.cpp').read_text()
    table = re.search(r'kCategories\[kCategoryCount\]\s*=\s*\{(.*?)\};', source, re.S)
    columns = re.search(r'constexpr int kTileColumns = (\d+);', source)
    assert table and columns, 'Settings layout changed; update the test navigator explicitly'
    labels = re.findall(r'TR\("([^"\\]+)"\)', table[1])
    assert labels.count(category) == 1, (category, labels)
    index, width = labels.index(category), int(columns[1])
    return {'category': category, 'index': index, 'category_count': len(labels), 'columns': width,
            'keys': ['r1'] * 3 + ['down'] * (index // width) + ['right'] * (index % width)}


def focus_settings(key, category):
    for button in settings_navigation(category)['keys']:
        key(button)


def sha256(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write_report(output, checks, scope, binary):
    provenance = os.environ.get('PE_UI_SNAPSHOT_INFO')
    source = json.loads(Path(provenance).read_text()) if provenance else None
    (Path(output) / 'report.json').write_text(json.dumps({
        'passed': True, 'checks': checks, 'scope': scope, 'source_snapshot': source,
        'preview_sha256': sha256(binary),
        'settings_navigation': [settings_navigation(name) for name in ('USB backup', 'Amiibo')],
    }, indent=2))


class PreviewSession:
    def __init__(self, binary, output, data, name, width=1280, height=720):
        self.name, self.output = name, Path(output)
        self.stderr_path = self.output / (name + '.stderr.log')
        self.stderr = self.stderr_path.open('w')
        self.transcript = (self.output / (name + '.commands.log')).open('w')
        env = dict(os.environ, EGL_PLATFORM='surfaceless', LIBGL_ALWAYS_SOFTWARE='1',
                   GALLIUM_DRIVER='llvmpipe', PE_LAUNCHER_DATA=str(data))
        self.p = subprocess.Popen([str(binary), str(ROOT / 'headless/prosperoeden/ui'), str(output),
                                   str(width), str(height)], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=self.stderr, text=True, env=env)
        ready = self.p.stdout.readline()
        self.transcript.write(ready); self.transcript.flush()
        assert ready.startswith('READY '), ready

    def command(self, text):
        self.transcript.write('> ' + text + '\n'); self.transcript.flush()
        self.p.stdin.write(text + '\n'); self.p.stdin.flush()
        lines = []
        while True:
            line = self.p.stdout.readline()
            assert line, 'Preview ended during ' + text
            self.transcript.write(line); self.transcript.flush(); lines.append(line)
            if line.startswith('OK '):
                assert 'GL=0' in line, line
                return ''.join(lines)

    def key(self, key):
        return self.command('key ' + key + ' 3')

    def json_command(self, command, prefix):
        return json.loads(next(line[len(prefix):] for line in self.command(command).splitlines()
                               if line.startswith(prefix)))

    def state(self, kind):
        return self.json_command(kind + '-state', kind.upper() + ' ')

    def capture(self, name):
        self.command('tick 24')
        self.command('capture ' + str(self.output / (self.name + '-' + name + '.png')))

    def close(self):
        self.p.stdin.write('exit\n'); self.p.stdin.flush()
        assert self.p.wait(timeout=20) == 0
        self.stderr.close(); self.transcript.close()
        errors = self.stderr_path.read_text()
        assert 'runtime error:' not in errors and 'ERROR: AddressSanitizer' not in errors, errors

    def __enter__(self):
        return self

    def __exit__(self, kind, value, traceback):
        if kind is None:
            self.close()
        else:
            self.p.terminate()
            try:
                self.p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.p.kill(); self.p.wait(timeout=5)
            self.stderr.close(); self.transcript.close()
