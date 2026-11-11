# SPDX-License-Identifier: GPL-3.0-or-later
"""Name-first export privacy, adapted from ProsperoEden's network_privacy.py.

Treat both separators, case and Windows trailing-dot/space aliases alike. This is
a narrow path policy, not content inspection or a blacklist of all config files.
Encore rejects contaminated inputs rather than filtering only its ZIPs: the same
staged app is also passed unchanged to the image packer.
"""
import pathlib
import stat


def private_network_path(path):
    parts = [part.rstrip(' .').casefold() for part in str(path).replace('\\', '/').split('/')]
    parts = [part for part in parts if part]
    return any(parts[n] == 'config' and (parts[n + 1].startswith('network-shares.json')
                                        or parts[n + 1] == 'nextendo')
               for n in range(len(parts) - 1))


def export_path_metadata(path):
    """Reject private names BEFORE lstat, and reject links/reparse/special nodes.

    Errors intentionally contain no input paths or credential-bearing filenames.
    """
    if private_network_path(path):
        raise ValueError('Private network configuration is forbidden in release inputs')
    for part in path.parts:
        if part == path.anchor:
            continue
        if part in ('.', '..') or any(c in part for c in '\\:') or any(ord(c) < 32 for c in part):
            raise ValueError('Unsafe release input path')
    metadata = path.lstat()
    if (getattr(metadata, 'st_file_attributes', 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT or
            not (stat.S_ISDIR(metadata.st_mode) or stat.S_ISREG(metadata.st_mode))):
        raise ValueError('Links, reparse points and special files are forbidden in release inputs')
    return metadata


def preflight_export_tree(root):
    """Return sorted regular files, or fail before publishing/deleting any output.

    A private child is rejected by name before stat/open/descent; listing its public
    parent necessarily sees its name. Root ancestry is checked without resolve(),
    which would hide a symlink/junction. Inputs must stay quiescent through ALL
    packaging passes; this preflight is not a concurrent-mutation sandbox.
    """
    root = pathlib.Path(root).absolute()
    # Check the whole spelling before even inspecting its ancestors.
    if private_network_path(root) or '..' in root.parts:
        raise ValueError('Private or unsafe release input root')
    for directory in (*reversed(root.parents), root):
        if not stat.S_ISDIR(export_path_metadata(directory).st_mode):
            raise ValueError('Release input root must be a directory')

    files = []

    def walk(directory):
        for path in sorted(directory.iterdir()):
            metadata = export_path_metadata(path)
            if stat.S_ISDIR(metadata.st_mode):
                walk(path)
            else:
                files.append(path)

    walk(root)
    return sorted(files)
