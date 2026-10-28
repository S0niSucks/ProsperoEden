#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds build/stubs/libSceAgcDriver.so, the libSceAgcDriver import facade both graphics
# drivers link against (tools/stubs/libSceAgcDriver.c), with the Payload SDK's compiler.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/tools/ci/native-evidence.sh"
sdk="$root/../ps5-native-app-boilerplate/.deps/native/ps5-payload-sdk"
source="$root/tools/stubs/libSceAgcDriver.c"
out="$root/build/stubs/libSceAgcDriver.so"
if [[ -f $out && $out -nt $source ]]; then
    eden_native_capture --phase agc-stub-reused --file "$source" --file "$out" --pending reused_stub_object_missing
    exit 0
fi
test -x "$sdk/bin/prospero-clang" || { echo "Missing the Payload SDK; run make deps" >&2; exit 1; }
mkdir -p "$root/build/stubs"
if [[ -n ${EDEN_NATIVE_EVIDENCE_DIR:-} ]]; then
    work=$(mktemp -d "$root/build/stubs/native-evidence.XXXXXX")
else
    work=$(mktemp -d)
fi
trap 'rm -rf "$work"' EXIT
PS5_PAYLOAD_SDK="$sdk" eden_native_run --phase agc-stub-compile --input "$source" \
    --output "$work/libSceAgcDriver.o" -- \
    "$sdk/bin/prospero-clang" -fPIC -c "$source" -o "$work/libSceAgcDriver.o"
eden_native_run --phase agc-stub-link --input "$work/libSceAgcDriver.o" \
    --output "$work/libSceAgcDriver.so" -- \
    "$sdk/bin/prospero-lld" --shared -soname libSceAgcDriver.prx -o "$work/libSceAgcDriver.so" "$work/libSceAgcDriver.o"
mv "$work/libSceAgcDriver.so" "$out"
eden_native_capture --phase agc-stub-installed --file "$out"
