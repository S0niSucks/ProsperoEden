#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""A10 c-themes r1 — negative/positive fixture for the structural .szs parser.

Consumes the REAL parser (nx_szs_structural.py, which composes the pinned
first-party inspectors). All containers are generated locally with the Python
stdlib only — no Nintendo asset, no game file, no theme file. The generator is
SELF-VERIFIED before any corruption: every pristine synthetic must pass the
real parser and match expected fields, and its layout invariants are asserted.

Negatives assert an explicit refusal at the RIGHT boundary — where "right"
means the FIRST check the mutated bytes actually reach in the pinned
inspectors' real check order, which is not necessarily the check a mutation
"intends" to test. A builder parameter can move upstream fields (size, SFNT
position/header); when it does, either the case documents the real emission
or it becomes post-build byte surgery that mutates exactly one field of a
pristine container. Each case maps to expected message substrings from the
pinned spec (tools/ps-glyph-botw-yaz0.py, tools/ps-glyph-sarc-inspect.py) or
this parser's own routing rules. A corruption that mutates nothing, or a
refusal with an unlisted message, fails the fixture.
"""
from __future__ import annotations

import importlib.util
import struct
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("nx_szs_structural", HERE / "nx_szs_structural.py")
nx = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(nx)

MEMBERS_ONE = [(b"b/sample.bin", b"SAMPLEdt" + bytes(24))]


# --------------------------------------------------------------------------
# synthetic builders (stdlib only)
# --------------------------------------------------------------------------
def build_sarc(members, *, header_size=0x14, version=0x0100, bom=0xFEFF,
               declared_size=None, data_start=None, sfat_magic=b"SFAT",
               sfat_header=0x0C, count=None, sfnt_magic=b"SFNT", sfnt_header=8,
               hashes=None, attr_override=None, start_override=None,
               end_override=None, no_name_nul=False) -> bytes:
    """Minimal big-endian SARC v0x100 exactly as ps-glyph-sarc-inspect.py reads.

    Layout per the pinned spec: magic(0) header_size u16(4) BOM u16(6)
    size u32(8) data_start u32(0x0C) version u16(0x10); SFAT at header_size
    with header u16 == 0x0C, count u16, multiplier u32; SFNT after the entry
    table with header u16 == 8; names 4-aligned (inspector :90-94); hashes
    ascending by default (:85-87). Entry attr follows the FIRST-PARTY pattern
    (tools/check-ps-glyph-botw-yaz0.py:38-49): 0x01000000 | (name_offset/4).
    That bit is required because the inspector treats attr==0 as an UNNAMED
    member (:88-89) — observed by dissection (dissect-sample.py):
    attr=0 -> name None; attr=0x01000000 -> name 'b/sample.bin'.
    Defaults build a VALID archive; each parameter perturbs exactly one
    documented boundary.
    """
    sfat_off = 0x14
    sfnt_off = sfat_off + 0x0C + 0x10 * len(members)
    names_off = sfnt_off + 8
    names = bytearray()
    attrs = []
    for index, (name, _) in enumerate(members):
        attrs.append(len(names) // 4)
        if no_name_nul and index == 0:
            names += name  # caller guarantees alignment and a following non-NUL run
        else:
            names += name + b"\x00"
            while len(names) % 4:
                names += b"\x00"
    if data_start is None:
        data_start = names_off + len(names)
        while data_start % 4:
            names += b"\x00"
            data_start = names_off + len(names)
    payload_total = sum(len(p) for _, p in members)
    size = data_start + payload_total
    blob = bytearray(size)
    blob[:4] = b"SARC"
    struct.pack_into(">H", blob, 4, header_size)
    struct.pack_into(">H", blob, 6, bom)
    struct.pack_into(">I", blob, 8, size if declared_size is None else declared_size)
    struct.pack_into(">I", blob, 0x0C, data_start)
    struct.pack_into(">H", blob, 0x10, version)
    blob[sfat_off:sfat_off + 4] = sfat_magic
    struct.pack_into(">H", blob, sfat_off + 4, sfat_header)
    struct.pack_into(">H", blob, sfat_off + 6, len(members) if count is None else count)
    struct.pack_into(">I", blob, sfat_off + 8, 0x65)
    blob[sfnt_off:sfnt_off + 4] = sfnt_magic
    struct.pack_into(">H", blob, sfnt_off + 4, sfnt_header)
    blob[names_off:names_off + len(names)] = names
    cursor = 0
    for index, (_, payload) in enumerate(members):
        entry = sfat_off + 0x0C + 0x10 * index
        struct.pack_into(">I", blob, entry,
                         hashes[index] if hashes else index + 1)
        attr = 0x01000000 | attrs[index]  # first-party pattern; attr==0 means unnamed
        if attr_override and index in attr_override:
            attr = attr_override[index]
        struct.pack_into(">I", blob, entry + 4, attr)
        start = cursor
        if start_override and index in start_override:
            start = start_override[index]
        end = cursor + len(payload)
        if end_override and index in end_override:
            end = end_override[index]
        struct.pack_into(">I", blob, entry + 8, start)
        struct.pack_into(">I", blob, entry + 12, end)
        blob[data_start + cursor:data_start + cursor + len(payload)] = payload
        cursor += len(payload)
    return bytes(blob)


def yaz0_header(declared: int) -> bytearray:
    return bytearray(b"Yaz0" + declared.to_bytes(4, "big") + bytes(8))


def yaz0_all_literals(inner: bytes) -> bytes:
    out = yaz0_header(len(inner))
    for i in range(0, len(inner), 8):
        out.append(0xFF)
        out.extend(inner[i:i + 8])
    return bytes(out)


def yaz0_copy_before_start() -> bytes:
    # control=0x00 then a SHORT copy descriptor (len_nibble=1 -> length 3,
    # distance 200) on empty output: refused before any length accounting.
    out = yaz0_header(64)
    out.append(0x00)
    dist_minus_1 = 199
    out.append((1 << 4) | ((dist_minus_1 >> 8) & 0x0F))
    out.append(dist_minus_1 & 0xFF)
    return bytes(out)


def yaz0_copy_exceeds_declared() -> bytes:
    # 8 literals first (output=8), then a LONG copy: dist=1, length=0x12+100=118
    # against declared=32 -> exceeds the exact declared output.
    out = yaz0_header(32)
    out.append(0xFF)
    out.extend(b"12345678")
    out.append(0x00)
    out.append(0x00)  # len_nibble=0 | dist high nibble, dist-1 = 0
    out.append(0x00)
    out.append(100)   # long-copy length byte
    return bytes(out)


# --------------------------------------------------------------------------
# generator self-verification (runs the REAL parser on pristine synthetics)
# --------------------------------------------------------------------------
def self_verify() -> None:
    plain = build_sarc(MEMBERS_ONE)
    wrapped = yaz0_all_literals(plain)
    # builder layout invariants against the pinned reading order
    assert struct.unpack_from(">H", plain, 4)[0] == 0x14
    assert plain[6:8] == b"\xfe\xff", "BOM must live at bytes 6..8 (spec :46-48)"
    assert struct.unpack_from(">I", plain, 8)[0] == len(plain)
    assert struct.unpack_from(">H", plain, 0x10)[0] == 0x0100
    assert plain[0x14:0x18] == b"SFAT" and plain[0x30:0x34] == b"SFNT"
    # round-trip through the pinned first-party decoder
    assert nx.yaz0.decode(wrapped) == plain, "literal Yaz0 wrapper broke round-trip"
    with tempfile.TemporaryDirectory(prefix="nx-szs-selfcheck-") as tmp:
        for index, (sample, compression) in enumerate(
                ((plain, "uncompressed"), (wrapped, "Yaz0"))):
            file = Path(tmp) / f"self{index}.szs"
            file.write_bytes(sample)
            report = nx.inventory(file)
            assert report["theme_contract"] == "none_documented_in_repo"
            assert report["theme_valid"] is None
            assert report["compression"] == compression
            assert report["source_sha256"], "source hash present"
            assert report["sarc"]["member_count"] == 1
            member = report["sarc"]["members"][0]
            assert member["name"] == "b/sample.bin"
            assert member["content_bytes"] == len(MEMBERS_ONE[0][1])
            assert member["content_offset"] == report["sarc"]["data_section_offset"]
    two = build_sarc([(b"b/a.bin", b"\x11" * 8), (b"b/b.bin", b"\x22" * 8)])
    with tempfile.TemporaryDirectory(prefix="nx-szs-selfcheck2-") as tmp:
        file = Path(tmp) / "two.szs"
        file.write_bytes(two)
        report = nx.inventory(file)
        assert [m["name"] for m in report["sarc"]["members"]] == ["b/a.bin", "b/b.bin"]
        assert report["sarc"]["hash_multiplier"] == 0x65
        # documented inspector behaviour (:88-89): attr==0 is an UNNAMED member
        unnamed = build_sarc(MEMBERS_ONE, attr_override={0: 0})
        file2 = Path(tmp) / "unnamed.szs"
        file2.write_bytes(unnamed)
        member = nx.inventory(file2)["sarc"]["members"][0]
        assert member["name"] is None and member["name_offset"] is None


# --------------------------------------------------------------------------
# negatives: explicit refusal at each documented boundary
# --------------------------------------------------------------------------
def make_cases() -> dict:
    plain = build_sarc(MEMBERS_ONE)
    wrapped = yaz0_all_literals(plain)
    names_off = 0x14 + 0x0C + 0x10 + 8  # one-member layout: SFNT header ends here

    cases: dict[str, tuple[bytes, tuple[str, ...]]] = {
        # --- own routing boundaries ---
        "empty": (b"", ("container too short",)),
        "foreign_magic_pk": (b"PK\x03\x04zip", ("not a Yaz0/SARC container",)),
        "nxtheme_named_refusal": (b"NXThe" + b"me00" + bytes(16), ("not a Yaz0/SARC container",)),
        # --- Yaz0 boundaries (ps-glyph-botw-yaz0.py:46-89) ---
        "yaz0_header_truncated": (wrapped[:8], ("too short",)),
        "yaz0_reserved_nonzero": (wrapped[:8] + b"NONZERO!" + wrapped[16:], ("reserved",)),
        "yaz0_declared_bomb": (bytes(yaz0_header(nx.yaz0.MAX_DECOMPRESSED + 1)), ("budget",)),
        "yaz0_declared_tiny": (bytes(yaz0_header(0x10)), ("budget",)),
        "yaz0_truncated_control": (bytes(yaz0_header(32)), ("truncated Yaz0 control",)),
        "yaz0_truncated_literal": (bytes(yaz0_header(32) + bytearray(b"\xffabc")), ("truncated Yaz0 literal",)),
        "yaz0_truncated_copy_descriptor": (bytes(yaz0_header(32) + bytearray(b"\x00\x0f")), ("truncated Yaz0 copy descriptor",)),
        "yaz0_truncated_long_copy": (bytes(yaz0_header(32) + bytearray(b"\x00\x00\x00")), ("truncated Yaz0 long-copy",)),
        "yaz0_backreference_before_start": (yaz0_copy_before_start(), ("backreference before output start",)),
        "yaz0_copy_exceeds_declared": (yaz0_copy_exceeds_declared(), ("exceeds exact declared output",)),
        "yaz0_dirty_padding": (wrapped + b"tail", ("nonpadding",)),
        "yaz0_stream_truncated": (wrapped[:-1], ("truncated",)),
        # --- SARC boundaries (ps-glyph-sarc-inspect.py:35-129) ---
        "sarc_truncated_header": (plain[:0x10], ("outside bounds",)),
        "sarc_size_mismatch": (plain[:8] + struct.pack(">I", len(plain) + 1) + plain[12:], ("declared SARC size mismatch",)),
        "sarc_bad_header_size": (plain[:4] + b"\x00\x18" + plain[6:], ("unsupported SARC header",)),
        "sarc_bad_bom": (plain[:6] + b"\x12\x34" + plain[8:], ("unsupported byte order",)),
        "sarc_bad_version": (plain[:0x10] + b"\x00\x99" + plain[0x12:], ("unsupported SARC version",)),
        "sarc_missing_sfat": (plain[:0x14] + b"XXXX" + plain[0x18:], ("missing SFAT",)),
        "sarc_bad_sfat_header": (plain[:0x18] + b"\x00\x0e" + plain[0x1A:], ("unsupported SFAT header",)),
        "sarc_count_over_budget": (plain[:0x1A] + struct.pack(">H", 4097) + plain[0x1C:], ("excessive SARC entries",)),
        "sarc_count_pushes_sfnt_out": (plain[:0x1A] + struct.pack(">H", 4096) + plain[0x1C:], ("missing/truncated SFNT",)),
        "sarc_missing_sfnt": (build_sarc(MEMBERS_ONE, sfnt_magic=b"XXXX"), ("missing/truncated SFNT",)),
        "sarc_bad_sfnt_header": (build_sarc(MEMBERS_ONE, sfnt_header=7), ("unsupported SFNT header",)),
        # data_start via builder also moves size and lets the payload overwrite
        # the SFNT header u16, so the FIRST check hit is :72, not :74 — the
        # expectation documents the real emission (A50 r1/r2 finding).
        "sarc_data_start_shift_overwrites_sfnt_header": (build_sarc(MEMBERS_ONE, data_start=names_off - 4), ("unsupported SFNT header",)),
        # byte-surgery on a pristine container isolates the :74 offset check
        # itself (layout, SFNT magic/header and size stay intact).
        "sarc_data_start_before_names": (plain[:0x0C] + struct.pack(">I", names_off - 4) + plain[0x10:], ("invalid data section offset",)),
        "sarc_data_start_equals_names": (build_sarc(MEMBERS_ONE, data_start=names_off), ("outside string table",)),
        "sarc_start_after_end": (build_sarc(MEMBERS_ONE, start_override={0: 16}, end_override={0: 8}), ("invalid member data range",)),
        "sarc_end_beyond_section": (build_sarc(MEMBERS_ONE, end_override={0: 64 * 1024}), ("invalid member data range",)),
        "sarc_name_pointer_outside": (build_sarc(MEMBERS_ONE, attr_override={0: 0x00F00000}), ("outside string table",)),
        "sarc_name_not_printable": (build_sarc([(b"b/bad\x01x", b"\x00" * 8)]), ("nonprintable SARC name",)),
        "sarc_name_over_512": (build_sarc([(b"a" * 600, b"\x00" * 8)]), ("bounded member name",)),
        "sarc_name_missing_nul": (build_sarc([(b"n" * 20, b"\x00" * 8)], no_name_nul=True), ("bounded member name",)),
        "sarc_hash_unsorted": (build_sarc([(b"b/a.bin", b"\x01"), (b"b/b.bin", b"\x02")], hashes=[5, 1]), ("unsorted SARC filename hashes",)),
        "sarc_duplicate_name_casefold": (build_sarc([(b"b/A.bin", b"\x01"), (b"b/a.bin", b"\x02")], hashes=[1, 2]), ("duplicate SARC member name",)),
    }
    # every byte-mutated case must actually differ from the pristine container
    for name, (sample, _) in cases.items():
        if name.startswith(("empty", "foreign", "nxtheme", "yaz0_header_truncated")):
            continue
        assert sample != plain and sample != wrapped, f"case {name} mutated nothing"
    return cases


def run_negatives() -> None:
    cases = make_cases()
    with tempfile.TemporaryDirectory(prefix="nx-szs-negative-") as tmp:
        root = Path(tmp)
        for name, (sample, expected) in cases.items():
            file = root / f"case-{name}.szs"
            file.write_bytes(sample)
            try:
                nx.inventory(file)
            except (nx.StructuralRejected, nx.yaz0.InvalidYaz0, nx.sarc.InvalidSarc) as error:
                message = str(error)
                assert any(mark in message for mark in expected), (
                    f"{name}: refusal message {message!r} lacks expected boundary "
                    f"marker {expected}")
                continue
            raise AssertionError(f"{name}: corrupt container was not rejected")

        # non-file inputs must be refused by the parser's own entry checks
        try:
            nx.inventory(root / "missing.szs")
        except nx.StructuralRejected:
            pass
        else:
            raise AssertionError("missing file was not rejected")
        link = root / "link.szs"
        link.symlink_to(root / "case-empty.szs")
        try:
            nx.inventory(link)
        except nx.StructuralRejected:
            pass
        else:
            raise AssertionError("symlink input was not rejected")


def main() -> int:
    self_verify()
    run_negatives()
    print("PASS: nx_szs structural subset — self-verified generator, explicit "
          "boundary refusals (NOT RUN by A10; executed only by A50)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
