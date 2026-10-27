#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
mkdir -p /work/source
rsync -a --exclude=/.git --exclude=/.deps --exclude=/.local --exclude=/build --exclude=/dist --exclude=/results /checkout/ /work/source/
cd /work/source
# Header timestamps copied from Windows are not a safe ABI cache key. A clean small launcher
# build prevents mixed objects after a shared status/Launcher layout changes (not an emulator build).
export PROSPEROEDEN_LAUNCHER_BUILD
PROSPEROEDEN_LAUNCHER_BUILD=$(mktemp -d /work/usb-ui-build.XXXXXX)
export HOST_CXX=clang++-18 HOST_BUILD_JOBS=2
export PE_PREVIEW_SANITIZE=1 ASAN_OPTIONS=detect_leaks=0
printf 'exit\n' | bash tools/launcher/runtime-preview.sh /work/usb-ui-smoke 1280 720
python3 -B tools/launcher/test-usb-backup-ui.py "$PROSPEROEDEN_LAUNCHER_BUILD/pe_preview" /work/usb-ui-results
