#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check lexical privacy policy, then exercise both actual packaging CLIs on synthetic data."""
from pathlib import Path
import subprocess
import sys
from network_privacy import private_network_path

for path in ('config/network-shares.json', '/data/prosperoeden/config/network-shares.json',
              r'C:\private\CONFIG\NETWORK-SHARES.JSON', 'PPSA99008/config/network-shares.json.new',
              'config/network-shares.jsonanything', 'CONFIG. /Network-Shares.Json.tmp/child',
              'wrapper/config /network-shares.json.backup', 'config/./network-shares.json',
              r'prefix\CONFIG.\NEXTENDO. \oauth\refresh.json', 'config/nextendo',
              'config/nextendo/account.json', 'wrapper/CoNfIg/NeXtEnDo/routes.json'):
    assert private_network_path(path)
for path in ('docs/NETWORK_SHARES.md', 'config/settings.json', 'headless/network/shares.h',
             'config/network-shares.txt', 'assets/network-shares.json', 'config/nextendo.json',
             'config/nextendo-public/token.txt', 'configuration/nextendo/account.json'):
    assert not private_network_path(path)
subprocess.run([sys.executable, '-B', str(Path(__file__).with_name('check-export-privacy.py')),
                *sys.argv[1:]], check=True)
print('Export privacy PASS: lexical aliases and actual synthetic distribution/launcher ZIPs')
