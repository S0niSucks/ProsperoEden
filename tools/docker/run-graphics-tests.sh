#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
[[ -f /checkout/UPSTREAM.json ]] || { echo 'Mount the checkout read-only at /checkout' >&2; exit 2; }
[[ -d /evidence ]] || { echo 'Mount a writable evidence folder at /evidence' >&2; exit 2; }
python3 -B /checkout/tools/check-quick-overlay.py /evidence/opengl
python3 -B /checkout/tools/check-quick-vulkan.py /evidence/vulkan
