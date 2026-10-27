#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic downloads only. Real libcurl over loopback TLS, without changing host CA trust."""
from pathlib import Path
import http.server
import json
import os
import ssl
import subprocess
import tempfile
import threading
import time

root = Path(__file__).resolve().parents[1]
cxx = os.environ.get('CXX', 'clang++-18')
with tempfile.TemporaryDirectory(prefix='eden-cheat-download-') as tmp:
    folder = Path(tmp)
    sources = [str(root / 'headless' / name) for name in
               ('cheat_download_check.cpp', 'cheat_download.cpp', 'cheat_catalog.cpp')]
    common = [cxx, '-std=c++20', '-O1', '-g', '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
              '-I', str(root / 'headless'), *sources, '-pthread']
    for sanitizer in ('', 'address,undefined'):
        flags = ['-fsanitize=' + sanitizer, '-fno-omit-frame-pointer'] if sanitizer else []
        subprocess.run([*common, *flags, '-o', str(folder / 'unit')], check=True)
        subprocess.run([str(folder / 'unit')], check=True, timeout=60)

    subprocess.run(['clang-18', '-c', str(root / 'headless/remote/http.c'),
                    '-I', str(root / 'headless/update_check'), '-o', str(folder / 'http.o')], check=True)
    subprocess.run([*common, '-DCHEAT_HTTP_REAL', str(root / 'headless/cheat_store.cpp'),
                    str(root / 'headless/cheat_manager.cpp'),
                    str(folder / 'http.o'), '-lcurl', '-lcrypto', '-o', str(folder / 'tls')], check=True)
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
                    '-subj', '/CN=localhost', '-addext', 'subjectAltName=DNS:localhost',
                    '-keyout', str(folder / 'key.pem'), '-out', str(folder / 'ca.pem')],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    body = json.dumps({'0100000000001234': {'ab00000000000000': {'a' * 64: {
        'title': '[Local test]', 'source': '04000000 00000000 00000001'}}}}).encode()

    class Handler(http.server.BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        def log_message(self, *_):
            pass

        def do_GET(self):
            try:
                path = self.path
                if path == '/stall':
                    time.sleep(2)
                if path in ('/redirect', '/downgrade', '/loop'):
                    self.send_response(302)
                    dest = '/ok' if path == '/redirect' else ('http://localhost:1/no' if path == '/downgrade' else '/loop')
                    self.send_header('Location', dest)
                    self.send_header('Content-Length', '0')
                    self.end_headers()
                    return
                self.send_response(404 if path == '/missing' else (206 if path == '/partial' else 200))
                data = b'{' if path == '/bad' else body
                if path == '/chunked':
                    self.send_header('Transfer-Encoding', 'chunked')
                    self.end_headers()
                    for offset in range(0, len(data), 7):
                        chunk = data[offset:offset + 7]
                        self.wfile.write(f'{len(chunk):x}\r\n'.encode() + chunk + b'\r\n')
                    self.wfile.write(b'0\r\n\r\n')
                else:
                    size = 16 * 1024 * 1024 + 1 if path == '/oversize' else len(data) + (1 if path == '/truncated' else 0)
                    self.send_header('Content-Length', str(size))
                    self.end_headers()
                    self.wfile.write(data)
                    if path in ('/oversize', '/truncated'):
                        self.close_connection = True
            except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
                pass

    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(folder / 'ca.pem', folder / 'key.pem')
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    env = dict(os.environ, CHEAT_TEST_CA=str(folder / 'ca.pem'), NO_PROXY='localhost,127.0.0.1')
    try:
        base = f'https://localhost:{server.server_port}'
        for path in ('/ok', '/chunked', '/redirect'):
            subprocess.run([str(folder / 'tls'), base + path, '3', '0'], check=True, env=env, timeout=10)
        for path in ('/downgrade', '/loop', '/missing', '/partial', '/bad', '/oversize', '/truncated'):
            subprocess.run([str(folder / 'tls'), base + path, '0', '0'], check=True, env=env, timeout=10)
        subprocess.run([str(folder / 'tls'), base + '/stall', '1', '1'], check=True, env=env, timeout=10)
        # Same CA, wrong host; then an untrusted certificate. Verification must stay on.
        subprocess.run([str(folder / 'tls'), base.replace('localhost', '127.0.0.1') + '/ok', '0', '0'], check=True, env=env, timeout=10)
        untrusted = dict(env, CHEAT_TEST_CA='/etc/ssl/certs/ca-certificates.crt')
        subprocess.run([str(folder / 'tls'), base + '/ok', '0', '0'], check=True, env=untrusted, timeout=10)
    finally:
        server.shutdown()
        server.server_close()
        thread.join()
    print('Cheat HTTP PASS: actual worker manager/libcurl loopback TLS -> parser -> offline cache -> default-OFF install, redirects, downgrade/loop refusal, trusted/untrusted/wrong-host certificates, chunked/truncated/error responses and cancellation')
