#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""A10 c-themes — fixture for the .nxtheme reader (pinned external spec).

Consumes the REAL reader (nxtheme_reader.py) and reuses the VALIDATED SARC
generator from ../cthemes-szs-subset-r1/check_nx_szs_structural.py (no code
fork). All nxthemes are synthetic, stdlib-only; no Nintendo asset, no real
theme file. Generator is SELF-VERIFIED (valid synthetics must pass the real
reader with expected fields) before any corruption.

Negatives follow the first-check-reached principle: each case maps to the
substring of the FIRST refusal the mutated bytes actually hit, in the reader's
documented order (container -> image.jpg presence -> config.json presence ->
config JSON/object -> field name/author/target -> pinned table -> JPEG SOI ->
SOF scan -> 1280x720).
"""
from __future__ import annotations

import importlib.util
import json
import os
import struct
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
# The sibling checker's own import chain resolves nx_szs_structural through
# NXSZS_TOOLS_DIR; point it at this workspace's byte-identical copies.
os.environ.setdefault("NXSZS_TOOLS_DIR", str(HERE))

_spec = importlib.util.spec_from_file_location("nxtheme_reader", HERE / "nxtheme_reader.py")
nxr = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(nxr)

GENERATOR = Path(os.environ.get(
    "A10_SARC_GENERATOR",
    HERE.parent / "cthemes-szs-subset-r1" / "check_nx_szs_structural.py"))
_gspec = importlib.util.spec_from_file_location("sarc_generator", GENERATOR)
gen = importlib.util.module_from_spec(_gspec)
_gspec.loader.exec_module(gen)

REJECTED = (nxr.NxthemeRejected, nxr.szs.StructuralRejected,
            nxr.szs.yaz0.InvalidYaz0, nxr.szs.sarc.InvalidSarc)


def synthetic_jpeg(width: int, height: int) -> bytes:
    """Header-only synthetic JPEG: SOI + JFIF APP0 + SOF0 + EOI (no pixels)."""
    out = bytearray(b"\xff\xd8")
    out += b"\xff\xe0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00"
    # SOF0 segment: marker(2) + length=17(2) + precision(1) + height(2) +
    # width(2) = 9 bytes here, then ncomp+components (10) complete the 17.
    # Pre-sized buffer so pack_into at offsets 5/7 fits (A50 a4af1e34...).
    sof = bytearray(9)
    sof[0:5] = b"\xff\xc0\x00\x11\x08"
    struct.pack_into(">H", sof, 5, height)
    struct.pack_into(">H", sof, 7, width)
    out += sof
    out += b"\x03\x01\x22\x00\x02\x11\x01\x03\x11\x01"
    out += b"\xff\xd9"
    return bytes(out)


def config_bytes(**overrides) -> bytes:
    payload = {"name": "Synthetic Theme", "author": "A10 fixture",
               "target": "ResidentMenu.szs"}
    payload.update(overrides)
    return json.dumps(payload).encode("utf-8")


def build_nxtheme(*, image=None, config=None, layout=None, extra=None,
                  wrap=False) -> bytes:
    """Assemble one synthetic .nxtheme via the validated SARC generator.

    Passing image/config as None DROPS that member; layout None omits it.
    """
    members: list[tuple[bytes, bytes]] = []
    if image is not None:
        members.append((b"image.jpg", image))
    if config is not None:
        members.append((b"config.json", config))
    if layout is not None:
        members.append((b"layout.json", layout))
    if extra is not None:
        members.extend(extra)
    plain = gen.build_sarc(members)
    return gen.yaz0_all_literals(plain) if wrap else plain


DEFAULT_IMAGE = synthetic_jpeg(1280, 720)
DEFAULT_CONFIG = config_bytes()


# --------------------------------------------------------------------------
# generator self-verification (against the REAL reader)
# --------------------------------------------------------------------------
def self_verify() -> None:
    with tempfile.TemporaryDirectory(prefix="nxtheme-selfcheck-") as tmp:
        root = Path(tmp)
        case = root / "plain-with-layout.nxtheme"
        case.write_bytes(build_nxtheme(
            image=DEFAULT_IMAGE, config=DEFAULT_CONFIG,
            layout=b'{"panes": "opaque-bytes"}',
            extra=[(b"icon.jpg", synthetic_jpeg(256, 256))]))
        report = nxr.read(case)
        assert report["kind"] == "nxtheme_reader"
        assert report["theme_application"] == "out_of_scope_ps5"
        assert report["config"]["target"] == "ResidentMenu.szs"
        assert report["config"]["canonical"]["menu"] == "Home Menu"
        assert report["config"]["canonical"]["applet"] == "qlaunch"
        assert report["image_jpg"] == {"bytes": len(DEFAULT_IMAGE),
                                       "jpeg_header": True,
                                       "width": 1280, "height": 720}
        assert report["layout_json"] == {"present": True,
                                         "bytes": len(b'{"panes": "opaque-bytes"}'),
                                         "opaque": True}
        assert report["container"]["sarc"]["member_count"] == 4

        wrapped = root / "yaz0-minimal.nxtheme"
        wrapped.write_bytes(build_nxtheme(image=DEFAULT_IMAGE,
                                          config=config_bytes(target="myPage"),
                                          wrap=True))
        report = nxr.read(wrapped)
        assert report["container"]["compression"] == "Yaz0"
        assert report["layout_json"]["present"] is False
        assert report["config"]["canonical"] == {
            "menu": "User Page", "szs": "MyPage.szs", "applet": "myPage"}
        assert report["container"]["sarc"]["member_count"] == 2


# --------------------------------------------------------------------------
# negatives: first check reached, per the reader's documented order
# --------------------------------------------------------------------------
def make_cases() -> dict:
    garbage_config = b"\x00not json at all"
    soi_no_sof = (b"\xff\xd8"
                  b"\xff\xe0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00"
                  b"\xff\xd9")
    return {
        # presence order: image.jpg first, then config.json
        "missing_image": (build_nxtheme(config=DEFAULT_CONFIG), ("required member image.jpg missing",)),
        "missing_config": (build_nxtheme(image=DEFAULT_IMAGE), ("required member config.json missing",)),
        "missing_both": (build_nxtheme(), ("required member image.jpg missing",)),
        # config parsing / shape
        "config_garbage": (build_nxtheme(image=DEFAULT_IMAGE, config=garbage_config), ("config.json is not valid JSON",)),
        "config_empty": (build_nxtheme(image=DEFAULT_IMAGE, config=b""), ("config.json is not valid JSON",)),
        "config_array": (build_nxtheme(image=DEFAULT_IMAGE, config=b"[1,2]"), ("config.json must be a JSON object",)),
        "config_string": (build_nxtheme(image=DEFAULT_IMAGE, config=b'"theme"'), ("config.json must be a JSON object",)),
        # fields, in name -> author -> target order
        "name_missing": (build_nxtheme(image=DEFAULT_IMAGE, config=config_bytes(name=None)), ("field 'name'",)),
        "name_not_string": (build_nxtheme(image=DEFAULT_IMAGE, config=config_bytes(name=11)), ("field 'name'",)),
        "name_whitespace": (build_nxtheme(image=DEFAULT_IMAGE, config=config_bytes(name="   ")), ("field 'name'",)),
        "author_missing": (build_nxtheme(image=DEFAULT_IMAGE, config=config_bytes(author=None)), ("field 'author'",)),
        "author_empty": (build_nxtheme(image=DEFAULT_IMAGE, config=config_bytes(author="")), ("field 'author'",)),
        "target_missing": (build_nxtheme(image=DEFAULT_IMAGE, config=config_bytes(target=None)), ("field 'target'",)),
        "target_not_string": (build_nxtheme(image=DEFAULT_IMAGE, config=config_bytes(target=7)), ("field 'target'",)),
        "target_empty": (build_nxtheme(image=DEFAULT_IMAGE, config=config_bytes(target=" ")), ("field 'target'",)),
        # pinned table (literal strings only; spec :32-39)
        "target_unknown": (build_nxtheme(image=DEFAULT_IMAGE, config=config_bytes(target="GameMenu")), ("target not in pinned target table",)),
        "target_strict_literal": (build_nxtheme(image=DEFAULT_IMAGE, config=config_bytes(target="ResidentMenu")), ("target not in pinned target table",)),
        # JPEG header checks (:21)
        "image_png_magic": (build_nxtheme(image=b"\x89PNG\r\n\x1a\n" + bytes(32), config=DEFAULT_CONFIG), ("JPEG SOI marker",)),
        "image_zeroes": (build_nxtheme(image=bytes(64), config=DEFAULT_CONFIG), ("JPEG SOI marker",)),
        "image_soi_no_sof": (build_nxtheme(image=soi_no_sof, config=DEFAULT_CONFIG), ("no JPEG SOF marker",)),
        "image_wrong_dims": (build_nxtheme(image=synthetic_jpeg(640, 480), config=DEFAULT_CONFIG), ("image.jpg is not 1280x720",)),
        # container layer delegated to the validated .szs parser
        "foreign_container": (b"PK\x03\x04zip-not-a-theme", ("not a Yaz0/SARC container",)),
        # same domain refusal through the Yaz0 path
        "yaz0_config_invalid": (build_nxtheme(image=DEFAULT_IMAGE, config=garbage_config, wrap=True), ("config.json is not valid JSON",)),
    }


def run_negatives() -> None:
    cases = make_cases()
    pristine = build_nxtheme(image=DEFAULT_IMAGE, config=DEFAULT_CONFIG)
    with tempfile.TemporaryDirectory(prefix="nxtheme-negative-") as tmp:
        root = Path(tmp)
        pristine = build_nxtheme(image=DEFAULT_IMAGE, config=DEFAULT_CONFIG)
        for name, (sample, expected) in cases.items():
            assert sample != pristine, f"case {name} is identical to pristine"
            file = root / f"case-{name}.nxtheme"
            file.write_bytes(sample)
            try:
                nxr.read(file)
            except REJECTED as error:
                message = str(error)
                assert any(mark in message for mark in expected), (
                    f"{name}: refusal {message!r} lacks expected marker {expected}")
                continue
            raise AssertionError(f"{name}: invalid nxtheme was not rejected")


def main() -> int:
    self_verify()
    run_negatives()
    print("PASS: nxtheme reader — self-verified generator, first-check "
          "refusals (NOT RUN by A10; executed only by A50)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
