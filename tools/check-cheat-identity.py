#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The generated real loader must use the same ExeFS resolver as cheat inspection."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
build = Path(sys.argv[1])
loader = (build / 'headless/deconstructed_rom_directory.cpp').read_text()
assert loader.count('Eden::ResolveProgramExeFS(dir, metadata, override_update,') == 1
assert 'dir = patch_manager.PatchExeFS(dir);' not in loader
assert '/effective_exefs.h"' in loader
assert 'ResolveProgramExeFS(exefs, metadata, apply_updates,' in (root / 'headless/cheat_identity.cpp').read_text()
metadata = (build / 'headless/program_metadata.cpp').read_text()
assert metadata.count('Eden::BoundNpdmCapabilities(file)') == 1
assert metadata.index('Eden::BoundNpdmCapabilities(file)') < metadata.index('aci_kernel_capabilities.resize(')
bis = (build / 'headless/bis_factory.cpp').read_text()
assert bis.count('Eden::GameFolderMods::Root(std::move(shared), title_id, wanted)') == 1
print('Effective ExeFS selection PASS: generated actual boot loader and cheat lookup share one resolver')
