#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
exec 9>/tmp/.amiibo-sync-test.lock
flock -n 9 || { echo 'The private Amiibo test volume is busy.' >&2; exit 2; }
python3 -B /checkout/tools/check-amiibo-sync.py
python3 -B /checkout/tools/check-launcher-admission.py
python3 -B /checkout/tools/check-cheat-manager.py
mkdir -p /tmp/source
rsync -a --exclude=/.git --exclude=/.deps --exclude=/.local --exclude=/build \
    --exclude=/dist --exclude=/results /checkout/ /tmp/source/
export PROSPEROEDEN_LAUNCHER_BUILD=/tmp/ui-build HOST_CXX=clang++-18 HOST_BUILD_JOBS=2 PE_PREVIEW_SANITIZE=1
export ASAN_OPTIONS=detect_leaks=0
printf 'exit\n' | bash /tmp/source/tools/launcher/runtime-preview.sh /tmp/ui-smoke 1280 720
python3 -B /tmp/source/tools/launcher/test-amiibo-sync-ui.py /tmp/ui-build/pe_preview /tmp/amiibo-ui-results
