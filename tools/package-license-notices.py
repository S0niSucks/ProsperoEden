#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage verified notices inside an app folder; not a native build, PKG or source offer.

Create-only: matching existing notices are retained; conflicts/extra owned inventory fail.
The existing app/licenses/Lapy-MIT.txt and all other app payload bytes are never opened.
Requires a POSIX packaging host with descriptor-relative, no-follow filesystem operations.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
from network_privacy import private_network_path
from release_licenses import release_license_entries

ROOT = Path(__file__).resolve().parents[1]


def directory(path):
    """Open every component without following a link, including the app's ancestors."""
    fd = os.open(path.anchor, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        for part in path.parts[1:]:
            if not stat.S_ISDIR(os.stat(part, dir_fd=fd, follow_symlinks=False).st_mode):
                raise ValueError(f'Directory links/special nodes are forbidden: {path}')
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd); fd = child
        return fd
    except BaseException:
        os.close(fd)
        raise


def inventory(fd, prefix=''):
    """Metadata only. Deny names before stat/open/listdir, even for hostile private nodes."""
    files, dirs = {}, set()
    for name in sorted(os.listdir(fd)):
        relative = prefix + name
        if private_network_path(relative):
            raise ValueError(f'Private configuration is forbidden in packaging input: {relative}')
        metadata = os.stat(name, dir_fd=fd, follow_symlinks=False)
        if stat.S_ISDIR(metadata.st_mode):
            child = os.open(name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            try:
                subfiles, subdirs = inventory(child, relative + '/')
            finally:
                os.close(child)
            files.update(subfiles); dirs.update(subdirs); dirs.add(relative)
        elif stat.S_ISREG(metadata.st_mode):
            files[relative] = metadata
        else:
            raise ValueError(f'Packaging links/special nodes are forbidden: {relative}')
    return files, dirs


def parent(fd, parts, create=False):
    current = os.dup(fd)
    try:
        for part in parts:
            try:
                child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=current)
            except FileNotFoundError:
                if not create: raise
                os.mkdir(part, 0o777, dir_fd=current)
                child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=current)
                os.fchmod(child, 0o777)  # Only directories this invocation just created.
            os.close(current); current = child
        return current
    except BaseException:
        os.close(current)
        raise


def preflight(fd, entries, check):
    files, dirs = inventory(fd)
    parents = {str(path) for name in entries for path in Path(name).parents if str(path) != '.'}
    expected_names = {name.casefold(): name for name in (*entries, *parents)}
    for name in (*files, *dirs):
        alias = '/'.join(part.rstrip(' .').casefold() for part in name.replace('\\', '/').split('/'))
        if alias in expected_names and name != expected_names[alias]:
            raise ValueError(f'Ambiguous notice destination: {name}')
    owned = {name for name in files if name == 'third_party/licenses' or name.startswith('third_party/licenses/')}
    owned_dirs = {name for name in dirs if name == 'third_party/licenses' or name.startswith('third_party/licenses/')}
    if owned - entries.keys() or owned_dirs - parents or parents & files.keys():
        raise ValueError('Conflicting/extra app third_party/licenses inventory')
    missing = []
    for name, data in entries.items():
        if name in dirs:
            raise ValueError(f'Notice destination is a directory: {name}')
        if name not in files:
            missing.append(name); continue
        if files[name].st_size != len(data):
            raise ValueError(f'Existing notice differs; use fresh staging: {name}')
        parts = Path(name).parts
        pfd = parent(fd, parts[:-1])
        try:
            file = os.open(parts[-1], os.O_RDONLY | os.O_NOFOLLOW, dir_fd=pfd)
            with os.fdopen(file, 'rb') as stream:
                if stream.read(len(data) + 1) != data:
                    raise ValueError(f'Existing notice differs; use fresh staging: {name}')
        finally:
            os.close(pfd)
    if check and missing:
        raise ValueError(f'Missing staged notices: {missing}')
    return missing, parents


def stage(app, check=False):
    if os.name != 'posix' or not hasattr(os, 'O_NOFOLLOW') or os.open not in os.supports_dir_fd:
        raise ValueError('A POSIX host with no-follow descriptor-relative IO is required')
    if private_network_path(str(app)):
        raise ValueError('Private configuration cannot be a staging destination')
    if any(part == '..' or part.rstrip(' .') != part or '\\' in part or ':' in part or
           any(ord(c) < 32 or ord(c) == 127 for c in part) for part in app.parts):
        raise ValueError('Unsafe/ambiguous app destination path')
    app = app.absolute()  # Do not resolve away symlink ancestors.
    if private_network_path(str(app)):
        raise ValueError('Private configuration cannot be a staging destination')
    if ROOT.is_relative_to(app) or (app.is_relative_to(ROOT) and app.relative_to(ROOT).parts[0] not in ('build', 'dist')):
        raise ValueError('Destination overlaps source inputs; use build/dist or external staging')
    app_fd = directory(app)
    try:
        # The shared helper has no privacy prefilter. Reject private source nodes before it
        # can inspect them, while retaining its exact inventory/hash/type validation.
        source_fd = directory(ROOT / 'third_party/licenses')
        try:
            source_files, source_dirs = inventory(source_fd, 'third_party/licenses/')
        finally:
            os.close(source_fd)
        entries = release_license_entries(ROOT)
        expected_files = {name for name in entries if name.startswith('third_party/licenses/')}
        expected_dirs = {str(path) for name in expected_files for path in Path(name).parents
                         if str(path).startswith('third_party/licenses/')}
        if source_files.keys() != expected_files or source_dirs != expected_dirs:
            raise ValueError('Extra/missing source notice inventory, including empty directories')
        missing, parents = preflight(app_fd, entries, check)
        if not check:
            for name in sorted(parents, key=lambda value: (value.count('/'), value)):
                os.close(parent(app_fd, Path(name).parts, create=True))
            for name in missing:
                parts, data = Path(name).parts, entries[name]
                pfd = parent(app_fd, parts[:-1])
                try:
                    file = os.open(parts[-1], os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o777, dir_fd=pfd)
                    try:
                        with os.fdopen(file, 'wb') as stream:
                            stream.write(data); stream.flush()
                            os.fchmod(stream.fileno(), 0o777); os.fsync(stream.fileno())
                    except BaseException:
                        os.unlink(parts[-1], dir_fd=pfd)  # Only our just-created partial file.
                        raise
                finally:
                    os.close(pfd)
            preflight(app_fd, entries, True)
        return {'app': str(app), 'check_only': check, 'created': [] if check else missing,
                'files': {name: {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
                          for name, data in entries.items()},
                'scope': 'Notice staging only; not PKG/native/ELF or source/relink qualification'}
    finally:
        os.close(app_fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('app', type=Path, help='Existing app folder; must not overlap source inputs')
    parser.add_argument('--check', action='store_true', help='Verify only; never create files/directories')
    args = parser.parse_args()
    try:
        print(json.dumps(stage(args.app, args.check), indent=2))
    except (ValueError, OSError, KeyError) as error:
        parser.exit(1, f'Notice staging refused: {error}\n')


if __name__ == '__main__':
    main()
