#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Source-extracted Vulkan layer timeline retirement, PS5 and desktop.

A single max tick on a monotonic GPU semaphore retires all earlier layer
resources. Host sanitizers validate nonmonotonic slot order, empty slots and
pitch-only staging replacement. This is not a PS5 latency measurement.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
native = (root / "vendor/encore-active/eden-vulkan/vulkan_present_layer.cpp").read_text()
generator = (root / "tools/prepare-vulkan-port.py").read_text()
def extract(start, end):
    assert native.count(start) == 1, (start, native.count(start))
    return native[native.index(start):native.index(end, native.index(start))].rstrip()
refresh = extract("void Layer::RefreshResources(", "\nvoid Layer::SetAntiAliasPass(")
release = extract("void Layer::ReleaseRawImages() {", "\nu64 Layer::CalculateBufferSize(")
max_tick = "std::max_element(resource_ticks.begin(), resource_ticks.end())"
assert refresh.count(max_tick) == release.count(max_tick) == 1
assert "scheduler.Wait(*last_tick);" in refresh and "scheduler.Wait(*last_tick);" in release
assert refresh.index("scheduler.Wait(*last_tick);") < refresh.index("CreateStagingBuffer(device, framebuffer);")
assert release.index("scheduler.Wait(*last_tick);") < release.index("raw_image_views.clear();")
assert release.index("raw_image_views.clear();") < release.index("raw_images.clear();")
assert "#ifdef PS5_NATIVE" in release and "#else" in release
assert generator.count(max_tick) >= 2
cpp = r"""
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <variant>
#include <vector>
using u32=std::uint32_t;
using u64=std::uint64_t;
namespace Tegra {
struct FramebufferConfig { u32 width{},height{},stride{}; int pixel_format{}; };
}
struct Device {
    struct Logical { void WaitIdle() const {++wait_idle_calls;} };
    inline static unsigned wait_idle_calls{};
    const Logical& GetLogical() const {static Logical x; return x;}
};
struct Scheduler {
    std::vector<u64> calls;
    u64 completed_tick{};
    unsigned finishes{};
    void Wait(u64 tick) { calls.push_back(tick); completed_tick=std::max(completed_tick,tick); }
    void Finish() { ++finishes; }
};
struct AA {};
struct Buffer {
    std::vector<unsigned char> bytes;
    std::span<unsigned char> Mapped() {return bytes;}
    void reset(){bytes.clear();}
};
class Layer {
public:
    u32 raw_width{},raw_height{};
    int pixel_format{};
    std::vector<int> raw_images,raw_image_views;
    std::vector<u64> resource_ticks;
    std::variant<std::monostate,AA> anti_alias;
    Buffer buffer;
    Scheduler scheduler;
    std::size_t image_count{6};
    unsigned allocations{};
    std::size_t CalculateBufferSize(const Tegra::FramebufferConfig& fb) const {
        return static_cast<std::size_t>(fb.stride)*fb.height*4*image_count;
    }
    void CreateStagingBuffer(const Device&,const Tegra::FramebufferConfig& fb) {
        buffer.bytes.resize(CalculateBufferSize(fb));++allocations;
    }
    void CreateRawImages(const Device&,const Tegra::FramebufferConfig&) {
        raw_images.assign(image_count,1);raw_image_views.assign(image_count,1);
    }
    void ReleaseRawImages();
    void RefreshResources(const Device&,const Tegra::FramebufferConfig&);
};
__RELEASE__
__REFRESH__
int main() {
    Device device;
    Layer layer;
    Tegra::FramebufferConfig fb{640,360,704,1};
    layer.RefreshResources(device,fb);
    assert(layer.allocations==1 && layer.raw_images.size()==6);
    layer.anti_alias.emplace<AA>();
    // Frame-ring slots are not in tick order. Waiting for 8 must retire
    // every resource (3,0,8,2,8,1), even if the last slot is older.
    layer.resource_ticks={3,0,8,2,8,1};
    layer.scheduler.calls.clear();
#ifdef PS5_NATIVE
    fb.stride=768;
    layer.RefreshResources(device,fb);
    assert(layer.allocations==2);
    assert(std::holds_alternative<AA>(layer.anti_alias));
    assert(layer.scheduler.calls.size()==1 && layer.scheduler.calls[0]==8);
    for(const u64 tick:layer.resource_ticks)
        assert(tick<=layer.scheduler.completed_tick);
    assert(layer.scheduler.finishes==0 && Device::wait_idle_calls==0);
    assert(layer.buffer.Mapped().size()==layer.CalculateBufferSize(fb));
    layer.scheduler.calls.clear();
    layer.ReleaseRawImages();
    assert(layer.scheduler.calls.size()==1 && layer.scheduler.calls[0]==8);
#else
    layer.ReleaseRawImages();
    assert(layer.scheduler.calls==layer.resource_ticks);
#endif
    assert(layer.raw_images.empty() && layer.raw_image_views.empty());
    assert(layer.buffer.Mapped().empty());
    layer.resource_ticks.clear();
    layer.scheduler.calls.clear();
    layer.ReleaseRawImages();
    assert(layer.scheduler.calls.empty());
    layer.resource_ticks={0,0,0};
    layer.ReleaseRawImages();
#ifdef PS5_NATIVE
    assert(layer.scheduler.calls.empty());
#else
    assert(layer.scheduler.calls.size()==3);
#endif
}
""".replace("__RELEASE__",release).replace("__REFRESH__",refresh)
compiler=shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler,"Host C++20 compiler required"
with tempfile.TemporaryDirectory(prefix="eden-layer-max-tick-") as td:
    path=Path(td)/"layer.cpp"
    path.write_text(cpp)
    for mode,defines in (("PS5",["-DPS5_NATIVE=1"]),("desktop",[])):
        for opt in ("-O0","-O2"):
            binary=Path(td)/f"{mode}_{opt[1:]}"
            subprocess.run([compiler,"-std=c++20",opt,"-pthread","-Wall","-Wextra",
                            "-Werror","-fsanitize=address,undefined",
                            "-fno-sanitize-recover=all",*defines,str(path),"-o",str(binary)],
                           check=True,timeout=55)
            subprocess.run([str(binary)],check=True,timeout=20)
            print(f"F07_VULKAN_LAYER_MAX_TICK_PASS mode={mode} opt={opt} source=1")
print("HOST-ONLY: PS5 frame times and output not verified")
