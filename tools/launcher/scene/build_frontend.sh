#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Complete UI preview, without emulator/native build. Uses compiler-recorded dependencies,
# not a blanket rebuild of every UI translation unit whenever one scene header changes.
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
build=${PE_SCENE_FRONTEND_BUILD:-/work/scene-linked}
source_dir="$root/headless/prosperoeden"
harfbuzz="$root/.deps/harfbuzz-12.3.2/src"
kit="$root/.deps/ps5-homebrew-ui-aed286b8/src"
[[ -f $harfbuzz/harfbuzz.cc && -f $kit/ui/theme.cpp ]] || { echo 'Fetch pinned harfbuzz and ui-kit with tools/deps.py on the authoring PC first.' >&2; exit 1; }
mkdir -p "$build/obj"
# Header-only scene types cross the runtime/updater TU boundary. Invalidate those two
# objects on content changes, including edits that occurred during a previous compilation.
scene_hash=$(find "$source_dir/pe/ui/scene" -type f -print0 | sort -z | xargs -0 sha256sum | sha256sum)
if [[ ! -f $build/scene-inputs || $(cat "$build/scene-inputs") != "$scene_hash" ]]; then
    rm -f "$build/obj/headless_prosperoeden_pe_ui_runtime_launcher.cpp.o" "$build/obj/headless_prosperoeden_pe_ui_launcher_updates.cpp.o"
fi
cxx=${HOST_CXX:-c++}
san=(-fsanitize=address,undefined -fno-omit-frame-pointer)
includes=(-I"$source_dir" -I"$source_dir/host" -I"$root/headless" -I"$root/third_party" -I"$root/third_party/miniz" -I"$root/third_party/lua" -I"$root/tools/launcher/stb" -I"$harfbuzz" -I"$kit")
objects=()
compile() {
    local source=$1 object=$2 language=$3 dirty=false
    objects+=("$object")
    [[ -e $object && -e $object.d ]] || dirty=true
    [[ ${BASH_SOURCE[0]} -nt $object ]] && dirty=true
    if [[ $dirty == false ]]; then
        while IFS= read -r dep; do [[ -f $dep && $dep -nt $object ]] && dirty=true; done < <(tr ' \\\n' '\n' < "$object.d")
    fi
    if [[ $dirty == true ]]; then
        local flags=(-O0 -g1 -Wall -Wextra -ffunction-sections -fdata-sections "${san[@]}")
        [[ $language == lua ]] && flags=(-O1 -w "${san[@]}" -x c++ -include "$root/third_party/lua/runtime_api.hpp")
        [[ $language == harfbuzz ]] && flags=(-O1 -w)
        "$cxx" -std=c++20 "${flags[@]}" -DPE_RUNTIME_PREVIEW=1 -DGL_GLEXT_PROTOTYPES=1 "${includes[@]}" -MMD -MF "$object.d" -c "$source" -o "$object"
    fi
}
for source in "$source_dir"/host/*.cpp "$source_dir"/pe/{core,gfx,ui,audio}/*.cpp "$kit/ui/theme.cpp" \
    "$root/headless/network/editor.cpp" "$root/headless/network/config.cpp" "$root/headless/network/keyboard.cpp"; do
    relative=${source#"$root/"};name=${relative//\//_};language=cpp
    [[ $source == */harfbuzz.cpp ]] && language=harfbuzz
    compile "$source" "$build/obj/$name.o" "$language"
done
for source in "$root"/third_party/lua/*.c; do
    name=${source##*/}
    case $name in lua.c|luac.c|linit.c|liolib.c|loslib.c|loadlib.c|ldblib.c) continue;; esac
    compile "$source" "$build/obj/lua_$name.o" lua
done
source "$root/tools/launcher/update-deps.sh"
current_hash=$(find "$source_dir/pe/ui/scene" -type f -print0 | sort -z | xargs -0 sha256sum | sha256sum)
[[ $scene_hash == "$current_hash" ]] || { echo 'Scene headers changed during compilation; refusing a mixed-layout link. Run again from a stable snapshot.' >&2; exit 1; }
"$cxx" "${san[@]}" -Wl,--gc-sections "${objects[@]}" -lEGL -lGL -lm -lcrypto -lcurl -pthread -o "$build/pe_preview"
printf '%s\n' "$scene_hash" > "$build/scene-inputs"
echo "$build/pe_preview"
