#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
build=${PE_SCENE_BUILD:-/work/scene-tests}
mkdir -p "$build"
"${HOST_CXX:-c++}" -std=c++20 -O1 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -DGL_GLEXT_PROTOTYPES=1 -I"$root/headless/prosperoeden" \
    "$root/tools/launcher/scene/render.cpp" "$root/headless/prosperoeden/pe/gfx/draw_list.cpp" \
    "$root/headless/prosperoeden/pe/gfx/gl_batch.cpp" "$root/headless/prosperoeden/pe/gfx/gl_program.cpp" \
    -Wl,--gc-sections -lEGL -lGL -o "$build/scene-render-test"
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    MESA_GL_VERSION_OVERRIDE=4.6 MESA_GLSL_VERSION_OVERRIDE=460 "$build/scene-render-test"
