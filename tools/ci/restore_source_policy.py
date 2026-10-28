#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline integration-only restore + upstream CPM source fetch + bounded overlays.

Run with --network none in a NEW private destination. Does not run top-level
configure, make/prepare, SDK tools, compilers, packaging, or a relink command.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import stat
import subprocess

import source_bundle as tool
from gather_release_sources import cpm_url


def patch_source(root, patch):
    for arguments in (['--dry-run', '-N'], ['-N'], ['--dry-run', '-R']):
        subprocess.run(['patch', '-d', str(root), '-p1', '-s', '-f', *arguments],
                       input=patch.read_bytes(), capture_output=True, check=True, timeout=30)


def source_inventory(root):
    """Only a NEW, provenance-verified restored component, never a user/cache root."""
    result = {}
    for parent, directories, filenames in os.walk(root, followlinks=False):
        for name in sorted(directories + filenames):
            path = Path(parent) / name
            mode = path.lstat().st_mode
            relative = path.relative_to(root).as_posix()
            if stat.S_ISLNK(mode):
                result[relative] = {'link_text': os.readlink(path)}
            elif stat.S_ISREG(mode):
                result[relative] = {'sha256': tool.digest(path), 'mode': 0o755 if mode & 0o111 else 0o644}
            else:
                tool.require(stat.S_ISDIR(mode), 'special restored source input')
    return result


def trial(args):
    # Verification independently rediscovers every CPM definition and audits every
    # archive, including the exact hardlink. External hash/snapshot are mandatory.
    manifest = tool.verify(args)
    unpack = [key for key, row in sorted(manifest['materials'].items())
              if key.startswith('deps/') and row.get('kind') == 'archive' or key.startswith('extra/')]
    args.unpack_material = unpack
    tool.restore(args)
    output = Path(args.output)
    app = output / 'sources/app'
    eden_roots = list((output / 'unpacked/deps/eden').iterdir())
    tool.require(len(eden_roots) == 1 and eden_roots[0].is_dir(), 'unexpected Eden archive layout')
    eden = eden_roots[0]
    definitions_before = (eden / 'cpmfile.json').read_bytes()
    applied = {}
    for patch in sorted((app / 'tools/eden-patches').glob('*.diff')):
        patch_source(eden, patch)
        applied[patch.relative_to(app).as_posix()] = tool.digest(patch)
    tool.require(definitions_before == (eden / 'cpmfile.json').read_bytes(), 'source recipe changed under patch set')
    definitions = tool.load_json(definitions_before)
    selected = {}
    for key, material in sorted(manifest['materials'].items()):
        if not key.startswith('cpm/deps/eden/'):
            continue
        name = key.rsplit('/', 1)[-1]
        definition = definitions[name]
        package = definition.get('package', name).lower()
        version = definition['version'].replace('%NUMERIC_VERSION%', definition.get('numeric_version', ''))
        tool.safe_name(package + '/' + version)
        url = definition.get('url') or cpm_url(definition)
        artifact = tool.safe_name(url.rsplit('/', 1)[-1])
        cache = eden / '.cache/cpm' / package
        cache.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(output / 'transport' / material['destination'], cache / artifact)
        selected[name] = {'relative_cache': package + '/' + version, 'archive_sha256': material['sha256']}
    tool.require(len(selected) == 22, 're-review selected CPM source set')
    command = ['sh', str(eden / 'tools/cpmutil.sh'), 'package', 'fetch', *sorted(selected)]
    completed = subprocess.run(command, cwd=eden, check=True, capture_output=True, timeout=600)
    (output / 'cpm-fetch.log').write_bytes(completed.stdout + completed.stderr)
    patch_checks, cache_inventory = {}, {}
    for name, row in selected.items():
        cache = eden / '.cache/cpm' / row['relative_cache']
        patches = [eden / '.patch' / name / tool.safe_name(p) for p in definitions[name].get('patches', [])]
        expected = hashlib.sha512(b''.join(p.read_bytes() for p in patches)).hexdigest() if patches else 'none'
        tool.require((cache / '.cpm_patch_key').read_text() == expected, 'CPM patch key differs from upstream recipe')
        for patch in patches:
            subprocess.run(['patch', '-d', str(cache), '-p1', '-s', '-f', '--dry-run', '-R'],
                           input=patch.read_bytes(), check=True, capture_output=True, timeout=30)
        patch_checks[name] = {p.relative_to(eden).as_posix(): tool.digest(p) for p in patches}
        cache_inventory[name] = source_inventory(cache)
    # A second original CPM fetch must leave every cached byte, mode, link-text
    # unchanged, and must not download/extract (Docker also disables networking).
    repeated = subprocess.run(command, cwd=eden, check=True, capture_output=True, timeout=180)
    (output / 'cpm-fetch-repeat.log').write_bytes(repeated.stdout + repeated.stderr)
    tool.require(b'Downloaded ' not in repeated.stdout and b'Extracting ' not in repeated.stdout, 'CPM cache was not reusable')
    for name, row in selected.items():
        tool.require(source_inventory(eden / '.cache/cpm' / row['relative_cache']) == cache_inventory[name], 'CPM repeat changed sources')
    # Execute only the source-header portion of the actual pinned overlay recipe.
    # No target/configure commands from the rest of that recipe are executed.
    recipe = app / 'headless/jit-alias.cmake'
    text = recipe.read_text()
    delimiter = 'target_include_directories(common BEFORE PUBLIC'
    tool.require(text.count(delimiter) == 1, 're-review changed Xbyak source overlay')
    prefix = text.split(delimiter)[0]
    script = output / 'xbyak-source-only.cmake'
    script.write_text('function(write_derived path content)\nfile(WRITE "${path}" "${content}")\nendfunction()\n' + prefix)
    derived = output / 'derived-source'
    subprocess.run(['cmake', '-DPROJECT_SOURCE_DIR=' + str(eden), '-DPORT_BUILD_DIR=' + str(derived), '-P', str(script)],
                   check=True, capture_output=True, timeout=60)
    xbyak = derived / 'include/xbyak/xbyak.h'
    tool.require(b'writeTop_' in xbyak.read_bytes() and b'writableAddress' in xbyak.read_bytes(), 'missing source overlay changes')
    (output / 'CPM-source-inventory.json').write_bytes(tool.canonical(cache_inventory))
    result = {'status': 'PASS_OFFLINE_SOURCE_RESTORE_CPM_CACHE_AND_SELECTED_OVERLAYS_ONLY',
        'snapshot': manifest['snapshot'], 'bundle_sha256': args.sha256, 'relink': 'PENDING',
        'unpacked_archives': unpack, 'git_sources': sorted(manifest['git_sources']), 'selected_cpm': selected,
        'eden_patches_sha256': applied, 'cpm_patch_reverse_checks': patch_checks,
        'cpm_source_inventory_sha256': tool.digest(output / 'CPM-source-inventory.json'),
        'upstream_cpmutil_sha256': tool.digest(eden / 'CMakeModules/CPMUtil.cmake'),
        'xbyak_overlay': {'recipe_sha256': tool.digest(recipe), 'source_only_prefix_sha256': hashlib.sha256(prefix.encode()).hexdigest(),
                         'generated_headers': source_inventory(derived)},
        'pending': ['Top-level/offline native configuration and complete used transitive/generated/toolchain closure.',
                    'Remaining headless CMake-derived sources/target edits, including Dynarmic and Boost.Process.',
                    'Apply/reconcile final local SDK and RADV/OpenGL source deltas, generators and binary build options.',
                    'Final clean HEAD and binary matching, actual final objects/receipt and modified-library relink trial.'],
        'not_run': ['full prepare/build', 'top-level CMake configure', 'SDK/runtime/toolchain build or execution',
                    'packaging', 'final binary provenance/object collection', 'modified-library relink']}
    (output / 'SOURCE-RESTORE-RESULT.json').write_bytes(tool.canonical(result))
    print('PASS OFFLINE', len(selected), 'CPM source caches;', len(applied), 'Eden patches; Xbyak source headers; NO BUILD; PENDING')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('bundle', 'sha256', 'commit', 'tree', 'output'):
        parser.add_argument('--' + option, required=True)
    trial(parser.parse_args())
