#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""F07 C01/T01: compile real PS5 graphics and compute sampler memo hotpaths.

No GPU, firmware, or FPS claim. The source-extracted bounded table must avoid
repeated texture-cache lookups when handles collide in their low four bits,
preserve all 16 entries under overflow, and never persist a sampler across
draws/dispatches or after a failed lookup.
"""
from pathlib import Path
import ast
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VULKAN = ROOT / "vendor/encore-active/eden-vulkan"
graphics = (VULKAN / "vk_graphics_pipeline_cost.cpp").read_text()
compute = (VULKAN / "vk_compute_pipeline_cost.cpp").read_text()
generator = (ROOT / "tools/prepare-vulkan-port.py").read_text()
oracle = (ROOT / "tools/check-vendored-vulkan-fork.py").read_text()

def extract(source, first, last):
    assert source.count(first) == 1 and source.count(last) >= 1
    start = source.index(first)
    end = source.index(last, start)
    return source[start:end]

graphics_memo = extract(
    graphics, "    struct SamplerMemo {",
    "    const auto config_stage{[&](size_t stage) LAMBDA_FORCEINLINE {")
compute_memo = extract(
    compute, "    struct ComputeSamplerMemo {",
    "#endif\n    const auto read_handle{[&](const auto& desc, u32 index) {")
assert graphics.count("std::array<SamplerMemo, 16> sampler_memo{};") == 1
assert compute.count("std::array<ComputeSamplerMemo, 16> sampler_memo{};") == 1
assert "sampler_memo[handle & 15U]" not in graphics + compute
assert "return texture_cache.GetSamplerId(handle, false);" in graphics_memo
assert "return texture_cache.GetSamplerId(handle, true);" in compute_memo
assert "for (std::size_t offset = 0; offset < sampler_memo.size(); ++offset)" in graphics_memo
assert "for (std::size_t offset = 0; offset < sampler_memo.size(); ++offset)" in compute_memo
assert compute_memo in generator, "compute generator drift"
oracle_line = next(line for line in oracle.splitlines()
                   if line.startswith("    sampler_memo = "))
assert ast.literal_eval(oracle_line.split(" = ", 1)[1]).lstrip().startswith(
    "// Sampler table mapping")
assert graphics_memo in ast.literal_eval(oracle_line.split(" = ", 1)[1]), (
    "graphics reconstruction oracle drift")
assert compute.index("#ifdef PS5_NATIVE\n    // Descriptor synchronization") < compute.index(
    "    struct ComputeSamplerMemo")
assert compute.index("    struct ComputeSamplerMemo") < compute.index(
    "#endif\n    const auto read_handle")

CPP = r"""
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
using u32=std::uint32_t;
namespace VideoCommon {using SamplerId=u32;}
struct FakeTextureCache {
    unsigned calls{};
    bool fail_once{};
    VideoCommon::SamplerId GetSamplerId(u32 handle, bool compute) {
        ++calls;
        if(fail_once) {fail_once=false;throw std::runtime_error("retry");}
        return handle+(compute ? 900000u : 100000u);
    }
};
int main() {
    FakeTextureCache texture_cache;
    {
__MEMO__
        // These 16 distinct handles all collided at handle & 15.
        for (u32 i=0;i<16;++i) {
            const u32 handle=0x100u+(i<<4);
            assert(resolve_sampler(handle)==handle+__BASE__);
        }
        assert(texture_cache.calls==16);
        // Old direct-mapped memo repeatedly looked up all 16 under lock.
        for(unsigned repeat=0;repeat<100;++repeat)
            for(u32 i=0;i<16;++i) {
                const u32 handle=0x100u+(i<<4);
                assert(resolve_sampler(handle)==handle+__BASE__);
            }
        assert(texture_cache.calls==16);
        // Saturation cannot overwrite valid earlier entries or invent IDs.
        assert(resolve_sampler(0x900)==0x900+__BASE__);
        assert(resolve_sampler(0x900)==0x900+__BASE__);
        assert(texture_cache.calls==18);
        for(u32 i=0;i<16;++i)
            assert(resolve_sampler(0x100u+(i<<4))==0x100u+(i<<4)+__BASE__);
        assert(texture_cache.calls==18);
    }
    {
__MEMO__
        // New draw/dispatch -> no cross-call memo: guest descriptor tables
        // may have changed, so even the same handle must be re-resolved.
        assert(resolve_sampler(0x100)==0x100+__BASE__);
        assert(texture_cache.calls==19);
        // Failed lookups must not publish a bogus cached sampler ID.
        texture_cache.fail_once=true;
        bool caught=false;
        try { (void)resolve_sampler(0xabcdefu); }
        catch(const std::runtime_error&) {caught=true;}
        assert(caught);
        assert(resolve_sampler(0xabcdefu)==0xabcdefu+__BASE__);
        const unsigned calls=texture_cache.calls;
        assert(resolve_sampler(0xabcdefu)==0xabcdefu+__BASE__);
        assert(texture_cache.calls==calls);
    }
}
"""
compiler = (shutil.which("clang++-18") or shutil.which("clang++")
            or shutil.which("g++"))
assert compiler, "C++20 compiler unavailable"
with tempfile.TemporaryDirectory(prefix="eden-f07-sampler-memo-") as temp:
    folder = Path(temp)
    for kind, body, base in (
        ("graphics", graphics_memo, "100000u"),
        ("compute", compute_memo, "900000u"),
    ):
        code = CPP.replace("__MEMO__", body).replace("__BASE__", base)
        source = folder / f"{kind}.cpp"
        source.write_text(code)
        for opt in ("-O0", "-O2"):
            exe = folder / f"{kind}{opt}"
            build = subprocess.run(
                [compiler, "-std=c++20", opt, "-Wall", "-Wextra", "-Werror",
                 "-fsanitize=undefined", "-fno-sanitize-recover=all",
                 str(source), "-o", str(exe)],
                capture_output=True, text=True, timeout=90)
            assert build.returncode == 0, (kind, opt, build.stderr[-4000:])
            run = subprocess.run([str(exe)], capture_output=True, text=True,
                                 timeout=35)
            assert run.returncode == 0, (kind, opt, run.stderr[-4000:])
            print(f"F07_SAMPLER_MEMO_PASS kind={kind} opt={opt} "
                  "collision16=1 overflow=1 new_scope=1 exception=1")
print("F07_SAMPLER_MEMO_HOST_PASS graphics=1 compute=1 source_extracted=1")
