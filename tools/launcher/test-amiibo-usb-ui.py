#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Cross-feature native modal ownership; real controllers, explicitly fake preview services."""
from pathlib import Path
import json
import shutil
import sys
import tempfile
from ui_test_helpers import PreviewSession, ROOT, focus_settings, write_report

binary, output = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)
checks = []
for width, height in ((1280, 720), (1920, 1080)):
    with tempfile.TemporaryDirectory(prefix='amiibo-usb-ownership-') as tmp:
        data = Path(tmp); config = data / 'config'; config.mkdir()
        pack = data / 'launchers/ownership-test'; pack.mkdir(parents=True)
        shutil.copytree(ROOT / 'examples/grid-launcher/assets', pack / 'assets')
        manifest = json.loads((ROOT / 'examples/grid-launcher/manifest.json').read_text())
        manifest['id'] = 'ownership-test'
        (pack / 'manifest.json').write_text(json.dumps(manifest))
        (pack / 'main.lua').write_text('''
local startup, requested, updates, inputs, background = {}, false, 0, 0, false
local results = {}
local function save()
 host.action("state.save",{startup=startup,results=results,updates=updates,inputs=inputs,
   background=background,status=host.query("amiibo_sync")})
end
return {init=function()
 startup.amiibo=host.action("amiibo_sync.start",{confirmed=true})
 startup.usb=host.action("usb_backup.begin",{confirmed=true})
end,draw=function() end,update=function()
 updates=updates+1
 if requested and not background then
  background=true; results.background_usb=host.action("usb_backup.begin",{confirmed=true}); save()
 end
end,input=function(e)
 inputs=inputs+1
 if e.key=="triangle" then
  results.begin=host.action("usb_backup.begin",{confirmed=true})
  results.backup=host.action("usb_backup.backup",{confirmed=true})
  results.restore=host.action("usb_backup.restore",{confirmed=true})
  results.amiibo=host.action("amiibo_sync.start",{ids={"0000000000000001"},confirmed=true})
  requested=true
 elseif e.key=="circle" then host.action("amiibo_sync.cancel",{}) end
 save()
end}
''')
        (config / 'launcher.json').write_text(json.dumps({'schema': 1, 'selected': 'ownership-test'}))
        state_file = config / 'launcher-state/ownership-test.json'
        with PreviewSession(binary, output, data, f'ownership-{height}', width, height) as s:
            def need(value, name):
                assert value, name
                checks.append({'height': height, 'check': name, 'passed': True})

            def lua(): return json.loads(state_file.read_text())
            def amiibo(): return s.state('amiibo')
            def usb(): return s.state('usb')
            def builtin_idle():
                return 'STATE active=builtin done=0 game= error=' in s.command('status')
            def no_launches(): return not s.json_command('launches', 'LAUNCHES ')
            def estimate():
                s.key('cross'); s.key('cross'); s.key('cross'); s.command('tick 8')

            need(amiibo()['requests'] == 0 and not usb()['active'], 'Neither feature starts on Lua startup')
            s.command('amiibo-mode slow'); s.key('triangle')
            need(not lua()['startup']['amiibo']['ok'] and not lua()['startup']['usb']['ok'],
                 'Lua init cannot forge either feature start')
            need(all(not lua()['results'][name]['ok'] and lua()['results'][name]['error'] == 'Unknown launcher action'
                     for name in ('begin', 'backup', 'restore')) and not usb()['active'],
                 'Unsupported Lua USB routes cannot initiate backup/restore to escape Amiibo consent')
            need(lua()['results']['amiibo']['ok'] and lua()['status']['confirmation_pending'] and amiibo()['requests'] == 0,
                 'Lua intent opens host Amiibo consent without a request')
            frozen = state_file.read_bytes()
            s.command('select builtin'); s.command('picker'); s.key('options'); s.key('r1'); s.key('triangle')
            s.command('tick 180'); s.capture('amiibo-host-owned')
            need('STATE active=ownership-test done=0' in s.command('status') and state_file.read_bytes() == frozen,
                 'Pending Amiibo consent prevents builtin replacement and suspends pack update/input')
            need(amiibo()['requests'] == 0 and not usb()['active'] and no_launches(),
                 'Navigation during native consent grants neither network nor USB nor launch authority')
            s.key('cross')
            need(amiibo()['busy'] and amiibo()['requests'] == 1 and lua()['inputs'] == 1,
                 'Separate physical Cross is consumed by host and starts exactly one Amiibo request')
            need(lua()['background'] and not lua()['results']['background_usb']['ok'] and not usb()['active'],
                 'Resumed Lua update cannot create a USB session')
            s.key('circle'); s.key('circle'); s.command('select builtin')
            need(builtin_idle() and amiibo()['phase'] == 'cancelled' and amiibo()['requests'] == 1,
                 'After physical confirmation and cancellation, builtin return preserves request count')
            # open_picker() may be called directly by the preview while consent owns draw/input.
            # Returning to builtin clears that picker before testing the actual Settings path.
            s.capture('builtin-return')
            focus_settings(s.key, 'USB backup'); estimate()
            need(usb()['active'] and usb()['phase'] == 'estimate' and not usb()['launch_allowed'],
                 'Builtin USB estimate owns launch admission without starting a copy')
            s.capture('usb-confirm-before-navigation')
            s.command('offer v9.999.999'); s.command('picker'); s.command('select ownership-test')
            for key in ('options', 'r1', 'triangle'): s.key(key)
            s.command('tick 60'); s.capture('usb-confirm-after-navigation')
            need(builtin_idle() and usb()['active'] and usb()['phase'] == 'estimate' and no_launches(),
                 'USB modal blocks picker/launcher switching and pending updater navigation')
            s.key('cross')
            need(not usb()['active'] and usb()['backups'] == 0 and usb()['restores'] == 0,
                 'Attempted navigation does not change the USB Cancel default')
            s.command('select builtin'); focus_settings(s.key, 'Amiibo'); s.key('cross')
            need(amiibo()['offline'] and amiibo()['requests'] == 1 and not usb()['active'],
                 'Amiibo remains the separate fourth tile and browsing after USB stays offline')
            s.capture('amiibo-after-usb'); s.key('circle')
            s.command('select builtin'); focus_settings(s.key, 'USB backup'); estimate()
            s.key('right'); s.command('usb-mode slow'); s.key('cross')
            for command in ('picker', 'select ownership-test'): s.command(command)
            for key in ('options', 'r1', 'triangle', 'cross'): s.key(key)
            s.capture('usb-copy-gate')
            need(usb()['busy'] and not usb()['launch_allowed'] and builtin_idle() and no_launches(),
                 'USB copy keeps launch and updater/picker controls modal, even on Cross')
            need(amiibo()['requests'] == 1 and usb()['backups'] == 0 and usb()['restores'] == 0,
                 'Blocked controls create no extra network request or fake copy/restore completion')
            s.key('circle'); s.key('circle')
            need(not usb()['active'] and usb()['cancels'] == 1 and no_launches(),
                 'USB cancellation closes through its owner without launching a game')

write_report(output, checks, 'Actual combined builtin/Lua host/controller/Mesa paths; fake Amiibo/USB services only. '
             'No real writer drain, service lifecycle, USB filesystem, NFC, PS5 or hardware qualification.', binary)
print(f'Amiibo/USB ownership UI PASS: {len(checks)} assertions at 720p/1080p; fake-service scope only')
