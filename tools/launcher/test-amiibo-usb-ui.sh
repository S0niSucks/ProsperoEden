#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# One immutable integrated snapshot + fresh Mesa build; only the workstream's private volume.
# Usage: test-amiibo-usb-ui.sh source.tar source.json private-volume-root export-directory
set -euo pipefail
archive=${1:?source.tar}; provenance=${2:?source.json}; volume=${3:?private volume}; export_dir=${4:?export directory}
exec 9>"$volume/.amiibo-sync-test.lock"
flock -n 9 || { echo 'The private Amiibo test volume is busy.' >&2; exit 2; }
run=$(mktemp -d "$volume/amiibo-usb-ui.XXXXXX")
root="$run/source"; results="$run/results"
mkdir -p "$root" "$results" "$export_dir"
trap 'cp -a "$results/." "$export_dir/"' EXIT
cp "$provenance" "$results/source.json"
cp "$archive" "$results/source.tar"
python3 -B - "$results" <<'PY'
from pathlib import Path
import hashlib, json, sys
p = Path(sys.argv[1])
with (p / 'source.tar').open('rb') as stream:
    actual = hashlib.file_digest(stream, 'sha256').hexdigest()
assert actual == json.loads((p / 'source.json').read_text())['archive_sha256']
PY
tar -xf "$archive" -C "$root"
bash -n "$root/tools/launcher/test-amiibo-usb-ui.sh"
python3 -B - "$root/tools/launcher" <<'PY'
from pathlib import Path
import ast, sys
for name in ('ui_test_helpers.py', 'test-usb-backup-ui.py', 'test-amiibo-sync-ui.py', 'test-amiibo-usb-ui.py'):
    path = Path(sys.argv[1]) / name
    ast.parse(path.read_text(), filename=str(path))
PY
export PROSPEROEDEN_LAUNCHER_BUILD="$run/build" HOST_CXX=clang++-18 HOST_CC=clang-18 HOST_BUILD_JOBS=2
export PE_PREVIEW_SANITIZE=1 ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
export MESA_GL_VERSION_OVERRIDE=4.6 MESA_GLSL_VERSION_OVERRIDE=460
export PE_UI_SNAPSHOT_INFO="$results/source.json"
step() {
    local name=$1; shift
    if ! "$@" >"$results/$name.log" 2>&1; then cat "$results/$name.log"; return 1; fi
    cat "$results/$name.log"
}
# The directory is new, so no Windows timestamp or mixed-ABI object cache can participate.
step build bash "$root/tools/launcher/runtime-preview.sh" "$results/smoke" 1280 720 <<< 'exit'
binary="$PROSPEROEDEN_LAUNCHER_BUILD/pe_preview"
ldd "$binary" >"$results/preview-libraries.log"
sha256sum "$PROSPEROEDEN_LAUNCHER_BUILD"/obj/*.o >"$results/preview-objects.sha256"
step usb timeout 180 python3 -B "$root/tools/launcher/test-usb-backup-ui.py" "$binary" "$results/usb"
step amiibo timeout 180 python3 -B "$root/tools/launcher/test-amiibo-sync-ui.py" "$binary" "$results/amiibo"
step ownership timeout 240 python3 -B "$root/tools/launcher/test-amiibo-usb-ui.py" "$binary" "$results/ownership"
# Link the existing focused Cover Flow driver to these very same renderer/runtime/fake-service
# objects and Mesa libraries. Do not rebuild the launcher, core, emulator, or console target.
objects=()
for object in "$PROSPEROEDEN_LAUNCHER_BUILD"/obj/*.o; do
    [[ $object == */host_runtime_preview.cpp.o ]] || objects+=("$object")
done
step cover-flow-build "$HOST_CXX" -std=c++20 -O2 -Wall -Wextra -DGL_GLEXT_PROTOTYPES=1 \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$root/headless/prosperoeden" -I"$root/headless/prosperoeden/host" -I"$root/headless" \
    -I"$root/tools/launcher/stb" -I"$root/third_party" -I"$root/third_party/lua" \
    "$root/tools/launcher/test-cover-flow.cpp" "${objects[@]}" \
    -lEGL -lGL -lm -lcrypto -lcurl -pthread -o "$PROSPEROEDEN_LAUNCHER_BUILD/test-cover-flow"
for height in 720 1080; do
    width=$((height * 16 / 9))
    step "cover-flow-$height" timeout 180 "$PROSPEROEDEN_LAUNCHER_BUILD/test-cover-flow" \
        "$root/headless/prosperoeden/ui" "$results/cover-flow/${height}p" "$width" "$height"
done
step cover-flow-api timeout 90 python3 -B "$root/tools/launcher/test-cover-flow-api.py" "$binary"
python3 -B - "$results" "$binary" "$PROSPEROEDEN_LAUNCHER_BUILD/test-cover-flow" <<'PY'
from pathlib import Path
import hashlib, json, subprocess, sys
p, preview, cover = map(Path, sys.argv[1:])
def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()
reports = {name: json.loads((p / name / 'report.json').read_text()) for name in ('usb', 'amiibo', 'ownership')}
assert all(r['passed'] and r['preview_sha256'] == digest(preview) for r in reports.values())
assert len(reports['usb']['checks']) == 20 and len(reports['amiibo']['checks']) == 26
manifest = {'passed': True, 'source_snapshot': json.loads((p / 'source.json').read_text()),
            'preview_sha256': digest(preview), 'cover_flow_sha256': digest(cover),
            'compiler': subprocess.check_output(['clang++-18', '--version'], text=True).splitlines()[0],
            'sanitizers': 'address,undefined; UBSan halt_on_error; Mesa leak accounting disabled',
            'checks': {name: len(report['checks']) for name, report in reports.items()},
            'scope': 'Actual integrated UI with fake services; no hardware, writer lifecycle or service-drain qualification',
            'artifacts': {str(f.relative_to(p)): digest(f) for f in sorted(p.rglob('*')) if f.is_file()}}
(p / 'run.json').write_text(json.dumps(manifest, indent=2))
print('COMBINED UI PASS', json.dumps(manifest['checks']), 'preview_sha256=' + manifest['preview_sha256'])
PY
echo "Private results: $results"
