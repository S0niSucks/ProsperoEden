#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare REAL source inputs in private scratch; never turns savednative into a final receipt.

Explicit cache filenames only. No cache/config scanner, builds, installs or source rewriting.
See docs/RELEASE_SOURCE_MATERIALS_1006000.md for the qualified handoff and limitations.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import tarfile
import urllib.error
import urllib.parse
import urllib.request


SAVED = 'cache/ps5-eden-headless.6d6053a486f3'
GIT_DIRS = {'boilerplate': 'ps5-native-app-boilerplate',
            'lapy-owned-helper': 'ProsperoEden/.deps/PS5-Lapy-JB-Daemon-57fcc19',
            'ps5-vulkan-tools': 'mihawk-vulkan-review', 'ps5-mesa': 'mihawk-mesa-review',
            'ps5-payload-sdk-fork': 'mihawk-sdk-review', 'mihawkgen': 'mihawk-framegen-review'}
USED_CPM = {'boost', 'enet', 'ffmpeg', 'fmt', 'frozen', 'gamemode', 'httplib', 'lz4', 'nlohmann',
            'tzdb', 'opus', 'reshade', 'sdl3', 'simpleini', 'sirit', 'spirv-headers', 'vulkan-headers',
            'vulkan-memory-allocator', 'vulkan-utility-libraries', 'xbyak', 'zbic', 'zstd'}
OMISSIONS = {
    'biscuit': 'Eden externals/CMakeLists.txt requires ARCHITECTURE_riscv64; native toolchain is x86_64, not RISC-V.',
    'oaknut': 'Eden externals/CMakeLists.txt requires arm64 or DYNARMIC_TESTS; savednative is x86_64 with tests OFF.',
    'lagoon': 'Eden externals/CMakeLists.txt requires ARCHITECTURE_loongarch64; native target is x86_64.',
    'boost_headers': 'Eden CMakeLists.txt adds this only if NOT TARGET Boost::headers. Boost_FORCE_BUNDLED=ON selects unified boost/1.92.0, whose headers are recorded in savednative includes.',
    'catch2': 'Eden BUILD_TESTING/YUZU_TESTS/DYNARMIC_TESTS are OFF in native release recipes and savednative; no Catch2 test target is linked.',
    'cpp-jwt': 'Eden externals/CMakeLists.txt requires ENABLE_WEB_SERVICE or ENABLE_UPDATE_CHECKER; both are OFF in savednative and native app networking uses its tracked HTTP implementation.',
    'cubeb': 'tools/build-headless-native.sh passes ENABLE_CUBEB=OFF; native output uses the app audio backend, not cubeb.',
    'discord-rpc': 'Eden CMakeLists.txt defines USE_DISCORD_PRESENCE dependent on ENABLE_QT. The native recipe and savednative have ENABLE_QT=OFF, and no DiscordRPC source/link input is present.',
    'libusb': 'tools/build-headless-native.sh passes ENABLE_LIBUSB=OFF; the PS5 controller transport is the tracked native implementation.',
    'libadrenotools': 'Eden Android ARM64 driver provider; native release is PS5 x86_64 and uses the separately pinned OpenGL/RADV inputs.',
    'linkernsbypass': 'Android-only dependency of libadrenotools, not the PS5 x86_64 native driver path.',
    'llvm-mingw': 'Eden Windows/MinGW runtime artifact; PS5 native uses the Payload SDK and tracked native linker recipes.',
    'moltenvk': 'Eden APPLE-only Vulkan implementation; native recipe selects EDEN_VULKAN_DRIVER=RADV on PS5.',
    'openssl': 'CPM OpenSSL source provider is bypassed by OpenSSL_FORCE_SYSTEM=ON and explicit pacbrew include/libcrypto/libssl paths. This does NOT supply pacbrew corresponding source.',
    'openssl-cmake': 'Bundled OpenSSL build wrapper is not selected with OpenSSL_FORCE_SYSTEM=ON; native links the explicit pacbrew libraries.',
    'quazip': 'Qt frontend archive provider; tools/build-headless-native.sh passes ENABLE_QT=OFF and no QuaZip link target is selected.',
    'vulkan-validation-layers': 'Eden Android validation-layer binary provider; PS5 native uses RADV, not Android VVL.',
    'zlib': 'CPM zlib source provider is bypassed by explicit ZLIB_LIBRARY/ZLIB_INCLUDE_DIR in tools/build-headless-native.sh; pacbrew source closure remains separate.',
}
for _name, _provider in [('ffmpeg-ci', 'FFmpeg'), ('openssl-ci', 'OpenSSL'), ('sdl3-ci', 'SDL3'), ('sirit-ci', 'sirit')]:
    OMISSIONS[_name] = f'Unhashed CI/prebuilt {_provider} variant is not selected: native configuration uses the explicit source/system provider, not this CI artifact.'


def canonical(value):
    return (json.dumps(value, indent=2, sort_keys=True) + '\n').encode()


def sha(path, algorithm='sha256'):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, algorithm).hexdigest()


def command(arguments):
    env = {**os.environ, 'GIT_TERMINAL_PROMPT': '0', 'GIT_CONFIG_NOSYSTEM': '1', 'GIT_CONFIG_GLOBAL': '/dev/null',
           'GIT_NO_REPLACE_OBJECTS': '1'}
    return subprocess.run(arguments, env=env, check=True, capture_output=True, timeout=240).stdout


def git(repo, *arguments):
    return command(['git', '-c', f'safe.directory={repo}', '-c', 'credential.helper=',
                    '-c', 'core.hooksPath=/dev/null', '-C', str(repo), *arguments])


def regular(path):
    # Explicit known cache paths may contain .deps/.cache; private NETWORK names may not.
    parts = [p.rstrip(' .').casefold() for p in path.parts]
    if any(parts[i] == 'config' and (parts[i+1] == 'nextendo' or parts[i+1].startswith('network-shares.json'))
           for i in range(len(parts)-1)):
        raise ValueError('private input refused before stat')
    for parent in [*reversed(path.parents), path]:
        if parent.is_symlink():
            raise ValueError('symlink cache input refused')
    if path.exists() and not stat.S_ISREG(path.stat().st_mode):
        raise ValueError('non-regular cache input refused')
    return path.is_file()


def check(path, hashes):
    return regular(path) and all(sha(path, algorithm) == expected for algorithm, expected in hashes.items())


def public_url(url):
    parsed = urllib.parse.urlsplit(url)
    if parsed.scheme != 'https' or parsed.username or parsed.password or parsed.query or parsed.fragment:
        raise ValueError('only credential-free public HTTPS inputs are allowed')
    return url


class PublicRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, response, code, message, headers, url):
        parsed = urllib.parse.urlsplit(url)
        # Release assets may acquire server-generated query strings. Never retain
        # those URLs, send user credentials, or permit an HTTPS downgrade.
        if parsed.scheme != 'https' or parsed.username or parsed.password:
            raise ValueError('unsafe public redirect')
        return super().redirect_request(request, response, code, message, headers, url)


def alternate(url):
    # Same GitHub archive endpoint, not a different version/pin. Hash verification is mandatory.
    match = re.fullmatch(r'https://github.com/([^/]+/[^/]+)/archive/(.+)\.tar\.gz', url)
    return f'https://codeload.github.com/{match[1]}/tar.gz/{match[2]}' if match else None


def acquire(root, name, hashes, candidates, urls, download, records, blockers):
    if not name or name.startswith('/') or any(p in {'', '.', '..'} for p in name.split('/')) or '\\' in name or ':' in name:
        raise ValueError('unsafe private material path')
    if not hashes or any(algorithm not in {'sha256', 'sha512'} or not re.fullmatch(
            '[a-f0-9]{' + str(64 if algorithm == 'sha256' else 128) + '}', expected)
            for algorithm, expected in hashes.items()):
        raise ValueError('material requires exact supported hashes')
    target = root / 'materials' / name
    for parent in [*reversed(target.parent.parents), target.parent]:
        if parent.is_symlink():
            raise ValueError('symlink private material parent refused')
    target.parent.mkdir(parents=True, exist_ok=True)
    if check(target, hashes):
        previous = records.get(name, {})
        records[name] = {**previous, 'path': name, 'sha256': sha(target), 'size': target.stat().st_size,
                         'expected': hashes, 'verified': True}
        return target
    if target.exists():
        raise ValueError('existing private material changed: ' + name)
    failures = []
    for source in candidates:
        if not regular(source):
            continue
        if not check(source, hashes):
            failures.append({'candidate': str(source), 'result': 'HASH_MISMATCH'})
            continue
        partial = target.with_suffix(target.suffix + '.partial')
        if partial.exists() or partial.is_symlink():
            raise ValueError('stale/symlink private partial input refused')
        with source.open('rb') as incoming, partial.open('xb') as outgoing:
            shutil.copyfileobj(incoming, outgoing)
        if not check(partial, hashes):
            raise ValueError('source changed while copying: ' + name)
        partial.rename(target)
        records[name] = {'path': name, 'sha256': sha(target), 'size': target.stat().st_size,
                         'expected': hashes, 'verified': True, 'origin': {'cache_path': str(source)}}
        print('VERIFIED cache', name, flush=True)
        return target
    if download:
        for url in dict.fromkeys(u for primary in urls for u in (primary, alternate(primary)) if u):
            public_url(url)
            partial = target.with_suffix(target.suffix + '.partial')
            if partial.exists() or partial.is_symlink():
                raise ValueError('stale/symlink private partial input refused')
            try:
                request = urllib.request.Request(url, headers={'User-Agent': 'ProsperoEden-source-materials'})
                opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), PublicRedirect())
                with opener.open(request, timeout=60) as response, partial.open('xb') as output:
                    size = 0
                    while data := response.read(1024 * 1024):
                        size += len(data)
                        if size > 512 * 1024 * 1024:
                            raise ValueError('public material exceeds 512 MiB safety cap')
                        output.write(data)
                if not check(partial, hashes):
                    failures.append({'url': url, 'result': 'HASH_MISMATCH', 'actual_sha256': sha(partial)})
                    continue
                partial.rename(target)
                records[name] = {'path': name, 'sha256': sha(target), 'size': target.stat().st_size,
                                 'expected': hashes, 'verified': True, 'origin': {'public_url': url}}
                print('VERIFIED public', name, flush=True)
                return target
            except (OSError, ValueError) as error:
                failures.append({'url': url, 'result': type(error).__name__,
                                 'http_status': error.code if isinstance(error, urllib.error.HTTPError) else None})
            finally:
                partial.unlink(missing_ok=True)
    blockers[name] = {'status': 'MISSING_VERIFIED_MATERIAL', 'expected': hashes,
                      'cache_candidates': [str(p) for p in candidates], 'attempts': failures,
                      'public_urls': urls}
    print('PENDING missing', name, flush=True)
    return None


def clone_pin(root, name, dep, source, download, records, blockers):
    target = root / 'materials/git' / name
    if not re.fullmatch(r'[A-Za-z0-9_-]+', name) or any(p.is_symlink() for p in [*target.parents, target]):
        raise ValueError('unsafe private Git destination')
    try:
        if not target.exists():
            command(['git', 'init', '-q', str(target)])
            origin = source if source.is_dir() else public_url(dep['url']) if download else None
            if origin is None:
                raise ValueError('missing pinned Git input')
            git(target, 'fetch', '-q', '--no-tags', '--depth=1', str(origin), dep['commit'])
            git(target, '-c', 'advice.detachedHead=false', 'checkout', '-q', '--detach', dep['commit'])
        head = git(target, 'rev-parse', 'HEAD').decode().strip()
        if head != dep['commit']:
            raise ValueError('private Git source differs from pin')
        git(target, 'fsck', '--full', '--no-reflogs', '--no-dangling')
        # The immutable object tree and every materialized tracked byte must agree.
        # This handles genuine Git symlinks as link-text evidence, not followed files.
        git(target, 'diff', '--quiet', dep['commit'], '--')
        entries = git(target, 'ls-tree', '-rz', dep['commit']).split(b'\0')
        special = [e.decode() for e in entries if e.startswith((b'120000', b'160000'))]
        records['git/' + name] = {'path': 'git/' + name, 'commit': head,
                                  'tree': git(target, 'rev-parse', 'HEAD^{tree}').decode().strip(),
                                  'origin': dep['url'], 'local_object_source': str(source),
                                  'tracked_entries': len(entries) - 1, 'special_entries': special,
                                  'source_bundle_status': 'REFUSED_GIT_SYMLINK' if any(e.startswith('120000') for e in special) else 'PINNED_SOURCE_AVAILABLE'}
        print('VERIFIED Git', name, 'special entries', len(special), flush=True)
        return target
    except (OSError, ValueError, subprocess.SubprocessError):
        records.pop('git/' + name, None)
        blockers['git/' + name] = {'status': 'MISSING_PINNED_GIT_SOURCE', 'commit': dep['commit'], 'public_url': dep['url']}
        return None


def cpm_url(definition):
    host = definition.get('git_host', 'github.com')
    version = definition['version'].replace('%NUMERIC_VERSION%', definition.get('numeric_version', ''))
    if not host or '%' in version or not definition.get('repo'):
        raise ValueError('unsupported/unresolved CPM URL definition')
    base = 'https://' + host + '/' + definition['repo']
    artifact = definition.get('artifact')
    if artifact:
        artifact = artifact.replace('%VERSION%', version).replace('%NUMERIC_VERSION%', definition.get('numeric_version', ''))
        return base + '/releases/download/' + version + '/' + artifact
    return base + '/archive/' + version + '.tar.gz'


def evidence(native, root, archives):
    saved = native / SAVED
    cache = saved / 'native-local/CMakeCache.txt'
    ninja = saved / 'native-local/build.ninja'
    selected = {}
    for line in cache.read_text().splitlines():
        if line.startswith(('#', '//')) or '=' not in line:
            continue
        keytype, value = line.split('=', 1)
        key = keytype.split(':')[0]
        if (key in {'CPM_PACKAGES', 'Boost_SOURCE_DIR', 'OpenSSL_FORCE_SYSTEM', 'ZLIB_LIBRARY',
                    'ZLIB_INCLUDE_DIR', 'FFMPEG_DIR', 'PS5_NATIVE', 'USE_DISCORD_PRESENCE', 'DYNARMIC_TESTS',
                    'BUILD_TESTING', 'CMAKE_SYSTEM_PROCESSOR', 'CMAKE_BUILD_TYPE'}
                or key.startswith(('ENABLE_', 'EDEN_PS5_', 'YUZU_USE_BUNDLED_'))
                or key.startswith('CPM_PACKAGE_') and key.endswith('_SOURCE_DIR')):
            selected[key] = value
    lines = ninja.read_text().splitlines()
    at = next(i for i, line in enumerate(lines) if line.startswith('build bin/eden-headless:'))
    link = next(line.partition(' = ')[2] for line in lines[at+1:at+18] if line.startswith('  LINK_LIBRARIES = '))
    includes = sorted(set(re.findall(r'-I([^\s]*?/boost/1\.92\.0/[^\s]+)', '\n'.join(
        line for line in lines if line.startswith('  INCLUDES = ')))))
    result = {'status': 'SAVEDNATIVE_PLANNING_ONLY_NOT_FINAL_BUILD',
              'files_sha256': {'CMakeCache.txt': sha(cache), 'build.ninja': sha(ninja)},
              'selected_cache': selected, 'link_libraries': link.split(), 'boost_include_roots': includes,
              'warning': 'No objects, binary, environment dump or logs are collected; missing newer features cannot be inferred absent from final build.'}
    if 'eden' in archives:
        with tarfile.open(archives['eden']) as archive:
            member = next(m for m in archive.getmembers() if m.name.endswith('/cpmfile.json'))
            pristine = archive.extractfile(member).read()
        actual = saved / 'source/cpmfile.json'
        result['eden_cpmfile'] = {'upstream_sha256': hashlib.sha256(pristine).hexdigest(),
                                  'savednative_sha256': sha(actual),
                                  'same_bytes': hashlib.sha256(pristine).hexdigest() == sha(actual)}
    version = saved / 'source/.cache/cpm/boost/1.92.0/libs/config/include/boost/version.hpp'
    if regular(version):
        result['boost_version_header'] = {'path': str(version), 'sha256': sha(version),
                                           'version_macro': re.search(r'^#define BOOST_VERSION (\d+)', version.read_text(), re.M)[1]}
    result['sampled_overlay_inputs'] = {}
    for relative in ('source/.cache/cpm/xbyak/v7.40.1/xbyak/xbyak.h',
                     'source/.cache/cpm/boost/1.92.0/.cpm_patch_key'):
        path = saved / relative
        if regular(path):
            result['sampled_overlay_inputs'][relative] = {'sha256': sha(path)}
    result['overlay_scope'] = 'Selected evidence only: cpmfile/header hashes do not establish a full savednative delta or a final-build source match. Preserve Eden .patch, app tools/eden-patches, headless overlays and native linker recipes.'
    (root / 'savednative-selected.json').write_bytes(canonical(result))
    return result


def gather(args):
    root = Path(args.workspace)
    root.mkdir(parents=True, exist_ok=True)
    native, network, coordinator = map(Path, (args.native, args.network, args.coordinator))
    repo = Path(args.snapshot_repo)
    commit = git(repo, 'rev-parse', args.snapshot + '^{commit}').decode().strip()
    tree = git(repo, 'rev-parse', commit + '^{tree}').decode().strip()
    deps = json.loads(git(repo, 'show', commit + ':tools/deps.json'))['items']
    upstream = json.loads(git(repo, 'show', commit + ':UPSTREAM.json'))
    # This release-specific omission map is not a generic inference engine. Refuse
    # changed recipe assumptions instead of carrying a savednative decision forward.
    native_recipe = git(repo, 'show', commit + ':tools/build-headless-native.sh').decode()
    for option in ('ENABLE_QT', 'ENABLE_CUBEB', 'ENABLE_LIBUSB', 'ENABLE_WEB_SERVICE', 'BUILD_TESTING',
                   'YUZU_USE_EXTERNAL_FFMPEG'):
        if '-D' + option + '=OFF' not in native_recipe:
            raise ValueError('re-review CPM omissions for changed native option: ' + option)
    if '-DBoost_FORCE_BUNDLED=ON' not in native_recipe or '-DOpenSSL_FORCE_SYSTEM=ON' not in native_recipe:
        raise ValueError('re-review changed native source providers')
    if 'set(CMAKE_SYSTEM_PROCESSOR x86_64)' not in git(repo, 'show', commit + ':tools/ps5.cmake').decode():
        raise ValueError('re-review changed native architecture')
    records = json.loads((root / 'material-hashes.json').read_bytes()) if (root / 'material-hashes.json').exists() else {}
    blockers = {}
    inventory = {'schema': 1, 'snapshot': {'commit': commit, 'tree': tree}, 'materials': {},
                 'cpm_omissions': {}, 'relink': {'status': 'PENDING'}}
    archives = {}
    for dep in deps:
        name, kind = dep['name'], dep['kind']
        if kind == 'git':
            target = clone_pin(root, name, dep, native / GIT_DIRS[name], args.download, records, blockers)
            if target:
                inventory['materials']['deps/' + name] = {'path': 'git/' + name}
        elif kind == 'archive':
            hashes = {k: dep[k] for k in ('sha256', 'sha512') if k in dep}
            prefix = next(iter(hashes.values()))[:12]
            candidates = [native / f'cache/prosperoeden-deps/{name}-{prefix}.tar.gz']
            if 'save' in dep:
                candidates += [native / 'ProsperoEden' / dep['save'], coordinator / 'ProsperoEden' / dep['save']]
            if name in {'libsmb2', 'libnfs'}:
                candidates += [network / f'deps/{name}.tar.gz']
            target = acquire(root, 'archives/' + name + '.archive', hashes, candidates, [dep['url']], args.download, records, blockers)
            if target:
                inventory['materials']['deps/' + name] = {'path': target.relative_to(root / 'materials').as_posix()}
                archives[name] = target
        elif kind == 'files':
            for file in dep['files']:
                namepath = 'files/' + name + '/' + file['name']
                candidates = [native / 'ProsperoEden' / dep['dest'] / file['name'],
                              native / ('cache/prosperoeden-deps/' + name + '-' + file['name'].replace('/', '-'))]
                target = acquire(root, namepath, {'sha256': file['sha256']}, candidates, [file['url']], args.download, records, blockers)
                if target:
                    inventory['materials']['deps/' + name + '/' + file['name']] = {'path': namepath}
        (root / 'material-hashes.json').write_bytes(canonical(records))
    # Historical inputs are not overrides for the actual linked Boost 1.92 source.
    for name, expected in upstream['archives'].items():
        candidates = [native / 'ProsperoEden/.deps' / name, coordinator / 'ProsperoEden/.deps' / name]
        for existing in archives.values():
            if sha(existing) == expected:
                candidates.insert(0, existing)
        urls = [upstream['boost_context_source']] if name == 'boost-context-1.90.0.tar.gz' else []
        if name == 'xbyak-7.40.1.tar.gz':
            urls = ['https://github.com/herumi/xbyak/archive/refs/tags/v7.40.1.tar.gz']
        if name == 'libboost1.90-dev_1.90.0-6ubuntu1_amd64.deb':
            # Exact filename was confirmed in the public Ubuntu main/boost1.90 index;
            # this is still the repository's original SHA-256, not a new pin.
            urls = ['https://archive.ubuntu.com/ubuntu/pool/main/b/boost1.90/' + name]
        target = acquire(root, 'historical/' + name, {'sha256': expected}, candidates, urls, args.download, records, blockers)
        if target:
            inventory['materials']['upstream/' + name] = {'path': 'historical/' + name}
    if 'eden' in archives:
        with tarfile.open(archives['eden']) as tar:
            member = next(m for m in tar.getmembers() if m.name.endswith('/cpmfile.json'))
            definitions = json.loads(tar.extractfile(member).read())
            for name, definition in definitions.items():
                key = 'cpm/deps/eden/' + member.name + '/' + name
                if name not in USED_CPM:
                    reason = OMISSIONS.get(name)
                    if reason is None:
                        blockers[key] = {'status': 'UNREVIEWED_CPM_APPLICABILITY'}
                    else:
                        evidence_path = 'tools/ps5.cmake' if name in {'biscuit', 'oaknut', 'lagoon', 'libadrenotools',
                                                                    'linkernsbypass', 'llvm-mingw', 'moltenvk',
                                                                    'vulkan-validation-layers'} else 'tools/build-headless-native.sh'
                        inventory['cpm_omissions'][key] = {'reason': reason, 'evidence': evidence_path}
                    continue
                hashes = {'sha512': definition['hash']}
                candidates = list(archives.values())
                candidates += [native / f'cache/prosperoeden-deps/cpm-{name}.archive']
                target = acquire(root, 'cpm/' + name + '.archive', hashes, candidates, [cpm_url(definition)], args.download, records, blockers)
                if target:
                    inventory['materials'][key] = {'path': 'cpm/' + name + '.archive'}
    evidence(native, root, archives)
    (root / 'material-hashes.json').write_bytes(canonical(records))
    (root / 'inputs-integration.json').write_bytes(canonical(inventory))
    (root / 'material-blockers.json').write_bytes(canonical(blockers))
    print('MATERIALS', len(records), 'BLOCKERS', len(blockers), 'RELINK PENDING', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', required=True)
    parser.add_argument('--snapshot-repo', required=True)
    parser.add_argument('--snapshot', required=True)
    parser.add_argument('--native', default='/native')
    parser.add_argument('--network', default='/network')
    parser.add_argument('--coordinator', default='/coordinator')
    parser.add_argument('--download', action='store_true')
    gather(parser.parse_args())
