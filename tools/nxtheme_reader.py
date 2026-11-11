#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""A10 c-themes — read-only .nxtheme structural reader (r1).

Pinned EXTERNAL spec: docs/feature-roadmap/sources/nxtheme-format.md (2026-11-10).
Reuses the validated structural .szs parser (tools/nx_szs_structural.py,
sha256 86af2818...) for the container layer (plain SARC or Yaz0-wrapped SARC).

Implements exactly what the pin documents (lines cited per check):
- members image.jpg (1280x720 JPG, :21), layout.json OPTIONAL carried OPAQUELY
  (:23-25, gap :46-48 — never parsed), config.json with name/author/target
  (:26);
- target validated against the pinned table (:32-39) by its LITERAL strings
  (menu name, SZS filename, or applet id — the pin documents no other target
  spelling; anything else is refused as unknown);
- explicit refusals at the first check reached, in this order: container
  (delegated), image.jpg presence, config.json presence, config JSON/object,
  field 'name', 'author', 'target', pinned table, image JPEG SOI, SOF scan,
  1280x720 (:21).

Pinned gaps NOT implemented (spec :44-52): layout.json semantics,
bflyt/bflan/bntx internals beyond first-party inspectors, applet icons/common
custom layouts. Theme APPLICATION on PS5 is out of scope (spec :62-63): every
report carries theme_application=out_of_scope_ps5. Read-only; no extraction.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
THEME_APPLICATION = "out_of_scope_ps5"
REQUIRED_IMAGE = "image.jpg"
REQUIRED_CONFIG = "config.json"
OPTIONAL_LAYOUT = "layout.json"
JPEG_SIZE = (1280, 720)  # spec :21

# Pinned target table, spec :32-39. Lookup accepts the literal menu name, SZS
# filename or applet id; nothing else is documented.
PINNED_TARGETS: tuple[tuple[str, str, str], ...] = (
    ("common", "common.szs", "qlaunch"),
    ("Home Menu", "ResidentMenu.szs", "qlaunch"),
    ("Lockscreen", "Entrance.szs", "qlaunch"),
    ("All Apps", "Flaunch.szs", "qlaunch"),
    ("News", "Notification.szs", "qlaunch"),
    ("Settings", "Set.szs", "qlaunch"),
    ("Player Select", "Psl.szs", "playerSelect"),
    ("User Page", "MyPage.szs", "myPage"),
)


class NxthemeRejected(ValueError):
    pass


def _load_szs_module():
    candidates: list[Path] = []
    env = os.environ.get("NXSZS_TOOLS_DIR")
    if env:
        candidates.append(Path(env))
    candidates.extend((HERE, HERE.parent))
    for base in candidates:
        path = base / "nx_szs_structural.py"
        if path.is_file():
            spec = importlib.util.spec_from_file_location("nx_szs_structural", path)
            if spec is None or spec.loader is None:
                raise NxthemeRejected(f"unusable .szs parser: {path}")
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            return module
    raise NxthemeRejected(
        "nx_szs_structural.py not found; set NXSZS_TOOLS_DIR or keep a "
        "byte-identical copy (sha256 86af2818...) beside this reader")


szs = _load_szs_module()


def _member_bytes(decompressed: bytes, report: dict, name: str) -> bytes:
    """Slice one member out of the ALREADY decompressed container, read-only.

    Offsets come from the pinned SARC inventory (content_offset/content_bytes).
    """
    for member in report["sarc"]["members"]:
        if member["name"] == name:
            start = member["content_offset"]
            end = start + member["content_bytes"]
            if end > len(decompressed):
                raise NxthemeRejected(f"member {name} range exceeds container")
            return decompressed[start:end]
    raise NxthemeRejected(f"required member {name} missing")


def _validate_config(raw: bytes) -> dict:
    try:
        parsed = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        raise NxthemeRejected("config.json is not valid JSON") from None
    if not isinstance(parsed, dict):
        raise NxthemeRejected("config.json must be a JSON object")
    for field in ("name", "author", "target"):
        value = parsed.get(field)
        if not isinstance(value, str) or not value.strip():
            raise NxthemeRejected(
                f"config.json field '{field}' missing or not a non-empty string")
    target = parsed["target"]
    for menu, szs_name, applet in PINNED_TARGETS:
        if target in (menu, szs_name, applet):
            return {"name": parsed["name"], "author": parsed["author"],
                    "target": target,
                    "canonical": {"menu": menu, "szs": szs_name, "applet": applet}}
    raise NxthemeRejected("config.json target not in pinned target table")


def _jpeg_dimensions(data: bytes) -> tuple[int, int]:
    """Header-only JPEG scan: SOI, then segments, until SOF0/SOF1/SOF2."""
    if len(data) < 2 or data[0:2] != b"\xff\xd8":
        raise NxthemeRejected("image.jpg does not start with a JPEG SOI marker")
    position = 2
    while position + 4 <= len(data):
        if data[position] != 0xFF:
            raise NxthemeRejected("image.jpg has no JPEG SOF marker")
        marker = data[position + 1]
        if marker == 0xD9 or 0xD0 <= marker <= 0xD7 or marker == 0x01:
            if marker == 0xD9:
                break
            position += 2
            continue
        length = struct.unpack_from(">H", data, position + 2)[0]
        if marker in (0xC0, 0xC1, 0xC2):
            try:
                height = struct.unpack_from(">H", data, position + 5)[0]
                width = struct.unpack_from(">H", data, position + 7)[0]
            except struct.error:
                raise NxthemeRejected(
                    "image.jpg has no JPEG SOF marker") from None
            return width, height
        position += 2 + length
    raise NxthemeRejected("image.jpg has no JPEG SOF marker")


def read(source: Path) -> dict:
    container_report = szs.inventory(source)
    raw = source.read_bytes()
    decompressed = szs.yaz0.decode(raw) if raw[:4] == b"Yaz0" else raw
    image_raw = _member_bytes(decompressed, container_report, REQUIRED_IMAGE)
    config_raw = _member_bytes(decompressed, container_report, REQUIRED_CONFIG)
    config = _validate_config(config_raw)
    width, height = _jpeg_dimensions(image_raw)
    if (width, height) != JPEG_SIZE:
        raise NxthemeRejected(
            f"image.jpg is not {JPEG_SIZE[0]}x{JPEG_SIZE[1]}")
    layout = {"present": False, "bytes": 0, "opaque": True}
    for member in container_report["sarc"]["members"]:
        if member["name"] == OPTIONAL_LAYOUT:
            # Pinned gap :46-48: carried opaquely, NEVER parsed or interpreted.
            layout = {"present": True, "bytes": member["content_bytes"],
                      "opaque": True}
    return {
        "schema": 1,
        "kind": "nxtheme_reader",
        "source_name": container_report.get("source_name"),
        "source_sha256": container_report.get("source_sha256"),
        "source_bytes": container_report.get("source_bytes"),
        "container": container_report,
        "config": config,
        "image_jpg": {"bytes": len(image_raw), "jpeg_header": True,
                      "width": width, "height": height},
        "layout_json": layout,
        "layout_semantics": "opaque (pinned gap; never parsed)",
        "theme_application": THEME_APPLICATION,
        "warning": ("Format compatibility only. Theme application on PS5 is "
                    "out of scope; layout.json semantics are a declared gap."),
    }


def main(argv: list[str] | None = None) -> int:
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("nxtheme", type=Path)
    cli.add_argument("--out", type=Path, help="NEW JSON file only")
    args = cli.parse_args(argv)
    try:
        if args.out and (args.out.exists() or args.out.is_symlink() or
                         not args.out.parent.is_dir() or args.out.parent.is_symlink()):
            raise NxthemeRejected("report output already exists or parent is unsafe")
        report = json.dumps(read(args.nxtheme), ensure_ascii=False, indent=2) + "\n"
        if args.out:
            args.out.write_text(report, encoding="utf-8")
            print(f"NXTHEME READ-ONLY REPORT {args.out}")
        else:
            print(report, end="")
        return 0
    except (NxthemeRejected, szs.StructuralRejected, szs.yaz0.InvalidYaz0,
            szs.sarc.InvalidSarc, OSError, ValueError) as error:
        print(f"REJECTED NXTHEME: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
