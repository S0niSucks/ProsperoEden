#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
build=${PE_SCENE_BUILD:-/work/scene-tests}
mkdir -p "$build/native-obj"
objects=()
for name in miniz miniz_tdef miniz_tinfl miniz_zip; do
    object="$build/native-obj/$name.o"
    if [[ ! -e $object ]]; then cc -O1 -I"$root/third_party/miniz" -c "$root/third_party/miniz/$name.c" -o "$object"; fi
    objects+=("$object")
done
"${HOST_CXX:-c++}" -std=c++20 -O0 -g1 -Wall -Wextra -Werror -fmax-errors=8 -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$root/headless/prosperoeden" -I"$root/third_party" -I"$root/third_party/miniz" \
    "$root/tools/launcher/scene/native.cpp" "$root/headless/prosperoeden/pe/audio/wav.cpp" "${objects[@]}" -lcrypto -o "$build/scene-native"
python3 -B "$root/tools/launcher/scene/test_native.py" "$build/scene-native"
