#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Source-extracted PS5 raw framebuffer pitch/offset optimization regression.

The native presentation uses a single pixel-format lookup for both the
linear staging length and the per-image offset; desktop retains original calls.
No synchronization, guest-memory visibility or upload behavior is changed.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "vendor/encore-active/eden-vulkan/vulkan_present_layer.cpp").read_text()
generator = (root / "tools/prepare-vulkan-port.py").read_text()
function = source.index("void Layer::UpdateRawImage(")
start = "    const std::span<u8> mapped_span = buffer.Mapped();\n\n#ifndef PS5_NATIVE\n    const u64 image_offset = GetRawImageOffset(framebuffer, image_index);"
end = "#ifdef PS5_NATIVE\n    // An unmapped guest framebuffer has no tiled source to measure."
assert source.count(start) == 1 and source.count(end) == 1
fragment = source[source.index(start, function):source.index(end, function)].rstrip()
assert fragment.count("GetBytesPerPixel(framebuffer)") == 1
assert fragment.count("GetSizeInBytes(framebuffer)") == 1
assert fragment.count("GetRawImageOffset(framebuffer, image_index)") == 1
assert fragment.count("const u64 image_offset = linear_size * image_index;") == 1
assert fragment.count("#ifndef PS5_NATIVE") == 1
assert fragment.count("#ifdef PS5_NATIVE") == 1
assert "Staging slots have the same guest row pitch" in generator
assert "const u64 image_offset = linear_size * image_index;" in generator

cpp = r"""
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <tuple>
#include <utility>
using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using DAddr = u64;
struct Framebuffer {
    u64 address{0x1000}, offset{16};
    u32 stride{}, height{}, bpp{};
};
unsigned bpp_calls{}, size_calls{}, offset_calls{};
u32 GetBytesPerPixel(const Framebuffer& f) {
    ++bpp_calls;
    assert(f.bpp == 2 || f.bpp == 4);
    return f.bpp;
}
std::size_t GetSizeInBytes(const Framebuffer& f) {
    ++size_calls;
    return static_cast<std::size_t>(f.stride) * f.height * GetBytesPerPixel(f);
}
struct Buffer { std::span<u8> Mapped() const { return {}; } };
struct Memory {
    template<class T> const T* GetPointer(DAddr address) const {
        assert(address == 0x1010);
        return nullptr;
    }
};
struct Layer {
    Buffer buffer;
    Memory device_memory;
    u64 GetRawImageOffset(const Framebuffer& f, std::size_t image_index) const {
        ++offset_calls;
        return GetSizeInBytes(f) * image_index;
    }
    std::tuple<u64,u64,u32> Probe(const Framebuffer& framebuffer,
                                  std::size_t image_index) {
__SOURCE__
        (void)mapped_span; (void)host_ptr; (void)block_height_log2;
        return {image_offset, linear_size, bytes_per_pixel};
    }
};
int main() {
    Layer layer;
    for (u32 bpp : {2u,4u}) {
        for (const auto& [stride,height] : {
            std::pair<u32,u32>{0,256}, {1,0}, {16,16},
            {2048,1080}, {65537,4096}, {8192,8192}}) {
            for (std::size_t image_index : {0u,1u,3u,7u}) {
                bpp_calls=size_calls=offset_calls=0;
                const auto [offset,size,seen_bpp] =
                    layer.Probe({0x1000,16,stride,height,bpp},image_index);
                const u64 expected = static_cast<std::size_t>(stride) * height * bpp;
                assert(size==expected && offset==expected*image_index);
                assert(seen_bpp==bpp);
#ifdef PS5_NATIVE
                assert(bpp_calls==1 && size_calls==0 && offset_calls==0);
#else
                assert(bpp_calls==3 && size_calls==2 && offset_calls==1);
#endif
            }
        }
    }
}
""".replace("__SOURCE__", fragment)
compiler = shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler, "Host C++20 compiler required"
with tempfile.TemporaryDirectory(prefix="eden-raw-offset-") as folder:
    src = Path(folder) / "test.cpp"
    src.write_text(cpp)
    for name,flags in (("PS5",["-DPS5_NATIVE=1"]),("desktop",[])):
        for opt in ("-O0","-O2"):
            binary = Path(folder) / (name+opt[1:])
            subprocess.run([compiler,"-std=c++20",opt,"-Wall","-Wextra","-Werror",
                            "-fsanitize=address,undefined","-fno-sanitize-recover=all",
                            *flags,str(src),"-o",str(binary)],check=True,timeout=60)
            subprocess.run([str(binary)],check=True,timeout=20)
            print(f"VULKAN_RAW_OFFSET_REUSE_PASS profile={name} opt={opt}")
print("Native PS5 FPS/frametime not yet measured")
