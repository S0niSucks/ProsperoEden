#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused C++ worker checks + actual remote/http libcurl over private loopback TLS. Docker only."""
from pathlib import Path
import http.server
import json
import os
import ssl
import subprocess
import tempfile
import threading
import time
import binascii
import struct
import zlib

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='amiibo-sync-') as tmp:
    folder = Path(tmp)
    sources = [str(root / 'headless' / name) for name in ('amiibo_sync_check.cpp', 'amiibo_sync.cpp',
               'amiibo_sync_format.cpp', 'amiibo_sync_http.cpp')]
    common = ['clang++-18', '-std=c++20', '-O1', '-g', '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
              *sources, str(folder / 'http.o'), '-lcurl', '-lz', '-pthread']
    # Compile the unchanged reader methods, not a fake NFC/core header or layout implementation.
    # The excluded functions are AmiiboModelId/DescribeAmiibo (already covered by the core suite).
    reader = (root / 'headless/amiibo_catalog.cpp').read_text().split('std::string AmiiboModelId(', 1)[0]
    reader = reader.replace('#include "file_amiibo.h"', '#include "amiibo_file.h"\n#include <filesystem>')
    reader = reader.replace('#include "core/hle/service/nfc/common/amiibo_crypto.h"', '')
    (folder / 'reader.cpp').write_text(reader + '} // namespace Eden\n')
    for flags in ([], ['-fsanitize=thread', '-fno-omit-frame-pointer'],
                  ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']):
        subprocess.run(['clang-18', '-c', str(root / 'headless/remote/http.c'),
                        '-I', str(root / 'headless/update_check'), *flags, '-o', str(folder / 'http.o')], check=True)
        subprocess.run([*common, *flags, '-o', str(folder / 'check')], check=True)
        subprocess.run([str(folder / 'check')], check=True, timeout=40)
        subprocess.run(['clang++-18', '-std=c++20', '-O1', '-g', '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-function', '-I', str(root / 'headless'), str(folder / 'reader.cpp'),
                        str(root / 'headless/amiibo_sync_catalog_check.cpp'), *flags, '-o', str(folder / 'reader')], check=True)
        subprocess.run([str(folder / 'reader')], check=True, timeout=20)
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
                    '-subj', '/CN=localhost', '-addext', 'subjectAltName=DNS:localhost',
                    '-keyout', str(folder / 'key.pem'), '-out', str(folder / 'ca.pem')],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    catalog = json.dumps({'amiibo': [{'head': '00000000', 'tail': f'{n:08x}', 'name': f'Synthetic {n}'}
                                   for n in (1, 2)]}).encode()

    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', binascii.crc32(kind + body))

    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(b'\0\x20\x40\x60\xff')) + chunk(b'IEND', b'')

    class Handler(http.server.BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        def log_message(self, *_):
            pass

        def do_GET(self):
            try:
                path = self.path
                if path == '/stall':
                    time.sleep(3)
                if path in ('/redirect', '/cross', '/downgrade', '/loop'):
                    self.send_response(302)
                    dest = {'/redirect': '/ok', '/cross': f'https://127.0.0.1:{self.server.server_port}/ok',
                            '/downgrade': 'http://localhost:1/no', '/loop': '/loop'}[path]
                    self.send_header('Location', dest)
                    self.send_header('Content-Length', '0')
                    self.end_headers()
                    return
                self.send_response(404 if path == '/missing' else 206 if path == '/partial' else 200)
                data = png if path.endswith('.png') else b'{' if path == '/bad' else catalog
                if path == '/chunked':
                    self.send_header('Transfer-Encoding', 'chunked')
                    self.end_headers()
                    for offset in range(0, len(data), 9):
                        part = data[offset:offset + 9]
                        self.wfile.write(f'{len(part):x}\r\n'.encode() + part + b'\r\n')
                    self.wfile.write(b'0\r\n\r\n')
                else:
                    length = 2 * 1024 * 1024 + 1 if path == '/large' else len(data) + (path == '/truncated')
                    self.send_header('Content-Length', str(length))
                    self.end_headers()
                    self.wfile.write(data)
                    if path in ('/large', '/truncated'):
                        self.close_connection = True
            except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
                pass

    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(folder / 'ca.pem', folder / 'key.pem')
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    env = dict(os.environ, AMIIBO_TEST_CA=str(folder / 'ca.pem'), NO_PROXY='localhost,127.0.0.1')
    try:
        base = f'https://localhost:{server.server_port}'
        for path in ('/ok', '/redirect', '/chunked'):
            subprocess.run([str(folder / 'check'), base + path, 'ready', '0'], check=True, env=env, timeout=15)
        for path in ('/cross', '/downgrade', '/loop', '/missing', '/partial', '/bad', '/large', '/truncated'):
            subprocess.run([str(folder / 'check'), base + path, 'failed', '0'], check=True, env=env, timeout=15)
        subprocess.run([str(folder / 'check'), base + '/stall', 'cancelled', '1'], check=True, env=env, timeout=10)
        subprocess.run([str(folder / 'check'), base.replace('localhost', '127.0.0.1') + '/ok', 'failed', '0'], check=True, env=env, timeout=10)
        untrusted = dict(env, AMIIBO_TEST_CA='/etc/ssl/certs/ca-certificates.crt')
        subprocess.run([str(folder / 'check'), base + '/ok', 'failed', '0'], check=True, env=untrusted, timeout=10)
    finally:
        server.shutdown()
        server.server_close()
        thread.join()
    print('Amiibo real TLS worker PASS: catalog/photo -> validated cache -> offline; same-origin redirect, cross-origin/downgrade/loop refusal, trusted/untrusted/wrong-host TLS, chunked/truncated/oversized/error responses, cancellation')
