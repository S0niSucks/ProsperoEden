#!/usr/bin/env python3
"""Cover Flow's additive Lua preferences contract, through the real runtime host."""
import json
import os
from pathlib import Path
import shutil
import runpy
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
marked = runpy.run_path(str(root / "tools/launcher/strings.py"))["marked_text"]()
for message in ["Library view", "Cover Flow", "Grid", "Cover unavailable",
                "Browse covers in a grid or in perspective with Cover Flow."]:
    assert message in marked, "Translation extractor missed " + message
print("PASS translation discovery for all five new msgids (no catalog writes)")
with tempfile.TemporaryDirectory(prefix="pe-cover-flow-api-") as scratch:
    out = Path(scratch)
    data = out / "data"
    folder = data / "launchers/cover-flow-test"
    folder.mkdir(parents=True)
    shutil.copytree(root / "examples/grid-launcher/assets", folder / "assets")
    manifest = json.loads((root / "examples/grid-launcher/manifest.json").read_text())
    manifest["id"] = "cover-flow-test"
    (folder / "manifest.json").write_text(json.dumps(manifest))
    (folder / "main.lua").write_text('''
return {init=function() end, draw=function() end, input=function(e)
    local before=host.query("preferences")
    local patch={}
    if e.key=="cross" then patch.library_view="cover-flow"
    elseif e.key=="right" then patch.menu_volume=45
    elseif e.key=="left" then patch.library_view="not-a-view"
    elseif e.key=="square" then patch.library_view=true
    elseif e.key=="down" then patch.library_view="grid" end
    local result=host.action("preferences.set",patch)
    host.action("state.save",{before=before.library_view,result=result,
        after=host.query("preferences").library_view})
end}
''')
    (data / "config").mkdir(exist_ok=True)
    (data / "config/launcher.json").write_text(json.dumps({"schema": 1, "selected": "cover-flow-test"}))
    env = dict(os.environ, EGL_PLATFORM="surfaceless", LIBGL_ALWAYS_SOFTWARE="1",
               GALLIUM_DRIVER="llvmpipe", PE_LAUNCHER_DATA=str(data))
    p = subprocess.Popen([sys.argv[1], str(root / "headless/prosperoeden/ui"), str(out), "640", "360"],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         text=True, env=env)
    assert p.stdout.readline().startswith("READY cover-flow-test")
    for key, success, before, after in [
        ("cross", True, "grid", "cover-flow"),
        ("right", True, "cover-flow", "cover-flow"),
        ("left", False, "cover-flow", "cover-flow"),
        ("square", False, "cover-flow", "cover-flow"),
        ("down", True, "cover-flow", "grid"),
    ]:
        p.stdin.write("key " + key + " 3\n")
        p.stdin.flush()
        while True:
            line = p.stdout.readline()
            assert line, "Runtime exited before responding"
            if line.startswith("OK "):
                assert "GL=0" in line, line
                break
        state = json.loads((data / "config/launcher-state/cover-flow-test.json").read_text())
        assert state["result"]["ok"] is success, state
        assert state["before"] == before and state["after"] == after, state
    stdout, stderr = p.communicate("select builtin\nprefs\nexit\n", timeout=30)
    assert p.returncode == 0, stderr
    assert '"library_view":"grid"' in stdout, stdout
    assert "GL=128" not in stdout, stdout
    print("PASS Lua query/set, unrelated legacy patch preservation, invalid value/type rejection, builtin return")
