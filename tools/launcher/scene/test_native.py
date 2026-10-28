#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic original archives and FLYT only; executes the console's C++ import backend."""
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from test_themezer import dds, sarc_bytes, yaz_literals, zip_bytes
from native_fixtures import flyt, material_flyt, picture_flyt, bntx, animation

binary=Path(sys.argv[1]).resolve()
checks=[]
def png(width,height):
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress((b'\0'+b'\x10\x90\x30\xff'*width)*height))+chunk(b'IEND',b'')
with tempfile.TemporaryDirectory(prefix='pe-native-import-') as temporary:
    work=Path(temporary);data=work/'data';data.mkdir();source=work/'fixture.nxtheme'
    def run(mode,path=source,root=data,*extra,ok=True):
        p=subprocess.run([str(binary),mode,str(path),str(root),*extra],text=True,capture_output=True,timeout=240 if mode=='fuzz' else 45)
        assert 'AddressSanitizer' not in p.stderr and 'runtime error:' not in p.stderr,p.stderr
        if ok:assert p.returncode==0,(mode,p.stdout,p.stderr)
        else:assert p.returncode!=0,(mode,p.stdout)
        return json.loads(p.stdout)
    files={'info.json':b'{"Version":17,"Target":"home","ThemeName":"Original native fixture","Author":"Synthetic tests"}','image.dds':dds()}
    texture=work/'fixture.bntx'
    for linear in [False,True]:
        for codec in [0x0b,0x1a,0x1b,0x1c]:
            blob,rgba=bntx(linear=linear,codec=codec);texture.write_bytes(blob)
            decoded=run('texture',texture,Path('Texture'))
            assert decoded=={'width':19,'height':33,'rgba_sha256':hashlib.sha256(rgba).hexdigest()},decoded
            checks.append('BNTX '+('linear' if linear else 'block-linear')+' RGBA8/BC codec '+hex(codec))
    blob,rgba=bntx();texture.write_bytes(blob)
    # Known byte offsets crossing GOB X/Y and block boundaries; not decoder roundtripping.
    assert blob[512:516]==rgba[:4] and blob[512+256:512+260]==rgba[32:36]
    assert blob[512+4096:512+4100]==rgba[32*19*4:32*19*4+4]
    assert run('fuzz-texture',texture)['mutations']==2000
    checks.append('2000 deterministic BNTX pointer/header/tile mutations under ASan/UBSan')
    for label,offset,kind,value in [('pointer64',68,'I',1),('swizzle',216,'I',0),('sRGB',156,'I',0x0b06),
                                   ('tile',146,'H',2),('extent',164,'I',100000),('codec',156,'I',0x9901),('block-height',180,'I',32)]:
        damaged=bytearray(blob);struct.pack_into('<'+kind,damaged,offset,value);texture.write_bytes(damaged)
        assert run('texture',texture,Path('Texture'),ok=False)['error'];checks.append('BNTX rejects '+label)
    damaged=bytearray(blob);struct.pack_into('<I',damaged,36,2);struct.pack_into('<Q',damaged,72,128);texture.write_bytes(damaged)
    assert 'ambiguous' in run('texture',texture,Path('Texture'),ok=False)['error']
    checks.append('BNTX duplicate named textures rejected')
    for order in ['<','>']:
        for encode in [lambda f:sarc_bytes(f,order),lambda f:yaz_literals(sarc_bytes(f,order)),zip_bytes]:
            source.write_bytes(encode(files));report=run('inspect');assert report['omissions']==[] and not report['faithful_parity']
            checks.append('native ZIP/SARC/Yaz0 '+order)
    # Fuzz the same archive/metadata/DDS paths with a tiny complete tile. The importer's
    # 1280x720 policy rejects it after decoding; thousands of full-size wallpapers would
    # spend the bounded CPU allowance on pixel throughput instead of reader mutations.
    source.write_bytes(sarc_bytes(dict(files,**{'image.dds':dds(4,4)})))
    fuzz=run('fuzz');assert fuzz['mutations']==2000 and fuzz['rejected']>0
    checks.append('2000 deterministic native archive/image/metadata mutations under ASan/UBSan')
    source.write_bytes(zip_bytes(files));report=run('inspect');theme_id=report['id']
    storage=work/'storage';storage.mkdir();credit=run('storage',source,storage)
    assert credit['reservation_calls']==3 and credit['released'] and credit['reserved_bytes']>2*len(files['image.dds'])
    checks.append('storage admission deny-before-write, single RAII credit, release and local overcapacity')
    run('install',source,data,ok=False)
    result=run('install',source,data,'consent');assert result['installed']==theme_id
    target=data/'launchers'/theme_id
    assert (target/'theme.json').exists() and not (target/'import-pending.json').exists()
    previous={str(p.relative_to(target)):p.read_bytes() for p in target.rglob('*') if p.is_file()}
    run('install',source,data,'consent',ok=False)
    assert previous=={str(p.relative_to(target)):p.read_bytes() for p in target.rglob('*') if p.is_file()}
    checks.append('consent, bounded staging, validated install, duplicate no-clobber')
    source.write_bytes(zip_bytes(dict(files,**{'layout.json':b'{"Files":[],"Anims":[]}'})))
    assert run('inspect')['omissions'][0]['code']=='missing-firmware'
    run('install',source,data,'consent',ok=False)
    assert run('install',source,data,'consent','partial')['installed']
    checks.append('missing firmware is explicit and requires distinct partial consent')
    for order in ['<','>']:
        layout=work/'test.bflyt';layout.write_bytes(flyt(order));patch=work/'patch.json'
        patch.write_text(json.dumps({'FileName':'blyt/test.bflyt','Patches':[{'PaneName':'Child','Position':{'X':30,'Y':40},'Visible':False}]}))
        result=run('layout',layout,patch)
        child=result['panes'][1];assert child['cx']==70 and child['cy']==140 and not child['visible']
        assert child['sx']==2 and child['sy']==3
        fw=data/'theme-import/firmware';fw.mkdir(parents=True,exist_ok=True);(fw/'home.szs').write_bytes(yaz_literals(sarc_bytes({'blyt/test.bflyt':layout.read_bytes()})))
        source.write_bytes(zip_bytes(dict(files,**{'layout.json':json.dumps({'Files':[json.loads(patch.read_text())]}).encode()})))
        result=run('inspect');assert result['mode']=='native-reconstruction-subset' and result['reconstruction'][0]['panes'][1]['visible']==False
        checks.append('FLYT byte order, hierarchy, patch replacement, affine/origin geometry '+order)
        binary_animation=work/'fixture.bflan';binary_animation.write_bytes(animation(order))
        normalized=run('animation',binary_animation)
        assert normalized['pai1']['Entries'][0]['Tags'][0]['Entries'][0]['KeyFrames'][1]['Value']==20
        aliases=work/'aliased.bflan';aliases.write_bytes(animation(order,32,5))
        assert 'aggregate' in run('animation',aliases,ok=False)['error']
        layout.write_bytes(material_flyt(order));(fw/'home.szs').write_bytes(sarc_bytes({'blyt/test.bflyt':layout.read_bytes(),'anim/test.bflan':binary_animation.read_bytes()},order))
        patch_data={'Files':[{'FileName':'blyt/test.bflyt','Patches':[],
                     'Materials':[{'MaterialName':'Material','ForegroundColor':'ff0080ff','Refs':[{'Name':'Texture','WrapS':1}],
                                   'Transforms':[{'Name':'Texture','X':0.25,'ScaleX':2}]}]}],
                    'Anims':[{'FileName':'anim/test.bflan'}]}
        source.write_bytes(zip_bytes(dict(files,**{'layout.json':json.dumps(patch_data).encode()})))
        material_data=work/('materials-'+('little' if order=='<' else 'big'));material_data.mkdir()
        shutil.copytree(data/'theme-import/firmware',material_data/'theme-import/firmware')
        result=run('install',source,material_data,'consent','partial');material=result['reconstruction'][0]['materials'][0]
        assert material['foreground']==0xff0080ff and material['references'][0]['WrapS']==1
        assert material['transforms'][0]['X']==0.25 and material['transforms'][0]['ScaleX']==2
        installed=material_data/'launchers'/result['id'];projected=run('project',installed,Path('fw-1'),'0.5')
        # Child base cx=20, half-width=50 => x=915; +10 * parent scale 2 => +30 host px.
        assert abs(projected[2]['x']-945)<0.01,projected
        calm=run('project',installed,Path('fw-1'),'0.5','calm');assert abs(calm[2]['x']-975)<0.01
        # The JSON replacement format follows exactly the same normalized path.
        patch_data['Anims'][0]['AnimJson']=json.dumps(normalized)
        source.write_bytes(zip_bytes(dict(files,**{'layout.json':json.dumps(patch_data).encode()})))
        result=run('inspect');assert any(item['code']=='animation-lifecycle' for item in result['omissions'])
        checks.append('material reference/color/SRT patches, binary/JSON FLAN, Hermite player and calm '+order)
    for order in ['<','>']:
        fw=data/'theme-import/firmware';blob,rgba=bntx()
        (fw/'home.szs').write_bytes(sarc_bytes({'blyt/picture.bflyt':picture_flyt(order),'timg/test.bntx':blob},order))
        patch_data={'Files':[{'FileName':'blyt/picture.bflyt','Patches':[]}]}
        source.write_bytes(zip_bytes(dict(files,**{'layout.json':json.dumps(patch_data).encode()})))
        case=work/('baked-'+('little' if order=='<' else 'big'));case.mkdir();shutil.copytree(fw,case/'theme-import/firmware')
        result=run('install',source,case,'consent','partial')
        picture=result['reconstruction'][0]['panes'][-1];assert picture['rendered_material']=='material-0-2'
        baked=(case/'launchers'/result['id']/'assets/material-0-2.tga').read_bytes()[18:]
        assert baked==bytes(v for i in range(0,len(rgba),4) for v in (rgba[i+2],rgba[i+1],rgba[i],rgba[i+3]))
        assert not any(i['code']=='material-pipeline' for i in result['omissions'])
        checks.append('real FLYT UV/material -> BNTX -> baked scene pixels '+order)
        # Uniform vertex tint, patched foreground, nearest-repeat SRT are actually baked.
        entry=patch_data['Files'][0];entry['Patches']=[dict(PaneName='Picture',**{key:'808080ff' for key in ['ColorTL','ColorTR','ColorBL','ColorBR']})]
        entry['Materials']=[{'MaterialName':'Material','ForegroundColor':'ff0080ff','Refs':[{'Name':'Texture','WrapS':1,'WrapT':1}],
                             'Transforms':[{'Name':'Texture','X':0.25,'ScaleX':2}]}]
        source.write_bytes(zip_bytes(dict(files,**{'layout.json':json.dumps(patch_data).encode()})))
        tinted=run('install',source,case,'consent','partial');pixels=(case/'launchers'/tinted['id']/'assets/material-0-2.tga').read_bytes()[18:]
        for x in [0,9,18]:
            u=(((x+0.5)/19)-0.5)*2+0.75;red=rgba[(math.floor(u*19)%19)*4]
            assert pixels[x*4:x*4+4]==bytes((9,0,round(red*128/255),255)),(x,pixels[x*4:x*4+4])
        checks.append('patched material colors, uniform vertex tint, nearest repeat and texture SRT '+order)
        entry['Patches'][0]['ColorTL']='ff0000ff';source.write_bytes(zip_bytes(dict(files,**{'layout.json':json.dumps(patch_data).encode()})))
        unsupported=run('inspect');assert any(i['code']=='material-pipeline' and 'four-corner' in i['detail'] for i in unsupported['omissions'])
        entry['Patches']=[];entry['Materials'][0]['Transforms'][0]['Rotation']=30
        source.write_bytes(zip_bytes(dict(files,**{'layout.json':json.dumps(patch_data).encode()})))
        unsupported=run('inspect');assert any(i['code']=='material-pipeline' and 'rotated' in i['detail'] for i in unsupported['omissions'])
        checks.append('unsupported gradient and rotated sampler are reported, not approximated '+order)
    source.write_bytes(zip_bytes(dict(files,**{'settings.png':png(64,56)})));report=run('inspect')
    assert report['omissions'][0]['code']=='native-icon-remap';run('install',source,data,'consent','partial')
    assert (data/'launchers'/report['id']/'assets/settings.tga').stat().st_size==18+64*56*4
    checks.append('native PNG icon decoding and explicit semantic remap')
    source.write_bytes(zip_bytes(dict(files,**{'apps.png':png(64,64),'user.png':png(48,48),'settings.png':png(64,56)})))
    mapped=run('install',source,data,'consent','partial');live=data/'launchers'/mapped['id']
    home=json.loads((live/'screens/home.json').read_text());buttons={n['id']:n for n in home['nodes'] if n['type']=='focus'}
    assert {key:buttons[key]['activate']['screen'] for key in buttons}=={'apps-button':'apps','settings-button':'set','user-button':'user'}
    assert (live/'screens/apps.json').exists() and (live/'screens/user.json').exists()
    checks.append('bounded artwork roles map explicitly to native games/profiles/settings workflows')
    parts={part+'.nxtheme':sarc_bytes(dict(files,**{'info.json':json.dumps({'Version':17,'Target':part}).encode()})) for part in ['home','lock','apps','set','news','user','psl']}
    source.write_bytes(zip_bytes(parts));report=run('install',source,data,'consent')
    assert len(json.loads((data/'launchers'/report['id']/'theme.json').read_text())['screens'])==7
    checks.append('seven-part native pack validation/install')
    # Durable state immediately before publication, after a subset of resources, and after the
    # final manifest link: recovery resumes only hash-bound files, never overwrites a conflict.
    for phase in ['reserved','resources','published','conflict','link']:
        case=work/phase;case.mkdir();source.write_bytes(zip_bytes(files));plan=run('stage',source,case)
        staging=Path(plan['staged']);live=case/'launchers'/plan['id'];live.mkdir(parents=True)
        hashes={str(p.relative_to(staging)):hashlib.sha256(p.read_bytes()).hexdigest() for p in staging.rglob('*') if p.is_file()}
        journal={'schema':1,'id':plan['id'],'staged':staging.name,'files':hashes}
        (live/'import-pending.json').write_text(json.dumps(journal))
        if phase=='resources':shutil.copy2(staging/'theme.json',live/'theme.json')
        if phase=='published':shutil.copytree(staging,live,dirs_exist_ok=True)
        if phase=='conflict':(live/'theme.json').write_text('user content')
        outside=work/'outside';outside.write_text('unchanged')
        if phase=='link':(live/'theme.json').symlink_to(outside)
        denied=run('recover-denied',case);assert not denied[0]['recovered'] and (live/'import-pending.json').exists()
        assert not (live/'manifest.json').exists() or phase=='published'
        result=run('recover',case)
        if phase in ['conflict','link']:
            assert not result[0]['recovered'] and not (live/'manifest.json').exists()
            assert outside.read_text()=='unchanged'
        else:assert result[0]['recovered'] and (live/'manifest.json').exists() and not (live/'import-pending.json').exists()
        checks.append('native recovery '+phase)
    for name,blob in [('truncated',b'Yaz0'+bytes(12)),('traversal',zip_bytes({'../escape':b'x'})),
                      ('raw firmware',zip_bytes(dict(files,**{'raw.bntx':b'not firmware'}))),
                      ('duplicate JSON',zip_bytes(dict(files,**{'info.json':b'{"Version":17,"Version":16,"Target":"home"}'}))),
                      ('unsupported DDS',zip_bytes(dict(files,**{'image.dds':dds().replace(b'DXT1',b'DX10')})))]:
        source.write_bytes(blob);run('inspect',ok=False);checks.append('reject '+name)
print(json.dumps({'passed':True,'checks':len(checks),'cases':checks}))
