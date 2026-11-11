#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""PS5 raw missing-frame path avoids unused tiled-size calculation.

C++ source-extracted raw staging operation exercises zeroing, guest return,
mapped-size arithmetic and mandatory flush on both native and desktop.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
SOURCE=(ROOT/"vendor/encore-active/eden-vulkan/vulkan_present_layer.cpp").read_text()
GEN=(ROOT/"tools/prepare-vulkan-port.py").read_text()
start="#ifdef PS5_NATIVE\n    // An unmapped guest framebuffer has no tiled source to measure."
end="    buffer.Flush();  // Both the upload and missing-memory clear need visibility."
assert SOURCE.count(start)==1 and SOURCE.count(end)==1
segment=SOURCE[SOURCE.index(start):SOURCE.index(end,SOURCE.index(start))+len(end)]
assert segment.count("Tegra::Texture::CalculateSize(")==2
assert segment.count("Tegra::Texture::UnswizzleTexture(")==1
assert segment.count("std::fill(linear_span.begin(), linear_span.end(), u8{0});")==1
assert segment.count("buffer.Flush();")==1
assert "An unmapped guest framebuffer has no tiled source to measure." in GEN
assert segment.index("#ifdef PS5_NATIVE") < segment.index("if (host_ptr) {")
assert segment.index("const u64 tiled_size") > segment.index("if (host_ptr) {")
cpp=r"""
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
using u8=std::uint8_t;
using u32=std::uint32_t;
using u64=std::uint64_t;
struct Framebuffer { u32 stride{4},width{4},height{4}; };
namespace Tegra::Texture {
inline unsigned tiled_size_calls{};
inline unsigned unswizzles{};
u64 CalculateSize(bool, u32,u32,u32,u32,u32,u32) {
    ++tiled_size_calls;return 64;
}
void UnswizzleTexture(std::span<u8> dst,std::span<const u8> src,
                      u32,u32,u32,u32,u32,u32) {
    ++unswizzles;
    assert(dst.size()==src.size());
    std::copy(src.begin(),src.end(),dst.begin());
}
}
struct Buffer { unsigned flushes{};void Flush(){++flushes;} };
void ReplayFrame(std::span<u8> mapped_span,const u8* host_ptr,
                 Framebuffer framebuffer,Buffer& buffer) {
    constexpr u32 block_height_log2=4;
    const u32 bytes_per_pixel=4;
    const u64 linear_size=64;
    const u64 image_offset=8;
__NATIVE_RAW_OPERATION__
}
int main() {
    std::vector<u8> raw(80,0xCC), guest(64,0x7F);
    std::span<u8> mapped_span(raw);
    Framebuffer fb;
    Buffer buffer;
    for (int i=0;i<100;++i) {
        raw.assign(80,0xEE);
        ReplayFrame(mapped_span,nullptr,fb,buffer);
        assert(std::all_of(raw.begin()+8,raw.begin()+72,[](u8 b){return b==0;}));
        assert(raw[7]==0xEE && raw[72]==0xEE);
    }
#ifdef PS5_NATIVE
    assert(Tegra::Texture::tiled_size_calls==0);
#else
    assert(Tegra::Texture::tiled_size_calls==100);
#endif
    ReplayFrame(mapped_span,guest.data(),fb,buffer);
    assert(std::equal(raw.begin()+8,raw.begin()+72,guest.begin()));
    assert(Tegra::Texture::unswizzles==1);
    for (int i=0;i<100;++i) {
        ReplayFrame(mapped_span,nullptr,fb,buffer);
        assert(std::all_of(raw.begin()+8,raw.begin()+72,[](u8 b){return b==0;}));
    }
#ifdef PS5_NATIVE
    assert(Tegra::Texture::tiled_size_calls==1);
#else
    assert(Tegra::Texture::tiled_size_calls==201);
#endif
    assert(buffer.flushes==201);
}
""".replace("__NATIVE_RAW_OPERATION__",segment)
compiler=shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler,"Host C++20 required"
with tempfile.TemporaryDirectory(prefix="eden-missing-raw-") as folder:
    src=Path(folder)/"raw.cpp";src.write_text(cpp)
    for profile,flags in (("PS5",["-DPS5_NATIVE=1"]),("desktop",[])):
        for opt in ("-O0","-O2"):
            output=Path(folder)/(profile+opt[1:])
            subprocess.run([compiler,"-std=c++20",opt,"-Wall","-Wextra","-Werror",
                            "-fsanitize=address,undefined","-fno-sanitize-recover=all",
                            *flags,str(src),"-o",str(output)],check=True,timeout=50)
            subprocess.run([str(output)],check=True,timeout=20)
            print(f"VULKAN_MISSING_RAW_TILED_SIZE_PASS profile={profile} opt={opt}")
print("Host source proof only; FC27/PS5 FPS not measured")
