#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real builtin/Lua controller + Mesa renderer; explicitly simulated service. Not PS5 evidence."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import tempfile

binary, output = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
root = Path(__file__).resolve().parents[2]
output.mkdir(parents=True, exist_ok=True)
checks = []
with tempfile.TemporaryDirectory(prefix='amiibo-ui-') as tmp:
    data = Path(tmp)
    config = data / 'config'
    config.mkdir()
    env = dict(os.environ, EGL_PLATFORM='surfaceless', LIBGL_ALWAYS_SOFTWARE='1',
               GALLIUM_DRIVER='llvmpipe', PE_LAUNCHER_DATA=str(data))

    class Session:
        def __init__(self, name, width=1280, height=720):
            self.name = name
            self.log = (output / (name + '.log')).open('w')
            self.p = subprocess.Popen([str(binary), str(root / 'headless/prosperoeden/ui'), str(output),
                                      str(width), str(height)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=self.log, text=True, env=env)
            assert self.p.stdout.readline().startswith('READY ')

        def command(self, text):
            self.p.stdin.write(text + '\n')
            self.p.stdin.flush()
            lines = []
            while True:
                line = self.p.stdout.readline()
                assert line, text
                lines.append(line)
                if line.startswith('OK '):
                    assert 'GL=0' in line
                    return ''.join(lines)

        def key(self, key):
            return self.command('key ' + key + ' 3')

        def state(self):
            return json.loads(next(line[7:] for line in self.command('amiibo-state').splitlines()
                                   if line.startswith('AMIIBO ')))

        def capture(self, name):
            self.command('tick 24')
            self.command('capture ' + str(output / (self.name + '-' + name + '.png')))

        def need(self, value, label):
            assert value, label
            checks.append({'case': self.name, 'check': label, 'passed': True})

        def close(self):
            self.p.stdin.write('exit\n')
            self.p.stdin.flush()
            assert self.p.wait(timeout=15) == 0
            self.log.close()

    for width, height in ((1280, 720), (1920, 1080)):
        (config / 'launcher.json').write_text(json.dumps({'schema': 1, 'selected': 'builtin'}))
        s = Session('builtin-' + str(height), width, height)
        s.need(s.state()['requests'] == 0, 'No startup network')
        # R1 sections: Library, Multiplayer, Settings. Bottom row, third tile is Amiibo.
        for _ in range(3): s.key('r1')
        for _ in range(3): s.key('down')
        s.key('right'); s.key('right'); s.key('cross')
        s.need(s.state()['requests'] == 0 and s.state()['offline'], 'Opening Amiibo reads only offline catalog')
        s.capture('offline')
        s.key('cross'); s.need(s.state()['requests'] == 0, 'First selection requests confirmation only')
        s.capture('confirmation')
        s.key('circle'); s.need(s.state()['requests'] == 0, 'Confirmation can be declined without network')
        s.command('amiibo-mode slow'); s.key('cross'); s.key('cross')
        s.need(s.state()['busy'] and s.state()['requests'] == 1, 'Explicit confirmed catalog download has visible progress')
        s.capture('progress'); s.key('circle')
        s.need(s.state()['phase'] == 'cancelled', 'Circle cancels active request')
        s.capture('cancelled')
        s.key('down'); s.key('cross'); s.key('down'); s.key('cross')
        s.need(s.state()['requests'] == 1, 'Selecting a catalog model needs separate photo confirmation')
        s.capture('photo-confirmation'); s.key('cross')
        s.need(s.state()['busy'] and s.state()['ids'] == ['0000000000000000'], 'Photo request uses exact public ID only')
        s.key('circle'); s.command('amiibo-mode failed')
        s.key('cross'); s.key('cross'); s.command('tick 70')
        s.need(s.state()['phase'] == 'failed', 'Failure offers offline fallback and retry')
        s.capture('failed'); s.key('circle'); s.close()

    pack = data / 'launchers/amiibo-test'
    pack.mkdir(parents=True)
    shutil.copytree(root / 'examples/grid-launcher/assets', pack / 'assets')
    manifest = json.loads((root / 'examples/grid-launcher/manifest.json').read_text())
    manifest['id'] = 'amiibo-test'
    (pack / 'manifest.json').write_text(json.dumps(manifest))
    (pack / 'main.lua').write_text('''
local startup
return {init=function()
 startup=host.action("amiibo_sync.start",{ids={"0000000000000001"},confirmed=true})
end,draw=function() end,input=function(e)
 local actions={cross="amiibo_sync.start",square="amiibo_sync.offline",triangle="amiibo_sync.start",circle="amiibo_sync.cancel"}
 local r=actions[e.key] and host.action(actions[e.key],{ids={"0000000000000001"},confirmed=e.key=="triangle"}) or {}
 host.action("state.save",{startup=startup,result=r,status=host.query("amiibo_sync")})
end}
''')
    (config / 'launcher.json').write_text(json.dumps({'schema': 1, 'selected': 'amiibo-test'}))
    s = Session('lua')

    def result():
        return json.loads((config / 'launcher-state/amiibo-test.json').read_text())

    s.need(s.state()['requests'] == 0, 'Lua init performs no network operation')
    s.key('cross')
    s.need(not result()['startup']['ok'], 'Lua init rejects a forged confirmed boolean')
    s.need(not result()['result']['ok'], 'Lua cannot bypass confirmation')
    s.key('square'); s.need(result()['status']['offline'] and len(result()['status']['entries']) == 20, 'Lua offline catalog contains names and exact IDs')
    s.command('amiibo-mode slow'); s.key('triangle')
    s.need(result()['result']['ok'] and result()['status']['confirmation_pending'] and s.state()['requests'] == 0,
           'Lua intent opens host-owned confirmation without network')
    s.capture('native-confirmation'); s.key('cross')
    s.need(s.state()['busy'] and s.state()['requests'] == 1, 'Separate physical confirmation starts Lua-requested sync')
    s.key('circle'); s.need(result()['status']['phase'] == 'cancelled', 'Lua cancellation reaches service')
    s.close()
(output / 'report.json').write_text(json.dumps({'passed': True, 'checks': checks,
    'scope': 'Actual builtin/Lua controllers + Mesa renderer; simulated Amiibo service, not PS5/network'}, indent=2))
print(f'Amiibo UI PASS: {len(checks)} assertions, builtin 720p/1080p and Lua confirmation/offline/progress/cancel/failure paths')
