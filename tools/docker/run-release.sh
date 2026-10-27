#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

[[ -f /checkout/UPSTREAM.json ]] || { echo 'Mount the checkout read-only at /checkout' >&2; exit 2; }
mkdir -p /work/ProsperoEden
rsync -a --exclude=/.git --exclude=/.deps --exclude=/.local --exclude=/build \
    --exclude=/dist --exclude=/results /checkout/ /work/ProsperoEden/
cd /work/ProsperoEden
bash tools/check-toolchain.sh
make release JOBS="${EDEN_BUILD_JOBS:-4}" 2>&1 | tee /work/release-build.log
echo 'Release artifacts: /work/ProsperoEden/dist (not deployed or PS5-qualified)'
