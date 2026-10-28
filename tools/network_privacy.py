# SPDX-License-Identifier: GPL-3.0-or-later
"""Name-first export privacy: share settings/staging and the Nextendo account subtree.
Treat both separators, case and Windows trailing-dot/space aliases alike on every host.
This is a narrow path policy, not content inspection or a blacklist of all config files.
"""
import stat


def private_network_path(path):
    parts = [part.rstrip(' .').casefold() for part in str(path).replace('\\', '/').split('/')]
    parts = [part for part in parts if part]
    return any(parts[n] == 'config' and (parts[n + 1].startswith('network-shares.json')
                                        or parts[n + 1] == 'nextendo')
               for n in range(len(parts) - 1))


def public_export_paths(root):
    """Yield (path, lstat) without inspecting or descending into denied nodes.

    Listing a public parent sees child names, necessarily, but a denied child receives no
    stat/open/scandir. Nonprivate symlinks and special nodes fail closed, never follow them.
    Inputs must remain quiescent while packaging; this is not a filesystem race sandbox.
    """
    def walk(directory):
        for path in sorted(directory.iterdir()):
            relative = path.relative_to(root)
            if private_network_path(relative):
                continue
            metadata = path.lstat()
            if not (stat.S_ISDIR(metadata.st_mode) or stat.S_ISREG(metadata.st_mode)):
                raise ValueError(f'Links and special files are forbidden: {relative.as_posix()}')
            yield path, metadata
            if stat.S_ISDIR(metadata.st_mode):
                yield from walk(path)
    yield from walk(root)
