#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real linked RuntimeLauncher, GL, fonts, picker, builtin and mixer; simulated game services."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import struct
import wave
from test_themezer import dds, sarc_bytes, zip_bytes
from native_fixtures import material_flyt, picture_flyt, bntx, animation

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('binary',type=Path)
parser.add_argument('output',type=Path)
args=parser.parse_args()
root=Path(__file__).resolve().parents[3]
out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
checks=[]

class Session:
    def __init__(self,name,width=1280,height=720,selected='canopy-scene',mutate=None):
        self.name=name;self.work=out/name
        assert not self.work.exists(), 'Use a fresh evidence directory: '+str(self.work)
        self.data=self.work/'data';self.pack=self.data/'launchers/canopy-scene'
        shutil.copytree(root/'examples/canopy-scene',self.pack)
        (self.data/'config').mkdir()
        (self.data/'config/launcher.json').write_text(json.dumps({'schema':1,'selected':selected}))
        if mutate:mutate(self)
        self.log=[];self.err=(self.work/'stderr.log').open('w')
        env=dict(os.environ,PE_LAUNCHER_DATA=str(self.data),EGL_PLATFORM='surfaceless',
                 LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe')
        self.p=subprocess.Popen([str(args.binary),str(root/'headless/prosperoeden/ui'),str(self.work),str(width),str(height)],
                                stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=self.err,text=True,env=env)
        ready=self.p.stdout.readline();self.log.append(ready)
        (self.work/'commands.log').write_text(''.join(self.log))
        assert ready.startswith('READY '),(name,ready)
        self.ready=ready
    def command(self,line):
        self.p.stdin.write(line+'\n');self.p.stdin.flush();result=[]
        while True:
            value=self.p.stdout.readline();assert value,(self.name,line,'frontend terminated; inspect stderr.log')
            self.log.append(value);result.append(value)
            if value.startswith('OK '):
                (self.work/'commands.log').write_text(''.join(self.log))
                assert 'GL=0' in value,value
                return ''.join(result)
    def key(self,key,frames=3):return self.command(f'key {key} {frames}')
    def need(self,condition,label):
        assert condition,(self.name,label,''.join(self.log[-10:]))
        checks.append({'case':self.name,'check':label,'passed':True})
    def capture(self,name):
        self.command('tick 30')
        self.command('capture '+str(self.work/(name+'.png')))
    def importing(self,phase):
        for _ in range(100):
            lines=self.command('import-status').splitlines();status=json.loads(next(line[7:] for line in lines if line.startswith('IMPORT ')))
            if status['phase']==phase:return status
            assert status['phase']!='failed',status
            self.command('tick 6')
        raise AssertionError(('import timed out',phase,status))
    def close(self):
        self.p.stdin.write('exit\n');self.p.stdin.flush();self.p.wait(timeout=30);self.log.append(self.p.stdout.read());self.err.close()
        (self.work/'commands.log').write_text(''.join(self.log))
        assert self.p.returncode==0,(self.name,self.p.returncode)
        self.need('TEARDOWN audio=stopped voices=0' in self.log[-1],'audio stopped before PCM teardown; post-destruction drain safe')
        log=(self.work/'stderr.log').read_text()
        assert 'ERROR: AddressSanitizer' not in log and 'runtime error:' not in log,log

for width,height in [(1280,720),(1920,1080)]:
    s=Session(f'captures-{height}',width,height)
    s.need('READY canopy-scene' in s.ready,'scene selected at startup')
    for look in ['normal','large','contrast','calm','large,contrast,calm']:
        s.command('look '+look);s.capture('library-'+look.replace(',','-'))
        s.key('square');s.command('tick 20');s.capture('profiles-'+look.replace(',','-'))
        s.key('circle');s.key('triangle');s.command('tick 20');s.capture('settings-'+look.replace(',','-'))
        s.key('circle')
    s.command('look normal');s.key('triangle');s.key('cross');s.capture('video-preferences')
    before=s.command('prefs');s.key('cross');after=s.command('prefs');s.need(before!=after,'scene video control reaches native preferences')
    s.key('circle');s.key('down');s.key('cross');s.capture('audio-preferences');s.key('cross')
    s.need('active=canopy-scene' in s.command('status'),'audio control stays in scene')
    s.key('circle');s.key('down');s.key('down');s.key('cross');s.key('cross');s.command('tick 30');s.capture('accessibility-larger-text')
    s.need('"large_text":true' in s.command('prefs'),'scene accessibility control saves host override')
    s.key('circle');s.key('circle');s.command('look normal');s.key('r1');s.capture('game-options')
    s.key('down');s.key('cross');s.capture('game-video');s.key('cross');s.key('circle')
    s.key('down');s.key('down');s.key('cross');s.capture('save-transfer');s.key('cross');s.capture('save-export-confirmation');s.key('circle');s.key('circle')
    for _ in range(4):s.key('down')
    s.key('cross');s.capture('mods');s.key('circle')
    for _ in range(5):s.key('down')
    s.key('cross');s.capture('cheats');s.key('circle');s.key('circle')
    s.command('recreate');s.need('active=canopy-scene' in s.command('status'),'context resource release/reload preserves scene')
    s.capture('library-context-recreated');s.command('picker');s.capture('picker')
    s.key('cross');s.need('active=builtin' in s.command('status'),'picker builtin recovery')
    s.capture('builtin-recovery');s.command('picker');s.key('down');s.key('cross')
    s.need('active=canopy-scene' in s.command('status'),'picker scene selection');s.close()

def broken(s):
    (s.pack/'screens/home.json').write_text('{"nodes":[{"id":"bad","type":"shader"}]}')
s=Session('startup-fallback',mutate=broken)
s.need('READY builtin' in s.ready,'invalid saved scene falls back to builtin')
s.need(json.loads((s.data/'config/launcher.json').read_text())['selected']=='canopy-scene','fallback preserves selection for repair')
s.capture('fallback');s.close()
s=Session('picker-failure',selected='builtin',mutate=broken)
s.command('picker');s.key('down');s.key('cross');s.need('active=builtin' in s.command('status'),'failed picker selection retains incumbent')
s.capture('picker-error');s.key('up');s.key('cross');s.close()
def forced(s):(s.data/'config/force-builtin-launcher').touch()
s=Session('force-builtin',mutate=forced);s.need('READY builtin' in s.ready,'force builtin marker wins');s.close()

s=Session('profile-workflows');s.key('square');s.key('down');s.key('cross')
def profiles(s):return json.loads(next(line[9:] for line in s.command('profiles').splitlines() if line.startswith('PROFILES ')))
s.need(profiles(s)[1]['playing'],'scene profile choice uses native service')
before=profiles(s);s.key('square');s.capture('profile-rename-confirmation')
s.need(profiles(s)==before,'profile rename waits for host confirmation');s.key('cross')
s.need(profiles(s)!=before,'confirmed profile rename reaches native service')
s.key('l1');s.key('cross');s.need(len(profiles(s))==3,'confirmed add profile')
s.key('triangle');s.capture('profile-remove-confirmation');s.key('circle')
s.need(len(profiles(s))==3,'cancel remove profile preserves data');s.close()

# A bounded script-free hostile fixture exercises host confirmation; it cannot self-confirm.
def destructive(s):
    path=s.pack/'screens/home.json';model=json.loads(path.read_text())
    model['input']['secondary']={'action':'game.delete','args':{'game':{'$bind':'/selected/handle'}}}
    path.write_text(json.dumps(model))
s=Session('host-confirmation',mutate=destructive)
s.key('square');s.capture('confirm-delete');s.need('LAUNCHES []' in s.command('launches'),'confirmation does not launch')
s.need('REMOVED []' in s.command('removed'),'theme cannot answer confirmation')
s.key('circle');s.key('cross');s.need('done=1' in s.command('status'),'cancelled deletion leaves game launchable');s.close()
s=Session('confirm-delete',mutate=destructive);s.key('square');s.key('cross');s.command('tick 60')
s.need('REMOVED ["' in s.command('removed'),'host confirmed deletion reaches native service');s.close()

for mode in ['off','conflict','failed']:
    s=Session('launch-'+mode);s.command('sync '+mode);s.key('cross');s.command('tick 30')
    if mode!='off':
        s.need('done=0' in s.command('status'),'save decision blocks launch');s.capture('save-'+mode)
        if mode=='failed':s.key('down')
        s.key('cross');s.command('tick 90')
    s.need('done=1' in s.command('status'),'validated launch completes')
    launches=s.command('launches');s.need(launches.count('"profile":0')==1,'will_play exactly once')
    s.key('cross');s.need(s.command('launches')==launches,'repeated input cannot duplicate committed launch');s.close()
s=Session('cancel-save');s.command('sync conflict');s.key('cross');s.key('circle')
s.need('done=0' in s.command('status') and 'LAUNCHES []' in s.command('launches'),'cancel save leaves launch uncommitted');s.close()
s=Session('stale-launch');s.command('sync conflict');s.key('cross');s.command('remove-first');s.key('cross');s.command('tick 90')
s.need('done=0' in s.command('status') and 'LAUNCHES []' in s.command('launches'),'game removed during sync is revalidated');s.close()

def music(s):
    (s.pack/'assets').mkdir(exist_ok=True)
    with wave.open(str(s.pack/'assets/test.wav'),'wb') as wav:
        wav.setparams((2,2,48000,0,'NONE','not compressed'));wav.writeframes(b'\x00\x20\x00\x20'*48000)
    path=s.pack/'theme.json';model=json.loads(path.read_text());model['audio']={'music':'assets/test.wav'};path.write_text(json.dumps(model))
    path=s.pack/'screens/home.json';model=json.loads(path.read_text());model['music']='music';path.write_text(json.dumps(model))
s=Session('audio-lifetime',mutate=music)
s.need('"voices":1' in s.command('audio'),'host schedules scene recording')
s.command('volume 0');s.need('"peak":0' in s.command('audio'),'menu sound Off silences recording')
s.command('volume 70');s.command('audio-pause on');s.command('select builtin');s.command('select canopy-scene')
s.command('audio-pause off');s.command('tick 30');s.need('active=canopy-scene' in s.command('status'),'queued audio retirement survives pack switch')
s.command('recreate');s.need('"dropped":0' in s.command('audio'),'context reload does not invalidate PCM or overflow mixer queue');s.close()

def import_fixture(s,kind):
    inbox=s.data/'theme-import/inbox';inbox.mkdir(parents=True)
    files={'info.json':b'{"Version":17,"Target":"home","ThemeName":"Native synthetic preview"}','image.dds':dds()}
    if kind=='partial':files['layout.json']=b'{"Files":[]}'
    if kind in ['firmware','material']:
        firmware=s.data/'theme-import/firmware';firmware.mkdir()
        members={'blyt/test.bflyt':material_flyt(),'anim/test.bflan':animation()}
        if kind=='material':
            members['blyt/test.bflyt']=picture_flyt(width=320,height=180);members['timg/original.bntx']=bntx(320,180)[0]
            files.update({'settings.dds':dds(64,56),'apps.dds':dds(64,64),'user.dds':dds(48,48)})
        (firmware/'home.szs').write_bytes(sarc_bytes(members))
        files['layout.json']=json.dumps({'Files':[{'FileName':'blyt/test.bflyt','Patches':[]}],
                                         'Anims':[{'FileName':'anim/test.bflan'}]}).encode()
    (inbox/'original.nxtheme').write_bytes(sarc_bytes(files))
for kind in ['artwork','partial','firmware','material','material-1080']:
    large=kind=='material-1080';kind='material' if large else kind
    partial=kind!='artwork'
    s=Session('native-import-'+kind+('-1080' if large else ''),1920 if large else 1280,1080 if large else 720,mutate=lambda s:import_fixture(s,kind))
    before=(s.data/'config/launcher.json').read_bytes()
    s.command('picker');s.key('square');s.capture('import-inbox');s.key('cross');status=s.importing('preview')
    s.need(not list((s.data/'launchers').glob('nx-*')),'preview has not installed or changed selection')
    s.capture('import-report');s.command('recreate');s.capture('import-preview-context-recreated')
    if partial:
        s.key('cross');s.need('"phase":"preview"' in s.command('import-status'),'ordinary confirm cannot accept unsupported items')
        s.key('square')
    else:s.key('cross')
    s.importing('consent');s.capture('import-consent');s.key('circle')
    s.need(not list((s.data/'launchers').glob('nx-*')),'cancelling consent has no installation side effect')
    s.key('square' if partial else 'cross');s.key('cross');status=s.importing('installed')
    installed=s.data/'launchers'/status['id'];s.need((installed/'manifest.json').exists(),'native host workflow installed validated scene')
    s.need((s.data/'config/launcher.json').read_bytes()==before,'installation does not change launcher selection')
    s.need(not (installed/'import-pending.json').exists(),'manifest-last publication completed and journal retired')
    s.capture('import-installed');s.key('cross');s.need(('active='+status['id']) in s.command('status'),'explicit post-install selection')
    s.capture('imported-native-scene')
    if kind in ['firmware','material']:
        s.key('l1');s.capture('firmware-geometry-motion');s.command('look calm');s.capture('firmware-geometry-settled')
        s.need('active='+status['id'] in s.command('status'),'firmware-backed retained geometry and FLAN render without fallback')
        s.need(status['report']['mode']=='native-reconstruction-subset','report preserves subset/non-parity classification')
        s.need((s.work/'firmware-geometry-motion.png').read_bytes()!=(s.work/'firmware-geometry-settled.png').read_bytes(),
                'supported FLAN translation is visibly different from reduced-motion settling')
        if kind=='material':
            s.need(status['report']['reconstruction'][0]['panes'][-1]['rendered_material']=='material-0-2','firmware BNTX/material was converted to a retained image')
            s.command('recreate');s.capture('firmware-material-context-recreated')
            s.need('active='+status['id'] in s.command('status'),'converted texture survives actual EGL context recreation')
    s.close()

captures=[]
for path in sorted(out.glob('*/*.png')):
    blob=path.read_bytes();assert blob.startswith(b'\x89PNG\r\n\x1a\n')
    width,height=struct.unpack('>II',blob[16:24]);captures.append({'file':str(path.relative_to(out)),'width':width,'height':height,'sha256':hashlib.sha256(blob).hexdigest()})
(out/'report.json').write_text(json.dumps({'passed':True,'checks':checks,'captures':captures,
    'binary_sha256':hashlib.sha256(args.binary.read_bytes()).hexdigest(),
    'scope':'Linked RuntimeLauncher, builtin UI, real GL/font/mixer, simulated services; not console qualification'},indent=2)+'\n')
print(json.dumps({'passed':True,'checks':len(checks),'evidence':str(out)}))
