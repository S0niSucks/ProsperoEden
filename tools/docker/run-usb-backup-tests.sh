#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
# This suite clears the fixture drive. Never run it against a physical USB or a bind mount.
[[ -f /.dockerenv && -d /work && -d /mnt/usb0 ]] &&
    mountpoint -q /mnt/usb0 && [[ $(stat -f -c %T /mnt/usb0) == tmpfs ]] || {
    echo 'USB checks require Docker, a private /work volume and an independent tmpfs at /mnt/usb0.' >&2
    exit 2
}
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$root"
clang++-18 -std=c++20 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    headless/usb_backup.cpp headless/usb_backup_check.cpp -lcrypto -pthread \
    -Wl,--wrap=openat,--wrap=fstatvfs,--wrap=fstatat,--wrap=fsync,--wrap=read,--wrap=write,--wrap=renameat,--wrap=unlinkat \
    -o /work/usb_backup_check
ASAN_OPTIONS=detect_leaks=1 /work/usb_backup_check
clang++-18 -std=c++20 -Wall -Wextra -Werror -D__PROSPERO__ -fsyntax-only headless/usb_backup.cpp
clang++-18 -std=c++20 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    headless/usb_backup.cpp headless/usb_backup_service_check.cpp -lcrypto -ldl -pthread -o /work/usb_backup_service_check
ASAN_OPTIONS=detect_leaks=1 /work/usb_backup_service_check
clang++-18 -std=c++20 -Wall -Wextra -Werror -fsyntax-only -Iheadless -Iheadless/prosperoeden \
    -Ithird_party -Ithird_party/lua headless/usb_backup_service_syntax.cpp
mkdir -p /work/usb-test-include
python3 -B - <<'PY'
import json
from pathlib import Path
version = json.loads(Path('sce_sys/param.json').read_text())['contentVersion']
Path('/work/usb-test-include/app_version.h').write_text('#define EDEN_APP_VERSION ' + json.dumps(version) + '\n')
PY
clang++-18 -std=c++20 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I/work/usb-test-include headless/update_notice.cpp headless/usb_backup_update_check.cpp \
    -pthread -o /work/usb_backup_update_check
ASAN_OPTIONS=detect_leaks=1 /work/usb_backup_update_check
# Compile just the changed UI translation units, not the emulator or a native build.
for source in usb_backup settings launcher runtime_launcher; do
    clang++-18 -std=c++20 -Wall -Wextra -Werror -fsyntax-only -Iheadless/prosperoeden \
        -Ithird_party -Ithird_party/lua "headless/prosperoeden/pe/ui/$source.cpp"
done
