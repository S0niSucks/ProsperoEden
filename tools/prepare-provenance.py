#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""A10 c-themes szs-subset r1 provenance: hash pinned inputs and artifacts.

Read-only over the checkout; writes only provenance.json beside this script.
"""
from pathlib import Path
import hashlib
import json

repo = Path("D:/leonardommello/PS5/ProsperoEden-Encore")
here = Path(__file__).resolve().parent

repo_inputs = [
    "tools/ps-glyph-botw-yaz0.py",
    "tools/ps-glyph-sarc-inspect.py",
    "tools/check-ps-glyph-botw-yaz0.py",
    "docs/feature-roadmap/sources/themes-switch2.md",
    "docs/feature-roadmap/campaign-20261107/agents/A22.md",
]
artifacts = ["nx_szs_structural.py", "check_nx_szs_structural.py", "README.md",
             "dissect-sample.py"]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


receipt = {
    "role": "A10/P10 developer — c-themes structural .szs subset r1",
    "status": "PATCH_READY (host scripts); ALL RUNTIME CHECKS NOT_RUN",
    "date": "2026-11-09",
    "pinned_internal_spec": {
        "yaz0": "tools/ps-glyph-botw-yaz0.py:46-89",
        "sarc": "tools/ps-glyph-sarc-inspect.py:35-129",
    },
    "inputs_repo_sha256": {name: sha(repo / name) for name in repo_inputs},
    "artifacts_sha256": {name: sha(here / name) for name in artifacts},
    "notes": ("r3 supersedes r2: one negative expected a message the pinned "
              "inspector never emits for those bytes (builder data_start "
              "mutation also moves size/overwrites SFNT header, so :72 fires "
              "before :74). Case renamed to the real emission + new byte-"
              "surgery case isolates :74; all 37 negatives re-audited against "
              "the inspectors' real check order. Parser unchanged "
              "(86af2818...). Fixture still NOT RUN by A10."),
    "limits": ("No execution/compilation/rendering; no PS5/fonts/performance "
               "claims; vendor/sources/eden never opened; checkout untouched "
               "except agents/A10.md (append)."),
}
out = here / "provenance.json"
if out.exists():
    raise SystemExit("provenance.json already exists; refusing to overwrite")
out.write_text(json.dumps(receipt, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(json.dumps(receipt["artifacts_sha256"], indent=2))
