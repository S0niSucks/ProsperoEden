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
    require(all(not any(c in '<>"|?*' for c in p) and not re.fullmatch(
                    r'(?i:con|prn|aux|nul|conin\$|conout\$|clock\$|com[1-9¹²³]|lpt[1-9¹²³])(?:\..*)?', p)
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


def git_name(name, review=None):
    # Only this literal root template can bypass the name denial. In particular,
    # aliases, nested templates, app secrets and other .env.* remain pre-read denied.
    if name == '.env.example' and review and review.get('templates'):
        return name
    return safe_name(name)


def object_hash(kind, data):
    return hashlib.sha1(kind.encode() + b' ' + str(len(data)).encode() + b'\0' + data).hexdigest()


def git_review(policy, owner, commit, tree):
    review = policy.get('git', {}).get(owner)
    if review is None:
        return None
    require(owner != 'app', 'app cannot authorize private paths or Git links')
    fields(review, {'commit', 'tree', 'reason', 'templates', 'symlinks'})
    require(review['commit'] == commit and review['tree'] == tree, 'Git policy pin mismatch')
    require(isinstance(review['reason'], str) and 12 <= len(review['reason']) <= 500, 'missing Git policy reason')
    require(isinstance(review['templates'], dict) and isinstance(review['symlinks'], dict), 'invalid Git policy entries')
    for name, row in review['templates'].items():
        require(name == '.env.example', 'template exception cannot authorize this name')
        fields(row, {'sha256', 'reason'})
        hexhash(row['sha256'])
        require(isinstance(row['reason'], str) and 12 <= len(row['reason']) <= 500, 'missing template reason')
    for name, checksum in review['symlinks'].items():
        safe_name(name)
        hexhash(checksum)
    return review


def link_graph(entries):
    """Validate a closed component namespace, then resolve links WITHOUT filesystem I/O.

    Every prefix is case-unique and directories cannot alias regular/link entries.
    Resolution follows prefix links before '..'; directory edges detect cycles
    through descendants (not just direct link chains). Hardlinks never chain.
    """
    nodes, aliases = {}, {}
    for name, row in entries.items():
        # Templates, if any, have already passed the exact Git review gate.
        parts = name.split('/')
        for i in range(1, len(parts) + 1):
            prefix = '/'.join(parts[:i])
            kind = row['kind'] if i == len(parts) else 'dir'
            alias = prefix.casefold()
            require(alias not in aliases or aliases[alias] == prefix, 'case/prefix alias refused')
            aliases[alias] = prefix
            require(prefix not in nodes or nodes[prefix]['kind'] == kind == 'dir', 'file/link prefix collision')
            if prefix not in nodes:
                nodes[prefix] = row if i == len(parts) else {'kind': 'dir'}

    def target_parts(target):
        require(isinstance(target, str) and target and not target.startswith('/') and '\\' not in target
                and ':' not in target, 'unsafe link target')
        for part in target.split('/'):
            if part not in {'', '.', '..'}:
                safe_name(part)  # private segments denied even if subsequently cancelled by '..'
        return target.split('/')

    def resolve(parts, seen=None):
        current, queue, seen = [], list(parts), set() if seen is None else set(seen)
        while queue:
            part = queue.pop(0)
            if part in {'', '.'}:
                continue
            if part == '..':
                require(current, 'escaping link target')
                current.pop()
                continue
            current.append(part)
            name = '/'.join(current)
            require(name in nodes, 'missing/case-aliased link target')
            node = nodes[name]
            if node['kind'] == 'symlink':
                require(name not in seen, 'link cycle')
                seen.add(name)
                current.pop()
                queue = target_parts(node['target']) + queue
            else:
                require(not queue or node['kind'] == 'dir', 'link target traverses a non-directory')
        return '/'.join(current)

    directories = {'': set()}
    for name, row in nodes.items():
        parent = str(PurePosixPath(name).parent)
        parent = '' if parent == '.' else parent
        directories.setdefault(parent, set())
        if row['kind'] == 'dir':
            directories.setdefault(name, set())
            directories[parent].add(name)
        elif row['kind'] == 'hardlink':
            target = row['target']
            safe_name(target)
            require(target in nodes and nodes[target]['kind'] == 'file', 'hardlink target must be an existing regular member')
        elif row['kind'] == 'symlink':
            target = resolve(([] if not parent else parent.split('/')) + target_parts(row['target']))
            require(not target or nodes[target]['kind'] in {'file', 'dir'}, 'link target is not regular/directory')
            if not target or nodes[target]['kind'] == 'dir':
                directories[parent].add(target)
    active, done = set(), set()

    def visit(directory):
        require(directory not in active, 'directory-link descendant cycle')
        if directory in done:
            return
        active.add(directory)
        for child in directories.get(directory, ()):
            visit(child)
        active.remove(directory)
        done.add(directory)

    for directory in directories:
        visit(directory)
    return nodes


def tree_entries(repo, commit, review=None):
    result = []
    aliases = set()
    for entry in git(repo, 'ls-tree', '-rtz', '--full-tree', commit).split(b'\0'):
        if not entry:
            continue
        meta, raw_name = entry.split(b'\t', 1)
        mode, kind, oid = meta.decode().split()
        if mode == '040000':
            safe_name(raw_name.decode('utf-8'))
            continue
        name = git_name(raw_name.decode('utf-8'), review)
        alias = name.casefold()
        require(alias not in aliases, 'case-alias paths refused')
        aliases.add(alias)
        require(mode in {'100644', '100755', '160000'} or
                mode == '120000' and review and name in review['symlinks'], 'Git symlink/special entry refused')
        require(kind == ('commit' if mode == '160000' else 'blob'), 'invalid Git entry type')
        result.append((name, mode, oid))
    return result


def tree_proof(repo, commit):
    raw = git(repo, 'cat-file', 'commit', commit)
    require(object_hash('commit', raw) == commit, 'Git commit object mismatch')
    root = raw.split(b'\n', 1)[0].decode().removeprefix('tree ')
    trees = {root}
    for entry in git(repo, 'ls-tree', '-rtz', commit).split(b'\0'):
        if entry.startswith(b'040000 tree '):
            trees.add(entry.split(b'\t', 1)[0].decode().split()[2])
    return {'commit_object': raw.hex(), 'tree_objects': {oid: git(repo, 'cat-file', 'tree', oid).hex() for oid in sorted(trees)}}


def proof_entries(proof, review=None):
    """Recompute every immutable Git tree/commit object; no export-ignore/subst."""
    raw = bytes.fromhex(proof['commit_object'])
    require(object_hash('commit', raw) == hexhash(proof['commit'], 40), 'commit proof mismatch')
    require(raw.split(b'\n', 1)[0] == ('tree ' + hexhash(proof['tree'], 40)).encode(), 'commit/tree proof mismatch')
    result, used = {}, set()

    def walk(oid, prefix, ancestors):
        require(oid not in ancestors, 'recursive Git tree')
        data = bytes.fromhex(proof['tree_objects'][oid])
        require(object_hash('tree', data) == hexhash(oid, 40), 'tree proof mismatch')
        used.add(oid)
        offset, names = 0, set()
        while offset < len(data):
            end = data.index(b'\0', offset)
            mode, raw_name = data[offset:end].split(b' ', 1)
            name = raw_name.decode('utf-8')
            require(name and '/' not in name and name not in names, 'invalid Git tree name')
            names.add(name)
            require(end + 21 <= len(data), 'truncated Git tree')
            child = data[end+1:end+21].hex()
            path = prefix + name
            if mode == b'40000':
                safe_name(path)
                result[path] = ('040000', child)
                walk(child, path + '/', ancestors | {oid})
            else:
                git_name(path, review)
                require(mode in {b'100644', b'100755', b'120000', b'160000'}, 'invalid Git proof mode')
                result[path] = (mode.decode(), child)
            offset = end + 21
    walk(proof['tree'], '', set())
    require(used == proof['tree_objects'].keys(), 'unused Git tree proof')
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


def archive_index(path, review=None, _checked=False):
    """Validate member names/types without extraction, return embedded CPM definitions.

    Upstream archives may contain relative internal symlinks (e.g. SDK libraries).
    They are retained only as bytes in the original archive; escaping links are refused.
    Local filesystem links are NEVER followed. Consumers must use safe unpacking.
    """
    hardlinks = {}
    if review is not None:
        fields(review, {'sha256', 'reason', 'hardlinks'})
        require(_checked or digest(path) == hexhash(review['sha256']), 'archive policy hash mismatch')
        require(isinstance(review['reason'], str) and 12 <= len(review['reason']) <= 500, 'missing archive policy reason')
        require(isinstance(review['hardlinks'], dict), 'invalid hardlink review')
        hardlinks = review['hardlinks']
        for name, row in hardlinks.items():
            safe_name(name)
            fields(row, {'target', 'reason'})
            safe_name(row['target'])
            require(isinstance(row['reason'], str) and 12 <= len(row['reason']) <= 500, 'missing hardlink reason')
    with path.open('rb') as source:
        magic = source.read(4)
    if magic == b'\x28\xb5\x2f\xfd':
        with tempfile.TemporaryDirectory(prefix='source-zstd-') as temporary:
            expanded = Path(temporary) / 'source.tar'
            expand_zstd(path, expanded)
            return archive_index(expanded, review, _checked=True)
    definitions = []
    seen, entries, used_hardlinks = set(), {}, set()

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
                entries[name] = {'kind': 'dir' if info.is_dir() else 'file'}
                if name.endswith('/cpmfile.json') or name == 'cpmfile.json':
                    require(info.file_size < 2_000_000, 'oversize CPM definition')
                    definitions.append((name, load_json(archive.read(info))))
    else:
        with tarfile.open(path, 'r:*') as archive:
            for info in archive:
                name = member(info.name, info.linkname if info.issym() else None)
                if info.islnk():
                    require(name in hardlinks and info.linkname == hardlinks[name]['target'], 'archive hardlink/special entry refused')
                    used_hardlinks.add(name)
                    entries[name] = {'kind': 'hardlink', 'target': info.linkname}
                else:
                    require(info.isfile() or info.isdir() or info.issym(), 'archive hardlink/special entry refused')
                    entries[name] = {'kind': 'symlink', 'target': info.linkname} if info.issym() else {
                        'kind': 'dir' if info.isdir() else 'file'}
                if info.isfile() and (info.name.endswith('/cpmfile.json') or info.name == 'cpmfile.json'):
                    require(info.size < 2_000_000, 'oversize CPM definition')
                    definitions.append((info.name, load_json(archive.extractfile(info).read())))
    require(used_hardlinks == hardlinks.keys(), 'unused hardlink exception')
    link_graph(entries)
    return definitions


def requirements(app):
    deps = load_json((app / 'tools/deps.json').read_bytes())
    upstream = load_json((app / 'UPSTREAM.json').read_bytes())
    return requirements_data(deps, upstream)


def requirements_data(deps, upstream):
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
        self.policy = inventory.get('source_policy', {'git': {}, 'archives': {}})
        fields(self.policy, {'git', 'archives'})
        require(isinstance(self.policy['git'], dict) and isinstance(self.policy['archives'], dict), 'invalid source policy')
        self.git_sources = {}
        self.policy_used = {'git': set(), 'archives': set()}

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
        tree = git(repo, 'rev-parse', commit + '^{tree}').decode().strip()
        review = git_review(self.policy, parent_id, commit, tree)
        entries = tree_entries(repo, commit, review)
        proof = {'commit': commit, 'tree': tree, 'prefix': prefix, **tree_proof(repo, commit), 'entries': {}}
        expected = proof_entries(proof, review)
        require({n: (m, o) for n, m, o in entries} == {n: v for n, v in expected.items() if v[0] != '040000'},
                'Git ls-tree/proof disagreement')
        graph = {n: {'kind': 'dir'} for n, (mode, _) in expected.items() if mode == '040000'}
        templates, links = set(), set()
        # cat-file --batch avoids one subprocess per blob and never runs working-tree filters.
        with subprocess.Popen(['git', '-c', f'safe.directory={repo}', '-C', str(repo), 'cat-file', '--batch'],
                              stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                              env={**os.environ, 'GIT_NO_REPLACE_OBJECTS': '1'}) as process:
            try:
                for name, mode, oid in entries:
                    if mode == '160000':
                        proof['entries'][name] = {'mode': mode, 'oid': oid}
                        graph[name] = {'kind': 'gitlink'}
                        key = safe_name('gitlink/' + parent_id + '/' + name)
                        self.required[key] = {'kind': 'git', 'commit': oid, 'origin': 'gitlink:' + parent_id + '/' + name}
                        self.collect(key, self.required[key])
                        continue
                    process.stdin.write((oid + '\n').encode())
                    process.stdin.flush()
                    header = process.stdout.readline().decode().split()
                    require(len(header) == 3 and header[:2] == [oid, 'blob'], 'Git blob unavailable')
                    size = int(header[2])
                    exceptional = mode == '120000' or name == '.env.example'
                    destination = ('git-blobs/' + hashlib.sha256(parent_id.encode()).hexdigest() + '/' + hashlib.sha256(name.encode()).hexdigest()
                                   if exceptional else prefix + '/' + name)
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
                    row = {'mode': mode, 'oid': oid, 'storage': destination, **self.files[destination]}
                    # Source mode is distinct from the outer regular transport mode.
                    row['mode'] = mode
                    if mode == '120000':
                        require(self.files[destination]['sha256'] == review['symlinks'][name], 'symlink review hash mismatch')
                        row['link_text'] = path.read_bytes().decode('utf-8')
                        graph[name] = {'kind': 'symlink', 'target': row['link_text']}
                        links.add(name)
                    else:
                        graph[name] = {'kind': 'file'}
                    if name == '.env.example':
                        require(mode in {'100644', '100755'} and self.files[destination]['sha256'] ==
                                review['templates'][name]['sha256'], 'template review hash/mode mismatch')
                        templates.add(name)
                    proof['entries'][name] = row
                    if mode != '120000' and PurePosixPath(name).name == 'cpmfile.json':
                        self.add_cpm(parent_id, name, load_json(path.read_bytes()))
            finally:
                process.stdin.close()
                process.wait()
        require(process.returncode == 0, 'Git blob reader failed')
        link_graph(graph)
        if review is not None:
            require(templates == review['templates'].keys() and links == review['symlinks'].keys(), 'unused Git review exception')
            self.policy_used['git'].add(parent_id)
        self.git_sources[parent_id] = proof

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
            tree = git(repo, 'rev-parse', commit + '^{tree}').decode().strip()
            require('tree' not in request or request['tree'] == tree, 'supplemental Git tree mismatch')
            review = git_review(self.policy, key, commit, tree)
            entries = tree_entries(repo, commit, review)
            for name, mode, oid in entries:
                if mode == '160000':
                    continue
                if mode == '120000' or name == '.env.example':
                    blob = git(repo, 'cat-file', 'blob', oid)
                    expected = review['symlinks'][name] if mode == '120000' else review['templates'][name]['sha256']
                    require(object_hash('blob', blob) == oid and hashlib.sha256(blob).hexdigest() == expected,
                            'immutable reviewed blob mismatch')
                if mode == '120000':
                    # Check every parent WITHOUT following filesystem symlinks; only
                    # the final reviewed link may be read, using readlink, never open.
                    parent = repo if '/' not in name else local_file(repo, name.rsplit('/', 1)[0], directory=True)
                    path = parent / name.rsplit('/', 1)[-1]
                    require(stat.S_ISLNK(path.lstat().st_mode), 'dirty dependency symlink mode')
                    require(object_hash('blob', os.fsencode(os.readlink(path))) == oid, 'dirty dependency symlink text')
                    continue
                if name == '.env.example':
                    path = repo / name  # exact immutable policy checked before any local metadata
                    require(stat.S_ISREG(path.lstat().st_mode), 'reviewed template is not a regular file')
                else:
                    path = local_file(repo, name)
                require(bool(path.stat().st_mode & 0o111) == (mode == '100755'), 'dirty dependency executable mode')
                h = hashlib.sha1(b'blob ' + str(path.stat().st_size).encode() + b'\0')
                with path.open('rb') as stream:
                    while data := stream.read(1024 * 1024):
                        h.update(data)
                require(h.hexdigest() == oid, 'dirty dependency tracked files')
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
                review = self.policy['archives'].get(key)
                for name, definitions in archive_index(target, review):
                    self.add_cpm(key, name, definitions)
                if review is not None:
                    self.policy_used['archives'].add(key)
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
    link_graph({name: {'kind': 'file'} for name in files})
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
    fields(inventory, {'schema', 'snapshot', 'materials'}, {'cpm_omissions', 'extra_sources', 'extra_git_sources', 'source_policy', 'relink'})
    require(inventory['schema'] == 1 and inventory['snapshot'] == snap, 'inventory snapshot mismatch')
    require(isinstance(inventory['materials'], dict), 'invalid materials inventory')
    require(isinstance(inventory.get('cpm_omissions', {}), dict)
            and isinstance(inventory.get('extra_sources', []), list), 'invalid supplemental inventory tables')
    collector = Collector(stage, root, inventory)
    collector.export_git(repo, snap['commit'], 'app', 'app')
    collector.required.update(requirements(stage / 'app'))
    require(isinstance(inventory.get('extra_git_sources', []), list), 'invalid supplemental Git table')
    for item in inventory.get('extra_git_sources', []):
        fields(item, {'id', 'path', 'commit', 'tree', 'origin'})
        key = safe_name('extra-git/' + item['id'])
        require(key not in collector.required and key not in inventory['materials']
                and re.fullmatch(r'https://[^\s?#@]+', item['origin']), 'duplicate/invalid supplemental Git input')
        collector.required[key] = {'kind': 'git', 'commit': hexhash(item['commit'], 40), 'tree': hexhash(item['tree'], 40),
                                   'origin': item['origin'], 'status': 'SUPPLEMENTAL_SOURCE_FINAL_BINARY_MATCH_REVIEW_PENDING'}
        inventory['materials'][key] = {'path': item['path']}
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
    extras = set()
    for item in inventory.get('extra_sources', []):
        fields(item, {'id', 'path', 'sha256', 'origin'})
        name = safe_name(item['id'])
        require(name not in extras and re.fullmatch(r'https://[^\s?#@]+', item['origin']), 'invalid supplemental provenance')
        extras.add(name)
        target = collector.copy_input({'path': item['path'], 'sha256': item['sha256']}, 'extra-sources/' + name)
        for source_path, definitions in archive_index(target):
            collector.add_cpm('extra/' + name, source_path, definitions)
        collector.materials['extra/' + name] = {'sha256': item['sha256'], 'origin': item['origin'],
                                               'status': 'OPERATOR_SUPPLIED_SOURCE_REQUIRES_BINARY_MATCH_REVIEW'}
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
    link = relink(collector, inventory.get('relink', {'status': 'PENDING'}), snap)
    require(all(collector.policy_used[k] == collector.policy[k].keys() for k in ('git', 'archives')), 'unused source policy')
    collector.write('bundle-tools/source_bundle.py', Path(__file__).read_bytes(), 0o755)
    manifest = {'schema': 2, 'snapshot': snap, 'tool_sha256': digest(Path(__file__)),
                'source_status': 'PINNED_INPUTS_COLLECTED_FINAL_BINARY_AND_TRANSITIVE_SOURCE_REVIEW_PENDING',
                'scope': 'No legal-compliance or final-package-readiness certification.',
                'materials': collector.materials, 'requirements': collector.required,
                'cpm_omissions': decisions, 'relink': link, 'source_policy': collector.policy, 'git_sources': collector.git_sources,
                'pending': ['Compare used dependency/source closure with final link/build inventory.',
                            'Resolve binary SDK/pacbrew/transitive sources and local upstream modifications.',
                            'Verify corresponding-source delivery and static relink route independently.'],
                'files': dict(collector.files)}
    collector.write('SOURCE_BUNDLE.json', canonical(manifest))
    output = Path(args.output).absolute()
    write_tar(stage, collector.files, output)
    print('PINNED_INPUTS_COLLECTED relink=' + link['status'] + ' sha256=' + digest(output))


def verify_git_sources(manifest, files=None, blobs=None):
    policy = manifest.get('source_policy', {'git': {}, 'archives': {}})
    fields(policy, {'git', 'archives'})
    require(isinstance(manifest.get('git_sources'), dict) and 'app' in manifest['git_sources'], 'missing Git source proofs')
    require({k for k, v in manifest['materials'].items() if v.get('kind') == 'git'} == manifest['git_sources'].keys() - {'app'},
            'missing/extra material Git proofs')
    used = set()
    for owner, proof in manifest.get('git_sources', {}).items():
        safe_name(owner)
        safe_name(proof['prefix'])
        if owner == 'app':
            require({k: proof[k] for k in ('commit', 'tree')} == manifest['snapshot'], 'app Git proof snapshot mismatch')
        else:
            material = manifest['materials'][owner]
            require(material['kind'] == 'git' and material['commit'] == proof['commit'] and
                    material['tree'] == proof['tree'] and material['destination'] == proof['prefix'], 'material Git proof mismatch')
        review = git_review(policy, owner, proof['commit'], proof['tree'])
        expected = proof_entries(proof, review)
        rows = proof['entries']
        require(rows.keys() == {n for n, v in expected.items() if v[0] != '040000'}, 'Git proof omitted/extra source')
        graph = {n: {'kind': 'dir'} for n, v in expected.items() if v[0] == '040000'}
        templates, links = set(), set()
        for name, row in rows.items():
            git_name(name, review)
            require((row['mode'], row['oid']) == expected[name], 'Git proof source mode/blob mismatch')
            if row['mode'] == '160000':
                key = 'gitlink/' + owner + '/' + name
                require(manifest['git_sources'][key]['commit'] == row['oid'], 'missing/mismatched gitlink proof')
                graph[name] = {'kind': 'gitlink'}
                continue
            safe_name(row['storage'])
            mode = 0o755 if row['mode'] == '100755' else 0o644
            if files is not None:
                require(files.get(row['storage']) == {'sha256': row['sha256'], 'size': row['size'], 'mode': mode}
                        and blobs[row['storage']] == row['oid'], 'Git source bytes/mode mismatch')
            if row['mode'] == '120000':
                require(review and name in review['symlinks'] and row['sha256'] == review['symlinks'][name], 'unreviewed Git symlink')
                data = row['link_text'].encode('utf-8')
                require(len(data) < 4096 and len(data) == row['size'] and hashlib.sha256(data).hexdigest() == row['sha256']
                        and object_hash('blob', data) == row['oid'], 'link text/blob mismatch')
                graph[name] = {'kind': 'symlink', 'target': row['link_text']}
                links.add(name)
            else:
                require(row['mode'] in {'100644', '100755'}, 'invalid source mode')
                graph[name] = {'kind': 'file'}
            if name == '.env.example':
                require(review and row['mode'] != '120000' and row['sha256'] == review['templates'][name]['sha256'], 'template policy mismatch')
                templates.add(name)
        link_graph(graph)
        if review is not None:
            require(templates == review['templates'].keys() and links == review['symlinks'].keys(), 'unused Git policy entry')
            used.add(owner)
    require(used == policy['git'].keys(), 'unused Git source policy')


def verify_materials(manifest, archive):
    """Rediscover direct pins, recursive CPM and Gitlinks from verified source bytes."""
    app = manifest['git_sources']['app']['entries']
    deps = load_json(archive.extractfile(app['tools/deps.json']['storage']).read())
    upstream = load_json(archive.extractfile(app['UPSTREAM.json']['storage']).read())
    parser = Collector(None, None, {})
    parser.required.update(requirements_data(deps, upstream))
    materials = manifest['materials']
    for owner, proof in manifest['git_sources'].items():
        for name, row in proof['entries'].items():
            if row['mode'] == '160000':
                parser.required['gitlink/' + owner + '/' + name] = {
                    'kind': 'git', 'commit': row['oid'], 'origin': 'gitlink:' + owner + '/' + name}
            elif row['mode'] != '120000' and PurePosixPath(name).name == 'cpmfile.json':
                parser.add_cpm(owner, name, load_json(archive.extractfile(row['storage']).read()))
        if owner.startswith('extra-git/'):
            row = materials[owner]
            parser.required[owner] = {key: row[key] for key in ('kind', 'commit', 'tree', 'origin', 'status')}
    archive_policies = manifest['source_policy']['archives']
    require(archive_policies.keys() <= materials.keys(), 'unused archive source policy')
    for key, row in materials.items():
        safe_name(key)
        extra = key.startswith('extra/')
        destination = safe_name('extra-sources/' + key.removeprefix('extra/') if extra else row['destination'])
        if row.get('kind') == 'git':
            continue
        require(manifest['files'][destination]['sha256'] == row['sha256'], 'material transport hash mismatch')
        review = archive_policies.get(key)
        if review is not None:
            require(not extra and row['kind'] == 'archive' and review['sha256'] == row['sha256'], 'archive policy identity mismatch')
        if row.get('kind') == 'archive' or extra:
            with tempfile.TemporaryDirectory(prefix='verify-source-') as temporary:
                source = Path(temporary) / 'input.archive'
                with source.open('xb') as output:
                    shutil.copyfileobj(archive.extractfile(destination), output)
                for name, definitions in archive_index(source, review):
                    parser.add_cpm(key, name, definitions)
    require(parser.required == manifest['requirements'], 'rediscovered source requirements differ')
    decisions = manifest['cpm_omissions']
    require(materials.keys() - {k for k in materials if k.startswith('extra/')} == parser.required.keys() - decisions.keys(),
            'missing/extra source material')
    for key, request in parser.required.items():
        if key in decisions:
            row = decisions[key]
            fields(row, {'reason', 'evidence', 'status', 'evidence_sha256'})
            evidence = 'app/' + safe_name(row['evidence'])
            require(key.startswith('cpm/') and row['status'] == 'OPERATOR_DECLARED_NOT_USED'
                    and isinstance(row['reason'], str) and 12 <= len(row['reason']) <= 500
                    and manifest['files'][evidence]['sha256'] == row['evidence_sha256'], 'invalid omission evidence')
            continue
        row = materials[key]
        require(all(row.get(k) == v for k, v in request.items()), 'material differs from immutable source pin')
        require(not request.get('unhashed'), 'unhashed CPM material')
        for algorithm, expected in request.get('hashes', {}).items():
            require(hashlib.file_digest(archive.extractfile(row['destination']), algorithm).hexdigest() == expected,
                    'immutable source material hash mismatch')
    require(decisions.keys() <= parser.required.keys(), 'unused omission')


def verify(args):
    path = Path(args.bundle)
    real_root(path.absolute().parent)
    require(not path.is_symlink(), 'bundle symlink refused')
    require(digest(path) == hexhash(args.sha256), 'bundle hash mismatch')
    files, blobs = {}, {}
    manifest = None
    seen = set()
    with tarfile.open(path, 'r:') as preflight:
        # Inspect all names, then immutable proof paths BEFORE payload/blob reads.
        for info in preflight.getmembers():
            safe_name(info.name)
            require(info.isfile(), 'special bundle member')
        info = preflight.getmember('SOURCE_BUNDLE.json')
        require(info.size < 64_000_000, 'oversize bundle manifest')
        manifest = load_json(preflight.extractfile(info).read())
        verify_git_sources(manifest)
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
                h, blob = hashlib.sha256(), hashlib.sha1(b'blob ' + str(info.size).encode() + b'\0')
                while data := stream.read(1024 * 1024):
                    h.update(data)
                    blob.update(data)
                blobs[info.name] = blob.hexdigest()
                files[info.name] = {'sha256': h.hexdigest(),
                                    'size': info.size, 'mode': info.mode}
    require(manifest is not None and manifest['schema'] == 2, 'missing/invalid bundle manifest')
    require(manifest['snapshot'] == {'commit': hexhash(args.commit, 40), 'tree': hexhash(args.tree, 40)},
            'bundle snapshot mismatch')
    require(manifest['files'] == files, 'bundle inventory/hash mismatch')
    require(files['bundle-tools/source_bundle.py']['sha256'] == manifest['tool_sha256'], 'exporter source mismatch')
    link_graph({name: {'kind': 'file'} for name in files})
    verify_git_sources(manifest, files, blobs)
    require(manifest['relink']['status'] in {'PENDING', 'COLLECTED_UNVERIFIED_RELINK'}, 'invalid relink claim')
    with tarfile.open(path, 'r:') as archive:
        verify_materials(manifest, archive)
    print('VERIFIED_BYTES_ONLY files=' + str(len(files)) + ' relink=' + manifest['relink']['status'])
    return manifest


def unpack_archive(source, destination, review=None):
    """Audit first; create regulars first, then reviewed hardlinks, then safe symlinks.

    Never uses extractall or follows a link while writing. No recipe is executed.
    """
    for parent in [*reversed(destination.parents), destination]:
        require(not parent.is_symlink(), 'symlink in archive restore destination')
    require(not destination.exists(), 'archive restore destination already exists')
    archive_index(source, review)
    destination.mkdir(parents=True, exist_ok=False)
    with source.open('rb') as stream:
        zstd = stream.read(4) == b'\x28\xb5\x2f\xfd'
    if zstd:
        with tempfile.TemporaryDirectory(prefix='restore-zstd-') as temporary:
            expanded = Path(temporary) / 'source.tar'
            expand_zstd(source, expanded)
            # Hash-bound original bytes were audited above; no new policy is invented.
            _unpack_archive_bytes(expanded, destination)
    else:
        _unpack_archive_bytes(source, destination)


def _unpack_archive_bytes(source, destination):
    if zipfile.is_zipfile(source):
        with zipfile.ZipFile(source) as archive:
            for info in archive.infolist():
                target = destination / safe_name(info.filename.rstrip('/'))
                if info.is_dir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with archive.open(info) as incoming, target.open('xb') as output:
                        shutil.copyfileobj(incoming, output)
                    target.chmod(0o755 if info.external_attr >> 16 & 0o111 else 0o644)
        return
    with tarfile.open(source) as archive:
        links = []
        for info in archive:
            target = destination / safe_name(info.name.rstrip('/'))
            if info.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif info.isfile():
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(info) as incoming, target.open('xb') as output:
                    shutil.copyfileobj(incoming, output)
                target.chmod(0o755 if info.mode & 0o111 else 0o644)
            else:
                links.append(info)
        for info in sorted(links, key=lambda i: i.issym()):
            target = destination / info.name
            target.parent.mkdir(parents=True, exist_ok=True)
            if info.islnk():
                os.link(destination / info.linkname, target, follow_symlinks=False)
            else:
                target.symlink_to(info.linkname)


def restore_git_sources(proofs, policy, transport, destinations):
    """Reify already verified Git source proofs; never run checkout filters/hooks."""
    def source_tree(owner, root, ancestors):
        require(owner not in ancestors, 'recursive restored gitlink')
        root.mkdir(parents=True, exist_ok=False)
        proof = proofs[owner]
        review = git_review(policy, owner, proof['commit'], proof['tree'])
        expected = proof_entries(proof, review)
        for name, (mode, _) in sorted(expected.items()):
            if mode == '040000':
                (root / name).mkdir(parents=True, exist_ok=True)
        links = []
        for name, row in proof['entries'].items():
            target = root / git_name(name, review)
            target.parent.mkdir(parents=True, exist_ok=True)
            if row['mode'] == '120000':
                links.append((target, row['link_text']))
            elif row['mode'] == '160000':
                source_tree('gitlink/' + owner + '/' + name, target, ancestors | {owner})
            else:
                shutil.copyfile(local_file(transport, row['storage']), target)
                target.chmod(0o755 if row['mode'] == '100755' else 0o644)
        for target, text in links:
            target.symlink_to(text)
        for name, row in proof['entries'].items():
            if row['mode'] == '160000':
                continue  # checked recursively in its own immutable source proof
            path = root / name
            mode = path.lstat().st_mode
            if row['mode'] == '120000':
                require(stat.S_ISLNK(mode) and os.readlink(path) == row['link_text'], 'restored link-text mismatch')
            else:
                require(stat.S_ISREG(mode) and bool(mode & 0o111) == (row['mode'] == '100755')
                        and path.stat().st_size == row['size'] and digest(path) == row['sha256'], 'restored Git source mismatch')
    for owner, destination in sorted(destinations.items()):
        source_tree(owner, destination, set())


def restore(args):
    manifest = verify(args)
    output = Path(args.output).absolute()
    real_root(output.parent)
    require(not output.exists() and not output.is_symlink(), 'restore output already exists')
    output.mkdir()
    transport = output / 'transport'
    transport.mkdir()
    with tarfile.open(args.bundle, 'r:') as archive:
        for info in archive:
            target = transport / safe_name(info.name)
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.extractfile(info) as incoming, target.open('xb') as stream:
                shutil.copyfileobj(incoming, stream)
            target.chmod(info.mode)
    sources = output / 'sources'
    sources.mkdir()
    restore_git_sources(manifest['git_sources'], manifest['source_policy'], transport,
                        {owner: sources / owner for owner in manifest['git_sources']})
    unpacked = []
    for key in args.unpack_material:
        safe_name(key)
        material = manifest['materials'][key]
        require(material.get('kind') == 'archive' or key.startswith('extra/'), 'not an archive material')
        review = manifest['source_policy']['archives'].get(key)
        unpack_archive(transport / material.get('destination', 'extra-sources/' + key.removeprefix('extra/')),
                       output / 'unpacked' / key, review)
        unpacked.append(key)
    (output / 'RESTORE.json').write_bytes(canonical({'snapshot': manifest['snapshot'], 'relink': 'PENDING',
        'git_sources': sorted(manifest.get('git_sources', {})), 'unpacked_archives': unpacked,
        'status': 'RESTORED_SOURCE_INPUTS_ONLY_NOT_A_BUILD_OR_RELINK_TRIAL'}))
    print('RESTORED_SOURCE_INPUTS_ONLY relink=PENDING')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    for command in ('plan', 'create'):
        p = sub.add_parser(command)
        for option in ('repo', 'commit', 'tree', 'inputs', 'materials-root', 'output'):
            p.add_argument('--' + option, required=True)
    for command in ('verify', 'restore'):
        p = sub.add_parser(command)
        for option in ('bundle', 'sha256', 'commit', 'tree'):
            p.add_argument('--' + option, required=True)
        if command == 'restore':
            p.add_argument('--output', required=True)
            p.add_argument('--unpack-material', action='append', default=[])
    args = parser.parse_args()
    try:
        if args.command == 'verify':
            verify(args)
        elif args.command == 'restore':
            restore(args)
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
