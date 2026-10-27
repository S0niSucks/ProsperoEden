#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
exec 9>/preview/.prosperoeden-preview.lock
flock -n 9 || { echo 'This Docker preview volume is busy.' >&2; exit 2; }
# Independent Linux snapshot/cache: never overwrite an active host/native build's sources.
mkdir -p /preview/source
rsync -a --exclude=/.git --exclude=/.deps --exclude=/.local --exclude=/build \
    --exclude=/dist --exclude=/results /checkout/ /preview/source/
cd /preview/source
python3 -B tools/ci/source-snapshot.py /checkout /preview/source /preview/cheat-ui-source-snapshot.json
export PROSPEROEDEN_LAUNCHER_BUILD=/preview/cache-cheat-ui-asan
export HOST_CXX=clang++-18 PE_PREVIEW_SANITIZE=1 HOST_BUILD_JOBS=4
export ASAN_OPTIONS=detect_leaks=0
# Mesa driver's process-global allocations are outside this check; address/UB checks stay on.
printf 'exit\n' | bash tools/launcher/runtime-preview.sh /preview/cheat-ui-smoke 1280 720
python3 -B tools/launcher/test-cheat-database-ui.py "$PROSPEROEDEN_LAUNCHER_BUILD/pe_preview" /preview/cheat-ui-results
python3 -B tools/launcher/test-runtime-workflows.py "$PROSPEROEDEN_LAUNCHER_BUILD/pe_preview"
echo 'Cheat launcher tests completed (real renderer/controllers, simulated backend; not PS5 gameplay)'
