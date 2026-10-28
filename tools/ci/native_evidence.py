#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Opt-in, explicit-file native build evidence. See native-evidence-README.md.

Not a source closure, provenance receipt or relink verifier. No directory inputs,
environment dump, tool probes, network access or local symlink dereferencing.
"""
import argparse
import contextlib
import hashlib
import os
from pathlib import Path
import posixpath
import re
import shlex
import stat
import subprocess
import sys
import uuid

from source_bundle import (Refused, canonical, fields, hexhash, link_graph,
                           load_json, private, require, safe_name)


PENDING = ['source_binary_match', 'indirect_inputs_and_tool_providers',
           'used_source_closure', 'compatible_object_closure',
           'independent_provenance_verification', 'modified_library_relink']
# These are storage anchors ONLY, never permission to traverse their contents.
ANCHORS = {'.deps', '.cache', '.local'}
LIMIT = 16 * 1024 * 1024


def identifier(value):
    require(isinstance(value, str) and re.fullmatch('[a-z][a-z0-9_-]{0,79}', value),
            'invalid evidence identifier')
    safe_name(value)
    return value


def absolute(value):
    """Lexical checks precede *all* stat/traversal, including root ancestors."""
    require(isinstance(value, str) and value.startswith('/') and '//' not in value,
            'absolute POSIX path required')
    parts = value[1:].split('/')
    safe_name('/'.join(p for p in parts if p not in ANCHORS))
    require(all(p not in {'', '.', '..'} for p in parts), 'noncanonical root/path')
    return Path(value)


def inspect(path, leaf_link=False):
    """Never follow links, even intermediate links or nonexistent leaf parents."""
    for item in [*reversed(path.parents), path]:
        try:
            info = item.lstat()
        except FileNotFoundError:
            return None
        if item == path and leaf_link and stat.S_ISLNK(info.st_mode):
            return info
        require(stat.S_ISDIR(info.st_mode) if item != path else
                stat.S_ISDIR(info.st_mode) or stat.S_ISREG(info.st_mode),
                'symlink/special path refused')
    return info


@contextlib.contextmanager
def open_input(path):
    """O_NOFOLLOW on every component, including a parent swapped after lstat."""
    parent = os.open('/', os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    fd = None
    try:
        for part in path.parts[1:-1]:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=parent)
            os.close(parent)
            parent = child
        fd = os.open(path.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=parent)
        yield fd, parent
    finally:
        if fd is not None:
            os.close(fd)
        os.close(parent)


def read_regular(path, limit=None):
    info = inspect(path)
    require(info is not None and stat.S_ISREG(info.st_mode), 'regular file required')
    with open_input(path) as (fd, parent), os.fdopen(fd, 'rb', closefd=False) as stream:
        opened = os.fstat(stream.fileno())
        require((info.st_dev, info.st_ino, info.st_mode, info.st_size, info.st_mtime_ns) ==
                (opened.st_dev, opened.st_ino, opened.st_mode, opened.st_size, opened.st_mtime_ns),
                'input changed before read')
        data = stream.read() if limit is None else stream.read(limit + 1)
        require(limit is None or len(data) <= limit, 'metadata/response file too large')
        after = os.fstat(stream.fileno())
        require((opened.st_size, opened.st_mtime_ns, opened.st_ctime_ns) ==
                (after.st_size, after.st_mtime_ns, after.st_ctime_ns), 'input changed during read')
        require(os.stat(path.name, dir_fd=parent, follow_symlinks=False) == after, 'input replaced during read')
    return data, info


def write_new(path, data):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    with os.fdopen(fd, 'wb') as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())


def copy_regular(path, destination=None):
    """Stream-copy or fingerprint large files; never load native archives into RAM."""
    info = inspect(path)
    require(info is not None and stat.S_ISREG(info.st_mode), 'regular file required')
    checksum, size = hashlib.sha256(), 0
    with open_input(path) as (fd, parent), os.fdopen(fd, 'rb', closefd=False) as source:
        opened = os.fstat(source.fileno())
        require((info.st_dev, info.st_ino, info.st_mode, info.st_size, info.st_mtime_ns) ==
                (opened.st_dev, opened.st_ino, opened.st_mode, opened.st_size, opened.st_mtime_ns),
                'input replaced before copy')
        out = (os.open(destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
               if destination is not None else None)
        with os.fdopen(out, 'wb') if out is not None else contextlib.nullcontext() as target:
            while block := source.read(1024 * 1024):
                if target is not None:
                    target.write(block)
                checksum.update(block)
                size += len(block)
            if target is not None:
                target.flush()
                os.fsync(target.fileno())
        after = os.fstat(source.fileno())
        require((opened.st_size, opened.st_mtime_ns, opened.st_ctime_ns) ==
                (after.st_size, after.st_mtime_ns, after.st_ctime_ns) and
                os.stat(path.name, dir_fd=parent, follow_symlinks=False) == after,
                'input changed during copy')
    return {'sha256': checksum.hexdigest(), 'size': size}


def write_json(path, value):
    data = canonical(value)
    write_new(path, data)
    write_new(path.with_suffix(path.suffix + '.sha256'), hashlib.sha256(data).hexdigest().encode() + b'\n')


def read_json(path):
    data, info = read_regular(path, LIMIT)
    anchor, anchor_info = read_regular(path.with_suffix(path.suffix + '.sha256'), 65)
    require(all(stat.S_IMODE(row.st_mode) == 0o600 and row.st_nlink == 1
                for row in (info, anchor_info)), 'nonprivate/aliased metadata')
    require(anchor == hashlib.sha256(data).hexdigest().encode() + b'\n', 'manifest hash mismatch')
    return load_json(data)


def roots_check(roots, probe=True):
    require(isinstance(roots, dict) and roots, 'explicit approved roots required')
    aliases = set()
    for label, value in roots.items():
        identifier(label)
        path = absolute(value)
        alias = str(path).casefold()
        require(alias not in aliases, 'root alias refused')
        aliases.add(alias)
        require(len(path.parts) > 2, 'overbroad root refused')
    link_graph({value[1:]: {'kind': 'dir'} for value in roots.values()})
    # Finish the entire lexical/alias gate before the first metadata probe.
    for value in roots.values() if probe else []:
        path = Path(value)
        # An absent future build root is allowed, a symlink root is not.
        info = inspect(path)
        require(info is None or stat.S_ISDIR(info.st_mode), 'root must be a directory')


def locate_file(value, roots, directory):
    path = absolute(value)  # full private-name gate BEFORE any input metadata
    matches = [(label, Path(root)) for label, root in roots.items()
               if path.is_relative_to(Path(root)) and path != Path(root)]
    require(matches, 'file outside approved roots')
    label, root = max(matches, key=lambda item: len(item[1].parts))
    relative = path.relative_to(root).as_posix()
    safe_name(relative)
    require(not path.is_relative_to(directory), 'evidence cannot capture itself')
    return path, label, relative


def validate_files(values, roots, directory):
    names, physical = {}, {}
    for value in values:
        path, label, relative = locate_file(value, roots, directory)
        names[label + '/' + relative] = {'kind': 'file'}
        physical[str(path)[1:]] = {'kind': 'file'}
    link_graph(names)
    link_graph(physical)


def phase_files_check(selections, roots, directory):
    require(isinstance(selections, dict), 'invalid phase selections')
    for phase, values in selections.items():
        identifier(phase)
        require(isinstance(values, list) and all(isinstance(v, str) for v in values), 'invalid file list')
        validate_files(values, roots, directory)


def initialize(directory, declarations, phase_files=()):
    destination = absolute(directory)
    roots = {}
    for item in declarations:
        label, value = item.split('=', 1)
        require(label not in roots, 'duplicate root')
        roots[label] = value
    roots_check(roots, probe=False)
    selections = {}
    for item in phase_files:
        phase, path = item.split('=', 1)
        selections.setdefault(phase, []).append(path)
    phase_files_check(selections, roots, destination)
    roots_check(roots)
    require(all(not destination.is_relative_to(Path(p)) and
                not Path(p).is_relative_to(destination) for p in roots.values()),
            'evidence and input roots overlap')
    info = inspect(destination.parent)
    require(info is not None and stat.S_ISDIR(info.st_mode), 'existing evidence parent required')
    destination.mkdir(mode=0o700)  # fail even if an existing directory is empty
    (destination / 'events').mkdir(mode=0o700)
    write_json(destination / 'run.json', {'schema': 1, 'kind': 'native-evidence',
               'roots': roots, 'phase_files': selections, 'qualification': 'PENDING', 'pending': PENDING,
               'privacy_policy': 'source_bundle.safe_name; no template exceptions'})


class Evidence:
    def __init__(self, directory, probe=True):
        self.directory = absolute(directory)
        info = inspect(self.directory)
        require(info is not None and stat.S_ISDIR(info.st_mode) and
                stat.S_IMODE(info.st_mode) == 0o700, 'private evidence directory required')
        self.run = read_json(self.directory / 'run.json')
        fields(self.run, {'schema', 'kind', 'roots', 'phase_files', 'qualification', 'pending', 'privacy_policy'})
        require(self.run['schema'] == 1 and self.run['kind'] == 'native-evidence' and
                self.run['qualification'] == 'PENDING' and self.run['pending'] == PENDING,
                'unsupported evidence qualification')
        self.roots = self.run['roots']
        roots_check(self.roots, probe=False)
        phase_files_check(self.run['phase_files'], self.roots, self.directory)
        roots_check(self.roots, probe=probe)
        require(all(not self.directory.is_relative_to(Path(p)) and
                    not Path(p).is_relative_to(self.directory) for p in self.roots.values()),
                'evidence and input roots overlap')

    def locate(self, value):
        return locate_file(value, self.roots, self.directory)

    def validate_files(self, values):
        validate_files(values, self.roots, self.directory)

    def capture(self, event, values):
        self.validate_files(values)
        rows, graph = [], {}
        for value in dict.fromkeys(values):
            path, label, relative = self.locate(value)
            row = {'root': label, 'path': relative}
            info = inspect(path, leaf_link=True)
            if info is None:
                row['kind'] = 'missing'
            else:
                row['mode'] = stat.S_IMODE(info.st_mode)
                if stat.S_ISLNK(info.st_mode):
                    text = os.readlink(path)
                    # Check privacy before retaining link text; closed graph checked below.
                    safe_link(relative, text)
                    row.update(kind='symlink', target=text)
                    data = os.fsencode(text)
                    require(path.lstat() == info, 'link changed during read')
                else:
                    require(stat.S_ISREG(info.st_mode), 'directory/special input refused')
                    row['kind'] = 'file'
                row['blob'] = 'files/' + uuid.uuid4().hex
                if row['kind'] == 'symlink':
                    row.update(size=len(data), sha256=hashlib.sha256(data).hexdigest())
                    write_new(event / row['blob'], data)
                else:
                    row.update(copy_regular(path, event / row['blob']))
            rows.append(row)
            if row['kind'] != 'missing':
                graph[label + '/' + relative] = {'kind': row['kind'], 'target': row.get('target')}
        link_graph(graph)  # links require explicitly supplied same-root targets; never follow them
        return rows

    def event(self, phase):
        identifier(phase)
        parent = self.directory / 'events'
        require(stat.S_ISDIR(inspect(parent).st_mode), 'invalid events directory')
        event = parent / (phase + '-' + uuid.uuid4().hex)
        event.mkdir(mode=0o700)
        (event / 'files').mkdir(mode=0o700)
        return event


def argv_privacy(arguments):
    for argument in arguments:
        require(isinstance(argument, str) and not any(ord(c) < 32 or ord(c) == 127 for c in argument),
                'control character in argv')
        require('://' not in argument, 'network argv refused')
        require(not re.search(r'(?i)(?:^|[-_])(?:password|token|secret|credential|api.key)=', argument),
                'credential argv refused')
        for token in re.split('[=,]', argument):
            token = token.lstrip('@')
            for prefix in ('-isystem', '-iquote', '-include', '-imacros', '-I', '-L', '-T'):
                if token.startswith(prefix) and len(token) > len(prefix):
                    token = token[len(prefix):]
                    break
            if not token:
                continue
            # Reuse the complete privacy/name policy, not a separate .env filter.
            parts = [p for p in token.split('/') if p not in ANCHORS]
            probe = '/'.join(parts).lstrip('/')
            if probe:
                # Flags/expressions may contain punctuation not valid in file names.
                require(not private(probe), 'private argv refused')


def safe_link(relative, text):
    require(isinstance(text, str) and not text.startswith('/') and '\\' not in text and ':' not in text,
            'absolute/alternate link target refused')
    safe_name('/'.join(p for p in text.split('/') if p not in {'', '.', '..'}))
    target = posixpath.normpath(posixpath.join(posixpath.dirname(relative), text))
    safe_name(target)  # never escape a selected root, even into another approved root


def command_path(value):
    # Normalize trusted command syntax, not caller-supplied explicit file selections.
    argv_privacy([value])
    return os.path.abspath(value)


def link_inputs(evidence, argv):
    """Capture explicit lld/response inputs, never search -L roots or parse GROUP scripts."""
    inputs, outputs, gaps, seen, raw_paths = [], [], [], set(), []

    def add(value, output=False):
        path = command_path(value)
        evidence.locate(path)
        (outputs if output else inputs).append(path)
        raw_paths.append(value)

    def parse(arguments, depth=0):
        require(depth <= 8, 'nested response depth exceeded')
        argv_privacy(arguments)
        i, responses = 0, []
        while i < len(arguments):
            arg = arguments[i]
            if arg.startswith('@'):
                path = command_path(arg[1:])
                evidence.locate(path)
                require(path not in seen, 'response cycle/reuse refused')
                seen.add(path)
                inputs.append(path)
                raw_paths.append(arg[1:])
                responses.append((path, arg[1:]))
            elif arg in {'-o', '-Map', '--Map', '-T', '--script', '--version-script',
                         '--dynamic-list', '--retain-symbols-file', '--just-symbols'}:
                require(i + 1 < len(arguments), 'missing linker option value')
                i += 1
                add(arguments[i], arg in {'-o', '-Map', '--Map'})
            elif any(arg.startswith(prefix) for prefix in
                     ('-Map=', '--Map=', '--output=', '--script=', '--version-script=', '--dynamic-list=',
                      '--retain-symbols-file=', '--just-symbols=')):
                add(arg.split('=', 1)[1], arg.startswith(('-Map=', '--Map=', '--output=')))
            elif arg in {'-L', '-l', '-u', '-e', '-m', '--defsym', '--wrap', '--undefined',
                         '-z', '-soname', '--soname', '-rpath', '--rpath', '-rpath-link',
                         '--rpath-link', '--hash-style', '--oformat', '--format', '-b', '-mllvm',
                         '--sysroot', '--library-path', '--entry', '--dynamic-linker'}:
                require(i + 1 < len(arguments), 'missing linker option value')
                i += 1
                if arg in {'-L', '-l', '--library-path'}:
                    gaps.append('linker_library_search_unresolved')
            elif arg.startswith(('-L', '-l')):
                gaps.append('linker_library_search_unresolved')
            elif arg.startswith('-T') and len(arg) > 2:
                add(arg[2:])
            elif not arg.startswith('-'):
                add(arg)
            else:
                # Preserve actual argv but do not pretend to understand every lld flag.
                gaps.append('linker_option_semantics_unverified')
            i += 1
        evidence.validate_files(inputs + outputs)  # all known aliases before response metadata
        for path, raw in responses:
            inspect(Path(os.path.join(os.getcwd(), raw)), leaf_link=True)
            data, _ = read_regular(Path(path), LIMIT)
            parse(shlex.split(data.decode('utf-8'), posix=True), depth + 1)

    parse(argv[1:])
    return inputs, outputs, sorted(set(gaps + ['linker_scripts_and_wrapper_indirect_inputs'])), raw_paths


def record(evidence, phase, inputs, outputs, pending, argv=None, link=False):
    inputs = list(inputs) + evidence.run['phase_files'].get(phase, [])
    pending = sorted(set(PENDING + [identifier(p) for p in pending]))
    arguments = list(argv or [])
    raw_paths = []
    if argv is not None:
        require(arguments and arguments[0].startswith('/'), 'absolute command executable required')
        argv_privacy(arguments)
        inputs = [command_path(arguments[0])] + inputs
        raw_paths.append(arguments[0])
        if link:
            found, produced, gaps, raw = link_inputs(evidence, arguments)
            inputs += found
            outputs += produced
            pending = sorted(set(pending + gaps))
            raw_paths += raw
    inputs, outputs = list(dict.fromkeys(inputs)), list(dict.fromkeys(outputs))
    evidence.validate_files(inputs + outputs)
    # After the lexical/alias gate, check unnormalized command paths as well: a
    # wrapper's conventional repo/../sibling syntax must not erase a symlink.
    for raw in raw_paths:
        inspect(Path(os.path.join(os.getcwd(), raw)), leaf_link=True)
    event = evidence.event(phase)
    cwd = absolute(str(Path.cwd()))
    require(any(cwd == Path(p) or cwd.is_relative_to(Path(p)) for p in evidence.roots.values()),
            'working directory outside approved roots')
    before = {'phase': phase, 'kind': 'command' if argv is not None else 'observation',
              'qualification': 'PENDING', 'pending': pending, 'cwd': str(cwd),
              'run_sha256': hashlib.sha256(canonical(evidence.run)).hexdigest(),
              'argv': arguments, 'inputs': evidence.capture(event, inputs),
              'outputs': evidence.capture(event, outputs)}
    # Commit before command execution: an interrupted producer remains visibly incomplete.
    write_json(event / 'before.json', before)
    code = None
    if argv is not None:
        try:
            code = subprocess.run(arguments, check=False).returncode
        except OSError:
            code = 127
        except KeyboardInterrupt:
            code = 130
    after = {'phase': phase, 'qualification': 'PENDING', 'pending': pending,
             'before_sha256': hashlib.sha256(canonical(before)).hexdigest(),
             'returncode': code, 'inputs': evidence.capture(event, inputs),
             'outputs': evidence.capture(event, outputs)}
    identity = lambda row: (row['root'], row['path'], row['kind'], row.get('sha256'), row.get('mode'))
    if list(map(identity, before['inputs'])) != list(map(identity, after['inputs'])):
        after['pending'] = sorted(set(after['pending'] + ['command_inputs_changed']))
    if any(row['kind'] == 'missing' for row in after['inputs'] + after['outputs']):
        after['pending'] = sorted(set(after['pending'] + ['selected_files_missing']))
    if argv is not None and any(identity(a) == identity(b) and b['kind'] != 'missing'
                               for a, b in zip(before['outputs'], after['outputs'])):
        after['pending'] = sorted(set(after['pending'] + ['unchanged_outputs_production_unverified']))
    write_json(event / 'after.json', after)
    return code or 0


def check(directory):
    evidence = Evidence(directory, probe=False)  # independently check retained bytes, not live inputs
    events = evidence.directory / 'events'
    require(stat.S_ISDIR(inspect(events).st_mode), 'invalid events directory')
    count, incomplete, failed, missing = 0, 0, 0, 0
    seen_phases = set()
    require({p.name for p in evidence.directory.iterdir()} == {'events', 'run.json', 'run.json.sha256'},
            'unexpected evidence metadata')
    for event in sorted(events.iterdir()):
        require(re.fullmatch('[a-z][a-z0-9_-]{0,79}-[0-9a-f]{32}', event.name), 'unexpected event')
        event_info = inspect(event)
        require(stat.S_ISDIR(event_info.st_mode) and stat.S_IMODE(event_info.st_mode) == 0o700,
                'invalid/nonprivate event directory')
        used, before = set(), None
        for name in ('before', 'after'):
            if inspect(event / (name + '.json')) is None:
                incomplete += 1
                continue
            value = read_json(event / (name + '.json'))
            fields(value, {'phase', 'qualification', 'pending', 'inputs', 'outputs'} |
                   ({'kind', 'cwd', 'argv', 'run_sha256'} if name == 'before' else
                    {'returncode', 'before_sha256'}))
            require(event.name == value['phase'] + '-' + event.name[-32:], 'phase mismatch')
            seen_phases.add(value['phase'])
            require(isinstance(value['pending'], list) and
                    all(identifier(p) for p in value['pending']), 'invalid pending ledger')
            require(value['qualification'] == 'PENDING' and set(PENDING) <= set(value['pending']),
                    'invalid phase qualification')
            if name == 'before':
                argv_privacy(value['argv'])
                absolute(value['cwd'])
                require(value['run_sha256'] == hashlib.sha256(canonical(evidence.run)).hexdigest(),
                        'run identity mismatch')
                before = value
            else:
                require(before is not None and value['before_sha256'] ==
                        hashlib.sha256(canonical(before)).hexdigest(), 'before/after identity mismatch')
                failed += int(value['returncode'] not in {None, 0})
            graph = {}
            for row in value['inputs'] + value['outputs']:
                identifier(row['root'])
                require(row['root'] in evidence.roots, 'unknown root')
                safe_name(row['path'])
                if row['kind'] == 'missing':
                    fields(row, {'root', 'path', 'kind'})
                    missing += int(name == 'after')
                    continue
                fields(row, {'root', 'path', 'kind', 'mode', 'sha256', 'size', 'blob'},
                       {'target'} if row['kind'] == 'symlink' else set())
                require(row['kind'] in {'file', 'symlink'}, 'invalid captured kind')
                require(re.fullmatch('files/[0-9a-f]{32}', row['blob']), 'unsafe blob')
                require(isinstance(row['mode'], int) and 0 <= row['mode'] <= 0o7777, 'invalid mode')
                hexhash(row['sha256'])
                blob_info = inspect(event / row['blob'])
                require(blob_info is not None and stat.S_ISREG(blob_info.st_mode), 'missing/invalid blob')
                require(stat.S_IMODE(blob_info.st_mode) == 0o600 and blob_info.st_nlink == 1,
                        'nonprivate/aliased blob')
                fingerprint = copy_regular(event / row['blob'])
                require(fingerprint['size'] == row['size'] and fingerprint['sha256'] == row['sha256'],
                        'captured blob mismatch')
                used.add(row['blob'])
                graph[row['root'] + '/' + row['path']] = {
                    'kind': row['kind'], 'target': row.get('target')}
                if row['kind'] == 'symlink':
                    safe_link(row['path'], row['target'])
                    data, _ = read_regular(event / row['blob'], 65536)
                    require(data == os.fsencode(row['target']), 'link-text mismatch')
            link_graph(graph)
        files = event / 'files'
        file_info = inspect(files)
        require(stat.S_ISDIR(file_info.st_mode) and stat.S_IMODE(file_info.st_mode) == 0o700,
                'invalid/nonprivate blob directory')
        actual = {'files/' + p.name for p in files.iterdir()}
        # Unreferenced bytes indicate interrupted/failed capture, never success.
        incomplete += int(actual != used)
        allowed = {'files', 'before.json', 'before.json.sha256', 'after.json', 'after.json.sha256'}
        require(all(p.name in allowed for p in event.iterdir()), 'unexpected event metadata')
        count += 1
    print(canonical({'events': count, 'incomplete': incomplete,
                     'failed_commands': failed, 'missing_selections': missing,
                     'unobserved_selection_phases': sorted(set(evidence.run['phase_files']) - seen_phases),
                     'qualification': 'PENDING', 'pending': PENDING}).decode(), end='')
    require(count > 0 and incomplete == 0 and failed == 0, 'empty/incomplete/failed evidence')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dir', default=os.environ.get('EDEN_NATIVE_EVIDENCE_DIR'))
    commands = parser.add_subparsers(dest='action', required=True)
    init = commands.add_parser('init')
    init.add_argument('--root', action='append', default=[])
    init.add_argument('--phase-file', action='append', default=[])
    commands.add_parser('check')
    for name in ('capture', 'run'):
        sub = commands.add_parser(name)
        sub.add_argument('--phase', required=True)
        sub.add_argument('--input', '--file', dest='inputs', action='append', default=[])
        sub.add_argument('--output', dest='outputs', action='append', default=[])
        sub.add_argument('--pending', action='append', default=[])
        if name == 'run':
            sub.add_argument('--link', action='store_true')
            sub.add_argument('argv', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    require(args.dir, 'EDEN_NATIVE_EVIDENCE_DIR or --dir required; initialize explicitly')
    if args.action == 'init':
        initialize(args.dir, args.root, args.phase_file)
        return 0
    if args.action == 'check':
        check(args.dir)
        return 0
    argv = None
    if args.action == 'run':
        require(args.argv and args.argv[0] == '--', 'command separator required')
        argv = args.argv[1:]
    return record(Evidence(args.dir), args.phase, args.inputs, args.outputs, args.pending,
                  argv, getattr(args, 'link', False))


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (Refused, OSError, ValueError, KeyError, TypeError) as error:
        # Do not expose a rejected private path/argv through exception text.
        print('native evidence refused: ' + type(error).__name__, file=sys.stderr)
        sys.exit(2)
