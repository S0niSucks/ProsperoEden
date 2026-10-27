#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real launcher controller/rendering, synthetic Nextendo service. No accounts or public network."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("binary", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
args.output.mkdir(parents=True, exist_ok=True)
for width, height in ((1280, 720), (1920, 1080)):
    with tempfile.TemporaryDirectory(prefix="nextendo-ui-") as data:
        config = Path(data) / "config"
        config.mkdir()
        (config / "launcher.json").write_text(json.dumps({"schema": 1, "selected": "builtin"}))
        env = dict(os.environ, EGL_PLATFORM="surfaceless", LIBGL_ALWAYS_SOFTWARE="1",
                   GALLIUM_DRIVER="llvmpipe", PE_LAUNCHER_DATA=data)
        with (args.output / f"ui-{height}.log").open("w") as log:
            process = subprocess.Popen([str(args.binary.resolve()), str(root / "headless/prosperoeden/ui"),
                str(args.output.resolve()), str(width), str(height)], stdin=subprocess.PIPE,
                stdout=subprocess.PIPE, stderr=log, text=True, env=env)
            assert process.stdout.readline().startswith("READY builtin")

            def command(text):
                process.stdin.write(text + "\n")
                process.stdin.flush()
                lines = []
                while True:
                    line = process.stdout.readline()
                    assert line, "Preview exited during " + text
                    lines.append(line)
                    if line.startswith("OK "):
                        assert "GL=0" in line
                        return "".join(lines)

            def key(value):
                command("key " + value + " 4")

            def state():
                text = command("nextendo-state")
                return json.loads(next(line[9:] for line in text.splitlines() if line.startswith("NEXTENDO ")))

            def capture(name):
                command("tick 20")
                command("capture " + str((args.output / f"nextendo-{height}-{name}.png").resolve()))

            key("r1"); key("r1")
            capture("choice")
            key("down"); key("down"); key("cross")
            assert state()["calls"] == 0  # opening never contacts a server or imports anything
            capture("off")
            key("cross"); command("tick 20")
            assert state()["linked"] and not state()["enabled"]
            capture("linked-off")
            key("cross"); command("tick 20")  # existing account replacement needs confirmation
            assert state()["calls"] == 1
            key("down")  # moving focus cancels replacement confirmation
            key("cross"); command("tick 20")
            assert state()["enabled"]
            capture("routing-on")
            key("down"); key("cross"); command("tick 20")
            assert state()["checked"]
            capture("checked-not-online")
            key("down"); key("cross")
            assert state()["unlinks"] == 0
            capture("confirm-unlink")
            key("up"); key("down"); key("cross")
            assert state()["unlinks"] == 0  # moving cancelled the confirmation
            key("cross"); command("tick 20")
            assert state()["unlinks"] == 1 and not state()["linked"] and not state()["enabled"]
            key("up"); key("up"); key("up")
            command("nextendo-mode slow"); key("cross")
            key("circle"); command("tick 20")
            assert state()["error"] == "Nextendo action cancelled."
            command("nextendo-mode ready"); key("down"); key("down"); key("cross")
            capture("cancelled")
            process.stdin.write("exit\n"); process.stdin.flush()
            process.wait(timeout=15)
            assert process.returncode == 0
print("Nextendo UI: distinct local/online choice, opening offline, import-off, explicit enable, check wording, replace/unlink confirmation and cancellation at 720p/1080p: PASS")
