#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Opt-in PC preparation of public Amiibo metadata/images. Never reads or downloads tags."""
import argparse
import binascii
import json
import os
from pathlib import Path
import re
import stat
import struct
import string
import tempfile
import time
import urllib.parse
import urllib.request
import zlib

CATALOG_LIMIT = 2 * 1024 * 1024
IMAGE_LIMIT = 4 * 1024 * 1024
CATALOG_URL = 'https://amiiboapi.org/api/amiibo/'
IMAGE_TEMPLATE = 'https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_{head}-{tail}.png'


def identifier(value):
    value = value.removeprefix('0x').lower()
    if not re.fullmatch('[0-9a-f]{16}', value):
        raise ValueError('A model ID must contain exactly 16 hexadecimal digits')
    return value


def endpoint(url):
    parts = urllib.parse.urlsplit(url)
    if (parts.scheme != 'https' or not parts.hostname or parts.username is not None
            or parts.password is not None or parts.fragment or any(ord(c) <= 32 for c in url)):
        raise ValueError('Sources must be HTTPS URLs without credentials or fragments')
    # Parsing port also rejects malformed authorities before a request can be sent.
    return parts.hostname.lower(), parts.port or 443


class Redirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, url):
        # A catalog/image server cannot redirect requests to HTTP or another authority.
        if endpoint(url) != endpoint(request.full_url):
            raise ValueError('Cross-origin redirects are not allowed; configure the final source URL')
        return super().redirect_request(request, fp, code, message, headers, url)


def fetch(url, limit):
    endpoint(url)
    request = urllib.request.Request(url, headers={'User-Agent': 'ProsperoEden-Amiibo-Setup/1',
                                                  'Accept-Encoding': 'identity'})
    with urllib.request.build_opener(Redirect()).open(request, timeout=20) as response:
        if response.status != 200 or endpoint(response.url) != endpoint(url):
            raise ValueError('The source did not return a complete same-origin response')
        length = response.headers.get('Content-Length')
        if length is not None and (not length.isdigit() or int(length) > limit):
            raise ValueError('Response length exceeds the download limit')
        # Do not trust Content-Length; chunked and dishonest responses have the same cap.
        result = bytearray()
        deadline = time.monotonic() + 60
        while True:
            if time.monotonic() >= deadline:
                raise ValueError('Download time limit exceeded')
            chunk = response.read(min(65536, limit + 1 - len(result)))
            if not chunk:
                break
            result.extend(chunk)
            if len(result) > limit:
                raise ValueError('Response body exceeds the download limit')
        if length is not None and len(result) != int(length):
            raise ValueError('Response was shorter than its declared length')
        return bytes(result)


def catalog(encoded):
    if not encoded or len(encoded) > CATALOG_LIMIT:
        raise ValueError('Catalog size is invalid')

    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('Duplicate catalog key')
            result[key] = value
        return result

    # Check nesting/events before the JSON decoder builds a potentially adversarial tree.
    depth = events = 0
    quoted = escaped = False
    for c in encoded:
        if quoted:
            if escaped:
                escaped = False
            elif c == 92:
                escaped = True
            elif c == 34:
                quoted = False
            continue
        if c == 34:
            quoted = True
        if c in (91, 123):
            depth += 1
        if c in (93, 125):
            depth -= 1
        events += c in (34, 44, 58, 91, 93, 123, 125)
        if depth > 16 or events > 65536:
            raise ValueError('Catalog nesting/event limit exceeded')
    def constant(_):
        raise ValueError('Invalid JSON constant')

    data = json.loads(encoded.decode('utf-8'), object_pairs_hook=unique, parse_constant=constant)
    if not isinstance(data, dict):
        raise ValueError('Catalog must be a JSON object')

    def label(value):
        return (isinstance(value, str) and 0 < len(value.encode('utf-8')) <= 128
                and not any(ord(c) < 32 or ord(c) == 127 for c in value))

    entries = {}
    if isinstance(data.get('amiibos'), dict):
        rows = ((key, item) for key, item in data['amiibos'].items())
    else:
        items = data.get('amiibo')
        if isinstance(items, dict):
            items = [items]
        if not isinstance(items, list) or len(items) > 4096:
            raise ValueError('Unsupported catalog schema')
        pairs = []
        for item in items:
            if (not isinstance(item, dict) or not isinstance(item.get('head'), str)
                    or not isinstance(item.get('tail'), str) or len(item['head']) != 8 or len(item['tail']) != 8):
                raise ValueError('Invalid API model ID')
            pairs.append((item['head'] + item['tail'], item))
        rows = pairs
    count = 0
    for key, item in rows:
        count += 1
        if count > 4096 or not isinstance(item, dict):
            raise ValueError('Catalog item limit or format is invalid')
        key = identifier(key)
        if key in entries or not label(item.get('name')):
            raise ValueError('Duplicate model ID or invalid catalog label')
        entries[key] = item
    if not entries:
        raise ValueError('Catalog is empty')
    return entries


def png(encoded):
    """Validate bounded PNG transport/scanlines; the console still uses its real PNG decoder."""
    if len(encoded) < 45 or len(encoded) > IMAGE_LIMIT or encoded[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Image is not a bounded PNG')
    position = 8
    ihdr = ended = saw_data = data_ended = False
    palette = transparency = False
    palette_entries = 0
    packed = bytearray()
    expected = 0
    rows = []
    while position < len(encoded):
        if len(encoded) - position < 12:
            raise ValueError('Truncated PNG chunk')
        size, = struct.unpack_from('>I', encoded, position)
        end = position + 12 + size
        if end > len(encoded):
            raise ValueError('Truncated PNG chunk data')
        kind = encoded[position + 4:position + 8]
        if not re.fullmatch(b'[A-Za-z]{4}', kind) or kind[2] & 32:
            raise ValueError('Invalid PNG chunk name')
        body = encoded[position + 8:end - 4]
        crc, = struct.unpack_from('>I', encoded, end - 4)
        if binascii.crc32(kind + body) != crc:
            raise ValueError('PNG CRC mismatch')
        if not ihdr and kind != b'IHDR':
            raise ValueError('PNG must start with IHDR')
        if kind == b'IHDR':
            if ihdr or size != 13:
                raise ValueError('Invalid PNG header')
            width, height, bits, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', body)
            allowed = {0: (1, 2, 4, 8, 16), 2: (8, 16), 3: (1, 2, 4, 8), 4: (8, 16), 6: (8, 16)}
            if (not 1 <= width <= 1024 or not 1 <= height <= 1024 or bits not in allowed.get(color, ())
                    or compression or filtering or interlace not in (0, 1)):
                raise ValueError('Unsupported PNG dimensions or format')
            channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
            passes = [(0, 0, 1, 1)] if not interlace else [
                (0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4),
                (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]
            for x, y, dx, dy in passes:
                w, h = max(0, (width - x + dx - 1) // dx), max(0, (height - y + dy - 1) // dy)
                if w and h:
                    stride = (w * channels * bits + 7) // 8 + 1
                    rows.extend([stride] * h)
                    expected += stride * h
            ihdr = True
        elif kind == b'IDAT':
            if data_ended or (color == 3 and not palette):
                raise ValueError('Non-contiguous PNG data')
            saw_data = True
            packed.extend(body)
        elif kind == b'PLTE':
            if palette or saw_data or color in (0, 4) or not size or size % 3 or size > 768:
                raise ValueError('Invalid PNG palette')
            palette = True
            palette_entries = size // 3
            if color == 3 and palette_entries > 1 << bits:
                raise ValueError('PNG palette exceeds its bit depth')
        elif kind == b'tRNS':
            if (transparency or saw_data or color in (4, 6) or
                    (color == 0 and size != 2) or (color == 2 and size != 6) or
                    (color == 3 and (not palette or not 0 < size <= palette_entries))):
                raise ValueError('Invalid PNG transparency')
            transparency = True
        elif kind == b'IEND':
            if size or not saw_data or end != len(encoded):
                raise ValueError('Invalid PNG ending')
            ended = True
        elif not kind[0] & 32:
            raise ValueError('Unknown critical PNG chunk')
        if saw_data and kind != b'IDAT':
            data_ended = True
        position = end
    if not ended:
        raise ValueError('Missing PNG ending')
    decoder = zlib.decompressobj()
    pixels = decoder.decompress(packed, expected + 1)
    if len(pixels) != expected or not decoder.eof or decoder.unconsumed_tail or decoder.unused_data:
        raise ValueError('Invalid or oversized PNG compressed data')
    offset = 0
    for stride in rows:
        if pixels[offset] > 4:
            raise ValueError('Invalid PNG row filter')
        offset += stride


def safe_path(path):
    # Windows junctions and other reparse points count as links too. The user owns this output
    # directory: do not concurrently rename its ancestors while this PC setup tool is running.
    path = Path(os.path.abspath(path))
    for component in reversed((path, *path.parents)):
        try:
            info = component.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400:
            raise ValueError('Output paths must not contain symlinks, junctions or reparse points')
    return path


def atomic_write(path, encoded):
    path = safe_path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    safe_path(path)
    if path.exists() and not stat.S_ISREG(path.lstat().st_mode):
        raise ValueError('A cache target must be a regular file')
    name = None
    try:
        with tempfile.NamedTemporaryFile(prefix='.amiibo-', dir=path.parent, delete=False) as file:
            name = file.name
            file.write(encoded)
            file.flush()
            os.fsync(file.fileno())
        safe_path(path)
        os.replace(name, path)
    finally:
        if name and os.path.exists(name):
            os.unlink(name)


def sync(folder, source=CATALOG_URL, image_template=IMAGE_TEMPLATE, ids=(), refresh_images=False, get=fetch):
    folder = safe_path(folder)
    ids = sorted({identifier(value) for value in ids})
    if len(ids) > 64:
        raise ValueError('At most 64 explicitly selected images per run')
    fields = [field for _, field, _, _ in string.Formatter().parse(image_template) if field is not None]
    if any(field not in ('head', 'tail') for field in fields):
        raise ValueError('Image templates support only {head} and {tail}')
    endpoint(source)
    endpoint(image_template.format(head='00000000', tail='00000002'))
    safe_path(folder / 'amiibo.json')
    encoded = get(source, CATALOG_LIMIT)
    entries = catalog(encoded)
    if any(value not in entries for value in ids):
        raise ValueError('A requested model ID is not in the catalog; nothing was changed')
    # Metadata never supplies request URLs or filenames. Only the user's source/template and IDs do.
    images = []
    for value in ids:
        image = safe_path(folder / 'images' / f'icon_{value[:8]}-{value[8:]}.png')
        if image.exists() and not refresh_images:
            if not stat.S_ISREG(image.lstat().st_mode):
                raise ValueError('An image cache entry must be a regular file')
            descriptor = os.open(image, os.O_RDONLY | getattr(os, 'O_NONBLOCK', 0) | getattr(os, 'O_NOFOLLOW', 0)
                                 | getattr(os, 'O_BINARY', 0))
            with os.fdopen(descriptor, 'rb') as file:
                if not stat.S_ISREG(os.fstat(file.fileno()).st_mode):
                    raise ValueError('An image cache entry must remain a regular file')
                cached = file.read(IMAGE_LIMIT + 1)
            try:
                png(cached)
                continue
            except (ValueError, zlib.error):
                pass
        url = image_template.format(head=value[:8], tail=value[8:])
        content = get(url, IMAGE_LIMIT)
        png(content)
        images.append((image, content))
    # Fetch and validate the complete requested batch before replacing any existing cache.
    # Individual replacements are atomic; an I/O failure is not a cross-file transaction.
    for path, content in images:
        atomic_write(path, content)
    atomic_write(folder / 'amiibo.json', encoded)
    return len(entries), len(images)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path, help='Local amiibo/ folder to prepare or refresh')
    parser.add_argument('--catalog-url', default=CATALOG_URL)
    parser.add_argument('--image-template', default=IMAGE_TEMPLATE)
    parser.add_argument('--id', action='append', default=[], help='Public 16-digit model ID; repeat for photos (max 64)')
    parser.add_argument('--refresh-images', action='store_true', help='Fetch selected images even when a valid cache exists')
    args = parser.parse_args()
    try:
        entries, images = sync(args.folder, args.catalog_url, args.image_template, args.id, args.refresh_images)
    except (OSError, ValueError, zlib.error) as error:
        parser.exit(1, f'Amiibo sync failed: {error}\n')
    print(f'Prepared {entries} public model names; downloaded {images} selected photos. No tags or keys read or downloaded.')


if __name__ == '__main__':
    main()
