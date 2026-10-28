#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Serial local-fixture network checks against this run's freshly built core.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
[[ -f /.dockerenv ]] || { echo 'Run the network checks inside Docker.' >&2; exit 2; }
scratch=${1:?usage: run-network-tests.sh CURRENT_HOST_CACHE}
[[ -f "$scratch/build/compile_commands.json" && -f "$scratch/source/CMakeLists.txt" ]]
[[ -f "$root/.deps/libsmb2-libsmb2-6.3/CMakeLists.txt" && -f "$root/.deps/libnfs-libnfs-8.0.0/CMakeLists.txt" ]] || {
    echo 'Fetch the pinned libsmb2/libnfs inputs first.' >&2; exit 2;
}
# Existing checks use this private container path, never a user share/config root.
if [[ ! -e /work/deps && ! -L /work/deps ]]; then ln -s "$root/.deps" /work/deps; fi
[[ $(realpath /work/deps) == "$(realpath "$root/.deps")" ]] || {
    echo '/work/deps must identify this snapshot dependency directory.' >&2; exit 2;
}
export EDEN_NETWORK_DEPS="$root/.deps" NETWORK_SAVED_CACHE="$scratch"
NETWORK_WORK=$(mktemp -d "$scratch/network-tests.XXXXXX")
export NETWORK_WORK
python3 -B "$root/tools/check-network-shares.py"
python3 -B "$root/tools/check-network-security.py"
python3 -B "$root/tools/check-network-integration.py"
python3 -B "$root/tools/check-network-loader.py"
python3 -B "$root/tools/check-network-frontend.py"
python3 -B "$root/tools/check-network-editor.py"
python3 -B "$root/headless/network/compose_check.py"
NETWORK_SANITIZE=1 NETWORK_WORK="$NETWORK_WORK/sanitized" python3 -B "$root/tools/check-network-shares.py"
NETWORK_SANITIZE=1 NETWORK_WORK="$NETWORK_WORK/sanitized" python3 -B "$root/tools/check-network-editor.py"
python3 -B "$root/tools/check-network-nfs-server.py"
echo "Network local-fixture suite passed; evidence: $NETWORK_WORK (not PS5 or live-provider qualification)."
