#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Focused launcher only, not an emulator/core build. Run in the assigned Docker volume.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
build=${PROSPEROEDEN_LAUNCHER_BUILD:-"$HOME/.cache/prosperoeden-cover-flow"}
output=${1:-"$root/build/cover-flow"}
export PROSPEROEDEN_LAUNCHER_BUILD="$build"
mkdir -p "$output"
# Reuse the production renderer/services/runtime objects and their established dependency setup.
printf 'exit\n' | bash "$root/tools/launcher/runtime-preview.sh" "$output/startup" 1280 720
objects=()
for object in "$build"/obj/*.o; do
    [[ $object == */host_runtime_preview.cpp.o ]] || objects+=("$object")
done
"${HOST_CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -DGL_GLEXT_PROTOTYPES=1 \
    -I"$root/headless/prosperoeden" -I"$root/headless/prosperoeden/host" -I"$root/headless" \
    -I"$root/tools/launcher/stb" -I"$root/third_party" -I"$root/third_party/lua" \
    "$root/tools/launcher/test-cover-flow.cpp" "${objects[@]}" \
    -lEGL -lGL -lm -lcrypto -lcurl -pthread -o "$build/test-cover-flow"
export EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
export MESA_GL_VERSION_OVERRIDE=4.6 MESA_GLSL_VERSION_OVERRIDE=460
for size in '1280 720' '1920 1080'; do
    read -r width height <<< "$size"
    "$build/test-cover-flow" "$root/headless/prosperoeden/ui" "$output/${height}p" "$width" "$height"
done
"${HOST_CXX:-c++}" -std=c++20 -O2 -I"$root/headless" "$root/headless/preferences_check.cpp" -o "$build/preferences-check"
"$build/preferences-check"
python3 -B "$root/tools/check-launcher-theme.py"
python3 -B "$root/tools/launcher/test-cover-flow-api.py" "$build/pe_preview"
