#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Acquire bounded, genuinely versioned SDK/driver/package source supplements.

These are NOT substitutions for deps.json pins and do not prove a binary/source match.
No PKGBUILD/shell recipe is executed. Optional example/CTS dependencies are not fetched.
"""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import re
import tarfile

from gather_release_sources import acquire, canonical, clone_pin, git


# Public release-tag resolutions, independently observed and retained in the report.
RELEASE_SOURCES = {
    'pacbrew-recipes-v0402': ('https://github.com/ps5-payload-dev/pacbrew-repo.git', '1687e827675c66e1b040751c7f0811222bd3aea0', 'v0.40.2'),
    'official-sdk-v042': ('https://github.com/ps5-payload-dev/sdk.git', '4eb701204fc3f8d31e84cf8ca272974e2be9c867', 'v0.42'),
    'opengl-v102': ('https://github.com/darkxex/ps5-opengl.git', '6b6bb2039f741287ed7f1bc6bc4b2c589709a0ba', 'v1.0.2'),
}


def gather(root, download):
    records = json.loads((root / 'closure-source-hashes.json').read_bytes()) if (root / 'closure-source-hashes.json').exists() else {}
    blockers = {}
    recipes = {}
    def persist():
        (root / 'closure-source-hashes.json').write_bytes(canonical(records))
        (root / 'closure-source-blockers.json').write_bytes(canonical(blockers))
        (root / 'closure-source-recipes.json').write_bytes(canonical(recipes))

    for name, (url, commit, tag) in RELEASE_SOURCES.items():
        target = clone_pin(root, name, {'url': url, 'commit': commit}, Path('/absent-public-source'), download, records, blockers)
        if target:
            records['git/' + name]['observed_release_tag'] = tag
            records['git/' + name]['scope'] = 'Source/recipe present; final binary provenance and transitive closure unverified.'
        persist()
    pacbrew = root / 'materials/git/pacbrew-recipes-v0402'
    if 'git/pacbrew-recipes-v0402' in records:
        commit = RELEASE_SOURCES['pacbrew-recipes-v0402'][1]
        versions = {}
        with tarfile.open(root / 'materials/archives/pacbrew.archive') as archive:
            for package, pc in [('openssl', 'openssl'), ('zlib', 'zlib'), ('curl', 'libcurl'), ('libpsl', 'libpsl')]:
                member = archive.getmember('opt/ps5-payload-sdk/target/user/homebrew/lib/pkgconfig/' + pc + '.pc')
                text = archive.extractfile(member).read(32000).decode()
                versions[package] = re.search(r'^Version:\s*(\S+)', text, re.M)[1]
        for package in ('openssl', 'zlib', 'curl', 'libpsl'):
            text = git(pacbrew, 'show', commit + ':' + package + '/PKGBUILD').decode()
            version = re.search(r'^pkgver=([\w.]+)$', text, re.M)[1]
            source = re.search(r'^source=\(["\x27]([^"\x27]+)["\x27]\)', text, re.M)[1]
            expected = re.search(r'^sha256sums=\(["\x27]([a-f0-9]{64})["\x27]\)', text, re.M)[1]
            url = source.replace('${pkgver}', version).replace('$pkgver', version)
            if '$' in url:
                raise ValueError('unsupported recipe substitution; refusing shell evaluation')
            recipes['pacbrew/' + package] = {'version': version, 'recipe_commit': commit,
                                             'recipe_path': package + '/PKGBUILD',
                                             'recipe_sha256': hashlib.sha256(text.encode()).hexdigest(),
                                             'source_url': url, 'source_sha256': expected,
                                             'binary_package_pc_version': versions[package],
                                             'recipe_version_agrees': version == versions[package],
                                             'scope': 'Version agrees with pinned binary-package .pc metadata; binary/source build match not established.'}
            if version != versions[package]:
                raise ValueError('pacbrew source recipe/package version disagreement: ' + package)
            acquire(root, 'closure/pacbrew-' + package + '.archive', {'sha256': expected}, [], [url], download, records, blockers)
            persist()
    opengl = root / 'materials/git/opengl-v102'
    if 'git/opengl-v102' in records:
        commit = RELEASE_SOURCES['opengl-v102'][1]
        definition = json.loads(git(opengl, 'show', commit + ':dependencies.json'))
        recipes['opengl/dependencies.json'] = {'commit': commit, 'definition': definition}
        mesa = definition['mesa']
        acquire(root, 'closure/opengl-mesa-' + mesa['version'] + '.archive', {'sha256': mesa['sha256']}, [], [mesa['url']], download, records, blockers)
        # Library/compiler/header sources only. imgui/nanovg/sokol/CTS are example/test
        # targets; they are not a justification for downloading their unused trees.
        for name in ('opengnm-psbc', 'opengnm', 'SPIRV-Headers', 'Vulkan-Headers'):
            dep = definition['repositories'][name]
            clone_pin(root, 'opengl-' + name, {'url': dep['url'], 'commit': dep['revision']},
                      Path('/absent-public-source'), download, records, blockers)
            persist()
    mesa = root / 'materials/git/ps5-mesa'
    if mesa.exists():
        commit = git(mesa, 'rev-parse', 'HEAD').decode().strip()
        text = git(mesa, 'show', commit + ':subprojects/zlib.wrap').decode()
        wrap = configparser.ConfigParser(interpolation=None)
        wrap.read_string(text)
        section = wrap['wrap-file']
        recipes['radv/zlib.wrap'] = {'commit': commit, 'source_url': section['source_url'],
                                    'source_sha256': section['source_hash'], 'directory': section['directory']}
        # The pinned wrap explicitly supplies an HTTPS fallback for its historical
        # HTTP source URL. Use that fallback, never downgrade transport or repin.
        urls = [url for url in (section['source_url'], section.get('source_fallback_url'))
                if url and url.startswith('https://')]
        acquire(root, 'closure/radv-' + section['source_filename'], {'sha256': section['source_hash']},
                [], urls, download, records, blockers)
        if 'patch_url' in section:
            acquire(root, 'closure/radv-' + section['patch_filename'], {'sha256': section['patch_hash']},
                    [], [section['patch_url']], download, records, blockers)
        persist()
    # Version comes from the pinned SDK recipe, hash is independently published
    # by FreeBSD/Homebrew. This is source, NOT a new SDK/binary pin or attestation.
    llvm_url = 'https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/llvm-project-18.1.8.src.tar.xz'
    llvm_hash = '0b58557a6d32ceee97c8d533a59b9212d87e0fc4d2833924eb6c611247db2f2a'
    sdk = root / 'materials/git/official-sdk-v042'
    if 'git/official-sdk-v042' not in records or 'LLVM_VER="18.1.8"' not in git(
            sdk, 'show', RELEASE_SOURCES['official-sdk-v042'][1] + ':libcxx.sh').decode():
        raise ValueError('LLVM runtime version requires SDK recipe review')
    recipes['llvm-runtimes-18.1.8'] = {
        'source_version': '18.1.8',
        'source_url': llvm_url, 'source_sha256': llvm_hash,
        'hash_evidence': 'https://raw.githubusercontent.com/Homebrew/homebrew-core/2310edcb41c6f24185de4518fb680bdace57e375/Formula/l/llvm@18.rb',
        'evidence': 'official-sdk-v042/libcxx.sh (LLVM_VER=18.1.8)',
        'scope': 'libcxx, libcxxabi, libunwind, compiler-rt and shared CMake sources available; actual SDK/RADV source/binary build match remains unverified.'}
    acquire(root, 'closure/llvm-project-18.1.8.src.tar.xz', {'sha256': llvm_hash}, [], [llvm_url], download, records, blockers)
    persist()
    inputs = json.loads((root / 'inputs-integration.json').read_bytes())
    inputs['extra_sources'] = [
        {'id': Path(record['path']).name, 'path': record['path'], 'sha256': record['sha256'],
         'origin': record['origin']['public_url']}
        for name, record in sorted(records.items()) if name.startswith('closure/')]
    (root / 'inputs-integration-with-source-supplements.json').write_bytes(canonical(inputs))
    print('CLOSURE MATERIALS', len(records), 'UNRESOLVED', len(blockers), 'NO FINAL BINARY CLAIM')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', required=True)
    parser.add_argument('--download', action='store_true')
    args = parser.parse_args()
    gather(Path(args.workspace), args.download)
