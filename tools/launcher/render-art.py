#!/usr/bin/env python3
# ProsperoEden - Launcher art from the source images (run by tools/launcher/assets.sh).
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the Prospero.Eden Encore launcher's baked art.

  brand.tga       Prospero.Eden Encore logo, for the header and games without cover art.
  controller.tga  a controller in white on transparent, tinted by the home screen.

The launcher preview generates the same wallpaper source used by the native PS5
package: assets/encore-background.jpg, not the obsolete upstream scenery.
render-art.py [name...] renders only the explicitly requested pictures.
"""

import sys
import base64
from io import BytesIO

from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "headless/prosperoeden/ui/art"


def save_tga(image, name):
    image.convert("RGBA").save(ART / name, orientation=1)
    print(f"{name}: {image.size[0]}x{image.size[1]}")


def controller():
    """The controller as a mask: the shell is opaque, its touchpad, sticks and buttons are holes.

    Drawn in a 72x50 box (the size on screen) at 16 times that, then reduced to 144x100.
    """
    k = 16
    mask = Image.new("L", (72 * k, 50 * k), 0)
    draw = ImageDraw.Draw(mask)

    def box(x0, y0, x1, y1, radius, fill):
        draw.rounded_rectangle((x0 * k, y0 * k, x1 * k, y1 * k), radius * k, fill=fill)

    def disc(x, y, radius, fill):
        draw.ellipse(((x - radius) * k, (y - radius) * k, (x + radius) * k, (y + radius) * k), fill=fill)

    def bar(x0, y0, x1, y1, width, fill):
        draw.line((x0 * k, y0 * k, x1 * k, y1 * k), fill=fill, width=round(width * k))
        disc(x0, y0, width / 2, fill)
        disc(x1, y1, width / 2, fill)

    # The shell: a wide body and two grips that lean outwards.
    box(3.5, 5, 68.5, 34.5, 13, 255)
    bar(15.5, 25, 10, 40.5, 13.5, 255)
    bar(56.5, 25, 62, 40.5, 13.5, 255)
    # Touchpad, with the two small buttons beside it.
    box(25, 8.5, 47, 20.5, 3.2, 0)
    bar(21.6, 9.6, 21.2, 12.6, 1.5, 0)
    bar(50.4, 9.6, 50.8, 12.6, 1.5, 0)
    # Direction pad and the four buttons.
    box(9.2, 15.4, 18.2, 18.2, 1.2, 0)
    box(12.3, 12.3, 15.1, 21.3, 1.2, 0)
    for dx, dy in ((0, -3.7), (3.7, 0), (0, 3.7), (-3.7, 0)):
        disc(58.3 + dx, 16.8 + dy, 1.55, 0)
    # Sticks (a ring around a cap) and the round button between them.
    for x in (26.2, 45.8):
        disc(x, 27.6, 4.7, 0)
        disc(x, 27.6, 2.9, 255)
    disc(36, 27.4, 1.5, 0)
    mask = mask.resize((144, 100), Image.Resampling.LANCZOS)
    icon = Image.new("RGBA", mask.size, (255, 255, 255, 0))
    icon.putalpha(mask)
    return icon


def fixed():
    """Decode user's immutable packaging panels for missing retail faces only.

    No image generation, web request or redraw: these are byte-for-byte
    encoded source templates and a deterministic PNG -> native TGA conversion.
    """
    for stem in ("switch-logo", "switch-rear-footer"):
        source = ART / f"{stem}.png.b64"
        raw = base64.b64decode(source.read_text().strip(), validate=True)
        with Image.open(BytesIO(raw)) as packed:
            packed.load()
            artwork = packed.convert("RGBA")
            if stem == "switch-rear-footer":
                # Restore the original reference's gold approval seal. The
                # source is a compact 1-bit print to keep install size low;
                # only the historic seal's fixed ink region is recoloured.
                ink = artwork.load()
                for y in range(195, 275):
                    for x in range(182, 264):
                        r, g, b, alpha = ink[x, y]
                        if r == 0 and g == 0 and b == 0:
                            ink[x, y] = (175, 136, 76, alpha)
            save_tga(artwork, f"{stem}.tga")




def backdrop():
    """Host preview: use the PS5 package's canonical Encore JPEG wallpaper.

    The native package creates a 1920x1080 centre-cropped TGA with an
    original top-to-bottom pixel order. The TGA decoder already normalizes
    rows, and the launcher samples its textures using top-left UI UVs.
    The checked-in legacy TGA must NEVER be trusted as a preview source.
    """
    source = ROOT / "assets/encore-background.jpg"
    with Image.open(source) as original:
        wallpaper = ImageOps.fit(original.convert("RGB"), (1920, 1080),
                                  method=Image.Resampling.LANCZOS)
    # Do not vertically flip: the 2D launcher samples UV(0,0) at screen top.
    target = ART / "backdrop.tga"
    wallpaper.save(target, format="TGA")
    # Pillow appends an optional 26-byte TGA footer. The native packager
    # requires exactly the 18-byte header followed by uncompressed RGB.
    length = 18 + 1920 * 1080 * 3
    data = target.read_bytes()
    if (len(data) < length or data[1] != 0 or data[2] != 2
            or data[16] != 24 or data[12:16] != bytes((128, 7, 56, 4))):
        raise RuntimeError("Encore canonical wallpaper TGA is malformed")
    target.write_bytes(data[:length])
    print(f"backdrop.tga: 1920x1080 from {source.relative_to(ROOT)}")


def main():
    ART.mkdir(parents=True, exist_ok=True)
    wanted = set(sys.argv[1:]) or {"brand", "controller", "fixed", "backdrop"}
    unknown = wanted - {"brand", "controller", "fixed", "backdrop"}
    if unknown:
        raise SystemExit("unknown Encore launcher art: " + ", ".join(sorted(unknown)))
    if "backdrop" in wanted:
        backdrop()
    if "brand" in wanted:
        source = ROOT / "assets/icon0.png"
        icon = Image.open(source).convert("RGBA")
        icon = icon.resize((384, 384), Image.Resampling.LANCZOS)
        save_tga(icon, "brand.tga")
    if "controller" in wanted:
        save_tga(controller(), "controller.tga")
    if "fixed" in wanted:
        fixed()


if __name__ == "__main__":
    main()
