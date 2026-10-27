#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline tests: no public service, tag files or third-party artwork used."""
import importlib.util
import json
import os
from pathlib import Path
import struct
import tempfile
import urllib.request
from unittest.mock import patch
import zlib

spec = importlib.util.spec_from_file_location('amiibo_sync', Path(__file__).with_name('sync-amiibo.py'))
sync = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sync)


def rejects(action):
    try:
        action()
    except (ValueError, OSError, zlib.error):
        return
    raise AssertionError('Invalid source/input was accepted')


def picture(rows=b'\0\xff\0\0\xff', width=1, height=1):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


identifier = '0000000000000002'
source = 'https://catalog.example/api/amiibo/'
template = 'https://images.example/icon_{head}-{tail}.png'
catalog = json.dumps({'amiibo': [{'head': identifier[:8], 'tail': identifier[8:], 'name': 'Synthetic',
                                'image': 'http://private.invalid/tag.bin'}]}).encode()
png = picture()
sync.png(png)
assert len(sync.catalog(catalog)) == 1
assert len(sync.catalog(b'{"amiibos":{"0x0000000000000002":{"name":"Synthetic"}}}')) == 1
for value in (b'{}', b'[]', b'{', b'{"amiibo":[],"amiibo":[]}', b' ' * (sync.CATALOG_LIMIT + 1),
              b'[' * 18 + b'0' + b']' * 18,
              b'{"amiibos":{"0000000000000a02":{"name":"A"},"0000000000000A02":{"name":"B"}}}',
              b'{"amiibo":[{"head":"00000000","tail":"00000002","name":"bad\\nname"}]}'):
    rejects(lambda: sync.catalog(value))
for value in (png[:40], png[:-1], b'not a PNG', png + b'junk', picture(b'\0' * (20 * 1024 * 1024)),
              picture(width=1025), picture(width=0), picture(b'\x05\0\0\0\0')):
    rejects(lambda: sync.png(value))
damaged = bytearray(png); damaged[-1] ^= 1
rejects(lambda: sync.png(damaged))
for url in ('http://catalog.example/', 'file:///tag.bin', 'https://user:secret@catalog.example/',
            'https://catalog.example/#tag', 'https://catalog.example:bad/', 'https://catalog.example/\n'):
    rejects(lambda: sync.endpoint(url))
redirect = sync.Redirect()
request = urllib.request.Request(source)
for url in ('http://catalog.example/api', 'https://other.example/api'):
    rejects(lambda: redirect.redirect_request(request, None, 302, '', {}, url))
assert redirect.redirect_request(request, None, 302, '', {}, source + 'next').full_url == source + 'next'


class Response:
    def __init__(self, body, headers=None, status=200, url=source):
        self.body, self.headers, self.status, self.url = body, headers or {}, status, url
        self.position = 0

    def __enter__(self):
        return self

    def __exit__(self, *_):
        pass

    def read(self, size):
        chunk = self.body[self.position:self.position + size]
        self.position += len(chunk)
        return chunk


def transfer(response, limit):
    class Opener:
        def open(self, request, timeout):
            assert request.full_url == source and timeout == 20
            assert request.get_header('Accept-encoding') == 'identity'
            return response
    with patch.object(urllib.request, 'build_opener', return_value=Opener()):
        return sync.fetch(source, limit)


assert transfer(Response(catalog), len(catalog)) == catalog
for response in (Response(catalog, {'Content-Length': '1'}), Response(catalog),
                 Response(b'', {'Content-Length': '99999999'}), Response(b'', {'Content-Length': 'bad'}),
                 Response(b'', status=206), Response(b'', url='https://other.example/api')):
    rejects(lambda: transfer(response, len(catalog) - 1))
rejects(lambda: transfer(Response(catalog, {'Content-Length': str(len(catalog) + 1)}), len(catalog) + 1))
with patch.object(sync.time, 'monotonic', side_effect=[0, 61]):
    rejects(lambda: transfer(Response(catalog), len(catalog)))

with tempfile.TemporaryDirectory(prefix='eden-amiibo-sync-') as temporary:
    root = Path(temporary)
    folder = root / 'amiibo'
    calls = []

    def get(url, limit):
        calls.append((url, limit))
        assert url in (source, 'https://images.example/icon_00000000-00000002.png')
        return catalog if url == source else png

    assert sync.sync(folder, source, template, [identifier], get=get) == (1, 1)
    image = folder / 'images/icon_00000000-00000002.png'
    assert image.read_bytes() == png and (folder / 'amiibo.json').read_bytes() == catalog
    assert calls == [(source, sync.CATALOG_LIMIT), ('https://images.example/icon_00000000-00000002.png', sync.IMAGE_LIMIT)]
    calls.clear()
    assert sync.sync(folder, source, template, [identifier, identifier], get=get) == (1, 0)
    assert calls == [(source, sync.CATALOG_LIMIT)] # valid image cache; no tag/URL in metadata was followed
    assert sync.sync(folder, source, template, [identifier], True, get=get) == (1, 1)
    before = {path: path.read_bytes() for path in folder.rglob('*') if path.is_file()}
    rejects(lambda: sync.sync(folder, source, template, ['f' * 16], get=get))
    rejects(lambda: sync.sync(folder, source, template, [identifier], True,
                             get=lambda url, limit: catalog if url == source else b'bad image'))
    rejects(lambda: sync.sync(folder, source, template, get=lambda *_: b'broken catalog'))
    rejects(lambda: sync.sync(folder, source, template, [f'{n:016x}' for n in range(65)], get=get))
    rejects(lambda: sync.sync(folder, source, 'https://images.example/{head.__class__}.png', get=get))
    assert before == {path: path.read_bytes() for path in folder.rglob('*') if path.is_file()}
    outside = root / 'outside'; outside.mkdir()
    alias = root / 'alias'; alias.symlink_to(outside, target_is_directory=True)
    rejects(lambda: sync.sync(alias / 'amiibo', source, template, get=get))
    image.unlink(); image.symlink_to(outside / 'image.png')
    rejects(lambda: sync.sync(folder, source, template, [identifier], True, get=get))
    assert not list(outside.iterdir())
    assert not list(folder.rglob('*.bin')) and not list(folder.rglob('*.nfc'))
    assert not list(folder.rglob('.amiibo-*'))
    if hasattr(os, 'mkfifo'):
        image.unlink(); os.mkfifo(image)
        rejects(lambda: sync.sync(folder, source, template, [identifier], get=get))

print('Amiibo PC sync PASS: opt-in metadata/selected images, strict IDs/JSON/HTTPS/redirects, bounded PNG streams, cache and failed-request preservation; no tag reads or downloads')
