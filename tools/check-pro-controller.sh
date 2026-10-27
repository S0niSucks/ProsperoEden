#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Focused checks only. Run in Docker with checkout read-only, output on own volume.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
out=${PRO_CONTROLLER_TEST_OUT:-/work/pro-controller/check}
mkdir -p "$out"
flags=(-g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer)
clang-18 -std=c11 "${flags[@]}" -c "$root/headless/pro_controller/pro_controller.c" -o "$out/protocol.o"
clang-18 -std=c11 "${flags[@]}" -c "$root/headless/pro_controller/pro_controller_usb.c" -o "$out/usb-host.o"
clang++-18 -std=c++20 "${flags[@]}" -I"$root/headless" -I"$root/third_party" \
    "$root/headless/pro_controller/check.cpp" "$out/protocol.o" "$out/usb-host.o" -o "$out/check"
"$out/check"
clang-18 -std=c11 -D_POSIX_C_SOURCE=200809L "${flags[@]}" \
    -c "$root/headless/prosperoeden/radio_input.c" -o "$out/radio-input.o"
clang++-18 -std=c++20 "${flags[@]}" -I"$root/headless" -I"$root/third_party" \
    "$root/headless/pro_controller/radio_check.cpp" "$out/protocol.o" "$out/radio-input.o" -o "$out/radio-check"
"$out/radio-check"
if [[ -n ${PS5_PAYLOAD_SDK:-} ]]; then
    for name in pro_controller pro_controller_usb; do
        clang-18 --target=x86_64-sie-ps5 -isysroot "$PS5_PAYLOAD_SDK" \
            -isystem "$PS5_PAYLOAD_SDK/target/include" -std=c11 -DPS5_NATIVE=1 \
            -fPIC -fno-stack-protector -Wall -Wextra -Werror -c \
            "$root/headless/pro_controller/$name.c" -o "$out/$name-native.o"
    done
    clang-18 --target=x86_64-sie-ps5 -isysroot "$PS5_PAYLOAD_SDK" \
        -isystem "$PS5_PAYLOAD_SDK/target/include" -std=c11 -DPS5_NATIVE=1 \
        -fno-stack-protector -Wall -Wextra -Werror -c \
        "$root/headless/prosperoeden/radio_input.c" -o "$out/radio-input-native.o"
    echo 'Pro Controller PS5-target objects PASS (not hardware execution)'
    ld.lld-18 -shared --no-undefined -L"$PS5_PAYLOAD_SDK/target/lib" \
        "$out/pro_controller-native.o" "$out/pro_controller_usb-native.o" \
        -lc -lkernel -lSceLibcInternal -lSceNet -o "$out/pro-controller-native-smoke.so"
    echo 'Pro Controller PS5 SDK symbol/link smoke PASS (not a packaged app)'
    if [[ -n ${EDEN_SOURCE:-} && -n ${BOOST_SOURCE:-} && -n ${FMT_SOURCE:-} ]]; then
        boost_includes=()
        while IFS= read -r dir; do boost_includes+=(-isystem "$dir"); done \
            < <(find "$BOOST_SOURCE/libs" -type d -name include)
        clang++-18 --target=x86_64-sie-ps5 -isysroot "$PS5_PAYLOAD_SDK" \
            -isystem "$PS5_PAYLOAD_SDK/target/include/c++/v1" \
            -isystem "$PS5_PAYLOAD_SDK/target/include" "${boost_includes[@]}" \
            -isystem "$FMT_SOURCE/include" -I"$EDEN_SOURCE/src" -I"$root/headless" -I"$root/third_party" \
            -std=c++23 -fno-rtti -DPS5_NATIVE=1 -D_LIBCPP_ENABLE_EXPERIMENTAL=1 \
            -fno-stack-protector -femulated-tls -fexceptions -fcxx-exceptions \
            -c "$root/headless/pad.cpp" -o "$out/pad-native.o"
        echo 'Pro Controller real Pad/Eden PS5-target object PASS (no emulator build)'
    fi
fi
