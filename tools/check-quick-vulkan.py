#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the shipped HUD draw include/shaders offscreen on a host Vulkan driver."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
icds = list(Path('/usr/share/vulkan/icd.d').glob('lvp_icd*.json'))
assert len(icds) == 1, 'The offscreen test requires the Docker Mesa lavapipe ICD'
prepare = (root / 'headless/vulkan_hud_prepare.inc').read_text()
# GPU objects and descriptors must never be replaced while older frames can still use them.
assert prepare.index('scheduler.Finish();') < prepare.index('hud_quick_view = {};')
assert prepare.index('hud_quick_view = {};') < prepare.index('hud_quick_art = {};')
assert 'hud_quick_picture != hud_commands.image' in prepare
assert 'GetVulkanHud(&hud_commands)' in prepare
with tempfile.TemporaryDirectory(prefix='eden-quick-vulkan-') as tmp:
    folder = Path(tmp)
    shaders = []
    for stage in ('vert', 'frag'):
        shader = folder / f'hud.{stage}.spv'
        subprocess.run(['glslangValidator', '-V', '--target-env', 'vulkan1.0', '-o', str(shader),
                        str(root / f'headless/vulkan_hud.{stage}')], check=True)
        subprocess.run(['spirv-val', '--target-env', 'vulkan1.0', str(shader)], check=True)
        shaders.append(str(shader))
    binary = folder / 'check'
    subprocess.run(['clang++-18', '-std=c++20', '-O1', '-UNDEBUG', '-I', str(root),
                    '-I', str(root / 'headless'), str(root / 'tools/quick_vulkan_check.cpp'),
                    '-lvulkan', '-pthread', '-o', str(binary)], check=True)
    subprocess.run([str(binary), *shaders, str(Path(sys.argv[1]).resolve())], check=True, timeout=120,
                   env={**os.environ, 'VK_DRIVER_FILES': str(icds[0])})
