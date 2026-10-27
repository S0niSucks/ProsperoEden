#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual built-in/Lua controller paths; simulated manager, no code/network/console access."""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('binary', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
args.output.mkdir(parents=True, exist_ok=True)
checks = []
with tempfile.TemporaryDirectory(prefix='eden-cheat-ui-') as temporary:
    data = Path(temporary)
    config = data / 'config'
    config.mkdir()
    env = dict(os.environ, EGL_PLATFORM='surfaceless', LIBGL_ALWAYS_SOFTWARE='1',
               GALLIUM_DRIVER='llvmpipe', PE_LAUNCHER_DATA=str(data))

    class Session:
        def __init__(self, name, width=1280, height=720):
            self.name = name
            self.log = (args.output / (name + '.log')).open('w')
            self.process = subprocess.Popen([str(args.binary.resolve()), str(root / 'headless/prosperoeden/ui'),
                str(args.output), str(width), str(height)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=self.log, text=True, env=env)
            assert self.process.stdout.readline().startswith('READY ')

        def command(self, line):
            self.process.stdin.write(line + '\n'); self.process.stdin.flush()
            result = []
            while True:
                value = self.process.stdout.readline()
                assert value, 'Preview ended during ' + line
                result.append(value)
                if value.startswith('OK '):
                    assert 'GL=0' in value
                    return ''.join(result)

        def key(self, key):
            self.command('key ' + key + ' 3')

        def state(self):
            text = self.command('cheat-state')
            return json.loads(next(line.removeprefix('CHEAT ') for line in text.splitlines() if line.startswith('CHEAT ')))

        def capture(self, name):
            self.command('tick 24') # settle focus before rendering a static evidence frame
            self.command('capture ' + str((args.output / (self.name + '-' + name + '.png')).resolve()))

        def need(self, value, label):
            assert value, label
            checks.append({'case': self.name, 'check': label, 'passed': True})

        def close(self):
            self.process.stdin.write('exit\n'); self.process.stdin.flush()
            self.process.wait(timeout=15)
            assert self.process.returncode == 0
            self.log.close()

    # Both output sizes use the real three-column game page and controller focus/confirmation.
    for width, height in ((1280, 720), (1920, 1080)):
        (config / 'launcher.json').write_text(json.dumps({'schema': 1, 'selected': 'builtin'}))
        s = Session('builtin-' + str(height), width, height)
        s.command('mods-empty'); s.key('r1'); s.key('triangle')
        for _ in range(8): s.key('down')
        s.key('cross')
        s.command('tick 45')
        s.need(s.state()['checks'] == 0, 'Entering Cheats never requests the network')
        s.capture('empty')
        s.key('down'); s.key('down'); s.key('cross')
        s.need(s.state()['installs'] == 0, 'Install without a checked catalog is refused')
        s.key('up'); s.key('up'); s.command('cheat-mode slow'); s.key('cross')
        s.need(s.state()['busy'], 'Check starts a visible cancellable worker')
        s.capture('progress')
        s.key('circle'); s.need(s.state()['phase'] == 'cancelled', 'Back cancels without enabling any code')
        s.capture('cancelled')
        s.command('cheat-mode ready'); s.key('cross'); s.command('tick 60')
        s.need(s.state()['phase'] == 'ready', 'Completion exposes a compatible catalog')
        s.need(s.state()['names'] == ['Preview optional cheat'], 'Compatible entries are named before installation')
        s.key('down'); s.key('down'); s.key('down'); s.key('cross')
        s.need(s.state()['installs'] == 0 and not s.state()['enabled'], 'Preview-name rows cannot install or enable a code')
        s.key('up'); s.key('up'); s.key('up')
        s.capture('ready')
        s.key('down'); s.key('down'); s.key('cross')
        s.need(s.state()['installs'] == 0, 'First Cross only confirms intent')
        s.capture('confirm')
        s.key('cross'); s.command('tick 60')
        s.need(s.state()['phase'] == 'installed' and not s.state()['enabled'], 'Confirmed installation stays disabled')
        s.capture('installed-off')
        s.key('down'); s.key('down'); s.key('cross')
        s.need(s.state()['enabled'], 'Explicit installed-code selection enables only that choice')
        s.key('cross'); s.need(not s.state()['enabled'], 'Installed-code selection can be turned off again')
        s.key('up'); s.key('up'); s.key('up'); s.key('up')
        s.command('cheat-mode no_match'); s.key('cross'); s.command('tick 60')
        s.need(s.state()['phase'] == 'no_match' and len(s.state()['other_builds']) == 2, 'Other build/version labels are visible without a fallback match')
        s.capture('other-builds')
        s.key('down'); s.key('down'); s.key('cross')
        s.need(s.state()['installs'] == 1, 'Mismatched catalog cannot be installed')
        s.close()

    pack = data / 'launchers/cheat-test'
    pack.mkdir(parents=True)
    shutil.copytree(root / 'examples/grid-launcher/assets', pack / 'assets')
    manifest = json.loads((root / 'examples/grid-launcher/manifest.json').read_text())
    manifest['id'] = 'cheat-test'
    (pack / 'manifest.json').write_text(json.dumps(manifest))
    (pack / 'main.lua').write_text('''
return {init=function() end,draw=function() end,input=function(e)
 local g=host.query("library").games[1].handle
 local actions={cross="cheat_database.check",square="cheat_database.cache",triangle="cheat_database.install",
   circle="cheat_database.cancel",right="launch.request"}
 local r=actions[e.key] and host.action(actions[e.key],{game=g,confirmed=e.key=="triangle"}) or {}
 host.action("state.save",{result=r,status=host.query("cheat_database")})
end}
''')
    (config / 'launcher.json').write_text(json.dumps({'schema': 1, 'selected': 'cheat-test'}))
    s = Session('lua')

    def result():
        return json.loads((config / 'launcher-state/cheat-test.json').read_text())

    s.key('triangle'); s.need(not result()['result']['ok'], 'Lua cannot install an unchecked catalog')
    s.command('cheat-mode slow'); s.key('cross')
    s.need(result()['result']['ok'] and result()['status']['busy'], 'Lua gets real status/progress model')
    s.key('right'); s.need(not result()['result']['ok'], 'Busy manager blocks Lua game launch')
    s.key('circle'); s.need(result()['status']['phase'] == 'cancelled', 'Lua cancellation reaches the service')
    s.command('cheat-mode ready'); s.key('square'); s.command('cheat-complete')
    s.need(s.state()['offline'] and s.state()['phase'] == 'ready', 'Offline cache is a separate explicit action')
    s.key('triangle'); s.command('cheat-complete')
    s.need(s.state()['phase'] == 'installed' and not s.state()['enabled'], 'Lua confirmed install remains disabled')
    s.close()

(args.output / 'cheat-ui-report.json').write_text(json.dumps({'passed': True, 'checks': checks,
    'scope': 'Real built-in and Lua UI/controllers/renderer, simulated manager; not a PS5 or network test.'}, indent=2))
print('Cheat UI PASS:', len(checks), 'controller/API checks at 720p and 1080p, GL error-free')
