#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
build=${PE_SCENE_BUILD:-/work/scene-tests}
mkdir -p "$build"
cxx=${HOST_CXX:-c++}
flags=(-std=c++20 -O0 -g1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I"$root/headless/prosperoeden")
"$cxx" "${flags[@]}" "$root/tools/launcher/scene/test.cpp" "$root/headless/prosperoeden/pe/audio/wav.cpp" -o "$build/scene-test"
"$cxx" "${flags[@]}" "$root/tools/launcher/scene/check.cpp" "$root/headless/prosperoeden/pe/audio/wav.cpp" -o "$build/scene-check"
"$build/scene-check" "$root/examples/canopy-scene"
"$build/scene-test" "$root/examples/canopy-scene"
# Compile only the two integration translation units. No full launcher/emulator build.
for unit in runtime_launcher launcher_updates; do
    "$cxx" -std=c++20 -Wall -Wextra -Werror -fmax-errors=4 -fsyntax-only -DGL_GLEXT_PROTOTYPES=1 -DPE_RUNTIME_PREVIEW=1 \
        -I"$root/headless/prosperoeden" -I"$root/headless/prosperoeden/host" -I"$root/third_party/lua" \
        -I"$root/third_party" -I"$root/third_party/miniz" "$root/headless/prosperoeden/pe/ui/$unit.cpp"
done
python3 -B "$root/tools/launcher/scene/test_themezer.py" "$build/scene-check"
