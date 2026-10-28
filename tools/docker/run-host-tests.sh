#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

# Keep Linux builds off the Windows bind mount and never write to the checkout.
[[ -f /checkout/UPSTREAM.json ]] || { echo 'Mount the checkout read-only at /checkout' >&2; exit 2; }
[[ -f /.dockerenv && -d /mnt/usb0 ]] && mountpoint -q /mnt/usb0 &&
    [[ $(stat -f -c %T /mnt/usb0) == tmpfs ]] || {
    echo 'The full host suite requires --tmpfs /mnt/usb0:rw,size=256m for its synthetic USB checks.' >&2
    exit 2
}
# Never rsync a new snapshot over a build already using this volume.
exec 9>/work/.prosperoeden-build.lock
flock -n 9 || { echo 'This Docker build volume is busy; use a separate volume or wait for its job.' >&2; exit 2; }
mkdir -p /work/ProsperoEden
rsync -a --exclude=/.git --exclude=/.deps --exclude=/.local --exclude=/build \
    --exclude=/dist --exclude=/results /checkout/ /work/ProsperoEden/
cd /work/ProsperoEden
python3 -B tools/ci/source-snapshot.py /checkout /work/ProsperoEden build/host-source-snapshot.json
python3 -B tools/deps.py fetch eden fmt compiler-rt libsmb2 libnfs
bash tools/check-elevation-client.sh
bash tools/build-headless-host.sh
bash tools/docker/run-usb-backup-tests.sh
