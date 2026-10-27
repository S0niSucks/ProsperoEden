#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic, original fixtures only. No downloaded themes, firmware or copyrighted icons."""
import io
import json
from pathlib import Path
import random
import struct
import sys
import tempfile
import unittest
import zipfile

import themezer as t


def zip_bytes(files):
    out = io.BytesIO()
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for name, blob in files.items():
            z.writestr(name, blob)
    return out.getvalue()


def sarc_bytes(files, order="<"):
    names = bytearray()
    nodes = bytearray()
    body = bytearray()
    for name, blob in files.items():
        while len(names) % 4:
            names.append(0)
        name_offset = len(names) // 4
        encoded = name.encode()
        names.extend(encoded + b"\0")
        h = 0
        for c in encoded:
            h = (h * 0x65 + c) & 0xffffffff
        nodes.extend(struct.pack(order + "4I", h, 0x1000000 | name_offset, len(body), len(body) + len(blob)))
        body.extend(blob)
    offset = 20 + 12 + len(nodes) + 8 + len(names)
    header = b"SARC" + struct.pack(order + "HHIIHH", 20, 0xfeff, offset + len(body), offset, 0x100, 0)
    return header + b"SFAT" + struct.pack(order + "HHI", 12, len(files), 0x65) + nodes + b"SFNT" + struct.pack(order + "HH", 8, 0) + names + body


def yaz_literals(data):
    out = bytearray(b"Yaz0" + struct.pack(">I", len(data)) + bytes(8))
    for at in range(0, len(data), 8):
        out.append(255)
        out.extend(data[at:at + 8])
    return bytes(out)


def dds(width=1280, height=720, kind=b"DXT1"):
    header = bytearray(128)
    header[:4] = b"DDS "
    struct.pack_into("<I", header, 4, 124)
    struct.pack_into("<II", header, 12, height, width)
    struct.pack_into("<II", header, 76, 32, 4)
    header[84:88] = kind
    block = struct.pack("<HHI", 0x07e0, 0, 0)  # authored solid green, not console artwork
    if kind == b"DXT3":
        block = bytes([255]) * 8 + block
    if kind == b"DXT5":
        block = bytes([255, 0]) + bytes(6) + block
    return bytes(header) + block * (((width + 3) // 4) * ((height + 3) // 4))


class ThemezerTests(unittest.TestCase):
    def test_formats_and_endianness(self):
        files = {"info.json": b'{"Version":16,"Target":"home","ThemeName":"Original fixture"}', "image.dds": dds()}
        for order in ("<", ">"):
            raw = sarc_bytes(files, order)
            self.assertEqual(t.sarc(raw), files)
            self.assertEqual(t.container(yaz_literals(raw)), files)
        self.assertEqual(t.container(zip_bytes(files)), files)
        for codec in (b"DXT1", b"DXT3", b"DXT5"):
            width, height, pixels = t.dds(dds(4, 4, codec))
            self.assertEqual((width, height, pixels[:4]), (4, 4, b"\0\xff\0\xff"))
        converted = t.read_theme(zip_bytes(files), False)
        self.assertEqual(converted["mode"], "lite")
        self.assertEqual(converted["wallpaper"][12:16], struct.pack("<HH", 1280, 720))

    def test_seven_parts_and_explicit_lite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            parts = {}
            for index, part in enumerate(t.TARGETS):
                files = {"info.json": json.dumps({"Version":17,"Target":part,"ThemeName":"Original fixture"}).encode(),
                         "image.dds": dds(), "layout.json": b'{"Files":[],"Anims":[]}'}
                parts[part + ".nxtheme"] = zip_bytes(files) if index % 2 else sarc_bytes(files)
            source = root / "parts.zip"
            source.write_bytes(zip_bytes(parts))
            with self.assertRaises(t.ImportError):
                t.convert(source, root / "refused", "refused", False)
            output = t.convert(source, root / "converted", "converted", True)
            model = json.loads((output / "theme.json").read_text())
            self.assertEqual(set(model["screens"]), set(t.TARGETS))
            report = json.loads((output / "import-report.json").read_text())
            self.assertFalse(report["firmware_used"])
            self.assertEqual(len(report["parts"]), 7)
            self.assertTrue(all(part["unsupported"] for part in report["parts"]))
            if len(sys.argv) > 1:
                installed = t.install(output, root / "data", Path(sys.argv[1]))
                self.assertTrue((installed / "theme.json").is_file())
                with self.assertRaises(t.ImportError):
                    t.install(output, root / "data", Path(sys.argv[1]))
            with self.assertRaises(t.ImportError):
                t.convert(source, output, "converted", True)

    def test_refusals(self):
        for raw in (b"", b"Yaz0" + struct.pack(">I", 0xffffffff) + bytes(8), b"Yaz0" + struct.pack(">I", 12) + bytes(10), b"SARC" + bytes(60)):
            with self.assertRaises(t.ImportError):
                t.container(raw)
        for name in ("../outside", "/root", "a\\b", "a//b", "a/./b", "a:", "x\0y"):
            with self.assertRaises(t.ImportError):
                t.safe_name(name)
        with self.assertRaises(t.ImportError):
            t.container(zip_bytes({"../outside": b"evil"}))
        files = {"info.json": b'{"Version":17,"Target":"home"}', "image.dds": dds(), "asset.bntx": b"not Nintendo data"}
        with self.assertRaises(t.ImportError):
            t.read_theme(zip_bytes(files), True)
        for metadata in ({"Version":18,"Target":"home"}, {"Version":17,"Target":[]}, {"Version":True,"Target":"home"}):
            with self.assertRaises(t.ImportError):
                t.read_theme(zip_bytes({"info.json":json.dumps(metadata).encode(),"image.dds":dds()}), True)
        with self.assertRaises(t.ImportError):
            t.strict_json(b'{"Version":16,"Version":17}')
        with self.assertRaises(t.ImportError):
            t.dds(dds(4, 4).replace(b"DXT1", b"DX10"))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "source").mkdir()
            (root / "source" / "manifest.json").write_text('{"schema":1,"id":"test","host_contract":"launcher-v1.0","api":{"major":1},"scene":"theme.json","required_capabilities":["themes.scene.v1"]}')
            (root / "source" / "link.json").symlink_to(root / "source" / "manifest.json")
            with self.assertRaises(t.ImportError):
                t.install(root / "source", root / "data", Path("/nonexistent-validator"))
            self.assertFalse((root / "data" / "launchers" / "test").exists())

    def test_mutations(self):
        rng = random.Random(173239)
        seeds = [sarc_bytes({"info.json":b"{}"}), yaz_literals(b"SARC" + bytes(30)), dds(4, 4)]
        for _ in range(2000):
            raw = bytearray(rng.choice(seeds))
            for _ in range(rng.randrange(1, 5)):
                raw[rng.randrange(len(raw))] = rng.randrange(256)
            try:
                t.dds(bytes(raw)) if raw[:4] == b"DDS " else t.container(bytes(raw))
            except t.ImportError:
                pass


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ThemezerTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    sys.exit(not result.wasSuccessful())
