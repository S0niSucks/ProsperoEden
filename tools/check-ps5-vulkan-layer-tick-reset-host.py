#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Source-extracted post-recreate Vulkan frame slot tick invalidation.

The old GPU timeline ticks were retired by Layer::ReleaseRawImages.
New PS5 images must not inherit those old waits. Desktop remains unchanged.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
native=(root/"vendor/encore-active/eden-vulkan/vulkan_present_layer.cpp").read_text()
generator=(root/"tools/prepare-vulkan-port.py").read_text()
start="    // ReleaseRawImages waited for every old timeline use."
end="    raw_images.resize(image_count);"
assert native.count(start)==1 and native.count(end)==1
snippet=native[native.index(start):native.index(end)+len(end)]
assert snippet.count("resource_ticks.assign(image_count, 0);")==1
assert snippet.count("resource_ticks.resize(image_count);")==1
assert snippet.index("#ifdef PS5_NATIVE") < snippet.index("resource_ticks.assign(image_count, 0);")
assert snippet.index("#else") < snippet.index("resource_ticks.resize(image_count);")
assert generator.count("resource_ticks.assign(image_count, 0);")==1
release=native[native.index("void Layer::ReleaseRawImages() {"):native.index("u64 Layer::CalculateBufferSize(")]
assert "scheduler.Wait(*last_tick);" in release
assert release.index("scheduler.Wait(*last_tick);") < release.index("raw_images.clear();")
cpp=r"""
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>
using u64=std::uint64_t;
class Layer {
public:
    std::size_t image_count{4};
    std::vector<u64> resource_ticks;
    std::vector<int> raw_images;
    void CreateRawImages();
};
void Layer::CreateRawImages() {
__ACTUAL_TICK_ASSIGNMENT__
}
int main() {
    Layer layer;
    layer.CreateRawImages();
    assert(layer.resource_ticks.size()==4);
    for(auto tick:layer.resource_ticks) assert(tick==0);
    layer.resource_ticks={13,4,25,8};
    layer.CreateRawImages();
#ifdef PS5_NATIVE
    for(auto tick:layer.resource_ticks) assert(tick==0);
#else
    assert(layer.resource_ticks==std::vector<u64>({13,4,25,8}));
#endif
    layer.image_count=6;
    layer.resource_ticks[0]=31;
    layer.CreateRawImages();
    assert(layer.resource_ticks.size()==6);
#ifdef PS5_NATIVE
    for(auto tick:layer.resource_ticks) assert(tick==0);
#else
    assert(layer.resource_ticks[0]==31);
#endif
    layer.resource_ticks={0,1,2,3,4,5};
    layer.image_count=2;
    layer.CreateRawImages();
    assert(layer.resource_ticks.size()==2);
#ifdef PS5_NATIVE
    assert(layer.resource_ticks[0]==0 && layer.resource_ticks[1]==0);
#else
    assert(layer.resource_ticks[0]==0 && layer.resource_ticks[1]==1);
#endif
}
""".replace("__ACTUAL_TICK_ASSIGNMENT__",snippet)
compiler=shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler,"C++20 compiler required"
with tempfile.TemporaryDirectory(prefix="eden-layer-tick-reset-") as temp:
    src=Path(temp)/"layer.cpp"
    src.write_text(cpp)
    for profile,flags in (("native",["-DPS5_NATIVE=1"]),("desktop",[])):
        for opt in ("-O0","-O2"):
            exe=Path(temp)/(profile+opt[1:])
            subprocess.run([compiler,"-std=c++20",opt,"-Wall","-Wextra","-Werror",
                            "-fsanitize=address,undefined","-fno-sanitize-recover=all",
                            *flags,str(src),"-o",str(exe)],check=True,timeout=40)
            subprocess.run([str(exe)],check=True,timeout=12)
            print(f"VULKAN_LAYER_RECREATE_TICK_RESET_PASS profile={profile} opt={opt}")
print("Host-only validation; no PS5 FPS claim")
