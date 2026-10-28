#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Regression for native package notice inventory; synthetic payloads, not ELF/PS5 acceptance."""
import importlib.util
from pathlib import Path
import runpy
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'headless'))
spec = importlib.util.spec_from_file_location('native_package', root / 'headless/check_package.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)
stage = runpy.run_path(str(root / 'tools/package-license-notices.py'))['stage']

with tempfile.TemporaryDirectory(prefix='native-package-inventory-') as directory:
    app = Path(directory) / 'PPSA99008'
    package.APP = app
    package.REQUIRED = package.BASE_REQUIRED | {'ui/fixture.txt'}
    for name in package.REQUIRED:
        path = app / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'Synthetic inventory fixture, not executable.\n')

    def refused():
        try:
            package.check_inventory()
        except (AssertionError, ValueError, OSError):
            return
        raise AssertionError('Invalid package inventory was accepted')

    refused()  # app-folder notices are required, not optional extras
    notices = stage(app)['files']
    expected = package.REQUIRED | notices.keys()
    assert package.check_inventory() == expected
    notice = app / 'LICENSE'
    original = notice.read_bytes()
    notice.unlink()
    refused()
    notice.write_bytes(original + b'altered')
    refused()
    notice.unlink()
    target = Path(directory) / 'outside'
    target.write_bytes(original)
    notice.symlink_to(target)
    refused()
    notice.unlink()
    notice.write_bytes(original)
    extra = app / 'unexpected.bin'
    extra.write_bytes(b'not a declared app file')
    refused()
    extra.unlink()
    payload = app / 'core-homebrew.nro'
    payload.write_bytes(b'')
    refused()
    payload.write_bytes(b'Synthetic inventory fixture, not executable.\n')
    private = app / 'config/network-shares.json'
    private.parent.mkdir()
    private.write_bytes(b'Synthetic private canary, not real credentials')
    refused()
    private.unlink()
    assert package.check_inventory() == expected
print('Native package inventory PASS: exact verified notices, missing/altered/link/extra/empty/private refusal (synthetic payloads only)')
