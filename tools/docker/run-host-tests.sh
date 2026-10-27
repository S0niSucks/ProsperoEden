#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

# Keep Linux builds off the Windows bind mount and never write to the checkout.
[[ -f /checkout/UPSTREAM.json ]] || { echo 'Mount the checkout read-only at /checkout' >&2; exit 2; }
mkdir -p /work/ProsperoEden
rsync -a --exclude=/.git --exclude=/.deps --exclude=/.local --exclude=/build \
    --exclude=/dist --exclude=/results /checkout/ /work/ProsperoEden/
cd /work/ProsperoEden
python3 -B tools/deps.py fetch eden fmt compiler-rt
bash tools/build-headless-host.sh
