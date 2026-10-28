#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile actual Nextendo core derivatives against the saved real host configuration.

No emulator build/configure, guest execution, fake API/type headers or credentials.
The coordinator cache must be read-only. Fresh sources/objects/evidence use --out.
Optional SDK checks use genuine SDK/libc++ and private Boost/fmt dependencies only.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True, help='New directory in private scratch')
    parser.add_argument('--sdk', type=Path)
    parser.add_argument('--boost', type=Path)
    parser.add_argument('--fmt', type=Path)
    parser.add_argument('--json-include', type=Path, default=Path('/usr/include'),
                        help='Portable nlohmann include prefix to stage for SDK checks')
    parser.add_argument('--cdeps', type=Path, help='Optional real PS5 PacBrew prefix, never host OpenSSL/curl')
    parser.add_argument('--regressions', action='store_true', help='Also run the existing host Nextendo/OAuth checks offline')
    args = parser.parse_args()
    if args.cdeps and not args.sdk:
        parser.error('--cdeps requires --sdk')
    root = Path(__file__).resolve().parents[1]
    build, out = args.build.resolve(), args.out.resolve()
    source = build.parent / 'source'
    assert not out.is_relative_to(build.parent) and not out.is_relative_to(root), 'Use separate private scratch'
    assert os.statvfs(build).f_flag & os.ST_RDONLY, 'Saved coordinator build must be mounted read-only'
    out.mkdir(parents=True, exist_ok=False)
    derived = out / 'derived'
    derived.mkdir()
    evidence = {'result': 'FAILED', 'inputs': {}, 'commands': [], 'checks': {}, 'outputs': {}}
    log = (out / 'run.log').open('w', buffering=1)
    failures = []

    def say(text):
        print(text, flush=True)
        log.write(text + '\n')

    def record(path):
        path = Path(path).resolve()
        value = sha(path)
        assert str(path) not in evidence['inputs'] or evidence['inputs'][str(path)] == value, 'Input changed: ' + str(path)
        evidence['inputs'][str(path)] = value

    def run(command, name, cwd=build, allow_missing=(), quiet=False):
        command = [str(a) for a in command]
        say(name)
        evidence['commands'].append({'name': name, 'cwd': str(cwd), 'argv': command})
        try:
            result = subprocess.run(command, cwd=cwd, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=180,
                                    env={**os.environ, 'TMPDIR': str(out), 'PYTHONDONTWRITEBYTECODE': '1',
                                         'CC': 'clang-18', 'CXX': 'clang++-18'})
        except subprocess.TimeoutExpired as error:
            if error.stdout:
                say(error.stdout.decode() if isinstance(error.stdout, bytes) else error.stdout)
            raise
        (out / (name + '.log')).write_text(result.stdout)
        if result.stdout and (not quiet or result.returncode):
            say(result.stdout.rstrip())
        if result.returncode:
            if any("fatal error: '" + header + "' file not found" in result.stdout for header in allow_missing):
                evidence['checks'][name] = 'BLOCKED: genuine target dependency missing'
                return False
            evidence['checks'][name] = 'FAILED'
            failures.append(name)
            return False
        evidence['checks'][name] = 'PASS'
        return True

    def entry(suffix):
        found = [c for c in commands if c['file'].endswith(suffix)]
        assert len(found) == 1, (suffix, len(found))
        return found[0]

    def host_compile(suffix, file, name, extra=()):
        record(file)
        original = entry(suffix)
        command = shlex.split(original['command'])
        command[command.index(original['file'])] = str(file)
        output = out / (name + '.o')
        command[command.index('-o') + 1] = str(output)
        command[1:1] = ['-I' + str(root / 'headless'), '-I' + str(derived / 'multiplayer/socket-headers'),
                        '-I' + str(derived / 'multiplayer')]
        if '-MF' in command:
            command[command.index('-MF') + 1] = str(out / (name + '.d'))
        command += ['-O1', '-MD', '-MF', str(out / (name + '.d')), *extra]
        return output if run(command, name) else None

    def symbols(path, name):
        if not run(['llvm-nm-18', '-C', str(path)], name, quiet=True):
            return ''
        return (out / (name + '.log')).read_text()

    def imports(table):
        return sorted(line.strip()[2:] for line in table.splitlines()
                      if line.strip().startswith(('U ', 'w ', 'v ')))

    def check_target_abi(table):
        assert not any(s in table for s in ('std::__cxx11::', '__errno_location', '__libc_start_main',
                                            '__isoc23_', 'GLIBC_')), 'Host ABI leaked into SDK objects'

    try:
        record(Path(__file__))
        pin = source / 'GIT-COMMIT'
        assert pin.read_text().strip() == '67bada77f8a43a90da2e94e89b8e7da73c256989'
        record(pin)
        record(build / 'compile_commands.json')
        record(build / 'CMakeCache.txt')
        for tool in ('clang++-18', 'ld.lld-18', 'llvm-nm-18', 'cmake', 'python3'):
            record(shutil.which(tool))
            assert run([tool, '--version'], tool + '-version', quiet=True)
        commands = json.loads((build / 'compile_commands.json').read_text())
        for file in (root / 'headless').glob('nextendo*'):
            if file.suffix in ('.h', '.cpp', '.cmake'):
                record(file)
        for file in ('CMakeLists.txt', 'multiplayer_packets.cmake', 'multiplayer_poll.inc', 'ps5.cmake',
                     'update_check/console_curl.c', 'update_check/console_curl.h'):
            record(root / 'headless' / file)
        record(root / 'tools/ps5.cmake')
        for name in ('acc/acc.cpp', 'sockets/sfdnsres.cpp', 'ssl/ssl_backend_openssl.cpp'):
            record(source / 'src/core/hle/service' / name)
        record(source / 'src/core/internal_network/network.cpp')

        # Execute the actual overlay generators. For native socketpair/resolver composition,
        # extract and execute the exact current production CMake block, not a hand-written clone.
        cmake = (root / 'headless/CMakeLists.txt').read_text()
        begin = '    file(READ "${MULTIPLAYER_OUTPUT}/socket_network.cpp" network_source)'
        end = '    write_derived("${PORT_BUILD_DIR}/network.cpp" "${network_source}")'
        assert cmake.count(begin) == 1 and cmake.count(end) == 1
        block = cmake[cmake.index(begin):cmake.index(end) + len(end)]
        rename_block = ''
        if args.cdeps:
            begin = '            set(update_own\n'
            end = '            endforeach()'
            assert cmake.count(begin) == 1
            start = cmake.index(begin)
            rename_block = cmake[start:cmake.index(end, start) + len(end)]
            rename_block += '\nfile(WRITE "${PORT_BUILD_DIR}/update-rename.args" "${update_rename}")\n'
        function = cmake[cmake.index('function(write_derived path content)'):]
        function = function[:function.index('endfunction()') + len('endfunction()')]
        generator = out / 'generate.cmake'
        generator.write_text(f'''set(MULTIPLAYER_SOURCE "{source}")
set(MULTIPLAYER_OUTPUT "{derived / 'multiplayer'}")
set(NEXTENDO_SOURCE "{source}")
set(NEXTENDO_OUTPUT "{derived / 'nextendo'}")
set(PORT_BUILD_DIR "{derived}")
include("{root / 'headless/multiplayer_packets.cmake'}")
include("{root / 'headless/nextendo.cmake'}")
include("{root / 'headless/nextendo_native.cmake'}")
{function}
{block}
{rename_block}
''')
        record(generator)
        assert run(['cmake', '-P', generator], 'generate-overlays', cwd=out)
        nextendo = derived / 'nextendo'
        assert (nextendo / 'acc.cpp').read_text().count('Eden::Nextendo::GuestPidForUser') == 3
        assert (nextendo / 'sfdnsres.cpp').read_text().count('Eden::Nextendo::GuestHost(') == 2
        network = (derived / 'network.cpp').read_text()
        assert 'Native wakeup socketpair' in network and 'lookup.Resolve(host.c_str()' in network
        assert '#include "multiplayer_poll.inc"' in network and 'lookup.Free(addrinfo)' in network

        host = []
        for suffix, file, name in (
            ('/sfdnsres.cpp', nextendo / 'sfdnsres.cpp', 'host-dns'),
            ('/ssl_backend_openssl.cpp', nextendo / 'ssl_backend_openssl.cpp', 'host-tls'),
            ('/acc/acc.cpp', nextendo / 'acc.cpp', 'host-acc'),
            ('/acc/acc.cpp', root / 'headless/nextendo.cpp', 'host-nextendo'),
            ('/socket_network.cpp', derived / 'network.cpp', 'host-network'),
            ('/ssl_backend_openssl.cpp', nextendo / 'ssl_backend_openssl.cpp', 'host-tls-native-ca')):
            obj = host_compile(suffix, file, name, ['-DEDEN_NEXTENDO_NATIVE=1'] if name == 'host-tls-native-ca' else [])
            if obj:
                host.append(obj)
                symbols(obj, name + '-symbols')
        assert not failures, 'Actual host object compilation failed: ' + ', '.join(failures)
        # Real implementation bodies resolve each other's Nextendo ABI in a partial link.
        # Remaining core/platform imports are deliberately unresolved: this is not an app link.
        for name, group in (('host-core-link', host[:4]), ('host-native-branches-link', host[3:])):
            bundle = out / (name + '.o')
            assert run(['ld.lld-18', '-r', '-o', bundle, *group], name)
            table = symbols(bundle, name + '-symbols')
            assert not any(' U Eden::Nextendo::' in line for line in table.splitlines())
        assert 'GuestPidForUser' in (out / 'host-acc-symbols.log').read_text()
        assert 'console_curl_ca_file' in (out / 'host-tls-native-ca-symbols.log').read_text()
        say('Actual DNS/TLS/ACC/Nextendo/native-composed network host objects and Nextendo ABI partial links PASS')

        if args.sdk:
            assert args.boost and args.fmt, 'Supply the private pinned Boost/fmt trees for SDK checks'
            sdk = args.sdk.resolve()
            assert (sdk / 'target/include/sys/socket.h').is_file()
            record(sdk / 'target/include/c++/v1/__config')
            assert (args.boost / 'libs/container/include/boost/container/small_vector.hpp').is_file(), '--boost must be the Boost source root'
            assert (args.fmt / 'include/fmt/format.h').is_file()
            # Stage only the portable JSON dependency, not its enclosing host/native include
            # directory. Keep Linux C/platform headers out of the SDK search path.
            json_headers = out / 'portable-include'
            json_headers.mkdir()
            json_source = (args.json_include / 'nlohmann').resolve(strict=True)
            shutil.copytree(json_source, json_headers / 'nlohmann')
            for file in json_source.rglob('*'):
                if file.is_file():
                    record(file)
            resource = subprocess.check_output(['clang-18', '-print-resource-dir'], text=True).strip()
            target = ['--target=x86_64-sie-ps5', '-isysroot', str(sdk), '-nostdinc', '-nostdinc++',
                      '-isystem', str(sdk / 'target/include/c++/v1'), '-isystem', str(sdk / 'target/include'),
                      '-isystem', resource + '/include', '-std=gnu++20', '-O1', '-fPIC', '-fno-rtti',
                      '-fno-stack-protector', '-fno-plt', '-femulated-tls', '-fvisibility-nodllstorageclass=default',
                      '-fexceptions', '-fcxx-exceptions', '-D_LIBCPP_ENABLE_EXPERIMENTAL=1', '-DPS5_NATIVE=1',
                      '-DEDEN_NEXTENDO_NATIVE=1', '-I' + str(root / 'headless'),
                      '-I' + str(derived / 'multiplayer/socket-headers'), '-I' + str(derived / 'multiplayer'),
                      '-I' + str(json_headers), '-isystem', str(args.fmt.resolve() / 'include')]
            for include in sorted(args.boost.resolve().glob('libs/*/include')):
                target += ['-isystem', str(include)]
            original = shlex.split(entry('/acc/acc.cpp')['command'])
            target += [a for a in original if a.startswith('-D')]
            for a in original:
                if a.startswith('-I') and not any(s in a for s in ('/boost/', '/fmt/', '/work/ProsperoEden')):
                    target.append(a)
            if args.cdeps:
                cdeps = args.cdeps.resolve(strict=True)
                assert os.statvfs(cdeps).f_flag & os.ST_RDONLY, 'PacBrew coordinator prefix must be read-only'
                assert os.statvfs(sdk).f_flag & os.ST_RDONLY, 'Coordinator SDK must be read-only with --cdeps'
                assert not cdeps.is_relative_to(Path('/usr')) and not cdeps.is_relative_to(Path('/lib'))
                libraries = [cdeps / 'lib' / ('lib' + name + '.a') for name in ('curl', 'psl', 'ssl', 'crypto', 'z')]
                for file in [cdeps / 'include/openssl/ssl.h', cdeps / 'include/openssl/opensslv.h',
                             cdeps / 'include/curl/curl.h', cdeps / 'include/curl/curlver.h', *libraries]:
                    record(file)
                target += ['-isystem', str(cdeps / 'include')]
            native = []
            for file, name, missing in (
                (root / 'headless/nextendo.cpp', 'ps5-nextendo', ()),
                (derived / 'network.cpp', 'ps5-network', ()),
                (nextendo / 'sfdnsres.cpp', 'ps5-dns', ()),
                (nextendo / 'acc.cpp', 'ps5-acc', ()),
                (nextendo / 'ssl_backend_openssl.cpp', 'ps5-tls', ('openssl/ssl.h',)),
            ):
                obj = out / (name + '.o')
                if run(['clang++-18', *target, '-MD', '-MF', out / (name + '.d'), '-c', file, '-o', obj], name,
                       allow_missing=() if args.cdeps else missing):
                    native.append(obj)
                    symbols(obj, name + '-symbols')
            # Compile the ACTUAL native producer, not the header-only mock bridge check.
            cflags = ['--target=x86_64-sie-ps5', '-isysroot', str(sdk), '-nostdinc', '-isystem',
                      str(sdk / 'target/include'), '-isystem', resource + '/include', '-std=c11', '-O1',
                      '-fPIC', '-fno-stack-protector', '-fno-plt', '-femulated-tls',
                      '-fvisibility-nodllstorageclass=default', '-DCURL_STATICLIB=1', '-I' + str(root / 'headless/update_check')]
            if args.cdeps:
                cflags += ['-isystem', str(args.cdeps.resolve() / 'include')]
            producer = run(['clang-18', *cflags, '-MD', '-MF', out / 'ps5-console-curl.d', '-c',
                            root / 'headless/update_check/console_curl.c', '-o', out / 'ps5-console-curl.o'],
                           'ps5-console-curl', allow_missing=() if args.cdeps else ('curl/curl.h',))
            if producer:
                symbols(out / 'ps5-console-curl.o', 'ps5-console-curl-symbols')
            if len(native) >= 2:
                assert run(['ld.lld-18', '-r', '-o', out / 'ps5-partial.o', *native], 'ps5-partial-link')
                table = symbols(out / 'ps5-partial.o', 'ps5-partial-symbols')
                assert not failures, 'Actual target object compilation failed: ' + ', '.join(failures)
                assert not any(' U Eden::Nextendo::' in line for line in table.splitlines())
                assert 'console_curl_getaddrinfo' in table and 'console_curl_freeaddrinfo' in table
                check_target_abi(table)
                # Confirm the portable shared_ptr atomics resolve to the genuine SDK runtime.
                runtime = sdk / 'target/lib/libc++.a'
                record(runtime)
                linked = out / 'ps5-with-libcxx.o'
                assert run(['ld.lld-18', '-r', '-o', linked, out / 'ps5-partial.o', runtime], 'ps5-sdk-runtime-link')
                table = symbols(linked, 'ps5-sdk-runtime-symbols')
                for line in table.splitlines():
                    assert not (' U ' in line and any(s in line for s in ('std::__1::__sp_mut::', 'std::__1::__get_sp_mut('))), line
                if args.cdeps:
                    assert producer and len(native) == 5, 'All genuine-header target objects must compile'
                    for tool in ('llvm-objcopy-18', 'llvm-readobj-18'):
                        record(shutil.which(tool))
                        assert run([tool, '--version'], tool + '-version', quiet=True)
                    # Same production isolation order: combine the actual producer with curl/psl,
                    # then rename libc definitions AND their internal references as one unit.
                    isolated = out / 'ps5-console-curl-isolated.o'
                    assert run(['ld.lld-18', '-r', '-o', out / 'ps5-console-curl-all.o',
                                out / 'ps5-console-curl.o', *libraries[:2]], 'ps5-curl-producer-link')
                    renames = (derived / 'update-rename.args').read_text().split(';')
                    assert run(['llvm-objcopy-18', *renames, out / 'ps5-console-curl-all.o', isolated], 'ps5-curl-producer-isolation')
                    table = symbols(isolated, 'ps5-curl-producer-isolated-symbols')
                    bridge = ('console_curl_getaddrinfo', 'console_curl_freeaddrinfo', 'console_curl_ca_file')
                    for name in bridge:
                        assert any(line.endswith(' T ' + name) for line in table.splitlines()), name
                    assert not any(line.endswith(' T ' + name) for line in table.splitlines()
                                   for name in ('getaddrinfo', 'freeaddrinfo', 'fnmatch', 'gmtime_r'))
                    contract = out / 'ps5-native-contract.o'
                    assert run(['ld.lld-18', '-r', '-o', contract, out / 'ps5-partial.o', isolated,
                                '--start-group', *libraries[2:], runtime, '--end-group'], 'ps5-native-contract-link')
                    table = symbols(contract, 'ps5-native-contract-symbols')
                    assert run(['llvm-nm-18', '--undefined-only', str(contract)], 'ps5-native-contract-undefined', quiet=True)
                    check_target_abi(table)
                    unresolved = imports(table)
                    assert not any(name in unresolved for name in bridge), 'Native resolver/CA bridge unresolved'
                    for name in ('SSL_set_verify', 'SSL_set1_host', 'SSL_ctrl', 'X509_STORE_new', 'X509_STORE_load_locations'):
                        assert name in imports((out / 'ps5-tls-symbols.log').read_text()), 'Missing actual TLS call: ' + name
                        assert name not in unresolved, 'Genuine OpenSSL API unresolved: ' + name
                    assert not any(name.startswith(('SSL_', 'X509_', 'curl_', 'psl_', 'Eden::Nextendo::',
                                                    'std::__1::__sp_mut::', 'std::__1::__get_sp_mut(')) for name in unresolved)
                    (out / 'native-unresolved.json').write_text(json.dumps(unresolved, indent=2) + '\n')
                    evidence['checks']['ps5-native-bridge-and-tls-symbols'] = 'PASS'
                    # Export availability is a link-contract check, not a claim that these
                    # modules/functions are usable from the console's runtime sandbox.
                    for library, names in (
                        ('libSceNet.so', ('sceNetPoolCreate', 'sceNetPoolDestroy', 'sceNetResolverCreate',
                                          'sceNetResolverDestroy', 'sceNetResolverStartNtoa')),
                        ('libkernel.so', ('sceKernelGetFsSandboxRandomWord',))):
                        stub = sdk / 'target/lib' / library
                        record(stub)
                        name = 'ps5-producer-imports-' + library
                        assert run(['llvm-nm-18', '-D', '--defined-only', str(stub)], name, quiet=True)
                        exports = (out / (name + '.log')).read_text()
                        for symbol in names:
                            assert symbol in unresolved and any(line.endswith(' T ' + symbol) for line in exports.splitlines()), symbol
                    assert run(['llvm-readobj-18', '--file-headers', out / 'ps5-tls.o', isolated, contract],
                               'ps5-native-object-formats', quiet=True)
                    say('Genuine-header TLS/CA producer and production-renamed bridge/OpenSSL partial-link contract PASS; '
                        + str(len(unresolved)) + ' core/platform imports remain (not an executable link)')
        else:
            evidence['checks']['ps5'] = 'NOT RUN: no SDK requested'

        if args.regressions:
            for file in ('tools/check-nextendo.py', 'tools/check-nextendo-oauth.py',
                         'headless/prosperoeden/nextendo_services.inc', 'headless/remote/http.c',
                         'headless/remote/http.h'):
                record(root / file)
            assert run(['python3', '-u', root / 'tools/check-nextendo.py', '--source', source], 'regression-nextendo', cwd=out)
            assert run(['python3', '-u', root / 'tools/check-nextendo-oauth.py'], 'regression-oauth', cwd=out)

        for dep in out.glob('*.d'):
            for file in shlex.split(dep.read_text().replace('\\\n', '').split(':', 1)[1]):
                path = (build / file).resolve()
                if dep.name.startswith('ps5-'):
                    assert not any(path.is_relative_to(Path(prefix)) for prefix in
                                   ('/usr/include', '/usr/local/include', '/usr/lib/gcc')), 'Linux header in target dependencies: ' + str(path)
                record(path)
        for file, expected in evidence['inputs'].items():
            assert sha(file) == expected, 'Input changed during qualification: ' + file
        assert not failures, 'Unexpected actual object compilation/link failures: ' + ', '.join(failures)
        evidence['result'] = 'PASS WITH DOCUMENTED TARGET BLOCKS' if any('BLOCKED' in v for v in evidence['checks'].values()) else 'PASS'
        say('Nextendo actual core qualification: ' + evidence['result'] + ' (compile/partial-link only, not hardware validation)')
    finally:
        for file in out.glob('*.o'):
            evidence['outputs'][file.name] = sha(file)
        evidence['source_manifest_sha256'] = hashlib.sha256(json.dumps(evidence['inputs'], sort_keys=True, separators=(',', ':')).encode()).hexdigest()
        (out / 'provenance.json').write_text(json.dumps(evidence, sort_keys=True, indent=2) + '\n')
        say('Source manifest SHA-256: ' + evidence['source_manifest_sha256'])
        say('Evidence: ' + str(out / 'provenance.json'))
        log.close()


if __name__ == '__main__':
    if not __debug__:
        raise SystemExit('Run without Python -O: qualification assertions must remain enabled')
    main()
