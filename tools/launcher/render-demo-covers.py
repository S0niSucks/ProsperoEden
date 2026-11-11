#!/usr/bin/env python3
"""Deterministic original art for the *fictional* sample titles in README host captures.

Never run in the native package builder. No copyrighted game artwork, downloads,
or changes to real players' games. The production C++ OpenGL launcher renders
the screenshot; this program only supplies sample game-cover textures.
"""
import math
import random
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

SIZE = 512
SAMPLES = [
    ("Starfall Odyssey", "#080e35", "#28528a", "#ffcf75"),
    ("Moss & Lantern", "#082e2c", "#2a7954", "#ffe4a0"),
    ("Kart Carnival Deluxe", "#551e83", "#ff6c47", "#f9d95d"),
    ("Tiny Harbor", "#337d9e", "#97d9cc", "#fff0aa"),
    ("Echoes of the Valley", "#211044", "#805aa0", "#edb7da"),
    ("Pocket Rally Turbo", "#34253f", "#cf6636", "#ffcb60"),
    ("Cloudline", "#255b9b", "#8acee6", "#fff5d6"),
    ("Ember Knights II", "#1b102f", "#a32c36", "#ffc25c"),
    ("Paper Garden", "#bbcbb8", "#e7d6a9", "#fff5dc"),
    ("Neon Drifters", "#090b31", "#62328e", "#51f8e5"),
    ("Café Nocturne", "#21192f", "#744352", "#ffd08e"),
    ("Sky Shepherds", "#267ab4", "#9adcdf", "#fff5d7"),
]


def rgb(hexa):
    return tuple(bytes.fromhex(hexa[1:]))


def paint(number, name, sky, land, light):
    img = Image.new("RGB", (SIZE, SIZE))
    bg = ImageDraw.Draw(img)
    a, b = rgb(sky), rgb(land)
    for y in range(SIZE):
        t = (y / 511) ** 0.85
        bg.line((0, y, 512, y), fill=tuple(int(a[i]*(1-t)+b[i]*t) for i in range(3)))
    d = ImageDraw.Draw(img, "RGBA")
    rand = random.Random(9000 + number)
    if number in (0, 4, 7, 9):
        for j in range(135):
            x, y = rand.randrange(512), rand.randrange(345)
            k = rand.choice((1, 1, 2, 3))
            d.ellipse((x-k, y-k, x+k, y+k), fill=(251, 239, 231, rand.randrange(40, 195)))
    if number == 0:  # planet and ship
        d.ellipse((252, 38, 464, 252), fill=(80, 104, 186, 245))
        d.ellipse((271, 59, 449, 239), fill=(38, 49, 124, 255))
        d.arc((195, 93, 494, 245), 188, 359, fill=(255, 216, 140, 255), width=11)
        d.polygon([(143, 358), (260, 173), (354, 360), (271, 317), (249, 405)], fill=(232, 239, 242, 255))
        d.polygon([(234, 350), (260, 454), (282, 350)], fill=(255, 166, 66, 245))
    elif number == 1:  # lantern in an enchanted forest
        for x, w in ((21, 51), (111, 37), (369, 49), (451, 46)):
            d.rectangle((x, 0, x+w, 392), fill=(9, 43, 41, 255))
            d.ellipse((x-75, -45, x+w+82, 147), fill=(25, 93, 65, 245))
        d.ellipse((169, 126, 365, 331), fill=(255, 209, 92, 65))
        d.line((261, 72, 261, 164), fill=(236, 223, 160, 255), width=8)
        d.rounded_rectangle((214, 173, 316, 298), radius=15, fill=(58, 68, 49, 255), outline=(255, 229, 149, 255), width=9)
        d.rectangle((232, 193, 300, 274), fill=(255, 215, 110, 255))
        d.polygon([(202, 173), (264, 127), (326, 173)], fill=(50, 79, 66, 255))
    elif number == 2:  # carnival karts
        d.polygon([(0, 330), (512, 207), (512, 512), (0, 512)], fill=(50, 41, 69, 255))
        d.polygon([(125, 512), (236, 216), (309, 216), (445, 512)], fill=(37, 41, 65, 255))
        for x, fill in ((127,(244,75,81,255)), (362,(69,218,239,255))):
            d.rounded_rectangle((x-60, 295, x+50, 366), 16, fill=fill)
            d.polygon([(x-38, 293), (x-18, 261), (x+27, 261), (x+40, 293)], fill=(34, 55, 85, 255))
            for wx in (x-39, x+31):
                d.ellipse((wx-16, 351, wx+16, 390), fill=(22, 28, 39, 255))
        for x in (44, 233, 421):
            d.line((x, 36, x+45, 153), fill=(255, 233, 103, 255), width=10)
    elif number == 3:  # lighthouse by a sea village
        d.rectangle((0, 249, 512, 512), fill=(22, 131, 166, 225))
        d.polygon([(301, 267), (351, 108), (398, 267)], fill=(250, 236, 210, 255))
        d.polygon([(295, 144), (351, 94), (402, 144)], fill=(200, 73, 68, 255))
        d.rectangle((338, 172, 369, 215), fill=(255, 204, 99, 255))
        d.line((51, 385, 465, 385), fill=(91, 65, 70, 255), width=16)
        for x in (76, 152, 219, 403):
            d.line((x, 386, x, 478), fill=(57, 58, 65, 255), width=13)
        d.polygon([(104, 322), (214, 322), (190, 356), (130, 356)], fill=(250, 243, 206, 255))
        d.polygon([(154, 216), (154, 322), (218, 322)], fill=(255, 244, 211, 255))
    elif number == 4:  # moonlit mountain valley
        d.ellipse((322, 45, 463, 186), fill=(254, 226, 234, 245))
        d.polygon([(0, 331), (105, 165), (194, 309), (285, 113), (410, 322), (512, 218), (512,512), (0,512)], fill=(86, 68, 131, 255))
        d.polygon([(0, 402), (96, 298), (191, 359), (297, 270), (417, 367), (512, 295), (512,512), (0,512)], fill=(48, 32, 87, 255))
        d.arc((147, 222, 374, 408), 180, 360, fill=(248, 205, 230, 255), width=12)
        for x in (156, 363):
            d.line((x, 315, x, 423), fill=(237, 197, 230, 255), width=12)
    elif number == 5:  # rally at sunset
        d.ellipse((331, 69, 456, 204), fill=(255, 221, 132, 245))
        d.polygon([(0, 294), (112, 198), (244, 302), (356, 207), (512, 300), (512,512), (0,512)], fill=(134, 72, 65, 240))
        d.polygon([(109,512),(234,271),(299,271),(435,512)], fill=(49, 45, 51, 255))
        d.rounded_rectangle((98, 301, 404, 373), 17, fill=(244, 105, 49, 255))
        d.polygon([(155, 301), (206, 255), (324, 255), (364, 301)], fill=(31, 75, 99, 255))
        for x in (145,352):
            d.ellipse((x-32, 352, x+34, 418), fill=(22, 25, 36, 255))
            d.ellipse((x-16, 368, x+17, 402), fill=(205, 215, 214, 255))
    elif number == 6:  # airship and floating islands
        for x, y, w in ((105, 123, 75), (370, 91, 91), (448, 239, 60)):
            d.ellipse((x-w, y-25, x+w, y+25), fill=(253, 250, 247, 165))
            d.ellipse((x-38, y-49, x+47, y+23), fill=(248, 249, 249, 190))
        d.polygon([(42, 323), (158, 250), (246, 307), (208, 367), (113, 373)], fill=(49, 129, 135, 255))
        d.polygon([(301, 272), (428, 198), (503, 257), (463, 331), (332, 332)], fill=(60, 116, 134, 255))
        for x,y in ((115,195), (371,147)):
            d.rectangle((x, y, x+49, y+91), fill=(246, 235, 192, 240))
            d.polygon([(x-8,y), (x+23,y-40), (x+58,y)], fill=(103,130,159,255))
        d.polygon([(167, 121), (314, 158), (231, 196)], fill=(255, 248, 215, 245))
    elif number == 7:  # swords and the ember citadel
        d.polygon([(0, 352),(104, 250),(179, 340),(267, 205),(353, 330),(445, 224),(512,296),(512,512),(0,512)],fill=(39,24,49,255))
        for x,y in ((94,247),(245,201),(378,270)):
            d.rectangle((x,y,x+51,422),fill=(24,22,39,255))
            d.polygon([(x-10,y),(x+25,y-50),(x+62,y)],fill=(32,28,45,255))
        d.polygon([(235,315),(259,75),(287,315)],fill=(255,234,166,255))
        d.line((160,307,354,307),fill=(255,184,97,255),width=18)
        for x in (157,210,319,369):
            d.polygon([(x-16,374),(x,277),(x+23,374),(x+9,327)],fill=(255,113,55,218))
    elif number == 8:  # origami flower garden
        d.rectangle((0, 0, 512, 512), fill=(226, 229, 200, 145))
        for x,y in ((98,211),(250,191),(417,240),(168,330),(361,349)):
            d.line((x,y+12,x-8,y+119),fill=(61,143,85,235),width=8)
            d.polygon([(x,y),(x-44,y-22),(x-23,y-73),(x+9,y-31)],fill=(243,163,150,245))
            d.polygon([(x,y),(x+47,y-23),(x+33,y-69),(x-1,y-36)],fill=(251,216,157,245))
            d.polygon([(x,y),(x-12,y-71),(x+19,y-85),(x+17,y-26)],fill=(254,237,208,255))
            d.polygon([(x-7,y+72),(x-58,y+44),(x-51,y+81)],fill=(91,157,114,245))
    elif number == 9:  # midnight neon cars
        for x,y in ((19,184),(94,261),(156,210),(218,311),(303,246),(387,281),(463,202)):
            d.rectangle((x,y,x+53,418),fill=(15,21,50,255))
            for yy in range(y+17,392,30):
                d.line((x+11,yy,x+41,yy),fill=(77,235,230,220),width=5)
        d.polygon([(0,512),(193,354),(316,354),(512,512)],fill=(27,28,57,255))
        for x in (129,385):
            d.line((255,357,x,512),fill=(255,70,194,255),width=9)
        d.rounded_rectangle((111,311,401,383),20,fill=(58,24,102,255),outline=(75,254,238,255),width=8)
        d.polygon([(160,311),(212,279),(325,279),(372,311)],fill=(55,200,225,255))
    elif number == 10:  # nocturnal coffee bar
        d.rectangle((70,146,451,417),fill=(65,39,65,255))
        d.rectangle((105,188,421,367),fill=(30,38,58,255))
        d.rounded_rectangle((166,229,356,325),20,fill=(248,177,105,255))
        d.ellipse((200,245,315,317),fill=(107,61,69,255))
        d.arc((201,183,322,277),210,30,fill=(255,234,172,255),width=9)
        d.rectangle((66,356,458,393),fill=(171,84,104,255))
        d.line((68,167,453,167),fill=(247,190,125,255),width=15)
        for x in (104,165,230,293,354):
            d.ellipse((x,100,x+11,111),fill=(255,222,152,245))
    else:  # floating sheep pastures
        for x,y in ((106,129),(381,111),(436,220)):
            d.ellipse((x-74,y-25,x+83,y+28),fill=(252,253,246,180))
            d.ellipse((x-35,y-51,x+47,y+21),fill=(252,251,242,195))
        d.polygon([(0,371),(98,282),(250,332),(370,253),(512,316),(512,512),(0,512)],fill=(64,155,131,255))
        for x,y in ((123,315),(244,304),(364,283)):
            for dx,dy in ((-20,0),(2,-16),(20,4),(1,18)):
                d.ellipse((x+dx-25,y+dy-21,x+dx+25,y+dy+22),fill=(253,250,240,255))
            d.ellipse((x+26,y-6,x+51,y+20),fill=(60,70,72,255))
            for xx in (x-16,x+12):
                d.line((xx,y+27,xx,y+61),fill=(58,78,73,255),width=8)
    # Title panel, inset to remain readable at 256-pixel thumbnail sizes.
    d.rectangle((0,384,512,512),fill=(10,12,22,228))
    d.line((34,384,478,384),fill=(*rgb(light),255),width=5)
    try:
        font = ImageFont.truetype("DejaVuSans-Bold.ttf", 34)
    except OSError:
        font = ImageFont.load_default()
    lines, current = [], ""
    for word in name.upper().split():
        trial=(current+" "+word).strip()
        if current and d.textbbox((0,0),trial,font=font)[2]>436:
            lines.append(current)
            current=word
        else:
            current=trial
    if current: lines.append(current)
    for i,line in enumerate(lines[:3]):
        d.text((32,411+i*37),line,font=font,fill=(255,250,240,255),
               stroke_width=1,stroke_fill=(12,12,19,255))
    return img


def main():
    if len(sys.argv)!=2:
        raise SystemExit("usage: render-demo-covers.py <temporary-preview-directory>")
    target = Path(sys.argv[1])
    target.mkdir(parents=True,exist_ok=True)
    for idx, (name,sky,land,light) in enumerate(SAMPLES):
        outfile=target / f"cover-{idx}.tga"
        paint(idx,name,sky,land,light).save(outfile,format="TGA")
        with Image.open(outfile) as check:
            assert check.size==(SIZE,SIZE) and check.convert("RGB").getbbox()
        print(f"FICTIONAL_COVER_{idx:02d}: {name}")
    print("ENCORE_ORIGINAL_FICTIONAL_PREVIEW_COVERS: PASS")


if __name__=="__main__":
    main()
