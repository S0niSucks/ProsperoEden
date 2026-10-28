# SPDX-License-Identifier: GPL-3.0-or-later
"""Original synthetic format fixtures shared by native backend and linked workflow tests."""
import struct

def flyt(order='<'):
    def section(kind,payload):return kind+struct.pack(order+'I',8+len(payload))+payload
    def pane(name,x,y,sx,sy,w,h,origin=0):
        p=bytearray(76);p[0]=1;p[1]=origin;p[2]=255;p[4:4+len(name)]=name.encode()
        struct.pack_into(order+'10f',p,36,x,y,0,0,0,0,sx,sy,w,h)
        return section(b'pan1',p)
    sections=[section(b'lyt1',struct.pack(order+'Iff',0,1280,720)),pane('Root',10,20,2,3,400,200),
              section(b'pas1',b''),pane('Child',5,-6,1,1,50,20),section(b'pae1',b'')]
    body=b''.join(sections)
    return b'FLYT'+struct.pack(order+'HHIIHH',0xfeff,20,0x08000000,20+len(body),len(sections),0)+body

def material_flyt(order='<'):
    base=flyt(order);sections=base[20:]
    def section(kind,payload):return kind+struct.pack(order+'I',8+len(payload))+payload
    textures=section(b'txl1',struct.pack(order+'HHI',1,0,4)+b'Texture\0')
    name=b'Material'+bytes(20)
    material=name+struct.pack(order+'II',5,0)+bytes.fromhex('ffffffff00000000')+struct.pack(order+'HBB5f',0,4,4,0,0,0,1,1)
    materials=section(b'mat1',struct.pack(order+'HHI',1,0,16)+material)
    sections+=textures+materials
    return base[:12]+struct.pack(order+'IHH',20+len(sections),7,0)+sections

def animation(order='<',tracks=1,tags=1):
    # Section-relative offset tables. Repeated pointers also exercise aggregate read budgets.
    keys=struct.pack(order+'6f',0,0,0,60,20,0)
    track=struct.pack(order+'BBHHHI',0,0,2,2,0,12)+keys
    tag=b'FLPA'+struct.pack(order+'I',tracks)+struct.pack(order+'I',8+4*tracks)*tracks+track
    entry=b'Child'+bytes(23)+struct.pack(order+'BBH',tags,0,0)+struct.pack(order+'I',32+4*tags)*tags+tag
    pai=struct.pack(order+'HBBHHII',60,0,0,0,1,20,24)+entry
    body=b'pai1'+struct.pack(order+'I',8+len(pai))+pai
    return b'FLAN'+struct.pack(order+'HHIIHH',0xfeff,20,0x08000000,20+len(body),1,0)+body
