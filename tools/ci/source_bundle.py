#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline, explicit-input source/relink evidence bundles. See docs/RELEASE_SOURCE_RELINK.md.

Never walks a checkout/cache/build directory, fetches, builds, or executes relink commands.
Git objects are read at a declared commit/tree; local inputs are an explicit allowlist.
"""
import argparse
import ctypes
import ctypes.util
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
import zipfile


class Refused(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise Refused(message)


def canonical(value):
    return (json.dumps(value, sort_keys=True, indent=2, ensure_ascii=True) + '\n').encode()


def digest(path, algorithm='sha256'):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, algorithm).hexdigest()


def hexhash(value, length=64):
    require(isinstance(value, str) and re.fullmatch('[0-9a-f]{%d}' % length, value), 'invalid hash')
    return value


def private(name):
    parts = [p.rstrip(' .').casefold() for p in name.replace('\\', '/').split('/')]
    return (any(p in {'.git', '.local', '.deps', '.cache', '__pycache__', '.env', '.ssh', '.aws'} for p in parts)
            or any(parts[i] == 'config' and (parts[i + 1] == 'nextendo'
                       or parts[i + 1].startswith('network-shares.json')) for i in range(len(parts) - 1))
            or any(p.startswith(('.env.', 'network-shares.json')) for p in parts)
            or any(p in {'oauth.json', 'prod.keys', 'title.keys', 'id_rsa', 'id_ed25519'} for p in parts))


def safe_name(name):
    require(isinstance(name, str) and name and not private(name), 'private or empty path refused')
    require(not name.startswith('/') and '\\' not in name and ':' not in name,
            'absolute/alternate-separator path refused')
    parts = name.split('/')
    require(all(p and p not in {'.', '..'} and p == p.rstrip(' .')
                and not any(ord(c) < 32 or ord(c) == 127 for c in p) for p in parts), 'unsafe path refused')
    require(all(not re.fullmatch(r'(?i:con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\..*)?', p)
                for p in parts), 'device path refused')
    return name


def fields(value, required, optional=()):
    require(isinstance(value, dict) and set(required) <= value.keys()
            and value.keys() <= set(required) | set(optional), 'invalid manifest fields')


def load_json(data):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, 'duplicate JSON key')
            result[key] = value
        return result
    return json.loads(data, object_pairs_hook=pairs)


def local_file(root, relative, directory=False):
    safe_name(relative)  # deny by name BEFORE looking at any child metadata
    current = root
    for part in relative.split('/'):
        current /= part
        metadata = current.lstat()
        require(not stat.S_ISLNK(metadata.st_mode), 'local symlink refused')
        require(stat.S_ISREG(metadata.st_mode) or stat.S_ISDIR(metadata.st_mode), 'special input refused')
    require(current.is_dir() if directory else current.is_file(), 'missing/wrong input type')
    return current


def real_root(value):
    path = Path(os.path.abspath(value))
    for parent in [*reversed(path.parents), path]:
        require(not parent.is_symlink(), 'symlink in root path refused')
    require(path.is_dir(), 'missing input root')
    return path


def git(repo, *arguments):
    env = {**os.environ, 'GIT_NO_REPLACE_OBJECTS': '1', 'GIT_CONFIG_NOSYSTEM': '1'}
    return subprocess.run(['git', '-c', f'safe.directory={repo}', '-C', str(repo), *arguments],
                          check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env).stdout


def snapshot(repo, commit, tree):
    hexhash(commit, 40)
    hexhash(tree, 40)
    require(git(repo, 'rev-parse', commit + '^{commit}').decode().strip() == commit, 'commit mismatch')
    require(git(repo, 'rev-parse', commit + '^{tree}').decode().strip() == tree, 'tree mismatch')
    return {'commit': commit, 'tree': tree}


def tree_entries(repo, commit):
    result = []
    aliases = set()
    for entry in git(repo, 'ls-tree', '-rz', '--full-tree', commit).split(b'\0'):
        if not entry:
            continue
        meta, raw_name = entry.split(b'\t', 1)
        mode, kind, oid = meta.decode().split()
        name = safe_name(raw_name.decode('utf-8'))
        alias = name.casefold()
        require(alias not in aliases, 'case-alias paths refused')
        aliases.add(alias)
        require(mode in {'100644', '100755', '160000'}, 'Git symlink/special entry refused')
        require(kind == ('commit' if mode == '160000' else 'blob'), 'invalid Git entry type')
        result.append((name, mode, oid))
    return result


def expand_zstd(source, destination):
    """Stream to private scratch using the image's existing libzstd (no install/CLI)."""
    library = ctypes.util.find_library('zstd')
    require(library, 'libzstd is required for this pinned archive')
    zstd = ctypes.CDLL(library)

    class Buffer(ctypes.Structure):
        _fields_ = [('data', ctypes.c_void_p), ('size', ctypes.c_size_t), ('pos', ctypes.c_size_t)]

    for name, result, arguments in (
        ('ZSTD_createDStream', ctypes.c_void_p, []),
        ('ZSTD_freeDStream', ctypes.c_size_t, [ctypes.c_void_p]),
        ('ZSTD_initDStream', ctypes.c_size_t, [ctypes.c_void_p]),
        ('ZSTD_decompressStream', ctypes.c_size_t, [ctypes.c_void_p, ctypes.POINTER(Buffer), ctypes.POINTER(Buffer)]),
        ('ZSTD_isError', ctypes.c_uint, [ctypes.c_size_t]),
    ):
        function = getattr(zstd, name)
        function.restype, function.argtypes = result, arguments
    stream = zstd.ZSTD_createDStream()
    require(stream, 'zstd allocation failed')
    try:
        require(not zstd.ZSTD_isError(zstd.ZSTD_initDStream(stream)), 'zstd initialization failed')
        remaining = 1
        with source.open('rb') as compressed, destination.open('wb') as expanded:
            while chunk := compressed.read(1024 * 1024):
                raw = ctypes.create_string_buffer(chunk)
                incoming = Buffer(ctypes.cast(raw, ctypes.c_void_p), len(chunk), 0)
                while True:
                    raw_output = ctypes.create_string_buffer(128 * 1024)
                    outgoing = Buffer(ctypes.cast(raw_output, ctypes.c_void_p), len(raw_output), 0)
                    previous = incoming.pos
                    remaining = zstd.ZSTD_decompressStream(stream, ctypes.byref(outgoing), ctypes.byref(incoming))
                    require(not zstd.ZSTD_isError(remaining), 'invalid zstd archive')
                    expanded.write(raw_output.raw[:outgoing.pos])
                    if incoming.pos == incoming.size and outgoing.pos < outgoing.size:
                        break
                    require(incoming.pos != previous or outgoing.pos, 'stalled zstd stream')
        require(remaining == 0, 'truncated zstd archive')
    finally:
        zstd.ZSTD_freeDStream(stream)


def archive_index(path):
    """Validate member names/types without extraction, return embedded CPM definitions.

    Upstream archives may contain relative internal symlinks (e.g. SDK libraries).
    They are retained only as bytes in the original archive; escaping links are refused.
    Local filesystem links are NEVER followed. Consumers must use safe unpacking.
    """
    with path.open('rb') as source:
        magic = source.read(4)
    if magic == b'\x28\xb5\x2f\xfd':
        with tempfile.TemporaryDirectory(prefix='source-zstd-') as temporary:
            expanded = Path(temporary) / 'source.tar'
            expand_zstd(path, expanded)
            return archive_index(expanded)
    definitions = []
    seen = set()

    def member(name, link=None):
        name = safe_name(name.rstrip('/'))
        require(name.casefold() not in seen, 'duplicate/aliased archive member')
        seen.add(name.casefold())
        if link is not None:
            require(link and not link.startswith('/') and '\\' not in link and ':' not in link,
                    'unsafe archive link')
            stack = list(PurePosixPath(name).parent.parts)
            for part in link.split('/'):
                if part == '..':
                    require(stack, 'escaping archive link')
                    stack.pop()
                elif part not in {'', '.'}:
                    stack.append(part)
            safe_name('/'.join(stack))
        return name

    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            for info in archive.infolist():
                name = member(info.filename)
                mode = info.external_attr >> 16
                require(not info.flag_bits & 1, 'encrypted archive refused')
                require(not stat.S_ISLNK(mode), 'ZIP symlink refused')
                require(stat.S_IFMT(mode) in {0, stat.S_IFREG, stat.S_IFDIR}, 'ZIP special member refused')
                if name.endswith('/cpmfile.json') or name == 'cpmfile.json':
                    require(info.file_size < 2_000_000, 'oversize CPM definition')
                    definitions.append((name, load_json(archive.read(info))))
    else:
        with tarfile.open(path, 'r:*') as archive:
            for info in archive:
                member(info.name, info.linkname if info.issym() else None)
                require(info.isfile() or info.isdir() or info.issym(), 'archive hardlink/special entry refused')
                if info.isfile() and (info.name.endswith('/cpmfile.json') or info.name == 'cpmfile.json'):
                    require(info.size < 2_000_000, 'oversize CPM definition')
                    definitions.append((info.name, load_json(archive.extractfile(info).read())))
    return definitions


def requirements(app):
    deps = load_json((app / 'tools/deps.json').read_bytes())
    upstream = load_json((app / 'UPSTREAM.json').read_bytes())
    result = {}
    require(isinstance(deps['items'], list) and isinstance(upstream['archives'], dict), 'invalid source pin tables')
    names = set()
    for item in deps['items']:
        key = safe_name('deps/' + item['name'])
        require(key not in names, 'duplicate dependency name')
        names.add(key)
        kind = item['kind']
        if kind == 'archive':
            hashes = {k: hexhash(item[k], 128 if k == 'sha512' else 64)
                      for k in ('sha256', 'sha512') if k in item}
            require(hashes, 'unpinned dependency archive')
            result[key] = {'kind': kind, 'hashes': hashes, 'origin': item['url']}
        elif kind == 'files':
            for file in item['files']:
                result[safe_name(key + '/' + file['name'])] = {
                    'kind': 'file', 'hashes': {'sha256': hexhash(file['sha256'])}, 'origin': file['url']}
        elif kind == 'git':
            result[key] = {'kind': 'git', 'commit': hexhash(item['commit'], 40), 'origin': item['url']}
        else:
            raise Refused('unknown dependency kind')
    eden = next(i for i in deps['items'] if i['name'] == 'eden')
    require(upstream['eden_commit'] in eden['url'], 'Eden revision pins disagree')
    require(upstream['archives'][Path(eden['save']).name] == eden['sha256'], 'Eden hash pins disagree')
    # Older UPSTREAM artifacts are still declared inputs, not silently discarded as caches.
    for name, sha in upstream['archives'].items():
        result[safe_name('upstream/' + name)] = {'kind': 'file', 'hashes': {'sha256': hexhash(sha)},
                                               'origin': 'UPSTREAM.json/archives/' + name}
    return result


class Collector:
    def __init__(self, stage, materials_root, inventory):
        self.stage = stage
        self.root = materials_root
        self.inventory = inventory
        self.files = {}
        self.aliases = set()
        self.materials = {}
        self.required = {}
        self.used = set()

    def target(self, name, mode=0o644):
        safe_name(name)
        require(name.casefold() not in self.aliases, 'output member collision')
        self.aliases.add(name.casefold())
        path = self.stage / name
        path.parent.mkdir(parents=True, exist_ok=True)
        self.files[name] = {'mode': mode}
        return path

    def record(self, name):
        path = self.stage / name
        self.files[name].update(size=path.stat().st_size, sha256=digest(path))

    def write(self, name, data, mode=0o644):
        self.target(name, mode).write_bytes(data)
        self.record(name)

    def export_git(self, repo, commit, prefix, parent_id):
        entries = tree_entries(repo, commit)
        # cat-file --batch avoids one subprocess per blob and never runs working-tree filters.
        with subprocess.Popen(['git', '-c', f'safe.directory={repo}', '-C', str(repo), 'cat-file', '--batch'],
                              stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                              env={**os.environ, 'GIT_NO_REPLACE_OBJECTS': '1'}) as process:
            try:
                for name, mode, oid in entries:
                    if mode == '160000':
                        key = safe_name('gitlink/' + parent_id + '/' + name)
                        self.required[key] = {'kind': 'git', 'commit': oid, 'origin': 'gitlink:' + parent_id + '/' + name}
                        self.collect(key, self.required[key])
                        continue
                    process.stdin.write((oid + '\n').encode())
                    process.stdin.flush()
                    header = process.stdout.readline().decode().split()
                    require(len(header) == 3 and header[:2] == [oid, 'blob'], 'Git blob unavailable')
                    size = int(header[2])
                    destination = prefix + '/' + name
                    path = self.target(destination, 0o755 if mode == '100755' else 0o644)
                    h = hashlib.sha1(b'blob ' + str(size).encode() + b'\0')
                    with path.open('wb') as stream:
                        remaining = size
                        while remaining:
                            data = process.stdout.read(min(remaining, 1024 * 1024))
                            require(data, 'truncated Git blob')
                            stream.write(data)
                            h.update(data)
                            remaining -= len(data)
                    require(process.stdout.read(1) == b'\n' and h.hexdigest() == oid, 'Git blob hash mismatch')
                    self.record(destination)
                    if PurePosixPath(name).name == 'cpmfile.json':
                        self.add_cpm(parent_id, name, load_json(path.read_bytes()))
            finally:
                process.stdin.close()
                process.wait()
        require(process.returncode == 0, 'Git blob reader failed')

    def add_cpm(self, owner, source_path, definitions):
        require(isinstance(definitions, dict), 'invalid CPM definitions')
        for name, definition in definitions.items():
            require(isinstance(definition, dict), 'invalid CPM entry')
            key = safe_name('cpm/' + owner + '/' + source_path + '/' + name)
            sha = definition.get('hash')
            request = {'kind': 'archive', 'origin': definition}
            if sha:
                request['hashes'] = {'sha512': hexhash(sha, 128)}
            else:
                request['unhashed'] = True
            require(key not in self.required or self.required[key] == request, 'conflicting CPM definition')
            self.required[key] = request

    def copy_input(self, spec, destination):
        fields(spec, {'path', 'sha256'})
        path = local_file(self.root, spec['path'])
        target = self.target(destination)
        # Copy first, then verify only the private copy: no hash/use gap in emitted bytes.
        with path.open('rb') as source, target.open('wb') as output:
            shutil.copyfileobj(source, output, 1024 * 1024)
        self.record(destination)
        require(self.files[destination]['sha256'] == hexhash(spec['sha256']), 'input hash mismatch')
        return target

    def collect(self, key, request):
        require(key not in self.used, 'recursive/duplicate material')
        self.used.add(key)
        spec = self.inventory['materials'].get(key)
        require(spec is not None, 'missing material: ' + key)
        fields(spec, {'path'})
        if request['kind'] == 'git':
            repo = local_file(self.root, spec['path'], directory=True)
            commit = request['commit']
            require(git(repo, 'rev-parse', 'HEAD').decode().strip() == commit, 'dependency HEAD differs from pin')
            # Worktree changes are not substituted for a pinned source tree.
            # Check allowed tracked bytes directly, not git status (which can execute
            # repository-configured clean filters/fsmonitor or examine private names).
            entries = tree_entries(repo, commit)
            for name, mode, oid in entries:
                if mode == '160000':
                    continue
                path = local_file(repo, name)
                h = hashlib.sha1(b'blob ' + str(path.stat().st_size).encode() + b'\0')
                with path.open('rb') as stream:
                    while data := stream.read(1024 * 1024):
                        h.update(data)
                require(h.hexdigest() == oid, 'dirty dependency tracked files')
            tree = git(repo, 'rev-parse', commit + '^{tree}').decode().strip()
            self.export_git(repo, commit, 'materials/' + key, key)
            record = {**request, 'tree': tree, 'destination': 'materials/' + key}
        else:
            require(not request.get('unhashed'), 'unhashed CPM material cannot be included')
            source = local_file(self.root, spec['path'])
            destination = 'materials/' + key + ('.archive' if request['kind'] == 'archive' else '')
            target = self.target(destination)
            with source.open('rb') as stream, target.open('wb') as output:
                shutil.copyfileobj(stream, output, 1024 * 1024)
            self.record(destination)
            for algorithm, expected in request['hashes'].items():
                require(digest(target, algorithm) == expected, 'pinned material hash mismatch: ' + key)
            if request['kind'] == 'archive':
                for name, definitions in archive_index(target):
                    self.add_cpm(key, name, definitions)
            record = {**request, 'destination': destination, 'sha256': digest(target)}
        self.materials[key] = record


def relink(collector, state, snap):
    fields(state, {'status'}, {'snapshot', 'final_binary', 'files', 'receipt'})
    if state['status'] == 'PENDING':
        fields(state, {'status'})
        return {'status': 'PENDING', 'reason': 'Final-build objects, link recipe and binary-bound receipt not supplied.'}
    require(state['status'] == 'COLLECT', 'unknown relink status')
    fields(state, {'status', 'snapshot', 'final_binary', 'files', 'receipt'})
    require(isinstance(state['files'], list), 'invalid relink files inventory')
    require(state['snapshot'] == snap, 'stale relink snapshot')
    fields(state['final_binary'], {'path', 'sha256'})
    binary = local_file(collector.root, state['final_binary']['path'])
    binary_hash = hexhash(state['final_binary']['sha256'])
    require(digest(binary) == binary_hash, 'final binary hash mismatch')
    receipt_path = collector.copy_input(state['receipt'], 'relink/receipt.json')
    receipt = load_json(receipt_path.read_bytes())
    fields(receipt, {'schema', 'snapshot', 'final_binary_sha256', 'files', 'lgpl_sources'})
    require(receipt['schema'] == 1 and receipt['snapshot'] == snap
            and receipt['final_binary_sha256'] == binary_hash, 'stale/invalid final build receipt')
    pins = {name: collector.materials['deps/' + name]['hashes']['sha256'] for name in ('libsmb2', 'libnfs')}
    require(receipt['lgpl_sources'] == pins, 'relink LGPL source pins mismatch')
    roles = set()
    files = {}
    for entry in state['files']:
        fields(entry, {'name', 'role', 'path', 'sha256'})
        name = safe_name(entry['name'])
        require(name not in files, 'duplicate relink input')
        require(entry['role'] in {'non_lgpl_object', 'other_link_input', 'lgpl_library', 'commands',
                                 'link_script', 'toolchain_recipe', 'generated_source', 'link_map'}, 'unknown relink role')
        collector.copy_input({'path': entry['path'], 'sha256': entry['sha256']}, 'relink/inputs/' + name)
        files[name] = {'sha256': entry['sha256'], 'role': entry['role']}
        roles.add(entry['role'])
    require({'non_lgpl_object', 'commands', 'link_script', 'toolchain_recipe', 'lgpl_library'} <= roles,
            'missing relink material roles (source is not an object substitute)')
    require(all(name in files and files[name]['role'] == 'lgpl_library' for name in ('libsmb2.a', 'libnfs.a')),
            'both final-build LGPL libraries must be identified')
    require(receipt['files'] == files, 'receipt input inventory differs')
    return {'status': 'COLLECTED_UNVERIFIED_RELINK', 'snapshot': snap,
            'final_binary_sha256': binary_hash, 'files': files,
            'pending': 'Independent final-binary provenance, complete link closure and modified-library relink trial.'}


def write_tar(stage, files, output):
    # Output is exclusive and appears only after full validation. No partial final artifact.
    require(not output.exists() and not output.is_symlink(), 'output already exists')
    real_root(output.parent)
    fd, temporary = tempfile.mkstemp(prefix='.source-bundle-', dir=output.parent)
    try:
        with os.fdopen(fd, 'wb') as stream, tarfile.open(fileobj=stream, mode='w', format=tarfile.PAX_FORMAT) as archive:
            for name in sorted(files):
                record = files[name]
                info = tarfile.TarInfo(name)
                info.size = record['size']
                info.mode = record['mode']
                info.uid = info.gid = info.mtime = 0
                with (stage / name).open('rb') as source:
                    archive.addfile(info, source)
        # Hard-link publication is no-clobber on the same filesystem.
        os.link(temporary, output)
    finally:
        Path(temporary).unlink(missing_ok=True)


def prepare(args, stage):
    repo, root = real_root(args.repo), real_root(args.materials_root)
    snap = snapshot(repo, args.commit, args.tree)
    inputs = Path(os.path.abspath(args.inputs))
    real_root(inputs.parent)
    require(not inputs.is_symlink() and inputs.is_file(), 'invalid inventory file')
    inventory = load_json(inputs.read_bytes())
    fields(inventory, {'schema', 'snapshot', 'materials'}, {'cpm_omissions', 'extra_sources', 'relink'})
    require(inventory['schema'] == 1 and inventory['snapshot'] == snap, 'inventory snapshot mismatch')
    require(isinstance(inventory['materials'], dict), 'invalid materials inventory')
    require(isinstance(inventory.get('cpm_omissions', {}), dict)
            and isinstance(inventory.get('extra_sources', []), list), 'invalid supplemental inventory tables')
    collector = Collector(stage, root, inventory)
    collector.export_git(repo, snap['commit'], 'app', 'app')
    collector.required.update(requirements(stage / 'app'))
    # Eden first: CPM requests are derived from VERIFIED archive bytes, never the cache tree.
    collector.collect('deps/eden', collector.required['deps/eden'])
    return collector, snap, inventory


def create(args, stage):
    collector, snap, inventory = prepare(args, stage)
    if args.command == 'plan':
        result = {'schema': 1, 'snapshot': snap, 'requirements': collector.required,
                  'note': 'Plan only. Nested CPM/gitlinks are discovered as remaining materials are verified.'}
        output = Path(args.output).absolute()
        real_root(output.parent)
        with output.open('xb') as stream:
            stream.write(canonical(result))
        print('PLAN_ONLY sha256=' + digest(output))
        return
    omissions = inventory.get('cpm_omissions', {})
    decisions = {}
    # Process newly discovered nested CPM and gitlink inputs to a fixed point.
    while set(collector.required) - collector.used - decisions.keys():
        key = sorted(set(collector.required) - collector.used - decisions.keys())[0]
        if key in omissions:
            require(key.startswith('cpm/'), 'direct pinned dependency cannot be omitted')
            fields(omissions[key], {'reason', 'evidence'})
            reason = omissions[key]['reason']
            require(isinstance(reason, str) and 12 <= len(reason) <= 500, 'missing CPM omission reason')
            evidence = safe_name('app/' + omissions[key]['evidence'])
            require(evidence in collector.files, 'omission evidence is not in declared snapshot')
            decisions[key] = {**omissions[key], 'status': 'OPERATOR_DECLARED_NOT_USED',
                              'evidence_sha256': collector.files[evidence]['sha256']}
        else:
            collector.collect(key, collector.required[key])
    require(set(omissions) == decisions.keys(), 'unknown/unused omission decision')
    require(set(inventory['materials']) == collector.used, 'unknown/unused material input')
    extras = set()
    for item in inventory.get('extra_sources', []):
        fields(item, {'id', 'path', 'sha256', 'origin'})
        name = safe_name(item['id'])
        require(name not in extras and re.fullmatch(r'https://[^\s?#@]+', item['origin']), 'invalid supplemental provenance')
        extras.add(name)
        target = collector.copy_input({'path': item['path'], 'sha256': item['sha256']}, 'extra-sources/' + name)
        archive_index(target)
        collector.materials['extra/' + name] = {'sha256': item['sha256'], 'origin': item['origin'],
                                               'status': 'OPERATOR_SUPPLIED_SOURCE_REQUIRES_BINARY_MATCH_REVIEW'}
    link = relink(collector, inventory.get('relink', {'status': 'PENDING'}), snap)
    manifest = {'schema': 1, 'snapshot': snap, 'tool_sha256': digest(Path(__file__)),
                'source_status': 'PINNED_INPUTS_COLLECTED_FINAL_BINARY_AND_TRANSITIVE_SOURCE_REVIEW_PENDING',
                'scope': 'No legal-compliance or final-package-readiness certification.',
                'materials': collector.materials, 'requirements': collector.required,
                'cpm_omissions': decisions, 'relink': link,
                'pending': ['Compare used dependency/source closure with final link/build inventory.',
                            'Resolve binary SDK/pacbrew/transitive sources and local upstream modifications.',
                            'Verify corresponding-source delivery and static relink route independently.'],
                'files': dict(collector.files)}
    collector.write('SOURCE_BUNDLE.json', canonical(manifest))
    output = Path(args.output).absolute()
    write_tar(stage, collector.files, output)
    print('PINNED_INPUTS_COLLECTED relink=' + link['status'] + ' sha256=' + digest(output))


def verify(args):
    path = Path(args.bundle)
    real_root(path.absolute().parent)
    require(not path.is_symlink(), 'bundle symlink refused')
    require(digest(path) == hexhash(args.sha256), 'bundle hash mismatch')
    files = {}
    manifest = None
    seen = set()
    with tarfile.open(path, 'r:') as archive:
        for info in archive:
            safe_name(info.name)
            require(info.isfile() and info.name.casefold() not in seen, 'duplicate/special bundle member')
            seen.add(info.name.casefold())
            require(info.uid == info.gid == info.mtime == 0 and not info.uname and not info.gname
                    and info.mode in {0o644, 0o755}, 'nondeterministic bundle metadata')
            stream = archive.extractfile(info)
            if info.name == 'SOURCE_BUNDLE.json':
                require(info.size < 64_000_000, 'oversize bundle manifest')
                data = stream.read()
                manifest = load_json(data)
                require(canonical(manifest) == data, 'noncanonical manifest')
            else:
                files[info.name] = {'sha256': hashlib.file_digest(stream, 'sha256').hexdigest(),
                                    'size': info.size, 'mode': info.mode}
    require(manifest is not None and manifest['schema'] == 1, 'missing/invalid bundle manifest')
    require(manifest['snapshot'] == {'commit': hexhash(args.commit, 40), 'tree': hexhash(args.tree, 40)},
            'bundle snapshot mismatch')
    require(manifest['files'] == files, 'bundle inventory/hash mismatch')
    require(manifest['relink']['status'] in {'PENDING', 'COLLECTED_UNVERIFIED_RELINK'}, 'invalid relink claim')
    print('VERIFIED_BYTES_ONLY files=' + str(len(files)) + ' relink=' + manifest['relink']['status'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    for command in ('plan', 'create'):
        p = sub.add_parser(command)
        for option in ('repo', 'commit', 'tree', 'inputs', 'materials-root', 'output'):
            p.add_argument('--' + option, required=True)
    p = sub.add_parser('verify')
    for option in ('bundle', 'sha256', 'commit', 'tree'):
        p.add_argument('--' + option, required=True)
    args = parser.parse_args()
    try:
        if args.command == 'verify':
            verify(args)
        else:
            # Private scratch only; no writes to repository or dependency roots.
            with tempfile.TemporaryDirectory(prefix='prosperoeden-source-') as temporary:
                create(args, Path(temporary))
    except (Refused, OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError,
            tarfile.TarError, zipfile.BadZipFile) as error:
        # Never echo an exception containing local paths, credentials or arbitrary input bytes.
        message = str(error) if isinstance(error, Refused) else type(error).__name__
        print('REFUSED: ' + message, file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
