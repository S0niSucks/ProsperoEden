#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""A10 dissection (allowed read-only observation, NOT the fixture):

runs the FIRST-PARTY inspector (the pinned spec) against two synthetic SARC
samples to observe exactly how member names are reconstructed:
  sample A: entry attr = 0             (offset-0 name, no attr bits)
  sample B: entry attr = 0x01000000    (first-party pattern, name at offset 0)
Prints the inspector's observed names. Writes nothing to production.
"""
from __future__ import annotations

import importlib.util
import struct
import sys
from pathlib import Path

REPO = Path("D:/leonardommello/PS5/ProsperoEden-Encore")
_spec = importlib.util.spec_from_file_location(
    "sarc_first_party", REPO / "tools/ps-glyph-sarc-inspect.py")
sarc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(sarc)


def sample(attr: int) -> bytes:
    name = b"b/sample.bin"
    payload = b"SAMPLEdt" + bytes(24)
    sfat_off, sfnt_off, names_off, data_start = 0x14, 0x30, 0x38, 0x50
    size = data_start + len(payload)
    blob = bytearray(size)
    blob[:4] = b"SARC"
    struct.pack_into(">H", blob, 4, 0x14)
    blob[6:8] = b"\xfe\xff"
    struct.pack_into(">I", blob, 8, size)
    struct.pack_into(">I", blob, 0x0C, data_start)
    struct.pack_into(">H", blob, 0x10, 0x0100)
    blob[sfat_off:sfat_off + 4] = b"SFAT"
    struct.pack_into(">H", blob, sfat_off + 4, 0x0C)
    struct.pack_into(">H", blob, sfat_off + 6, 1)
    struct.pack_into(">I", blob, sfat_off + 8, 0x65)
    struct.pack_into(">III", blob, 0x20, 1, attr, 0)
    struct.pack_into(">I", blob, 0x2C, len(payload))
    blob[sfnt_off:sfnt_off + 4] = b"SFNT"
    struct.pack_into(">H", blob, sfnt_off + 4, 8)
    blob[names_off:names_off + len(name)] = name
    blob[names_off + len(name)] = 0
    blob[data_start:] = payload
    return bytes(blob)


for label, attr in (("A attr=0", 0), ("B attr=0x01000000", 0x01000000)):
    info = sarc.inventory_bytes(sample(attr))
    member = info["members"][0]
    print(f"{label}: observed name={member['name']!r} "
          f"name_offset={member['name_offset']!r}")
print("dissection done")
sys.exit(0)
