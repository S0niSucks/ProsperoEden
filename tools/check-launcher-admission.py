#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Shared entrypoint for the same real launcher/Nextendo admission fixture and sanitizer matrix."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='launcher-admission-') as tmp:
    subprocess.run(['python3', '-u', str(root / 'tools/check-nextendo-composed.py'),
                    '--out', str(Path(tmp) / 'proof')], check=True)
