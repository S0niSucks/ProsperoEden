#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Run the repository's own NRO through Eden's real loader/JIT, without a console or game dumps.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
scratch=$(cat "$root/.local/headless-cache")
[[ "$(cat "$scratch/owner")" == "$root" ]]
mkdir -p "$root/results"
run=$(mktemp -d "$root/results/nro-host-$(date -u +%Y%m%d-%H%M%S).XXXXXX")
sha256sum "$scratch/build/bin/eden-headless" "$root/build/fixture/core-homebrew.nro" > "$run/hashes.txt"
mkdir "$run/user"
cd "$run"
timeout --kill-after=5s 60s "$scratch/build/bin/eden-headless" \
    "$root/build/fixture/core-homebrew.nro" --repeat > stdout.log 2> stderr.log
# Host receipts go to stdout beside this one startup diagnostic; retain every other line.
sed '/^EDEN_JIT_TABLE direct=1$/d' stdout.log > result.tsv
python3 -B "$root/headless/check.py" "$run" --repeat
echo "NRO host evidence: $run (three loader/JIT/guest-exit cycles, not PS5 gameplay)"
