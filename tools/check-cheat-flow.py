#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused #74 cross-layer qualification; reuse ONLY read-only host archives, never run a build.

Requires the coordinator cache mounted at both /work and /root/.cache (read-only).
All fresh objects, derived sources, TLS material, logs and provenance go under --out.
The existing identity test's fixture-writer prefix is reused verbatim, hash-bound,
without its main or any identity implementation. Assertions remain enabled.
"""
import argparse
import hashlib
import http.server
import json
import os
from pathlib import Path
import shlex
import ssl
import subprocess
import sys
import threading
import time


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True, help='NEW directory on the private volume')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build, out = args.build.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    controls = out / 'http'
    controls.mkdir()
    commands = json.loads((build / 'compile_commands.json').read_text())
    log = (out / 'run.log').open('w', buffering=1)
    provenance = {'root': str(root), 'build': str(build), 'commands': [], 'inputs': {},
                  'archives_before': {}, 'archives_after': {}, 'result': 'FAILED'}

    def say(text):
        print(text, flush=True)
        log.write(text + '\n')

    def run(command, cwd=build, env=None, timeout=120, quiet=False):
        provenance['commands'].append({'cwd': str(cwd), 'argv': [str(a) for a in command]})
        # No assertion failures are hidden or reinterpreted as expected success.
        result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, timeout=timeout)
        if quiet:
            log.write(result.stdout)
        elif result.stdout:
            say(result.stdout.rstrip())
        result.check_returncode()
        return result.stdout

    def record(path):
        path = Path(path).resolve()
        value = digest(path)
        assert str(path) not in provenance['inputs'] or provenance['inputs'][str(path)] == value, 'Input changed during compilation: ' + str(path)
        provenance['inputs'][str(path)] = value

    def compile_file(suffix, source, name):
        record(source)
        entry = next(c for c in commands if c['file'].endswith(suffix))
        command = shlex.split(entry['command'])
        command[1:1] = ['-I' + str(out), '-I' + str(root / 'headless'), '-I' + str(root / 'third_party')]
        command[command.index(entry['file'])] = str(source)
        output = out / (name + '.o')
        command[command.index('-o') + 1] = str(output)
        if '-MF' in command:
            command[command.index('-MF') + 1] = str(out / (name + '.d'))
        command += ['-O1', '-UNDEBUG', '-MMD', '-MF', str(out / (name + '.d'))]
        say('compile ' + name)
        run(command)
        return output

    try:
        # These three files describe the cached compilation/link configuration; read only.
        for path in (build / 'compile_commands.json', build / 'build.ninja', Path(__file__),
                     root / 'headless/cheat_flow_check.cpp', root / 'headless/cheat_identity_check.cpp'):
            record(path)
        for path in (root / 'headless').rglob('*'):
            if path.suffix in ('.h', '.hpp'):
                record(path)
        link = run(['ninja', '-C', str(build), '-t', 'commands', 'eden-cheat-identity-check'], quiet=True).splitlines()[-1]
        parts = shlex.split(link)
        assert parts[:2] == [':', '&&'] and parts[-2:] == ['&&', ':']
        link_args = parts[2:-2]
        for arg in link_args:
            if arg.endswith(('.a', '.so')):
                path = (build / arg).resolve()
                provenance['archives_before'][str(path)] = digest(path)
        say('read-only archive hashes captured')

        # Same derivation as the actual CMake/helper, from the pinned original core source.
        original_path = build.parent / 'source/src/core/file_sys/program_metadata.cpp'
        original = original_path.read_text()
        anchor = 'Loader::ResultStatus ProgramMetadata::Load(VirtualFile file) {'
        assert original.count(anchor) == 1 and 'Eden::BoundNpdmCapabilities' not in original
        metadata = out / 'program_metadata.cpp'
        metadata.write_text('#include "' + str(root / 'headless/npdm_bounds.h') + '"\n' +
            original.replace(anchor, anchor + '\n    if (const auto status = Eden::BoundNpdmCapabilities(file); status != Loader::ResultStatus::Success) return status;'))
        record(original_path)
        fixture = (root / 'headless/cheat_identity_check.cpp').read_text()
        assert fixture.count('\nint main() {') == 1 and 'Bytes WithBktrRomFs(' in fixture
        prefix = fixture.split('\nint main() {')[0]
        assert prefix.endswith('} // namespace\n') and 'ProbeCheatIdentity(' not in prefix
        (out / 'cheat_identity_fixtures.h').write_text(prefix)
        objects = [compile_file('/program_metadata.cpp', metadata, 'metadata')]
        for name in ('cheat_identity', 'cheat_probe', 'cheat_manager', 'cheat_store', 'cheat_catalog', 'cheat_download'):
            # The full core include configuration is needed by identity/probe; using it
            # for the portable job/store code too preserves the cached ABI/toolchain.
            objects.append(compile_file('/cheat_identity.cpp', root / 'headless' / (name + '.cpp'), name))
        http_object = out / 'http.o'
        record(root / 'headless/remote/http.c')
        run(['clang-18', '-std=c11', '-I' + str(root / 'headless/update_check'), '-MMD', '-MF',
             str(out / 'http.d'), '-c', str(root / 'headless/remote/http.c'), '-o', str(http_object)])
        objects.append(http_object)

        def link_check(source, name, identity_only=False):
            main_object = compile_file('/cheat_identity_check.cpp', source, name)
            command = list(link_args)
            index = next(i for i, a in enumerate(command) if a.endswith('/cheat_identity_check.cpp.o'))
            linked_objects = objects[:3] if identity_only else objects
            command[index:index + 1] = [str(p) for p in (*linked_objects, main_object)]
            binary = out / name
            command[command.index('-o') + 1] = str(binary)
            command = [a if not a.startswith('--dependency-file=') else '--dependency-file=' + str(out / (name + '-link.d')) for a in command]
            command += ['-lcurl', '-pthread']
            say('link ' + name + ' against read-only real archives')
            run(command)
            return binary

        # Keep the prerequisite wrong-key/BKTR mixed-byte controls runnable as well.
        identity = link_check(root / 'headless/cheat_identity_check.cpp', 'identity-check', identity_only=True)
        run([str(identity)], cwd=out, timeout=90)
        binary = link_check(root / 'headless/cheat_flow_check.cpp', 'flow-check')
        for depfile in out.glob('*.d'):
            if '-link' in depfile.name:
                continue
            for dep in shlex.split(depfile.read_text().replace('\\\n', '').split(':', 1)[1]):
                record(build / dep)
        # Snapshot exact fresh sources/headers BEFORE running the test.
        (out / 'source-before.json').write_text(json.dumps(provenance['inputs'], sort_keys=True, indent=2) + '\n')
        run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1', '-subj', '/CN=localhost',
             '-addext', 'subjectAltName=DNS:localhost', '-keyout', str(out / 'tls-key.pem'), '-out', str(out / 'ca.pem')], quiet=True)

        def body(title='0100000000001000', ids=(5, 6, 7, 8, 13, 14, 15, 16, 24)):
            return json.dumps({title: {(bytes([n]) + bytes(6) + b'\x77').hex(): {'a' * 64: {
                'title': '[Local optional]', 'source': '04000000 00000000 00000001'}} for n in ids}}).encode()

        requests, server_errors = [], []
        lock = threading.Lock()

        class Handler(http.server.BaseHTTPRequestHandler):
            protocol_version = 'HTTP/1.1'

            def log_message(self, *_):
                pass

            def do_GET(self):
                with lock:
                    requests.append(self.path)
                    with (controls / 'requests.log').open('a') as stream:
                        stream.write(self.path + '\n')
                data = body('0100000000001001') if self.path == '/wrong-title' else body(ids=(254,)) if self.path == '/wrong-build' else body()
                try:
                    self.send_response(200)
                    self.send_header('Content-Length', str(len(data)))
                    self.end_headers()
                    if self.path.startswith('/hold/'):
                        name = self.path.rsplit('/', 1)[1]
                        self.wfile.write(data[:1]); self.wfile.flush()
                        (controls / (name + '.entered')).touch()
                        until = time.monotonic() + 30
                        while not (controls / (name + '.release')).exists():
                            if time.monotonic() >= until:
                                server_errors.append('gate timed out: ' + name)
                                return
                            time.sleep(0.002)
                        self.wfile.write(data[1:])
                    else:
                        self.wfile.write(data)
                except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
                    # The deliberately cancelled client may have closed its socket.
                    if self.path != '/hold/cancel':
                        raise

        server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(out / 'ca.pem', out / 'tls-key.pem')
        server.socket = context.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        env = {k: v for k, v in os.environ.items() if k.lower() not in ('http_proxy', 'https_proxy', 'all_proxy')}
        env.update(CHEAT_TEST_CA=str(out / 'ca.pem'), NO_PROXY='localhost,127.0.0.1')
        try:
            run([str(binary), f'https://localhost:{server.server_port}', str(out / 'runtime'), str(controls)],
                cwd=out, env=env, timeout=180)
        finally:
            server.shutdown(); server.server_close(); thread.join()
        assert not server_errors, server_errors
        assert sorted(requests) == sorted(['/ok'] * 4 + ['/wrong-title', '/wrong-build'] +
            ['/hold/' + n for n in ('update', 'mod', 'title', 'full-build', 'cancel')]), requests
        provenance['http_requests'] = requests
        for path, expected in provenance['inputs'].items():
            assert digest(path) == expected, 'Source changed during qualification: ' + path
        provenance['result'] = 'PASS'
        say('Cheat flow PASS: actual current sources, guarded real NPDM reader, real core archives and loopback libcurl; all assertions executed')
    finally:
        for path, before in provenance['archives_before'].items():
            provenance['archives_after'][path] = digest(path)
            if provenance['archives_after'][path] != before:
                provenance['result'] = 'FAILED: archive changed'
        source_hash = hashlib.sha256(json.dumps(provenance['inputs'], sort_keys=True, separators=(',', ':')).encode()).hexdigest()
        provenance['source_manifest_sha256'] = source_hash
        (out / 'provenance.json').write_text(json.dumps(provenance, sort_keys=True, indent=2) + '\n')
        say('Source manifest SHA-256: ' + source_hash)
        say('Exact commands/hashes: ' + str(out / 'provenance.json'))
        log.close()
        assert provenance['archives_before'] == provenance['archives_after'], 'Read-only archive changed'


if __name__ == '__main__':
    main()
