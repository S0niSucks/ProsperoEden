#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""A10 c-themes r1 — structural read-only .szs container inventory.

Pinned INTERNAL spec (first-party, per coordinator decision 2026-11-09):
- Yaz0 decode contract: tools/ps-glyph-botw-yaz0.py:46-89
- SARC v0x100 inventory contract: tools/ps-glyph-sarc-inspect.py:35-129
Those two inspectors ARE the specification; this module composes them and adds
explicit boundary refusals of its own. It does NOT interpret any member as a
theme role: no member-to-theme mapping is documented anywhere in this repo
(that is the c-themes gap). Every report is labelled
theme_contract=none_documented_in_repo and theme_valid=null.

This container tool never extracts members, never writes Nintendo files, never
touches the launcher. .nxtheme is out of contract and refused by name.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

THEME_CONTRACT = "none_documented_in_repo"


class StructuralRejected(ValueError):
    pass


def _load_first_party(filename: str):
    """Import a first-party inspector (stdlib-only) from an allowed location.

    Search order: $NXSZS_TOOLS_DIR, this file's directory, its parent. Copies
    must be byte-identical to the repo versions (A50 verifies by hash).
    """
    candidates: list[Path] = []
    env = os.environ.get("NXSZS_TOOLS_DIR")
    if env:
        candidates.append(Path(env))
    candidates.extend((HERE, HERE.parent))
    for base in candidates:
        path = base / filename
        if not path.is_file():
            continue
        spec = importlib.util.spec_from_file_location(f"nx_szs_{path.stem}", path)
        if spec is None or spec.loader is None:
            raise StructuralRejected(f"unusable inspector: {path}")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    raise StructuralRejected(
        f"first-party inspector {filename} not found; set NXSZS_TOOLS_DIR to the "
        "directory holding byte-identical copies of tools/ps-glyph-botw-yaz0.py "
        "and tools/ps-glyph-sarc-inspect.py")


yaz0 = _load_first_party("ps-glyph-botw-yaz0.py")
sarc = _load_first_party("ps-glyph-sarc-inspect.py")


def decompress_container(raw: bytes) -> tuple[bytes, str]:
    """Route a container by magic. Yaz0 -> decoded payload; plain SARC -> as-is.

    Own boundary refusals (before delegating): too short to identify, and any
    magic other than Yaz0/SARC — including an explicit .nxtheme refusal, since
    that format is not documented in this repository.
    """
    if not isinstance(raw, bytes):
        raise StructuralRejected("container must be bytes")
    if len(raw) < 4:
        raise StructuralRejected("container too short to identify a magic")
    if raw[:4] == b"Yaz0":
        return yaz0.decode(raw), "Yaz0"
    if raw[:4] == b"SARC":
        return raw, "uncompressed"
    raise StructuralRejected(
        "not a Yaz0/SARC container; .nxtheme and every other theme format are "
        "not documented in this repo (c-themes gap) and are refused")


def inventory_bytes(raw: bytes) -> dict:
    decompressed, compression = decompress_container(raw)
    info = sarc.inventory_bytes(decompressed)
    return {
        "schema": 2,
        "kind": "nx_szs_structural_inventory",
        "compression": compression,
        "decompressed_sha256": hashlib.sha256(decompressed).hexdigest(),
        "decompressed_bytes": len(decompressed),
        "sarc": info,
        "theme_contract": THEME_CONTRACT,
        "theme_valid": None,
        "member_semantics": ("undocumented: no member-to-theme-role mapping "
                             "exists in this repository"),
    }


def inventory(source: Path) -> dict:
    if not source.exists():
        raise StructuralRejected("container file missing")
    if source.is_symlink() or not source.is_file():
        raise StructuralRejected("container must be a regular non-symlink file")
    length = source.stat().st_size
    raw = source.read_bytes()
    if len(raw) != length:
        raise StructuralRejected("incomplete container read")
    report = inventory_bytes(raw)
    report["source_name"] = source.name
    report["source_sha256"] = hashlib.sha256(raw).hexdigest()
    report["source_bytes"] = len(raw)
    # keep the first-party pixel warning visible at the top level too
    report["warning"] = info_warning = report["sarc"].get("warning")
    assert info_warning  # first-party inspector always provides it
    return report


def main(argv: list[str] | None = None) -> int:
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("container", type=Path)
    cli.add_argument("--out", type=Path, help="NEW JSON file only")
    args = cli.parse_args(argv)
    try:
        if args.out and (args.out.exists() or args.out.is_symlink() or
                         not args.out.parent.is_dir() or args.out.parent.is_symlink()):
            raise StructuralRejected("report output already exists or parent is unsafe")
        report = json.dumps(inventory(args.container), ensure_ascii=False,
                            indent=2) + "\n"
        if args.out:
            args.out.write_text(report, encoding="utf-8")
            print(f"NX SZS STRUCTURAL INVENTORY {args.out} (read-only)")
        else:
            print(report, end="")
        return 0
    except (StructuralRejected, yaz0.InvalidYaz0, sarc.InvalidSarc,
            OSError, ValueError) as error:
        print(f"REJECTED NX SZS: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
