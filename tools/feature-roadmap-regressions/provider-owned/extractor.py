#!/usr/bin/env python3
"""Identify live inputs / extract complete real functions. Never patches production.

--identify only records hashes in a NEW EXTERNAL directory: it is NOT a test.
The execution runner consumes those explicit pins; no production snapshots ship.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import stat
import struct

ROOT = Path(__file__).resolve().parent
PROVIDER = "headless/prosperoeden/eden_services.cpp"
DESCRIPTOR = "headless/user_content.h"
IMAGE_HEADER = "headless/prosperoeden/pe/gfx/image.hpp"
IMAGE_SOURCE = "headless/prosperoeden/pe/gfx/image.cpp"
SOURCE_PATHS = (PROVIDER, DESCRIPTOR, IMAGE_HEADER, IMAGE_SOURCE)
SIGNATURES = {
    "list_entries.inc": b"std::vector<std::string> ListEntries(const std::string& path, bool folders, bool& ok) {",
    "load_image.inc": b"bool EdenServices::load_image(const std::string& path, pe::gfx::Image* image) {",
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def sha(path):
    return digest(path.read_bytes())


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key: " + key)
        result[key] = value
    return result


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_object)


def external(path, repo):
    result = path.resolve()
    if any(result == root or result.is_relative_to(root) for root in (repo, ROOT)):
        raise ValueError("must be external to checkout and harness: " + str(result))
    return result


def new_external_directory(path, repo):
    result = external(path, repo)
    result.mkdir(parents=True, exist_ok=False)
    return result


def live_sources(repo):
    result = {}
    for relative in SOURCE_PATHS:
        lexical = repo / relative
        resolved = lexical.resolve(strict=True)
        if resolved != lexical or not resolved.is_file():
            raise ValueError("source alias / non-regular input: " + relative)
        result[relative] = resolved
    return result


def load_pins(path):
    manifest = read_json(path)
    if manifest.get("schema") != 1 or manifest.get("kind") != "provider-owned-source-pins":
        raise ValueError("invalid source manifest schema/kind")
    pins = manifest.get("sources", {})
    if set(pins) != set(SOURCE_PATHS):
        raise ValueError("source manifest must pin exactly the four live first-party inputs")
    for relative, value in pins.items():
        if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value):
            raise ValueError("invalid SHA-256: " + relative)
    return pins


def verify_sources(paths, pins):
    for relative, path in paths.items():
        if path.resolve(strict=True) != path or sha(path) != pins[relative]:
            raise ValueError("live input changed / hash mismatch: " + relative)


def lexical_view(source):
    """Scan FROM FILE START; mask comments/literals without changing view offsets.

    Phase-2 backslash/newline removal has an original-byte offset map. This is
    deliberately not a C++ parser/preprocessor: unsupported constructs reject.
    """
    if b"\0" in source or re.search(rb"\?\?[=/'()!<>-]", source):
        raise ValueError("NUL/trigraph source requires review")
    text, offsets, index = bytearray(), [], 0
    while index < len(source):
        if source[index:index + 3] == b"\\\r\n":
            index += 3
        elif source[index:index + 2] == b"\\\n":
            index += 2
        else:
            text.append(source[index]); offsets.append(index); index += 1
    text = bytes(text)
    code, clean = bytearray(text), bytearray(text)

    def blank(start, end, comment=False):
        for at in range(start, end):
            if text[at] not in (10, 13):
                code[at] = 32
                if comment:
                    clean[at] = 32

    index = 0
    while index < len(text):
        pair = text[index:index + 2]
        if pair == b"//":
            end = text.find(b"\n", index)
            end = len(text) if end < 0 else end
            blank(index, end, True); index = end
        elif pair == b"/*":
            end = text.find(b"*/", index + 2)
            if end < 0:
                raise ValueError("unterminated block comment")
            blank(index, end + 2, True); index = end + 2
        elif pair == b'R"':
            opening = text.find(b"(", index + 2, index + 19)
            delimiter = text[index + 2:opening] if opening >= 0 else b""
            if opening < 0 or any(c <= 32 or c >= 127 or c in b"()\\" for c in delimiter):
                raise ValueError("unsupported raw delimiter")
            terminator = b")" + delimiter + b'"'
            closing = text.find(terminator, opening + 1)
            if closing < 0:
                raise ValueError("unterminated raw literal")
            end = closing + len(terminator)
            if any(offsets[at] != offsets[index] + at - index for at in range(index, end)):
                raise ValueError("line splicing inside raw literal requires review")
            blank(index, end); index = end
        elif text[index] in (34, 39):
            quote, end = text[index], index + 1
            while end < len(text) and text[end] != quote:
                if text[end] in (10, 13):
                    raise ValueError("newline in ordinary literal / unsupported digit separator")
                end += 2 if text[end] == 92 else 1
            if end >= len(text):
                raise ValueError("unterminated ordinary literal")
            blank(index, end + 1); index = end + 1
        else:
            index += 1
    if any(token in code for token in (b"%:", b"<%", b"%>", b"<:", b":>")):
        raise ValueError("digraph source requires review")
    directives, depth, start = [], 0, 0
    known = {b"include", b"define", b"undef", b"if", b"ifdef", b"ifndef", b"elif", b"else", b"endif", b"pragma", b"error", b"line"}
    while start < len(text):
        end = text.find(b"\n", start)
        end = len(text) if end < 0 else end
        line = bytes(code[start:end])
        if line.lstrip(b" \t\v\f\r").startswith(b"#"):
            match = re.match(rb"[ \t\v\f\r]*#[ \t\v\f\r]*([A-Za-z_][A-Za-z_0-9]*)", line)
            if not match or match[1] not in known:
                raise ValueError("unsupported preprocessing directive (including include_next)")
            word = match[1]
            operand = bytes(clean[start + match.end():end]).strip()
            directives.append((start, end, word, operand, depth))
            if word in (b"if", b"ifdef", b"ifndef"):
                depth += 1
            elif word in (b"else", b"elif", b"endif"):
                if depth == 0:
                    raise ValueError("unbalanced conditional directive")
                if word == b"endif":
                    depth -= 1
            blank(start, end)
        start = end + 1
    if depth:
        raise ValueError("unterminated conditional directive")
    return text, bytes(code), bytes(clean), offsets, directives


def complete_function(source, signature):
    text, code, clean, offsets, directives = lexical_view(source)
    if code.count(signature) != 1:
        raise ValueError("ambiguous/missing REAL code signature")
    start = code.index(signature)
    if code[code.rfind(b"\n", 0, start) + 1:start].strip():
        raise ValueError("unsupported declaration prefix/context")
    depth_before = 0
    for at, end, word, operand, before in directives:
        if end < start:
            depth_before = before + (1 if word in (b"if", b"ifdef", b"ifndef") else -1 if word == b"endif" else 0)
    if depth_before:
        raise ValueError("conditional target requires preprocessor review")
    depth = 0
    for index in range(start + signature.rindex(b"{"), len(code)):
        if code[index] == 123:
            depth += 1
        elif code[index] == 125:
            depth -= 1
            if depth == 0:
                end = text.find(b"\n", index)
                end = len(text) if end < 0 else end
                if clean[index + 1:end].strip() or any(start <= item[0] <= end for item in directives):
                    raise ValueError("trailing tokens/directive inside extracted function")
                if re.search(rb"\b(?:_Pragma|__pragma|__has_include)\b", code[start:end]):
                    raise ValueError("unsupported preprocessing intrinsic")
                original_start = offsets[start]
                original_end = offsets[end] + 1 if end < len(text) else len(source)
                return source[original_start:original_end], source[:original_start].count(b"\n") + 1
    raise ValueError("unterminated function")


def checked_includes(data):
    _, code, _, _, directives = lexical_view(data)
    if re.search(rb"\b(?:_Pragma|__pragma|__has_include)\b", code):
        raise ValueError("unsupported include/preprocessor intrinsic")
    result = []
    for _, _, word, operand, _ in directives:
        if word == b"include":
            match = re.fullmatch(rb'([<"])([^<>"\r\n]+)([>"])', operand)
            if not match or (match[1], match[3]) not in ((b"<", b">"), (b'"', b'"')):
                raise ValueError("include must be one supported literal after line splicing")
            result.append((match[1].decode("ascii"), match[2].decode("ascii")))
        elif word == b"pragma" and operand != b"once":
            raise ValueError("only pragma once is supported in compiled helpers")
        elif word == b"line" or re.search(rb"\b(?:_Pragma|__pragma|__has_include)\b", operand):
            raise ValueError("unsupported directive/intrinsic in compiled helper")
    return result


def check_dependency_surface(paths):
    angles = {
        DESCRIPTOR: {"algorithm", "atomic", "cerrno", "cctype", "cstdint", "dirent.h", "fcntl.h", "memory", "string", "string_view", "sys/stat.h", "unistd.h", "utility", "vector"},
        IMAGE_HEADER: {"cstdint", "string", "vector"},
        IMAGE_SOURCE: {"cstdio", "array", "cstddef", "cstdint", "vector"},
    }
    for relative, accepted in angles.items():
        for style, name in checked_includes(paths[relative].read_bytes()):
            if (style == "<" and name in accepted) or (relative == IMAGE_SOURCE and style == '"' and name == "pe/gfx/image.hpp"):
                continue
            raise ValueError("unreviewed helper include: " + relative + ": " + name)


def preflight_first_party(paths, repo, harness, fragment_dirs):
    """Metadata-only shadow checks BEFORE any compiler; no unknown file is read."""
    check_dependency_surface(paths)

    def node(parent, name):
        parts = Path(name).parts
        if Path(name).is_absolute() or any(part in (".", "..") for part in parts):
            raise ValueError("unsupported header path")
        current = parent
        for index, part in enumerate(parts):
            current = current / part
            try:
                metadata = current.lstat()
            except FileNotFoundError:
                return None
            if stat.S_ISLNK(metadata.st_mode):
                raise ValueError("header search alias: " + str(current))
            if index + 1 < len(parts) and not stat.S_ISDIR(metadata.st_mode):
                raise ValueError("non-directory header search component")
        return current, metadata

    units = {
        paths[IMAGE_SOURCE]: {"pe/gfx/image.hpp": paths[IMAGE_HEADER]},
        harness / "native_gate.cpp": {"user_content.h": paths[DESCRIPTOR]},
        harness / "stb_impl.cpp": {},
        harness / "fd_fixture.cpp": {"user_content.h": paths[DESCRIPTOR], "list_entries.inc": None},
        harness / "jpeg_fixture.cpp": {"user_content.h": paths[DESCRIPTOR], "pe/gfx/image.hpp": paths[IMAGE_HEADER], "load_image.inc": None},
        paths[DESCRIPTOR]: {}, paths[IMAGE_HEADER]: {},
    }
    records = []
    for fragments in fragment_dirs:
        # Same order as run.py's -iquote arguments; source directory is first.
        roots = [repo / "headless", repo / "headless/prosperoeden", fragments]
        for unit, quotes in units.items():
            includes = checked_includes(unit.read_bytes())
            for style, name in includes:
                if style == '"':
                    if name not in quotes:
                        raise ValueError("extra first-party quote include: " + name)
                    expected = quotes[name] or fragments / name
                    for parent in [unit.parent] + roots:
                        found = node(parent, name)
                        if found is None:
                            continue
                        candidate, metadata = found
                        if candidate != expected or not stat.S_ISREG(metadata.st_mode) or candidate.resolve(strict=True) != expected:
                            raise ValueError("first-party quote shadow/alias: " + str(candidate))
                        records.append({"unit": str(unit), "include": name, "resolved": str(expected)})
                        break
                    else:
                        raise ValueError("missing pinned quote include: " + name)
                else:
                    # -iquote keeps these out of angle lookup; reject latent
                    # shadows too, before any future flag/search change.
                    for parent in [unit.parent] + roots:
                        found = node(parent, name)
                        if found is not None:
                            raise ValueError("first-party standard/codec header shadow: " + str(found[0]))
    return records


def audit_dependencies(observed, expected, roots, external_required=()):
    """Exact per-unit first-party set, not a subset; preserve lexical alias evidence."""
    actual, canonical = set(), set()
    for dependency in observed:
        lexical = Path(dependency).absolute()
        resolved = lexical.resolve(strict=True)
        canonical.add(resolved)
        local = any(lexical == root or lexical.is_relative_to(root) or resolved == root or resolved.is_relative_to(root) for root in roots)
        if local:
            if lexical != resolved:
                raise ValueError("aliased first-party dependency")
            actual.add(resolved)
    if actual != set(expected):
        raise ValueError("first-party dependency set differs from exact per-unit allowlist")
    if not set(external_required).issubset(canonical):
        raise ValueError("explicit host dependency was not consumed")
    return actual


def write_extracts(out, variant, source):
    folder = out / "extracted" / variant
    folder.mkdir(parents=True, exist_ok=False)
    records = []
    for filename, signature in SIGNATURES.items():
        body, first_line = complete_function(source, signature)
        target = folder / filename
        target.write_bytes(body)
        records.append({"variant": variant, "path": str(target.relative_to(out)),
            "first_line": first_line, "lines": body.count(b"\n"), "bytes": len(body),
            "sha256": digest(body), "verbatim": True})
    return records


def jpeg(width, height):
    # Identical input generator to ownership-only-v1, not an image decoder.
    def segment(marker, payload):
        return bytes((255, marker)) + struct.pack(">H", len(payload) + 2) + payload
    quant = segment(0xDB, bytes((0,)) + bytes((1,)) * 64)
    frame = segment(0xC0, struct.pack(">BHHB", 8, height, width, 1) + bytes((1, 0x11, 0)))
    counts = bytes((1,)) + bytes(15)
    tables = segment(0xC4, bytes((0,)) + counts + bytes((0,)) +
                     bytes((0x10,)) + counts + bytes((0,)))
    scan = segment(0xDA, bytes((1, 1, 0, 0, 63, 0)))
    bit_count = ((width + 7) // 8) * ((height + 7) // 8) * 2
    entropy = bytes(bit_count // 8)
    if bit_count % 8:
        entropy += bytes(((1 << (8 - bit_count % 8)) - 1,))
    return b"\xff\xd8" + quant + frame + tables + scan + entropy + b"\xff\xd9"


def write_test_data(out, hashes):
    folder = out / "data"
    folder.mkdir(exist_ok=False)
    for name, data in (("tiny.jpg", jpeg(2, 2)), ("wide.jpg", jpeg(8193, 1)),
                       ("invalid.jpg", b"A46 synthetic invalid image\n")):
        if digest(data) != hashes[name]:
            raise ValueError("synthetic input differs from previously qualified data: " + name)
        (folder / name).write_bytes(data)
    directory = out / "fd-root"
    directory.mkdir()
    (directory / "abc").write_bytes(b"A46 synthetic fixture\n")
    (directory / "sub").mkdir()
    (directory / "alias").symlink_to("abc")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--identify", action="store_true", required=True,
                        help="hash identification ONLY, no test/pass claim")
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    repo = args.repo.resolve(strict=True)
    paths = live_sources(repo)
    out = new_external_directory(args.out, repo)
    result = {"schema": 1, "kind": "provider-owned-source-pins",
        "purpose": "INPUT_IDENTIFICATION_ONLY_NOT_TESTED",
        "sources": {relative: sha(path) for relative, path in paths.items()}}
    (out / "source-inputs.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("INPUT_IDENTIFICATION_ONLY_NOT_TESTED: " + str(out / "source-inputs.json"))


if __name__ == "__main__":
    main()
