#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Linked launcher only; never builds the emulator or console target. Dependencies are pinned
# by tools/deps.json and must already have been fetched on the authoring PC.
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
export PE_SCENE_FRONTEND_BUILD=${PE_SCENE_FRONTEND_BUILD:-/work/scene-linked}
export HOST_BUILD_JOBS=1 PE_PREVIEW_SANITIZE=1
export PROSPEROEDEN_UPDATE_TEST_BUILD=/work/scene-updater
export XDG_CACHE_HOME=/work/cache
bash "$root/tools/launcher/scene/build_frontend.sh"
bash "$root/tools/launcher/test-launcher-updates.sh"
evidence=$(mktemp -d "$PE_SCENE_FRONTEND_BUILD/evidence.XXXXXX")
python3 -B "$root/tools/launcher/scene/test_frontend.py" "$PE_SCENE_FRONTEND_BUILD/pe_preview" "$evidence"
