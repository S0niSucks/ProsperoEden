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

def picture_flyt(order='<',width=19,height=33):
    base=material_flyt(order);sections=base[20:]
    pane=bytearray(128);pane[0]=1;pane[2]=255;pane[4:11]=b'Picture'
    struct.pack_into(order+'10f',pane,36,250,0,0,0,0,0,1,1,width,height)
    pane[76:92]=bytes.fromhex('ffffffff')*4
    struct.pack_into(order+'HBB8f',pane,92,0,1,0,0,0,1,0,0,1,1,1)
    sections+=b'pic1'+struct.pack(order+'I',8+len(pane))+pane
    return base[:12]+struct.pack(order+'IHH',20+len(sections),8,0)+sections

def bntx(width=19,height=33,linear=False,codec=0x0b,log=2):
    """Synthetic pixels, independent GOB-coordinate interleaving; no firmware artwork."""
    unit=4 if codec==0x0b else 8 if codec==0x1a else 16
    bw,bh=(width,height) if codec==0x0b else ((width+3)//4,(height+3)//4)
    rgba=bytes(v for y in range(height) for x in range(width) for v in ((x*17)%256,(y*11)%256,37,255))
    if codec==0x0b:raw=rgba
    else:
        block=struct.pack('<HHI',0xf800,0x07e0,0)
        if codec==0x1b:block=b'\xff'*8+block
        if codec==0x1c:block=b'\xff\0'+bytes(6)+block
        raw=block*(bw*bh);rgba=bytes((255,0,0,255))*(width*height)
    block_height=1<<log
    while block_height>1 and bh<=4*block_height:block_height//=2
    gobs=(bw*unit+63)//64;pitch=(bw*unit+31)//32*32
    tiled=bytearray(pitch*bh if linear else gobs*((bh+8*block_height-1)//(8*block_height))*block_height*512)
    for y in range(bh):
        for xbyte in range(bw*unit):
            if linear:address=y*pitch+xbyte
            else:
                gx,lx=divmod(xbyte,64);gy,ly=divmod(y,8)
                group,inner=divmod(gy,block_height)
                # GOB byte bits: X[3:0], Y[0], X[4], Y[2:1], X[5].
                inside=(lx&15)|((ly&1)<<4)|((lx&16)<<1)|((ly&6)<<5)|((lx&32)<<3)
                address=((group*gobs+gx)*block_height+inner)*512+inside
            tiled[address]=raw[y*bw*unit+xbyte]
    result=bytearray(512)+tiled;result[:8]=b'BNTX\0\0\0\0'
    struct.pack_into('<H',result,12,0xfeff);struct.pack_into('<I',result,28,len(result))
    result[32:36]=b'NX  ';struct.pack_into('<IQ',result,36,1,64);struct.pack_into('<Q',result,64,128)
    result[128:132]=b'BRTI';result[145]=2
    struct.pack_into('<H',result,146,int(linear));struct.pack_into('<HH',result,150,1,1)
    struct.pack_into('<7I',result,156,(codec<<8)|1,0,width,height,1,1,log)
    struct.pack_into('<III',result,208,len(tiled),512,0x05040302)
    struct.pack_into('<QQQ',result,224,256,0,288)
    struct.pack_into('<H',result,256,7);result[258:265]=b'Texture';struct.pack_into('<Q',result,288,512)
    return bytes(result),rgba

def animation(order='<',tracks=1,tags=1):
    # Section-relative offset tables. Repeated pointers also exercise aggregate read budgets.
    keys=struct.pack(order+'6f',0,0,0,60,20,0)
    track=struct.pack(order+'BBHHHI',0,0,2,2,0,12)+keys
    tag=b'FLPA'+struct.pack(order+'I',tracks)+struct.pack(order+'I',8+4*tracks)*tracks+track
    entry=b'Child'+bytes(23)+struct.pack(order+'BBH',tags,0,0)+struct.pack(order+'I',32+4*tags)*tags+tag
    pai=struct.pack(order+'HBBHHII',60,0,0,0,1,20,24)+entry
    body=b'pai1'+struct.pack(order+'I',8+len(pai))+pai
    return b'FLAN'+struct.pack(order+'HHIIHH',0xfeff,20,0x08000000,20+len(body),1,0)+body
