#!/usr/bin/env python3
"""CodeQL #5-16: verify production RADV byte offsets widen BEFORE multiply.

Original Mesa snapshots stay immutable. Production inputs and exact source
oracle must agree. This is a host arithmetic regression, not a PS5 GPU test.
"""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
active = root / "vendor/encore-active/ps5-mesa/src/amd/vulkan"
pinned = root / "vendor/sources/ps5-mesa/src/amd/vulkan"
patcher = (root / "tools/patch-radv-wsi.py").read_text()

cmd_old = "uint64_t index_va = state->index_buffer.va + draw->firstIndex * index_size;"
cmd_new = "uint64_t index_va = state->index_buffer.va + (uint64_t)draw->firstIndex * index_size;"
cmd = (active / "radv_cmd_buffer.c").read_text()
orig_cmd = (pinned / "radv_cmd_buffer.c").read_text()
assert cmd.count(cmd_new) == 4 and cmd_old not in cmd
assert orig_cmd.count(cmd_old) == 4
assert json.dumps(cmd_old) in patcher and json.dumps(cmd_new) in patcher

changes = (
    ("cmask_offset += slice_size * range->baseArrayLayer;",
     "cmask_offset += (uint64_t)slice_size * range->baseArrayLayer;", 1),
    ("size = slice_size * vk_image_subresource_layer_count(&image->vk, range);",
     "size = (uint64_t)slice_size * vk_image_subresource_layer_count(&image->vk, range);", 2),
    ("fmask_offset += slice_size * range->baseArrayLayer;",
     "fmask_offset += (uint64_t)slice_size * range->baseArrayLayer;", 1),
    ("dcc_offset += image->planes[0].surface.meta_slice_size * range->baseArrayLayer +",
     "dcc_offset += (uint64_t)image->planes[0].surface.meta_slice_size * range->baseArrayLayer +", 1),
    ("size = image->planes[0].surface.u.gfx9.meta_levels[level].size * layer_count;",
     "size = (uint64_t)image->planes[0].surface.u.gfx9.meta_levels[level].size * layer_count;", 1),
    ("dcc_offset += dcc_level->dcc_offset + dcc_level->dcc_slice_fast_clear_size * range->baseArrayLayer;",
     "dcc_offset += dcc_level->dcc_offset + (uint64_t)dcc_level->dcc_slice_fast_clear_size * range->baseArrayLayer;", 1),
    ("size = dcc_level->dcc_slice_fast_clear_size * vk_image_subresource_layer_count(&image->vk, range);",
     "size = (uint64_t)dcc_level->dcc_slice_fast_clear_size * vk_image_subresource_layer_count(&image->vk, range);", 1),
    ("uint64_t size = image->planes[0].surface.meta_slice_size * layer_count;",
     "uint64_t size = (uint64_t)image->planes[0].surface.meta_slice_size * layer_count;", 1),
    ("image->planes[0].surface.meta_offset + image->planes[0].surface.meta_slice_size * range->baseArrayLayer;",
     "image->planes[0].surface.meta_offset + (uint64_t)image->planes[0].surface.meta_slice_size * range->baseArrayLayer;", 1),
)
meta = (active / "meta/radv_meta_clear.c").read_text()
raw_meta = (pinned / "meta/radv_meta_clear.c").read_text()
for before, after, count in changes:
    assert meta.count(after) == count, (after, meta.count(after))
    assert before not in meta, before
    assert raw_meta.count(before) == count, (before, raw_meta.count(before))
    assert json.dumps(before) in patcher and json.dumps(after) in patcher
assert sum(n for _, _, n in changes) == 10
assert "4 if (relative == 'src/amd/vulkan/radv_cmd_buffer.c'" in patcher
assert "2 if (relative == 'src/amd/vulkan/meta/radv_meta_clear.c'" in patcher

# The four extra stb_image_write reports are in a byte-pinned non-shipping
# NanoVG example. They must not prompt a silent rewrite of upstream source.
stb = root / "vendor/sources/opengl-nanovg/example/stb_image_write.h"
assert stb.is_file()
assert "malloc((x*n+1) * y)" in stb.read_text()
scope = (root / ".github/codeql/codeql-config.yml").read_text()
scan_root = scope.split("paths:\n", 1)[1].split("paths-ignore:", 1)[0]
assert "vendor/encore-active" in scan_root
assert "vendor/sources" not in scan_root

c = r"""
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <stddef.h>
int main(void) {
    // 32-bit multiplication would wrap; the byte-address offset must not.
    uint32_t firstIndex = UINT32_MAX - 7u;
    int index_size = 4;
    uint64_t base_va = 0x1000ULL;
    uint64_t index_va = base_va + (uint64_t)firstIndex * index_size;
    assert(index_va == base_va + 4ULL * ((uint64_t)UINT32_MAX - 7ULL));
    assert(index_va > UINT32_MAX);
    // Same regime for CMASK/FM​​ASK, DCC, and HTILE in a layered clear.
    unsigned slice_size = UINT32_MAX - 3u;
    uint32_t layers = 128u;
    uint64_t cmask_offset = (uint64_t)slice_size * layers;
    uint64_t fmask_offset = (uint64_t)slice_size * layers;
    uint64_t dcc_size = (uint64_t)slice_size * layers;
    uint64_t htile_size = (uint64_t)slice_size * layers;
    assert(cmask_offset == 128ULL * ((uint64_t)UINT32_MAX - 3ULL));
    assert(cmask_offset == fmask_offset && dcc_size == htile_size);
    assert(htile_size > UINT32_MAX);
    // Exact boundary where the legacy 32-bit product would overflow.
    uint64_t one_layer = (uint64_t)slice_size * 1u;
    assert(one_layer == UINT32_MAX - 3ULL);
}
"""
cc = shutil.which("clang") or shutil.which("gcc")
assert cc, "host C compiler missing"
with tempfile.TemporaryDirectory(prefix="eden-radv-width-") as tmp:
    src = Path(tmp) / "width.c"
    src.write_text(c)
    for opt in ("-O0", "-O2"):
        exe = Path(tmp) / ("width" + opt)
        build = subprocess.run(
            [cc, "-std=c11", opt, "-Wall", "-Wextra", "-Werror",
             "-fsanitize=undefined", "-fno-sanitize-recover=all",
             str(src), "-o", str(exe)], capture_output=True, text=True, timeout=30)
        assert build.returncode == 0, build.stderr[-3000:]
        run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15)
        assert run.returncode == 0, run.stderr[-3000:]
        print("RADV_CODEQL_WIDTH_PASS", opt, "UBSan=1 indexed4=1 meta10=1")
print("RADV_CODEQL_ACTIVE_INPUTS_PASS 12_alert_sites=1 immutable_sources=1")
