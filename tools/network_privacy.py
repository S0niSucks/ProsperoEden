# SPDX-License-Identifier: GPL-3.0-or-later
"""Shared diagnostic/backup/export deny rule. Match case-insensitively on every platform.
Keep the private JSON (and editor-staged siblings) out even when it is empty/disabled.
"""
from pathlib import PurePosixPath


def private_network_path(path):
    parts = PurePosixPath(str(path).replace('\\', '/').lower()).parts
    return any(parts[n] == 'config' and parts[n + 1].startswith('network-shares.json')
               for n in range(len(parts) - 1))
