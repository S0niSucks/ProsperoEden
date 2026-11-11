"""Negative-fixture suite for csaves_validator.py — NOT_RUN by A12.

Prepared for A50 (existing Docker Python, offline, read-only inputs, external
outputs). Fixtures are generated synthetically, mirroring the repo's own host
check (headless/ryujinx_check.cpp:36-74): no real saves, games or user data.

Covers, per the pinned spec A12/switch-formats-spec-r1/SPEC.md:
- valid synthetics for F-A/F-B/F-C and corrupted variants;
- truncation at EVERY boundary (full sweep), wrong magics, inconsistent lengths;
- unread offsets (spec gaps G3/G4) must NOT change any verdict;
- truncation exactly at an entry boundary is structurally undetectable
  (there are no checksums — SPEC F-D) and the output must keep saying so.
"""

import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

import csaves_validator as v

GAME = 0x0100AAAA00000000
OTHER = 0x0100BBBB00000000
UPPER, LOWER = "%016X" % GAME, "%016x" % GAME


def make_entry(program, user_low, save_type, save_id):
    """One IMEN entry exactly as the repo fixture writes it
    (headless/ryujinx_check.cpp:37-48), including the bytes the reader never
    interprets (u32 0x40 at +0x04/+0x08, user at +0x14)."""
    item = bytearray(v.ENTRY)
    item[0:4] = b"IMEN"
    item[4:8] = (0x40).to_bytes(4, "little")
    item[8:12] = (0x40).to_bytes(4, "little")
    item[0x0C:0x14] = program.to_bytes(8, "little")
    item[0x14:0x1C] = user_low.to_bytes(8, "little")
    item[0x2C] = save_type
    item[0x4C:0x54] = save_id.to_bytes(8, "little")
    return bytes(item)


def make_index(*entries):
    return b"IMKV" + bytes(8) + b"".join(make_entry(*e) for e in entries)


def fixture_index():
    """Repo host-check parity: account, second user's account, device, cache,
    other game (headless/ryujinx_check.cpp:62-66)."""
    return make_index(
        (GAME, 1, 1, 0x1), (GAME, 2, 1, 0x3), (GAME, 0, 3, 0x2),
        (GAME, 0, 5, 0x5), (OTHER, 1, 1, 0x4),
    )


def make_profiles(records):
    """profiles.dat: 0x10-byte header then 0xC8-byte records with the UUID at
    +0/+8 (metadata_bridge.cpp:490-493). Exactly 8 records fill 0x650."""
    data = bytearray(v.PROFILES_MIN)
    for index, (low, high) in enumerate(records):
        offset = v.PROFILES_HDR + index * v.PROFILES_REC
        data[offset:offset + 8] = low.to_bytes(8, "little")
        data[offset + 8:offset + 16] = high.to_bytes(8, "little")
    return bytes(data)


def envelope_ok(result):
    return (result.get("content_integrity_guaranteed") is False
            and result.get("integrity") == "structural_only_no_checksum"
            and result.get("spec") == v.SPEC)


class ImkvdbTests(unittest.TestCase):
    def test_valid_index_decodes_only_spec_fields(self):
        result, entries = v.validate_imkvdb(fixture_index())
        self.assertEqual(result["status"], "VALID")
        self.assertEqual(len(entries), 5)
        self.assertEqual([e["save_type"] for e in entries], [1, 1, 3, 5, 1])
        self.assertEqual(entries[0]["program_id"], GAME)
        self.assertEqual(entries[0]["save_id"], 1)
        self.assertEqual(entries[3]["save_id"], 5)  # cache-type entry decoded, skipped later
        self.assertTrue(envelope_ok(result))

    def test_selection_first_account_and_device(self):
        result = v.select_saves(fixture_index(), GAME)
        self.assertEqual(result["selection"]["status"], "found")
        self.assertEqual([(s["kind"], s["save_id"]) for s in result["selection"]["saves"]],
                         [("account", "0000000000000001"), ("device", "0000000000000002")])
        self.assertEqual(result["selection"]["saves"][0]["folder"],
                         "bis/user/save/0000000000000001/0")

    def test_selection_skips_other_program_and_unknown_types(self):
        result = v.select_saves(fixture_index(), OTHER)
        self.assertEqual([s["save_id"] for s in result["selection"]["saves"]], ["0000000000000004"])
        only_cache = make_index((GAME, 0, 5, 0x5), (GAME, 0, 0, 0x6), (GAME, 0, 255, 0x7))
        result = v.select_saves(only_cache, GAME)
        self.assertEqual(result["selection"]["status"], "none_for_this_game")

    def test_unread_offsets_never_change_a_verdict(self):
        """Pins the no-invention boundary (spec gaps G3/G4): bytes the repo
        reader ignores — IMKV header 4-11 and IMEN +0x04/+0x08/+0x14 and the
        remaining entry bytes — must be ignorable here too."""
        baseline = fixture_index()
        tampered = bytearray(baseline)
        for offset in range(4, 12):        # header bytes past the magic
            tampered[offset] ^= 0xFF
        for offset in (0x04, 0x08, 0x14, 0x30, 0x8B):
            position = v.HEADER + offset
            while position < len(tampered):  # same relative offset in each entry
                tampered[position] ^= 0xFF
                position += v.ENTRY
        self.assertEqual(v.validate_imkvdb(bytes(tampered)), v.validate_imkvdb(baseline))
        self.assertEqual(v.select_saves(bytes(tampered), GAME), v.select_saves(baseline, GAME))

    def test_header_truncation_sweep(self):
        data = fixture_index()
        for length in range(0, v.HEADER):
            result, entries = v.validate_imkvdb(data[:length])
            self.assertEqual((result["status"], result["reason"], entries),
                             ("INVALID", "truncated_header", []))
        self.assertTrue(envelope_ok(result))

    def test_full_truncation_sweep_every_boundary(self):
        """Every possible cut of a 2-entry index gets the exact repo verdict.
        A cut at an entry boundary is VALID (and undetectable) — see below."""
        data = make_index((GAME, 1, 1, 0x1), (GAME, 0, 3, 0x2))
        for length in range(0, len(data) + 1):
            result, entries = v.validate_imkvdb(data[:length])
            if length < v.HEADER:
                expected = ("INVALID", "truncated_header", 0)
            elif (length - v.HEADER) % v.ENTRY != 0:
                expected = ("INVALID", "length_not_multiple_of_entry", 0)
            else:
                expected = ("VALID", None, (length - v.HEADER) // v.ENTRY)
            self.assertEqual(result["status"], expected[0], length)
            if expected[1]:
                self.assertEqual(result["reason"], expected[1], length)
            self.assertEqual(len(entries), expected[2], length)

    def test_entry_magic_rejected_at_each_entry(self):
        for index in (0, 1, 4):
            data = bytearray(fixture_index())
            data[v.HEADER + index * v.ENTRY: v.HEADER + index * v.ENTRY + 4] = b"IMEX"
            result, _ = v.validate_imkvdb(bytes(data))
            self.assertEqual((result["status"], result["reason"], result["detail"]),
                             ("INVALID", "entry_magic", f"entry {index}"))

    def test_header_magic(self):
        data = bytearray(fixture_index())
        data[3] = ord("X")
        result, _ = v.validate_imkvdb(bytes(data))
        self.assertEqual(result["reason"], "header_magic")

    def test_header_only_is_valid_and_selects_none(self):
        data = b"IMKV" + bytes(8)
        result, entries = v.validate_imkvdb(data)
        self.assertEqual((result["status"], entries), ("VALID", []))
        self.assertEqual(v.select_saves(data, GAME)["selection"]["status"],
                         "none_for_this_game")

    def test_truncation_at_entry_boundary_is_undetectable(self):
        """Declared limit (SPEC F-D): without checksums, losing whole trailing
        entries is structurally invisible. Both outputs must stay honest."""
        full = make_index((GAME, 1, 1, 0x1), (GAME, 0, 3, 0x2))
        cut = full[:v.HEADER + v.ENTRY]
        single = make_index((GAME, 1, 1, 0x1))
        self.assertEqual(v.validate_imkvdb(cut), v.validate_imkvdb(single))
        self.assertEqual(v.select_saves(cut, GAME)["selection"]["saves"],
                         [{"kind": "account", "save_id": "0000000000000001",
                           "folder": "bis/user/save/0000000000000001/0"}])
        for result in (v.validate_imkvdb(cut)[0],):
            self.assertFalse(result["content_integrity_guaranteed"])

    def test_save_id_zero_renders_sixteen_zeros(self):
        result = v.select_saves(make_index((GAME, 1, 1, 0)), GAME)
        self.assertEqual(result["selection"]["saves"][0]["save_id"], "0000000000000000")

    def test_folder_gate_falls_through_to_duplicate(self):
        """Repo semantics: a missing folder drops the first candidate and a
        later duplicate of the same kind is retried (ryujinx_saves.h:95-102)."""
        data = make_index((GAME, 1, 1, 0x1), (GAME, 2, 1, 0x3))
        folders = {"0000000000000001": False, "0000000000000003": True}
        result = v.select_saves(data, GAME, folders)
        self.assertEqual([s["save_id"] for s in result["selection"]["saves"]],
                         ["0000000000000003"])
        self.assertEqual(v.select_saves(data, GAME, {"0000000000000001": False,
                                                     "0000000000000003": False})
                         ["selection"]["status"], "none_for_this_game")


class ProfilesTests(unittest.TestCase):
    def test_first_nonzero_record_wins(self):
        data = make_profiles([(0, 0), (0x1122334455667788, 0x99AABBCCDDEEFF00)] + [(0, 0)] * 6)
        result = v.validate_profiles(data)
        self.assertEqual(result["status"], "VALID")
        self.assertEqual(result["user_folder"], "99AABBCCDDEEFF001122334455667788")
        self.assertEqual(result["record_offset"], v.PROFILES_HDR + v.PROFILES_REC)
        self.assertTrue(envelope_ok(result))

    def test_high_rendered_before_low(self):
        result = v.validate_profiles(make_profiles([(2, 1)] + [(0, 0)] * 7))
        self.assertEqual(result["user_folder"], "00000000000000010000000000000002")

    def test_all_zero_records_yield_no_user_with_fallback_note(self):
        result = v.validate_profiles(make_profiles([(0, 0)] * 8))
        self.assertEqual(result["status"], "VALID")
        self.assertIsNone(result["user_folder"])
        self.assertIn("metadata_bridge.cpp:501-507", result["note"])

    def test_smaller_than_0x650_is_invalid(self):
        result = v.validate_profiles(make_profiles([(1, 0)])[:v.PROFILES_MIN - 1])
        self.assertEqual((result["status"], result["reason"]),
                         ("INVALID", "smaller_than_0x650"))

    def test_last_record_boundary_and_extra_bytes_ignored(self):
        data = make_profiles([(0, 0)] * 7 + [(5, 0)])
        result = v.validate_profiles(data)
        self.assertEqual(result["record_offset"], v.PROFILES_MIN - v.PROFILES_REC)
        self.assertEqual(result["user_folder"], "%032X" % 5)
        padded = v.validate_profiles(data + bytes(100))
        self.assertEqual(padded["user_folder"], result["user_folder"])
        self.assertEqual(padded["bytes_beyond_0x650_ignored"], 100)


class NamesTests(unittest.TestCase):
    def test_title_accepts_both_cases_digits_match_both(self):
        self.assertEqual(v.validate_title_name(UPPER)["matched_cases"], ["upper"])
        self.assertEqual(v.validate_title_name(LOWER)["matched_cases"], ["lower"])
        self.assertEqual(v.validate_title_name("0100000000000100")["matched_cases"],
                         ["upper", "lower"])

    def test_title_rejects_malformed(self):
        for name in ("0100aAAA00000000", "0100AAAA0000000", "0100AAAA000000000",
                     "0100AAAA0000000g", "", "0x0100AAAA00000000", "0100AAAA0000000 "):
            with self.subTest(name=name):
                result = v.validate_title_name(name)
                self.assertEqual((result["status"], result["reason"]),
                                 ("INVALID", "title_folder_name"))

    def test_user_generated_rule(self):
        self.assertEqual(v.validate_user_name("00112233445566778899AABBCCDDEEFF")["status"], "VALID")
        for name in ("00112233445566778899aabbccddeeff", "00112233445566778899AABBCCDDEEF",
                     "00112233445566778899AABBCCDDEEFF0"):
            with self.subTest(name=name):
                self.assertEqual(v.validate_user_name(name)["status"], "INVALID")

    def test_user_fallback_rule_mirrors_repo(self):
        """Fallback only needs length 32 and not 32 zeros (metadata_bridge.cpp:505)."""
        self.assertEqual(v.validate_user_name("00112233445566778899aabbccddeeff",
                                              rule="fallback")["status"], "VALID")
        self.assertEqual(v.validate_user_name("not-hex-at-all-but-exactly-32ch!",
                                              rule="fallback")["status"], "VALID")
        self.assertEqual(v.validate_user_name("0" * 32, rule="fallback")["status"], "INVALID")

    def test_device_folder(self):
        self.assertEqual(v.validate_device_folder("0" * 32)["status"], "VALID")
        self.assertEqual(v.validate_device_folder("0" * 31 + "1")["status"], "INVALID")


class SavePathTests(unittest.TestCase):
    ROOT = ["nand", "user", "save", "0000000000000000"]

    def test_valid_account_and_device_paths(self):
        account = self.ROOT + ["00112233445566778899AABBCCDDEEFF", UPPER]
        device = self.ROOT + ["0" * 32, UPPER]
        self.assertEqual((v.validate_save_path(account)["status"],
                          v.validate_save_path(account)["kind"]), ("VALID", "account"))
        self.assertEqual(v.validate_save_path(device)["kind"], "device")

    def test_rejections(self):
        cases = {
            "save_root": self.ROOT[:-1] + ["0000000000000001", "0" * 32, UPPER],
            "user_segment": self.ROOT + ["00112233445566778899AABBCCDDEEF", UPPER],
            "title_segment_uppercase_only": self.ROOT + ["0" * 32, LOWER],
            "path_depth": self.ROOT + ["0" * 32],
        }
        for reason, parts in cases.items():
            with self.subTest(reason=reason):
                result = v.validate_save_path(parts)
                self.assertEqual(result["status"], "INVALID")
                self.assertEqual(result["reason"], reason)
        self.assertEqual(v.validate_save_path(self.ROOT + ["0" * 32, UPPER, "extra"])["reason"],
                         "path_depth")


class SaveImportSemanticsTests(unittest.TestCase):
    def tree(self, root_children, **subtrees):
        children = {"": root_children}
        children.update(subtrees)
        return children

    def test_whole_folder_is_the_account_save(self):
        children = self.tree({LOWER: True}, **{LOWER: {"save.bin": False, "sub": True}})
        result = v.validate_save_import(children, GAME)
        self.assertEqual(result["status"], "FOUND")
        self.assertEqual(result["saves"], [{"kind": "account", "folder": LOWER}])

    def test_split_mode_account_and_device(self):
        children = self.tree({UPPER: True}, **{
            UPPER: {"account": True, "device": True},
            UPPER + "/account": {"a.bin": False},
            UPPER + "/device": {"d.bin": False},
        })
        result = v.validate_save_import(children, GAME)
        self.assertEqual([(s["kind"], s["folder"]) for s in result["saves"]],
                         [("account", UPPER + "/account"), ("device", UPPER + "/device")])

    def test_split_with_empty_account_keeps_only_device(self):
        children = self.tree({UPPER: True}, **{
            UPPER: {"account": True, "device": True},
            UPPER + "/device": {"d.bin": False},
        })
        result = v.validate_save_import(children, GAME)
        self.assertEqual([s["kind"] for s in result["saves"]], ["device"])

    def test_split_mode_suppresses_root_files(self):
        """Surprising but specified: once account/ or device/ exists as a
        directory, loose files in the game folder no longer form a save."""
        children = self.tree({UPPER: True}, **{UPPER: {"account": True, "save.bin": False}})
        result = v.validate_save_import(children, GAME)
        self.assertEqual((result["status"], result["reason"]),
                         ("NONE", "none for this game"))

    def test_empty_folder_title_as_file_and_mixed_case(self):
        empty = self.tree({UPPER: True}, **{UPPER: {}})
        as_file = self.tree({UPPER: False}, **{UPPER: {}})
        mixed = self.tree({"0100aAAA00000000": True},
                          **{"0100aAAA00000000": {"save.bin": False}})
        for children in (empty, as_file, mixed):
            result = v.validate_save_import(children, GAME)
            self.assertEqual(result["status"], "NONE")

    def test_upper_case_wins_when_both_exist(self):
        children = self.tree({UPPER: True, LOWER: True}, **{
            UPPER: {"u.bin": False}, LOWER: {"l.bin": False},
        })
        result = v.validate_save_import(children, GAME)
        self.assertEqual((result["matched_case"], result["saves"][0]["folder"]),
                         ("upper", UPPER))


class CliTests(unittest.TestCase):
    def run_cli(self, *argv):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            code = v.main(list(argv))
        return code, json.loads(output.getvalue())

    def test_imkvdb_cli_valid_and_truncated(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "imkvdb.arc")
            with open(path, "wb") as stream:
                stream.write(fixture_index())
            code, result = self.run_cli("imkvdb", path, "--title-id", "%X" % GAME)
            self.assertEqual(code, 0)
            self.assertTrue(envelope_ok(result))
            self.assertEqual(result["selection"]["status"], "found")
            with open(path, "wb") as stream:
                stream.write(fixture_index()[:7])
            code, result = self.run_cli("imkvdb", path)
            self.assertEqual((code, result["status"], result["reason"]),
                             (1, "INVALID", "truncated_header"))

    def test_imkvdb_cli_folder_gate(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "imkvdb.arc")
            index = make_index((GAME, 1, 1, 0x1), (GAME, 2, 1, 0x3))
            with open(path, "wb") as stream:
                stream.write(index)
            saves = os.path.join(tmp, "save")
            os.makedirs(os.path.join(saves, "0000000000000003", "0"))
            code, result = self.run_cli("imkvdb", path, "--title-id", "%X" % GAME,
                                        "--saves-root", saves)
            self.assertEqual(code, 0)
            self.assertEqual([s["save_id"] for s in result["selection"]["saves"]],
                             ["0000000000000003"])

    def test_profiles_cli(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "profiles.dat")
            with open(path, "wb") as stream:
                stream.write(make_profiles([(0, 0)] * 8))
            code, result = self.run_cli("profiles", path)
            self.assertEqual((code, result["user_folder"]), (0, None))
            with open(path, "wb") as stream:
                stream.write(bytes(v.PROFILES_MIN - 1))
            code, result = self.run_cli("profiles", path)
            self.assertEqual((code, result["reason"]), (1, "smaller_than_0x650"))

    def test_names_and_save_path_cli(self):
        self.assertEqual(self.run_cli("names", "title", UPPER)[0], 0)
        self.assertEqual(self.run_cli("names", "title", "0100aAAA00000000")[0], 1)
        self.assertEqual(self.run_cli("names", "user", "not-hex-at-all-but-exactly-32ch!",
                                      "--rule", "fallback")[0], 0)
        self.assertEqual(self.run_cli("names", "device", "0" * 32)[0], 0)
        good = "nand/user/save/0000000000000000/" + "0" * 32 + "/" + UPPER
        self.assertEqual(self.run_cli("save-path", good)[0], 0)
        self.assertEqual(self.run_cli("save-path", good.replace(UPPER, LOWER))[0], 1)

    def test_tree_cli_found_none_symlink_and_missing_root(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = os.path.join(tmp, "save-import")
            os.makedirs(os.path.join(root, LOWER, "account"))
            with open(os.path.join(root, LOWER, "account", "save.bin"), "wb") as stream:
                stream.write(b"x")
            code, result = self.run_cli("tree", root, "--title-id", "%X" % GAME)
            self.assertEqual((code, result["status"], result["saves"][0]["folder"]),
                             (0, "FOUND", LOWER + "/account"))
            code, result = self.run_cli("tree", root, "--title-id", "%X" % OTHER)
            self.assertEqual((code, result["status"]), (1, "NONE"))
            os.makedirs(os.path.join(tmp, "outside"))
            os.symlink(os.path.join(tmp, "outside"), os.path.join(root, "escape"))
            code, result = self.run_cli("tree", root, "--title-id", "%X" % GAME)
            self.assertEqual((code, result["status"]), (2, "INVALID_INPUT"))
            self.assertIn("symlink", result["reason"])
            code, result = self.run_cli("tree", os.path.join(tmp, "absent"),
                                        "--title-id", "%X" % GAME)
            self.assertEqual((code, result["reason"]), (2, "root_not_a_directory"))

    def test_invalid_inputs_fail_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "bad.json")
            with open(path, "wb") as stream:
                stream.write(b"\xff" * 10)
            self.assertEqual(self.run_cli("profiles", path)[0], 1)  # short but readable
            self.assertEqual(self.run_cli("profiles", os.path.join(tmp, "absent"))[0], 2)
            self.assertEqual(self.run_cli("imkvdb", path, "--title-id", "not-hex")[0], 2)


if __name__ == "__main__":
    unittest.main()
