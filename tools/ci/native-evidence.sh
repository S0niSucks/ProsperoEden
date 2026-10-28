#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Sourced by build scripts. No Python or evidence side effects without opt-in.
eden_native_capture() {
    [[ -n ${EDEN_NATIVE_EVIDENCE_DIR:-} ]] || return 0
    python3 -B "$root/tools/ci/native_evidence.py" capture "$@"
}

eden_native_run() {
    local -a options=()
    while [[ $# -gt 0 && $1 != -- ]]; do options+=("$1"); shift; done
    [[ ${1:-} == -- ]] || return 2
    shift
    if [[ -n ${EDEN_NATIVE_EVIDENCE_DIR:-} ]]; then
        python3 -B "$root/tools/ci/native_evidence.py" run "${options[@]}" -- "$@"
    else
        "$@"
    fi
}

# Explicit RADV ten-file postimage selection, not a source/cache directory walk.
eden_native_radv_sources() {
    [[ -n ${EDEN_NATIVE_EVIDENCE_DIR:-} ]] || return 0
    local phase=$1 path
    local -a options=(--phase "$phase")
    for path in src/vulkan/wsi/wsi_common_videoout.c src/amd/vulkan/radv_cmd_buffer.c \
        src/amd/vulkan/winsys/ps5/radv_ps5_cs.c src/amd/vulkan/radv_physical_device.c \
        src/amd/vulkan/winsys/ps5/radv_ps5_winsys.c src/amd/vulkan/winsys/ps5/radv_ps5_platform.c \
        src/amd/vulkan/cl/radv_gs_compute.cl src/util/disk_cache_os.c \
        src/amd/vulkan/meta/radv_meta_buffer.c src/amd/vulkan/meta/radv_meta_copy.c; do
        options+=(--file "$vulkan/.deps/work/radv-src/$path")
    done
    eden_native_capture "${options[@]}" --pending generated_driver_source_selection_pending
}
