#!/usr/bin/env python3
"""Structural validator for Switch save-transfer formats on PS5 (front c-saves).

Validates ONLY the structure documented in the pinned spec
A12/switch-formats-spec-r1/SPEC.md (r1, 2026-11-08), whose every field was
extracted from permitted repo code with file:line citations:

- F-A Ryujinx index imkvdb.arc   (headless/ryujinx_saves.h:54-106)
- F-B manual save-import folder  (headless/ryujinx_saves.h:110-139)
- F-C internal layout + profiles.dat (headless/metadata_bridge.cpp:486-599)

No field from spec gaps G1-G12 is invented, read or validated: unread offsets
(IMEN +0x04/+0x08/+0x14 and the remaining entry bytes, IMKV bytes 4-11) are
ignored on purpose, exactly like the repo reader. There are NO checksums in
these formats (SPEC F-D): this validator is STRUCTURAL ONLY and every output
states content_integrity_guaranteed=false. Truncation exactly at an entry
boundary is structurally undetectable; the tests pin that honestly.

Not an emulator, importer or copier: never reads games/keys/firmware, touches
production code or the network. Pure stdlib, no side effects, bounded reads.
"""

import argparse
import json
import os
import sys

SPEC = "A12/switch-formats-spec-r1/SPEC.md"
HEADER = 12           # IMKV header size      (SPEC F-A.2; ryujinx_saves.h:78)
ENTRY = 0x8C          # IMEN entry size       (SPEC F-A.2; ryujinx_saves.h:78)
ACCOUNT, DEVICE = 1, 3  # save types           (SPEC F-A.3; ryujinx_saves.h:36)
PROFILES_MIN = 0x650  # profiles.dat minimum  (SPEC F-C.2; metadata_bridge.cpp:489)
PROFILES_HDR = 0x10   # profiles.dat header   (SPEC F-C.2; metadata_bridge.cpp:490)
PROFILES_REC = 0xC8   # profiles.dat entry    (SPEC F-C.2; metadata_bridge.cpp:490)
MAX_INPUT_BYTES = 1 << 20
KIND_NAMES = {ACCOUNT: "account", DEVICE: "device"}


def base(format_name):
    """Common output envelope: structural-only integrity, always."""
    return {
        "spec": SPEC,
        "format": format_name,
        "integrity": "structural_only_no_checksum",
        "content_integrity_guaranteed": False,
    }


def u64(data, offset):
    return int.from_bytes(data[offset:offset + 8], "little")  # SPEC: LE memcpy


def validate_imkvdb(data):
    """Structural check of an imkvdb.arc index (SPEC F-A.2/F-A.3).

    Returns (result, entries). Unread offsets are deliberately not interpreted
    (spec gap G3); tampering them cannot change the verdict.
    """
    result = base("ryujinx_imkvdb")
    if len(data) < HEADER:
        result.update(status="INVALID", reason="truncated_header",
                      detail=f"{len(data)} bytes < {HEADER}")
        return result, []
    if data[:4] != b"IMKV":
        result.update(status="INVALID", reason="header_magic",
                      detail=repr(data[:4]))
        return result, []
    if (len(data) - HEADER) % ENTRY != 0:
        result.update(status="INVALID", reason="length_not_multiple_of_entry",
                      detail=f"{len(data) - HEADER} bytes after header not multiple of {ENTRY}")
        return result, []
    entries = []
    for index in range((len(data) - HEADER) // ENTRY):
        item = data[HEADER + index * ENTRY: HEADER + (index + 1) * ENTRY]
        if item[:4] != b"IMEN":
            result.update(status="INVALID", reason="entry_magic",
                          detail=f"entry {index}")
            return result, []
        entries.append({
            "index": index,
            "program_id": u64(item, 0x0C),   # SPEC F-A.3; ryujinx_saves.h:91
            "save_type": item[0x2C],         # SPEC F-A.3; ryujinx_saves.h:93
            "save_id": u64(item, 0x4C),      # SPEC F-A.3; ryujinx_saves.h:92
        })
    result.update(status="VALID",
                  header_uninterpreted="bytes 4-11",       # gap G3
                  entry_uninterpreted="0x04,0x08,0x14 and remaining bytes",  # G3/G4
                  entries=entries)
    return result, entries


def select_saves(data, title_id, folders=None):
    """Mirror of the repo selection semantics (SPEC F-A.4; ryujinx_saves.h:94-102).

    folders: optional dict {16 lowercase hex save id: bool folder exists}. When
    None (bytes-only mode) folder existence is NOT evaluated: candidates carry
    the expected folder name and remain unverified against any filesystem. Kinds other than 1/3 and other program IDs are
    skipped; the FIRST save per kind wins; a duplicate is retried only when the
    earlier candidate's folder is missing (repo falls through the same way).
    """
    result, entries = validate_imkvdb(data)
    selection = {"status": "none_for_this_game", "saves": []}
    result["selection"] = selection
    if result["status"] != "VALID":
        return result
    taken = set()
    for entry in entries:
        kind = entry["save_type"]
        if entry["program_id"] != title_id or kind not in KIND_NAMES:
            continue                                    # ryujinx_saves.h:94
        if kind in taken:
            continue                                    # ryujinx_saves.h:95-97
        name = "%016x" % entry["save_id"]               # ryujinx_saves.h:99
        folder = "bis/user/save/" + name + "/0"         # ryujinx_saves.h:100
        exists = None if folders is None else bool(folders.get(name, False))
        if exists is False:
            continue                                    # ryujinx_saves.h:101-102
        taken.add(kind)
        selection["saves"].append({"kind": KIND_NAMES[kind], "save_id": name,
                                   "folder": folder})
    if selection["saves"]:
        selection["status"] = "found"
    return result


def validate_profiles(data):
    """Structural check of profiles.dat user lookup (SPEC F-C.2).

    Mirrors metadata_bridge.cpp:486-500 exactly: requires >= 0x650 bytes,
    walks 0xC8-byte records from 0x10 up to the 0x650 cap, skips records whose
    UUID low and high are both zero, returns the FIRST valid user folder name
    as "%016X%016X" (high before low). Bytes past 0x650 are ignored.
    The single-folder fallback (metadata_bridge.cpp:501-507) is app logic on
    the live tree, not on this file; reported as a note when no user is found.
    """
    result = base("prosperoeden_profiles_dat")
    if len(data) < PROFILES_MIN:
        result.update(status="INVALID", reason="smaller_than_0x650",
                      detail=f"{len(data)} bytes")
        return result
    for offset in range(PROFILES_HDR, PROFILES_MIN - PROFILES_REC + 1, PROFILES_REC):
        low = u64(data, offset)          # metadata_bridge.cpp:492
        high = u64(data, offset + 8)     # metadata_bridge.cpp:493
        if not low and not high:
            continue                     # metadata_bridge.cpp:494
        result.update(status="VALID",
                      user_folder="%016X%016X" % (high, low),  # :495-498
                      uuid_high=high, uuid_low=low, record_offset=offset,
                      bytes_beyond_0x650_ignored=max(0, len(data) - PROFILES_MIN))
        return result
    result.update(status="VALID", user_folder=None,
                  note="no nonzero UUID record; app falls back to the single-folder rule (metadata_bridge.cpp:501-507)",
                  bytes_beyond_0x650_ignored=max(0, len(data) - PROFILES_MIN))
    return result


def title_folder_name(title_id, case):
    return ("%016X" if case == "upper" else "%016x") % title_id  # ryujinx_saves.h:119


def validate_title_name(name):
    """F-B title folder name: 16 hex digits, all one case (either).

    The repo matches the folder against exactly "%016X" or "%016x" of the ID
    (ryujinx_saves.h:119-124), so mixed case never matches. Digits-only names
    match both renderings.
    """
    result = base("save_import_title_folder")
    hexdigits = set("0123456789abcdefABCDEF")
    cases = []
    if isinstance(name, str) and len(name) == 16 and set(name) <= hexdigits:
        if name == name.upper():
            cases.append("upper")
        if name == name.lower():
            cases.append("lower")
    if not cases:
        result.update(status="INVALID", reason="title_folder_name",
                      detail="expected 16 hex digits in a single case (ryujinx_saves.h:119)")
        return result
    result.update(status="VALID", matched_cases=cases,
                  title_id=int(name, 16))
    return result


def validate_user_name(name, rule="generated"):
    """F-C user folder name.

    rule "generated": the profiles.dat rendering "%016X%016X" (uppercase,
    metadata_bridge.cpp:495-498). rule "fallback": the live-tree fallback only
    requires length 32 and a name different from 32 zeros (metadata_bridge.cpp:
    505) — no hex or case constraint, mirrored verbatim.
    """
    result = base("prosperoeden_user_folder")
    if rule == "generated":
        ok = (isinstance(name, str) and len(name) == 32
              and name == name.upper() and set(name) <= set("0123456789ABCDEF"))
        if not ok:
            result.update(status="INVALID", reason="user_folder_name_generated",
                          detail="expected 32 uppercase hex digits (metadata_bridge.cpp:495-498)")
            return result
        result.update(status="VALID", rule=rule, device_folder=False)
    elif rule == "fallback":
        ok = isinstance(name, str) and len(name) == 32 and name != "0" * 32
        if not ok:
            result.update(status="INVALID", reason="user_folder_name_fallback",
                          detail="expected 32 chars, not 32 zeros (metadata_bridge.cpp:505)")
            return result
        result.update(status="VALID", rule=rule, device_folder=False)
    else:
        result.update(status="INVALID", reason="unknown_rule", detail=rule)
    return result


def validate_device_folder(name):
    """F-C device save user folder: exactly 32 zeros (metadata_bridge.cpp:566)."""
    result = base("prosperoeden_device_folder")
    if name == "0" * 32:
        result.update(status="VALID", device_folder=True)
    else:
        result.update(status="INVALID", reason="device_folder_name",
                      detail="expected 32 zeros (metadata_bridge.cpp:566)")
    return result


def validate_save_path(parts):
    """F-C internal save folder path segments.

    Exactly: nand/user/save/0000000000000000/<user 32>/<title 16 UPPERCASE hex>
    (metadata_bridge.cpp:557,566; title rendering :531-535).
    """
    result = base("prosperoeden_save_path")
    expected_root = ["nand", "user", "save", "0000000000000000"]
    if not isinstance(parts, list) or len(parts) != 6:
        result.update(status="INVALID", reason="path_depth",
                      detail="expected 6 segments nand/user/save/0000000000000000/<user>/<title>")
        return result
    if parts[:4] != expected_root:
        result.update(status="INVALID", reason="save_root",
                      detail="/".join(parts[:4]))
        return result
    user, title = parts[4], parts[5]
    device = user == "0" * 32
    if not device and validate_user_name(user)["status"] != "VALID":
        result.update(status="INVALID", reason="user_segment", detail=user)
        return result
    if not (isinstance(title, str) and len(title) == 16 and title == title.upper()
            and set(title) <= set("0123456789ABCDEF")):
        result.update(status="INVALID", reason="title_segment_uppercase_only",
                      detail=title)
        return result
    result.update(status="VALID", kind="device" if device else "account",
                  user=user, title=title)
    return result


def validate_save_import(children, title_id):
    """F-B semantics on a folder tree (SPEC F-B; ryujinx_saves.h:113-139).

    children: {parent_path: {child_name: is_directory}} with "" as the root.
    Mirrors the repo: title folder in either case (upper tried first), split
    mode when account/ or device/ is a directory, whole folder is the account
    save otherwise, non-empty (any entry) required, first case with a save
    wins. Folder existence of the Ryujinx side is not involved here.
    """
    result = base("save_import_folder")

    def holds(path):
        return len(children.get(path, {})) > 0        # ryujinx_saves.h:115-118

    for case in ("upper", "lower"):                   # ryujinx_saves.h:119
        name = title_folder_name(title_id, case)
        game = name
        root_children = children.get("", {})
        if name not in root_children or not root_children[name]:
            continue                                  # must be a directory
        game_children = children.get(game, {})
        account = "account" in game_children and game_children["account"]
        device = "device" in game_children and game_children["device"]
        split = account or device                     # ryujinx_saves.h:125-126
        saves = []
        if not split:
            if holds(game):
                saves.append({"kind": "account", "folder": game})  # :127-128
        else:
            if account and holds(game + "/account"):
                saves.append({"kind": "account", "folder": game + "/account"})
            if device and holds(game + "/device"):
                saves.append({"kind": "device", "folder": game + "/device"})
        if saves:                                     # ryujinx_saves.h:135
            result.update(status="FOUND", matched_case=case, saves=saves)
            return result
    result.update(status="NONE", reason="none for this game")  # :137
    return result


def scan_tree(root):
    """Build the children map for validate_save_import from a real directory.

    Refuses symlinks anywhere under root (parity with the copy-layer refusal,
    SPEC F-E; ryujinx_saves.h:186-200): a symlinked tree must not be validated
    as if it were a plain save structure. Bounded to 4096 entries.
    """
    children = {}

    def walk(directory, rel):
        entries = {}
        with os.scandir(directory) as iterator:
            for count, entry in enumerate(iterator):
                if count >= 4096:
                    raise ValueError("tree too large")
                if entry.is_symlink():
                    raise ValueError("symlink_not_allowed: " + entry.path)
                entries[entry.name] = entry.is_dir(follow_symlinks=False)
        children[rel] = entries
        for name, is_dir in entries.items():
            if is_dir:
                walk(os.path.join(directory, name), (rel + "/" + name) if rel else name)

    walk(root, "")
    return children


def parse_title_id(text):
    value = int(text, 16)
    if not 0 <= value <= 0xFFFFFFFFFFFFFFFF:
        raise ValueError(text)
    return value


def emit(result, code):
    print(json.dumps(result, sort_keys=True))
    return code


def read_bounded(path):
    with open(path, "rb") as stream:
        data = stream.read(MAX_INPUT_BYTES + 1)
    if len(data) > MAX_INPUT_BYTES:
        raise ValueError("input exceeds 1 MiB")
    return data


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)

    p_index = sub.add_parser("imkvdb", help="validate a Ryujinx imkvdb.arc index")
    p_index.add_argument("file")
    p_index.add_argument("--title-id", help="16 hex digits; enables selection")
    p_index.add_argument("--saves-root", help="optional bis/user/save dir to check entry folders exist")

    p_profiles = sub.add_parser("profiles", help="validate a profiles.dat")
    p_profiles.add_argument("file")

    p_names = sub.add_parser("names", help="validate folder name renderings")
    p_names.add_argument("kind", choices=["title", "user", "device"])
    p_names.add_argument("value")
    p_names.add_argument("--rule", choices=["generated", "fallback"], default="generated")

    p_path = sub.add_parser("save-path", help="validate an F-C save path")
    p_path.add_argument("path", help="six slash-separated segments")

    p_tree = sub.add_parser("tree", help="apply F-B semantics to a save-import folder")
    p_tree.add_argument("root")
    p_tree.add_argument("--title-id", required=True)

    args = parser.parse_args(argv)
    try:
        if args.command == "imkvdb":
            if args.title_id:
                folders = None
                if args.saves_root:
                    probe = validate_imkvdb(read_bounded(args.file))[1]
                    folders = {
                        "%016x" % entry["save_id"]:
                            os.path.isdir(os.path.join(args.saves_root, "%016x" % entry["save_id"], "0"))
                        for entry in probe
                    }
                result = select_saves(read_bounded(args.file), parse_title_id(args.title_id), folders)
            else:
                result = validate_imkvdb(read_bounded(args.file))[0]
            return emit(result, 0 if result["status"] == "VALID" else 1)
        if args.command == "profiles":
            result = validate_profiles(read_bounded(args.file))
            return emit(result, 0 if result["status"] == "VALID" else 1)
        if args.command == "names":
            if args.kind == "title":
                result = validate_title_name(args.value)
            elif args.kind == "user":
                result = validate_user_name(args.value, args.rule)
            else:
                result = validate_device_folder(args.value)
            return emit(result, 0 if result["status"] == "VALID" else 1)
        if args.command == "save-path":
            result = validate_save_path(args.path.split("/"))
            return emit(result, 0 if result["status"] == "VALID" else 1)
        if args.command == "tree":
            if not os.path.isdir(args.root):
                return emit({**base("save_import_folder"), "status": "INVALID_INPUT",
                             "reason": "root_not_a_directory"}, 2)
            children = scan_tree(args.root)
            result = validate_save_import(children, parse_title_id(args.title_id))
            return emit(result, 0 if result["status"] == "FOUND" else 1)
    except (OSError, ValueError) as error:
        return emit({**base("unknown"), "status": "INVALID_INPUT", "reason": str(error)}, 2)
    return 2


if __name__ == "__main__":
    sys.exit(main())
