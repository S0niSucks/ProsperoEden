#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real Settings controller/renderer, synthetic USB service. Not a console test."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile

binary, output = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
root = Path(__file__).resolve().parents[2]
output.mkdir(parents=True, exist_ok=True)
checks = []

for width, height in ((1280, 720), (1920, 1080)):
    with tempfile.TemporaryDirectory(prefix='eden-usb-ui-') as data:
        config = Path(data) / 'config'
        config.mkdir()
        (config / 'launcher.json').write_text(json.dumps({'schema': 1, 'selected': 'builtin'}))
        env = dict(os.environ, EGL_PLATFORM='surfaceless', LIBGL_ALWAYS_SOFTWARE='1',
                   GALLIUM_DRIVER='llvmpipe', PE_LAUNCHER_DATA=data)
        with (output / f'usb-{height}.log').open('w') as log:
            p = subprocess.Popen([str(binary), str(root / 'headless/prosperoeden/ui'), str(output), str(width), str(height)],
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=log, text=True, env=env)
            assert p.stdout.readline().startswith('READY ')

            def command(value):
                p.stdin.write(value + '\n'); p.stdin.flush()
                lines = []
                while True:
                    line = p.stdout.readline()
                    assert line, 'Preview ended during ' + value
                    lines.append(line)
                    if line.startswith('OK '):
                        assert 'GL=0' in line
                        return ''.join(lines)

            def key(value): command('key ' + value + ' 3')
            def state():
                return json.loads(next(x[4:] for x in command('usb-state').splitlines() if x.startswith('USB ')))
            def need(value, name):
                assert value, name
                checks.append({'height': height, 'check': name})
            def capture(name):
                command('tick 18')
                command('capture ' + str(output / f'usb-{height}-{name}.png'))
            def open_usb(): key('cross')
            def estimate(restore=False):
                if restore: key('down')
                key('cross'); key('cross'); command('tick 8')
                if restore: key('cross'); command('tick 8')

            key('r1'); key('r1'); key('r1')
            key('down'); key('down'); key('down'); key('right'); key('right')
            open_usb(); estimate()
            s = state()
            need(s['phase'] == 'estimate' and s['required'] > 0 and s['backups'] == 0, 'Space estimate precedes any copy')
            capture('estimate')
            key('cross')
            need(not state()['active'] and state()['backups'] == 0, 'Confirmation defaults to Cancel')
            open_usb(); estimate(); key('right'); command('usb-mode slow'); key('cross')
            need(state()['busy'] and not state()['launch_allowed'], 'Visible worker blocks launch')
            capture('progress'); key('circle')
            need(state()['cancels'] == 1 and state()['backups'] == 0, 'Cancel never publishes a backup')
            capture('cancelled'); key('circle')
            command('usb-mode ready'); open_usb(); estimate(); key('down'); key('cross'); command('tick 100')
            need(state()['phase'] == 'complete' and state()['backups'] == 1, 'Confirmed backup completes')
            capture('complete'); key('circle')
            command('usb-mode no_drive'); open_usb(); key('cross')
            capture('no-drive'); key('circle')
            command('usb-mode low_space'); open_usb(); estimate(); key('down'); key('cross')
            need(state()['backups'] == 1 and not state()['busy'], 'Low space disables confirmation')
            capture('low-space'); key('circle')
            command('usb-mode bad_manifest'); open_usb(); estimate(True)
            need(state()['restores'] == 0, 'Invalid backup cannot stage a restore'); key('circle')
            command('usb-mode ready'); open_usb(); estimate(True)
            need(state()['phase'] == 'restore_estimate' and state()['restores'] == 0, 'Restore estimate and preservation warning precede confirmation')
            capture('restore-confirm'); key('down'); key('cross'); command('tick 100')
            need(state()['restart'] and state()['restores'] == 1, 'Restore is staged, never hot-swapped')
            capture('restart'); key('circle'); key('r1'); key('options')
            command('picker'); command('select builtin')
            need(state()['active'] and not state()['launch_allowed'], 'Restart requirement cannot be bypassed by navigation')
            p.stdin.write('exit\n'); p.stdin.flush(); p.wait(timeout=20)
            assert p.returncode == 0

(output / 'report.json').write_text(json.dumps({'passed': True, 'checks': checks,
    'scope': 'Actual Settings/controller/renderer with synthetic service; not PS5/filesystem qualification'}, indent=2))
print('USB UI PASS:', len(checks), 'checks at 720p/1080p, GL error-free')
