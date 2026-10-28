#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""No config contents are read. Check the shared export/diagnostic deny rule and release filter."""
import ast
from pathlib import Path
from network_privacy import private_network_path

for path in ('config/network-shares.json', '/data/prosperoeden/config/network-shares.json',
             r'C:\private\CONFIG\NETWORK-SHARES.JSON', 'PPSA99008/config/network-shares.json.new'):
    assert private_network_path(path)
for path in ('docs/NETWORK_SHARES.md', 'config/settings.json', 'headless/network/shares.h'):
    assert not private_network_path(path)
source = (Path(__file__).parent / 'ci/make-dist.py').read_text()
tree = ast.parse(source)
assignment = next(n for n in tree.body if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'files' for t in n.targets))
assert any(isinstance(n, ast.Call) and isinstance(n.func, ast.Name) and n.func.id == 'private_network_path'
           for n in ast.walk(assignment.value))
packager = ast.parse((Path(__file__).parent / 'launcher/package-update.py').read_text())
assert any(isinstance(n, ast.If) and isinstance(n.test, ast.Call) and isinstance(n.test.func, ast.Name)
           and n.test.func.id == 'private_network_path' and any(isinstance(child, ast.Continue) for child in n.body)
           for n in ast.walk(packager))
print('Private network config/staging deny rule and distribution/launcher export filters PASS (no package generated)')
