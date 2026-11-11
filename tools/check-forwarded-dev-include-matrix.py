#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Docker-only syntax probe of main's extracted dev includes/consumer references.

NOT a full-main, CMake, native SDK, argv, handoff or exit test. The exact initial
preamble is retained, except development_input.h is replaced by an include-gating
marker: its SDK-dependent implementation is NOT compiled. Both portable policy
headers are real files, never redeclared in the harness. Consumer guards and
BootIntent/replay references are extracted from the selected main.cpp.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys


TITLE = "0100000000000000"
MATRIX = {
    "shipping": ["-DEDEN_FINAL_RELEASE_PROFILE=1"],
    "rom-id-only": [f'-DEDEN_DEV_ROM_ID="{TITLE}"'],
    "profile-only": ["-DEDEN_DEV_PROFILE=1", f'-DEDEN_DEV_PROFILE_TITLE="{TITLE}"'],
    "both": [f'-DEDEN_DEV_ROM_ID="{TITLE}"', "-DEDEN_DEV_PROFILE=1",
             f'-DEDEN_DEV_PROFILE_TITLE="{TITLE}"'],
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def unique(source, text):
    if source.count(text) != 1:
        raise ValueError(f"Expected one source anchor: {text!r}")
    return source.index(text)


def preceding_line(source, anchor):
    return source[:unique(source, anchor)].rstrip().splitlines()[-1].strip()


def extract(main):
    source = main.read_text(encoding="utf-8")
    preamble = source[:unique(source, "#include <utility>")]
    includes = ['#include "development_input.h"', '#include "dev_replay_policy.h"',
                '#include "dev_launch_policy.h"']
    for include in includes:
        unique(preamble, include)
    for line in preamble.splitlines():
        if line.lstrip().startswith("#include") and line.strip() not in includes:
            raise ValueError("Preamble changed: refusing additional, unreviewed headers")
    boot_guard = preceding_line(source, "        bool autoboot_pending = false;")
    if boot_guard != "#if defined(EDEN_DEV_PROFILE) || defined(EDEN_DEV_ROM_ID)":
        raise ValueError("BootIntent consumer guard changed; review extraction")
    declaration = "Eden::DevLaunch::BootIntent boot;"
    unique(source, declaration)
    replay_guard = preceding_line(
        source, "// Live DualSense is ALWAYS the default, including the all-on")
    if replay_guard != "#ifdef EDEN_DEV_PROFILE":
        raise ValueError("Replay consumer guard changed; review extraction")
    replay = "replay_requested = Eden::DevInput::ReplayRequestedAfter(replay_requested, entry);"
    unique(source, replay)
    timed = re.findall(
        r"const bool timed_replay = Eden::DevInput::ScriptedReplayEnabled\(\s*"
        r"replay_requested, development_id, EDEN_DEV_PROFILE_TITLE\);", source)
    if len(timed) != 1:
        raise ValueError("Replay consumer expression changed; review extraction")
    projected = preamble.replace(includes[0], "#define A11_ROM_INPUT_SELECTED 1")
    probe = projected + r'''
// Marker checks the original ROM-only guard, not DevelopmentInput's SDK ABI.
#if defined(EDEN_DEV_ROM_ID) && !defined(A11_ROM_INPUT_SELECTED)
#error ROM_ID must retain its development_input include
#elif !defined(EDEN_DEV_ROM_ID) && defined(A11_ROM_INPUT_SELECTED)
#error development_input must remain ROM_ID-only
#endif
#include <string>
int main() {
''' + boot_guard + "\n    " + declaration + r'''
    (void)boot;
#endif
''' + replay_guard + r'''
    bool replay_requested = false;
    const std::string entry = "replay=off";
    const std::string development_id = EDEN_DEV_PROFILE_TITLE;
    ''' + replay + "\n    " + timed[0] + r'''
    (void)timed_replay;
#endif
    return 0;
}
'''
    return probe, hashlib.sha256(preamble.encode("utf-8")).hexdigest()


def compile_probe(command, cwd):
    process = subprocess.Popen(command, cwd=cwd, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True,
                               start_new_session=True)
    try:
        output, _ = process.communicate(timeout=30)
        return process.returncode, output, False
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        output, _ = process.communicate()
        return process.returncode, output, True


def main():
    if sys.platform != "linux" or not Path("/.dockerenv").is_file():
        raise SystemExit("Run only in A50's approved Docker sandbox; not the host.")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--baseline-main", type=Path,
                        help="Optional immutable pre-fix main; PROFILE-only must fail")
    parser.add_argument("--output", type=Path, required=True, help="New external output directory")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    args = parser.parse_args()
    repo = args.repo.resolve()
    compiler = shutil.which(args.cxx)
    if not compiler:
        raise SystemExit(f"C++20 compiler not found: {args.cxx}")
    output = args.output.resolve()
    if output == repo or repo in output.parents:
        raise SystemExit("Output must be outside the read-only input tree")
    output.mkdir(parents=True, exist_ok=False)
    headers = repo / "headless"
    inputs = {"candidate": headers / "main.cpp"}
    if args.baseline_main:
        inputs["baseline"] = args.baseline_main.resolve()
    receipt = {
        "scope": "extracted preamble/consumer syntax only; no full main/native/runtime",
        "status": "FAIL", "compiler": compiler,
        "fixture_sha256": sha256(Path(__file__).resolve()),
        "headers": {name: sha256(headers / name)
                    for name in ("dev_launch_policy.h", "dev_replay_policy.h")},
        "inputs": {}, "matrix": [],
    }
    failures = []
    try:
        for label, selected_main in inputs.items():
            probe, preamble_hash = extract(selected_main)
            translation_unit = output / f"{label}-extracted.cpp"
            translation_unit.write_text(probe, encoding="utf-8")
            receipt["inputs"][label] = {
                "path": str(selected_main), "sha256": sha256(selected_main),
                "preamble_utf8_lf_sha256": preamble_hash,
                "probe_sha256": sha256(translation_unit),
            }
            for variant, definitions in MATRIX.items():
                command = [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                           "-fsyntax-only", "-DPS5_NATIVE=1", *definitions,
                           "-I", str(headers), str(translation_unit)]
                code, log, timed_out = compile_probe(command, output)
                (output / f"{label}-{variant}.log").write_text(log, encoding="utf-8")
                negative = label == "baseline" and variant == "profile-only"
                okay = not timed_out and (
                    (code > 0 and "DevLaunch" in log and "DevInput" in log
                     and "internal compiler error" not in log.lower())
                    if negative else code == 0)
                receipt["matrix"].append({
                    "input": label, "variant": variant, "command": command,
                    "returncode": code, "timeout": timed_out,
                    "expected": "missing DevLaunch/DevInput" if negative else "syntax success",
                    "matched_expectation": okay,
                })
                if not okay:
                    failures.append(f"{label}/{variant}")
        receipt["status"] = "FAIL" if failures else "PASS_EXTRACTED_SYNTAX_ONLY"
    except Exception as error:
        receipt["error"] = str(error)
        raise
    finally:
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    if failures:
        raise SystemExit("Matrix did not match expectations: " + ", ".join(failures))
    print("PASS extracted dev-include matrix only; no full-main/native/runtime claim")


if __name__ == "__main__":
    main()
