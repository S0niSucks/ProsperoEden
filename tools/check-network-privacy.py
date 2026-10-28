#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check lexical privacy, actual packaging CLIs and native-only share editor boundary."""
import json
from pathlib import Path
import re
import subprocess
import sys
from network_privacy import private_network_path

for path in ('config/network-shares.json', '/data/prosperoeden/config/network-shares.json',
              r'C:\private\CONFIG\NETWORK-SHARES.JSON', 'PPSA99008/config/network-shares.json.new',
              'config/network-shares.jsonanything', 'CONFIG. /Network-Shares.Json.tmp/child',
              'wrapper/config /network-shares.json.backup', 'config/./network-shares.json',
              r'prefix\CONFIG.\NEXTENDO. \oauth\refresh.json', 'config/nextendo',
              'config/nextendo/account.json', 'wrapper/CoNfIg/NeXtEnDo/routes.json',
              'PPSA99008/config/network-shares.json.tmp-123-1-456/settings',
              r'USB\backup\CONFIG\NETWORK-SHARES.JSON.TMP-1-2-3\settings'):
    assert private_network_path(path)
for path in ('docs/NETWORK_SHARES.md', 'config/settings.json', 'headless/network/shares.h',
             'config/network-shares.txt', 'assets/network-shares.json', 'config/nextendo.json',
             'config/nextendo-public/token.txt', 'configuration/nextendo/account.json'):
    assert not private_network_path(path)
subprocess.run([sys.executable, '-B', str(Path(__file__).with_name('check-export-privacy.py')),
                *sys.argv[1:]], check=True)
root = Path(__file__).resolve().parents[1]
runtime = (root / 'headless/prosperoeden/pe/ui/runtime_launcher.cpp').read_text()
assert 'network_editor_' not in runtime and 'NativeNetworkKeyboard' not in runtime
native = (root / 'headless/prosperoeden/pe/ui/launcher.hpp').read_text()
assert 'std::future<Eden::Network::EditorResult> network_job_' in native
handoff = {json.loads(line) for line in (root / 'docs/NETWORK_EDITOR_MSGIDS.txt').read_text().splitlines()
           if line and not line.startswith('#')}
view = (root / 'headless/prosperoeden/pe/ui/network_shares.cpp').read_text()
required = set(re.findall(r'(?:TR|tr)\("([^"]*)"\)', view))
for name in ('editor.cpp', 'config.cpp'):
    required.update(re.findall(r'return "([^"]+)"', (root / 'headless/network' / name).read_text()))
assert required <= handoff, 'Feature-local translation handoff is missing a native editor string'
print('Export privacy and native/Lua editor boundary/msgid handoff PASS; synthetic packages only')
