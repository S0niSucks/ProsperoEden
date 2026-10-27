#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline, bounded NXTheme lite converter. No Nintendo assets, firmware or code included.

Format observations: SwitchThemeInjector 66ccc56c6370908cbc1b09059122004cbb6ee605.
This is an independent reader, not a port of SwitchThemeInjector's GPLv2 source.
Only lite conversion is implemented. Unsupported data is never silently called full import.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import stat
import struct
import subprocess
import tempfile
import zipfile

MAX_ARCHIVE = 64 * 1024 * 1024
MAX_FILE = 16 * 1024 * 1024
MAX_TOTAL = 128 * 1024 * 1024
TARGETS = {"home": "Library", "lock": "Welcome", "apps": "All games", "set": "Settings",
           "news": "News", "user": "Profiles", "psl": "Choose player"}


class ImportError(ValueError):
    pass


def need(condition, message):
    if not condition:
        raise ImportError(message)


def read_bounded(path, limit):
    before = path.lstat()
    need(stat.S_ISREG(before.st_mode), "Expected a regular file (no symbolic links)")
    fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
    with os.fdopen(fd, "rb") as file:
        current = os.fstat(file.fileno())
        need(stat.S_ISREG(current.st_mode) and (before.st_dev, before.st_ino) == (current.st_dev, current.st_ino) and
             0 <= current.st_size <= limit, "Source changed or exceeds size limit")
        data = file.read(limit + 1)
        need(len(data) == current.st_size and len(data) <= limit, "Source changed during read")
        return data


def safe_name(name):
    need(isinstance(name, str) and 0 < len(name) <= 240 and not any(ord(c) < 32 for c in name), "Invalid archive path")
    need("\\" not in name and ":" not in name and not name.startswith("/"), "Unsafe archive path")
    need(all(p not in ("", ".", "..") for p in name.split("/")), "Unsafe archive path component")
    return name


def strict_json(data):
    need(len(data) <= 512 * 1024, "JSON too large")
    def members(pairs):
        out = {}
        for key, value in pairs:
            need(key not in out, "Duplicate JSON key")
            out[key] = value
        return out
    try:
        value = json.loads(data, object_pairs_hook=members, parse_constant=lambda _: (_ for _ in ()).throw(ImportError("Non-finite JSON number")))
        def visit(item, depth=0):
            need(depth <= 32, "JSON nesting too deep")
            if isinstance(item, dict):
                for child in item.values():
                    visit(child, depth + 1)
            elif isinstance(item, list):
                for child in item:
                    visit(child, depth + 1)
        visit(value)
        return value
    except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
        raise ImportError("Invalid JSON") from error


def yaz0(data):
    need(len(data) >= 16 and data[:4] == b"Yaz0", "Not Yaz0")
    length = int.from_bytes(data[4:8], "big")
    need(0 < length <= MAX_ARCHIVE, "Yaz0 expansion exceeds limit")
    out = bytearray()
    at = 16
    while len(out) < length:
        need(at < len(data), "Truncated Yaz0 flags")
        code = data[at]
        at += 1
        for shift in range(7, -1, -1):
            if len(out) == length:
                break
            if code & (1 << shift):
                need(at < len(data), "Truncated Yaz0 literal")
                out.append(data[at])
                at += 1
            else:
                need(at + 2 <= len(data), "Truncated Yaz0 reference")
                first, second = data[at:at + 2]
                at += 2
                distance = ((first & 15) << 8 | second) + 1
                count = first >> 4
                if count == 0:
                    need(at < len(data), "Truncated Yaz0 length")
                    count = data[at] + 18
                    at += 1
                else:
                    count += 2
                need(distance <= len(out) and len(out) + count <= length, "Invalid Yaz0 back-reference")
                for _ in range(count):
                    out.append(out[-distance])
    return bytes(out)


def sarc(data):
    need(len(data) >= 40 and data[:4] == b"SARC", "Not a supported SARC container")
    need(data[6:8] in (b"\xfe\xff", b"\xff\xfe"), "Invalid SARC byte order")
    order = ">" if data[6:8] == b"\xfe\xff" else "<"
    def u16(at):
        need(at + 2 <= len(data), "Truncated SARC")
        return struct.unpack_from(order + "H", data, at)[0]
    def u32(at):
        need(at + 4 <= len(data), "Truncated SARC")
        return struct.unpack_from(order + "I", data, at)[0]
    header, size, body = u16(4), u32(8), u32(12)
    need(header == 20 and size == len(data) and header <= body <= size and u16(16) == 0x100, "Invalid SARC header")
    need(data[header:header + 4] == b"SFAT" and u16(header + 4) == 12, "Missing SFAT")
    count, multiplier = u16(header + 6), u32(header + 8)
    need(0 < count <= 256, "SARC entry limit exceeded")
    sfnt = header + 12 + count * 16
    need(sfnt + 8 <= body and data[sfnt:sfnt + 4] == b"SFNT" and u16(sfnt + 4) == 8, "Missing SFNT")
    names = sfnt + 8
    result, occupied, folded = {}, [], set()
    for index in range(count):
        entry = header + 12 + index * 16
        name_hash, attributes, start, end = (u32(entry + i) for i in (0, 4, 8, 12))
        need(attributes >> 24 == 1, "Unnamed/unsupported SARC member")
        name_at = names + (attributes & 0xffffff) * 4
        need(names <= name_at < body, "Invalid SARC filename offset")
        name_end = data.find(b"\0", name_at, min(body, name_at + 241))
        need(name_end >= 0, "Unterminated SARC filename")
        try:
            name = safe_name(data[name_at:name_end].decode("utf-8"))
        except UnicodeError as error:
            raise ImportError("Invalid SARC filename encoding") from error
        computed = 0
        for char in data[name_at:name_end]:
            computed = (computed * multiplier + char) & 0xffffffff
        need(computed == name_hash, "SARC name hash mismatch")
        need(name.casefold() not in folded, "Duplicate/case-colliding SARC member")
        folded.add(name.casefold())
        need(0 <= start <= end <= size - body and end - start <= MAX_FILE, "Invalid SARC member extent")
        need(all(end <= a or start >= b for a, b in occupied), "Overlapping SARC members")
        occupied.append((start, end))
        result[name] = data[body + start:body + end]
    return result


def unzip(data):
    result, folded = {}, set()
    total = 0
    try:
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            need(0 < len(archive.infolist()) <= 256, "ZIP entry limit exceeded")
            for entry in archive.infolist():
                name = safe_name(entry.filename.rstrip("/"))
                mode = entry.external_attr >> 16
                need(not stat.S_ISLNK(mode) and (stat.S_IFMT(mode) in (0, stat.S_IFREG, stat.S_IFDIR)), "Unsupported ZIP resource type")
                need(not entry.flag_bits & 1 and entry.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED), "Unsupported ZIP encryption/compression")
                need(name.casefold() not in folded, "Duplicate/case-colliding ZIP member")
                folded.add(name.casefold())
                if entry.is_dir():
                    continue
                total += entry.file_size
                need(entry.file_size <= MAX_FILE and total <= MAX_ARCHIVE, "ZIP expansion exceeds limit")
                with archive.open(entry) as member:
                    blob = member.read(MAX_FILE + 1)
                need(len(blob) == entry.file_size and len(blob) <= MAX_FILE, "ZIP size mismatch")
                result[name] = blob
    except (zipfile.BadZipFile, RuntimeError, NotImplementedError) as error:
        raise ImportError("Invalid ZIP container/CRC") from error
    return result


def container(data):
    need(0 < len(data) <= MAX_ARCHIVE, "Archive size exceeds limit")
    if data.startswith(b"Yaz0"):
        return sarc(yaz0(data))
    if data.startswith(b"SARC"):
        return sarc(data)
    if data.startswith(b"PK\x03\x04"):
        return unzip(data)
    raise ImportError("Unsupported container: expected NXTheme ZIP or Yaz0/SARC; firmware/NCA/SZS dumps are not imported")


def dds(data):
    need(len(data) >= 128 and data[:4] == b"DDS ", "Invalid DDS header")
    size, _, height, width = struct.unpack_from("<4I", data, 4)
    need(size == 124 and 0 < width <= 4096 and 0 < height <= 4096 and width * height <= 4194304, "DDS dimensions exceed limit")
    need(struct.unpack_from("<I", data, 76)[0] == 32 and struct.unpack_from("<I", data, 80)[0] & 4, "DDS must use block compression")
    kind = data[84:88]
    need(kind in (b"DXT1", b"DXT3", b"DXT5"), "Unsupported DDS codec (only DXT1/3/5; no BNTX or proprietary assets)")
    need(struct.unpack_from("<I", data, 112)[0] == 0, "Cubemap/volume DDS is unsupported")
    block_size = 8 if kind == b"DXT1" else 16
    need(len(data) >= 128 + ((width + 3) // 4) * ((height + 3) // 4) * block_size, "Truncated DDS pixels")
    out = bytearray(width * height * 4)
    def rgb565(value):
        r, g, b = value >> 11, value >> 5 & 63, value & 31
        return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), 255)
    at = 128
    for by in range(0, height, 4):
        for bx in range(0, width, 4):
            block = data[at:at + block_size]
            at += block_size
            colors = block[-8:]
            c0, c1, indices = struct.unpack("<HHI", colors)
            palette = [rgb565(c0), rgb565(c1)]
            if c0 > c1 or kind != b"DXT1":
                palette += [tuple((2 * palette[0][i] + palette[1][i]) // 3 for i in range(3)) + (255,),
                            tuple((palette[0][i] + 2 * palette[1][i]) // 3 for i in range(3)) + (255,)]
            else:
                palette += [tuple((palette[0][i] + palette[1][i]) // 2 for i in range(3)) + (255,), (0, 0, 0, 0)]
            alpha = None
            if kind == b"DXT5":
                a0, a1 = block[:2]
                alpha = [a0, a1]
                alpha += [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)] if a0 > a1 else [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
                alpha_bits = int.from_bytes(block[2:8], "little")
            for py in range(4):
                for px in range(4):
                    if bx + px >= width or by + py >= height:
                        continue
                    index = py * 4 + px
                    rgba = list(palette[indices >> (2 * index) & 3])
                    if kind == b"DXT3":
                        rgba[3] = ((int.from_bytes(block[:8], "little") >> (4 * index)) & 15) * 17
                    elif alpha is not None:
                        rgba[3] = alpha[alpha_bits >> (3 * index) & 7]
                    dest = ((by + py) * width + bx + px) * 4
                    out[dest:dest + 4] = bytes(rgba)
    return width, height, bytes(out)


def tga(image):
    width, height, pixels = image
    header = bytearray(18)
    header[2], header[16], header[17] = 2, 32, 40
    struct.pack_into("<HH", header, 12, width, height)
    out = bytearray(pixels)
    out[0::4], out[2::4] = pixels[2::4], pixels[0::4]
    return bytes(header) + bytes(out)


def image(name, data):
    if name.endswith(".dds"):
        return dds(data)
    # Pillow is optional author tooling, never shipped in the console application.
    need(name.endswith((".png", ".jpg")), "Unsupported image format")
    try:
        from PIL import Image
    except ModuleNotFoundError as error:
        raise ImportError("PNG/JPEG conversion requires Pillow on the authoring PC; DDS DXT1/3/5 works without dependencies") from error
    try:
        with Image.open(io.BytesIO(data)) as source:
            need(source.format in ("PNG", "JPEG") and 0 < source.width <= 4096 and 0 < source.height <= 4096 and source.width * source.height <= 4194304, "Image dimensions exceed limit")
            return source.width, source.height, source.convert("RGBA").tobytes()
    except (OSError, Image.DecompressionBombError) as error:
        raise ImportError("Invalid PNG/JPEG image") from error


def read_theme(data, allow_lite):
    files = container(data)
    need("info.json" in files, "Not an NXTheme: info.json is missing; raw firmware archives are unsupported")
    info = strict_json(files["info.json"])
    need(isinstance(info, dict) and type(info.get("Version")) is int and 1 <= info["Version"] <= 17, "Unsupported NXTheme version (1–17)")
    target = info.get("Target")
    need(isinstance(target, str) and target in TARGETS, "Unsupported NXTheme target")
    need(isinstance(info.get("ThemeName", ""), str) and len(info.get("ThemeName", "")) <= 160, "Invalid theme name")
    need(isinstance(info.get("Author", ""), str) and len(info.get("Author", "")) <= 160, "Invalid theme author")
    unsupported = []
    for name in files:
        lower = name.lower()
        need(not lower.endswith((".bflyt", ".bflan", ".bntx", ".nca", ".nsp", ".xci", ".nso", ".szs", ".keys", ".glsl", ".lua")), "Unsupported/proprietary executable or firmware asset: " + name)
        if name in ("layout.json", "common.json"):
            patch = strict_json(files[name])
            need(isinstance(patch, dict), "Invalid NXTheme layout patch")
            # These describe firmware pane/material/animation patches, not complete scenes.
            for key in ("Files", "Anims"):
                need(key not in patch or patch[key] is None or isinstance(patch[key], list), "Malformed layout patch")
            unsupported.append(name + ": firmware-specific panes/materials/animations are not converted")
        elif name not in ("info.json", "image.dds", "image.jpg", "preview.png", "Preview.png"):
            unsupported.append(name + ": icon/extra asset has no supported original-scene mapping")
    need(not unsupported or allow_lite, "Full import unavailable; use --lite to explicitly accept: " + "; ".join(unsupported))
    wallpaper = "image.dds" if "image.dds" in files else "image.jpg" if "image.jpg" in files else None
    need(wallpaper is not None, "Lite import requires a supported wallpaper; layout-only themes cannot be imported")
    decoded = image(wallpaper, files[wallpaper])
    need(decoded[:2] == (1280, 720), "NXTheme wallpaper must be 1280×720")
    return {"target": target, "name": info.get("ThemeName", "Imported theme"), "author": info.get("Author", ""),
            "version": info["Version"], "sha256": hashlib.sha256(data).hexdigest(), "mode": "lite",
            "unsupported": unsupported, "wallpaper": tga(decoded)}


def screen(target):
    nodes = [
        {"id": "wallpaper", "type": "image", "asset": "wallpaper-" + target, "w": 1920, "h": 1080},
        {"id": "scrim", "type": "rect", "w": 1920, "h": 1080, "color": 0, "alpha": 0.72},
        {"id": "title", "type": "text", "x": 140, "y": 140, "w": 1500, "size": 52, "text": TARGETS[target]},
        {"id": "origin", "type": "text", "x": 140, "y": 210, "w": 1500, "size": 24, "text": "ProsperoEden · Lite wallpaper import"},
        {"id": "hint", "type": "text", "x": 140, "y": 1000, "w": 1600, "text": "L1 / R1: theme parts · Back: library · Options: choose launcher"},
    ]
    focus = ""
    if target in ("home", "apps"):
        focus = "games"
        nodes.append({"id": "games", "type": "grid", "x": 140, "y": 300, "w": 1650, "h": 600,
                      "columns": 6, "rows": 1, "cell_w": 250, "cell_h": 310, "gap": 24, "source": "/library/games",
                      "activate": {"action": "launch.request", "args": {"game": {"$bind": "/selected/handle"}}},
                      "children": [{"id": "cover", "type": "cover", "w": 250, "h": 250, "bind": "/item/handle"},
                                   {"id": "name", "type": "text", "y": 295, "w": 245, "bind": "/item/name"}]})
    elif target in ("user", "psl"):
        focus = "profiles"
        nodes.append({"id": "profiles", "type": "list", "x": 140, "y": 300, "w": 1600, "h": 600,
                      "rows": 5, "cell_w": 1500, "cell_h": 90, "source": "/profiles",
                      "activate": {"action": "profile.choose", "args": {"handle": {"$bind": "/selected/handle"}}},
                      "children": [{"id": "name", "type": "text", "x": 24, "y": 54, "w": 1400, "size": 32, "bind": "/item/name"}]})
    elif target == "set":
        focus = "builtin"
        nodes.append({"id": "builtin", "type": "focus", "x": 140, "y": 330, "w": 1500, "h": 90,
                      "activate": {"action": "launcher.select", "args": {"id": "builtin"}},
                      "children": [{"id": "label", "type": "text", "x": 24, "y": 54, "w": 1400, "size": 32, "text": "Open built-in launcher settings"}]})
    else:
        nodes.append({"id": "notice", "type": "text", "x": 140, "y": 380, "w": 1500, "size": 32,
                      "text": "This is an original ProsperoEden scene, not the Switch HOME menu."})
    return {"nodes": nodes, "focus": focus, "transition": 0.2, "input": {"back": {"screen": "home"}}}


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def convert(source, output, theme_id, allow_lite=False):
    need(re.fullmatch(r"[a-z0-9-]{1,48}", theme_id) and theme_id != "builtin", "Invalid pack ID")
    need(source.is_file() and not source.is_symlink() and source.stat().st_size <= MAX_ARCHIVE, "Invalid/oversized source file")
    data = read_bounded(source, MAX_ARCHIVE)
    files = container(data)
    if "info.json" in files:
        themes = [read_theme(data, allow_lite)]
    else:
        need(source.suffix.lower() == ".zip", "Expected a theme or ZIP pack of .nxtheme files")
        need(0 < len(files) <= 7 and all(name.lower().endswith(".nxtheme") for name in files), "Packs must contain only up to seven .nxtheme files")
        themes = [read_theme(blob, allow_lite) for blob in files.values()]
    need(len({theme["target"] for theme in themes}) == len(themes), "Pack contains duplicate target parts")
    # One atomic rename publishes the complete, freshly generated folder. Never replace user data.
    need(not output.exists() and not output.is_symlink(), "Output already exists; choose a new folder")
    output.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=".scene-import-", dir=output.parent))
    try:
        (staging / "assets").mkdir()
        (staging / "screens").mkdir()
        label = themes[0]["name"].encode("utf-8")[:140].decode("utf-8", errors="ignore") + " — lite"
        manifest = {"schema": 1, "id": theme_id, "name": label, "host_contract": "launcher-v1.0",
                    "api": {"major": 1, "min_minor": 1}, "state_schema": 1,
                    "required_capabilities": ["themes.scene.v1", "actions.launch.v1"], "scene": "theme.json"}
        images, screens = {}, {}
        parts = [t for t in TARGETS if any(theme["target"] == t for theme in themes)]
        # Always provide a functional library recovery screen, even for single non-home parts.
        for theme in themes:
            target = theme["target"]
            (staging / "assets" / (target + ".tga")).write_bytes(theme.pop("wallpaper"))
            images["wallpaper-" + target] = "assets/" + target + ".tga"
        if "home" not in parts:
            images["wallpaper-home"] = images["wallpaper-" + parts[0]]
            parts.insert(0, "home")
        for index, target in enumerate(parts):
            model = screen(target)
            model["input"].update({"l1": {"screen": parts[(index - 1) % len(parts)]}, "r1": {"screen": parts[(index + 1) % len(parts)]}})
            screens[target] = "screens/" + target + ".json"
            write_json(staging / screens[target], model)
        write_json(staging / "theme.json", {"schema": 1, "id": theme_id, "name": manifest["name"], "entry": "home",
                                          "screens": screens, "images": images, "audio": {}})
        write_json(staging / "manifest.json", manifest)
        write_json(staging / "import-report.json", {"mode": "lite", "firmware_used": False, "parts": themes,
                                                    "limitations": ["No firmware layouts, animations, materials or icon mapping", "No Nintendo assets are supplied", "Imported artwork retains its author's license; do not redistribute without permission"]})
        need(sum(path.stat().st_size for path in staging.rglob("*") if path.is_file()) <= MAX_TOTAL, "Converted package exceeds limit")
        # The generated package uses only the supported v1 schema. Runtime validates again on load.
        staging.rename(output)
    finally:
        if staging.exists():
            shutil.rmtree(staging)
    return output


def install(source, data_root, checker):
    """Install an authored scene folder only after the native validator accepts the staging copy."""
    need(hasattr(os, "O_NOFOLLOW") and os.open in os.supports_dir_fd, "Safe installation requires POSIX no-follow descriptors (use the Docker authoring tools)")
    need(source.is_dir() and not source.is_symlink(), "Expected an unpacked scene folder")
    manifest = strict_json(read_bounded(source / "manifest.json", 512 * 1024))
    theme_id = manifest.get("id", "")
    need(re.fullmatch(r"[a-z0-9-]{1,48}", theme_id) and theme_id != "builtin", "Invalid pack ID")
    need(manifest.get("schema") == 1 and manifest.get("host_contract") == "launcher-v1.0" and
         manifest.get("api", {}).get("major") == 1 and manifest.get("api", {}).get("min_minor", 0) in (0, 1), "Incompatible launcher manifest")
    need(manifest.get("scene") == "theme.json" and "entry" not in manifest and "loading_scene" not in manifest and
         not manifest.get("assets") and not manifest.get("sounds") and not manifest.get("font"), "Expected a script-free scene pack")
    capabilities = manifest.get("required_capabilities", [])
    need("themes.scene.v1" in capabilities and set(capabilities) <= {"themes.scene.v1", "actions.launch.v1"}, "Unsupported capability")
    launchers = data_root / "launchers"
    launchers.mkdir(parents=True, exist_ok=True)
    need(not launchers.is_symlink(), "Launcher destination cannot be a symlink")
    target = launchers / theme_id
    need(not target.exists() and not target.is_symlink(), "Pack is installed; use the host's confirmed updater or a new ID")
    staging = Path(tempfile.mkdtemp(prefix=".scene-install-", dir=launchers))
    try:
        count = total = 0
        for directory, dirs, files in os.walk(source, followlinks=False):
            for name in dirs + files:
                path = Path(directory) / name
                mode = path.lstat().st_mode
                need(not path.is_symlink() and (stat.S_ISDIR(mode) or stat.S_ISREG(mode)), "Links/special files are forbidden")
                relative = path.relative_to(source)
                safe_name(relative.as_posix())
                need(len(relative.parts) <= 12, "Package directory depth exceeded")
                destination = staging / relative
                if path.is_dir():
                    destination.mkdir()
                else:
                    count += 1
                    need(count <= 256, "Package file-count limit exceeded")
                    # Never retain scripts/shaders or unrelated binary assets in installed scene packages.
                    need(path.suffix.lower() in (".json", ".tga", ".pefont", ".wav", ".md", ".txt"), "Unsupported package resource")
                    # Pin each source component, then copy from its verified regular-file FD.
                    # Never ask copyfile to create a symlink after a check/read race.
                    fd = os.open(source, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
                    try:
                        for part in relative.parts[:-1]:
                            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
                            os.close(fd)
                            fd = child
                        child = os.open(relative.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=fd)
                    finally:
                        os.close(fd)
                    with os.fdopen(child, "rb") as file:
                        details = os.fstat(file.fileno())
                        need(stat.S_ISREG(details.st_mode) and details.st_size <= MAX_ARCHIVE, "Invalid/oversized source resource")
                        total += details.st_size
                        need(total <= MAX_TOTAL, "Package size limit exceeded")
                        with destination.open("xb") as out:
                            remaining = details.st_size
                            while remaining:
                                chunk = file.read(min(remaining, 65536))
                                need(chunk, "Source resource changed during install")
                                out.write(chunk)
                                remaining -= len(chunk)
                            out.flush()
                            os.fsync(out.fileno())
        checked = strict_json(read_bounded(staging / "theme.json", 512 * 1024))
        need(checked.get("id") == theme_id, "Scene ID does not match manifest")
        subprocess.run([str(checker), str(staging)], check=True, timeout=60)
        staging.rename(target)
    finally:
        if staging.exists():
            shutil.rmtree(staging)
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    converter = sub.add_parser("convert")
    converter.add_argument("source", type=Path)
    converter.add_argument("output", type=Path)
    converter.add_argument("--id", required=True)
    converter.add_argument("--lite", action="store_true", help="Explicitly discard unsupported firmware layout/animation/icon data")
    installer = sub.add_parser("install")
    installer.add_argument("source", type=Path)
    installer.add_argument("data_root", type=Path)
    installer.add_argument("--validator", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = convert(args.source, args.output, args.id, args.lite) if args.command == "convert" else install(args.source, args.data_root, args.validator)
        print(result)
    except (ImportError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
