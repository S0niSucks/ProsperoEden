#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""PS5 Vulkan disabled-AA steady state: exact Layer::SetAntiAliasPass source.

Native should skip disabled-AA matrix extent calculation on repeat frames;
resizing and AA mode changes must still allocate and synchronize. Unmodified
desktop should retain its original behavior. Host-only O0/O2 ASan/UBSan.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/"vendor/encore-active/eden-vulkan/vulkan_present_layer.cpp").read_text()
generator=(root/"tools/prepare-vulkan-port.py").read_text()
a="void Layer::SetAntiAliasPass(const Device& device) {"
b="\nvoid Layer::ReleaseRawImages() {"
assert source.count(a)==1 and source.count(b)==1
method=source[source.index(a):source.index(b,source.index(a))].rstrip()
assert "requested_aa != Settings::AntiAliasing::Fxaa" in method
assert "requested_aa != Settings::AntiAliasing::Smaa" in method
assert "#ifdef PS5_NATIVE" in method and "#else" in method
assert "The old guard skipped" in generator
assert method.index("return;") < method.index("scheduler.Finish();")
cpp=r"""
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <variant>
struct VkExtent2D { unsigned width{},height{}; };
namespace Settings {
enum class AntiAliasing { None,Fxaa,Smaa };
struct ResolutionInfo {
    inline static unsigned scales{};
    unsigned ScaleUp(unsigned n) const { ++scales; return n; }
};
struct Values { ResolutionInfo resolution_info; };
inline Values values{};
}
struct Device {
    struct Logical { inline static unsigned idles{}; void WaitIdle() const {++idles;} };
    const Logical& GetLogical() const {static Logical x;return x;}
};
struct Scheduler { unsigned finishes{}; void Finish(){++finishes;} };
struct MemoryAllocator {};
struct Filters {
    Settings::AntiAliasing aa{Settings::AntiAliasing::None};
    Settings::AntiAliasing get_anti_aliasing() const {return aa;}
};
struct FXAA {
    FXAA(const Device&,MemoryAllocator&,std::size_t,VkExtent2D){}
};
struct SMAA {
    SMAA(const Device&,MemoryAllocator&,std::size_t,VkExtent2D){}
};
class Layer {
public:
    Scheduler scheduler;
    MemoryAllocator memory_allocator;
    Filters filters;
    const std::size_t image_count{3};
    unsigned raw_width{1280},raw_height{720};
    Settings::AntiAliasing anti_alias_setting{Settings::AntiAliasing::None};
    std::variant<std::monostate,FXAA,SMAA> anti_alias{};
    void SetAntiAliasPass(const Device&);
};
__ACTUAL_AA_METHOD__
int main() {
    Device d; Layer l;
    for(int i=0;i<256;++i) l.SetAntiAliasPass(d);
#ifdef PS5_NATIVE
    assert(Settings::ResolutionInfo::scales==0);
#else
    assert(Settings::ResolutionInfo::scales==512);
#endif
    assert(l.scheduler.finishes==0 && Device::Logical::idles==0);
    l.filters.aa=Settings::AntiAliasing::Fxaa;
    l.SetAntiAliasPass(d);
    assert(std::holds_alternative<FXAA>(l.anti_alias));
    assert(Settings::ResolutionInfo::scales>=2);
    const auto first_aa_scales=Settings::ResolutionInfo::scales;
    for(int i=0;i<256;++i) l.SetAntiAliasPass(d);
    assert(Settings::ResolutionInfo::scales==first_aa_scales);
    // An actual resize invalidates the pass but not the requested filter.
    l.anti_alias.emplace<std::monostate>();
    l.raw_width=1920;
    l.SetAntiAliasPass(d);
    assert(std::holds_alternative<FXAA>(l.anti_alias));
    assert(Settings::ResolutionInfo::scales==first_aa_scales+2);
    l.filters.aa=Settings::AntiAliasing::Smaa;
    l.SetAntiAliasPass(d);
    assert(std::holds_alternative<SMAA>(l.anti_alias));
#ifdef PS5_NATIVE
    assert(l.scheduler.finishes==1 && Device::Logical::idles==1);
#else
    assert(l.scheduler.finishes==0 && Device::Logical::idles==0);
#endif
    l.filters.aa=Settings::AntiAliasing::None;
    l.SetAntiAliasPass(d);
    assert(std::holds_alternative<std::monostate>(l.anti_alias));
#ifdef PS5_NATIVE
    assert(l.scheduler.finishes==2 && Device::Logical::idles==2);
#else
    assert(l.scheduler.finishes==0 && Device::Logical::idles==0);
#endif
    const auto none_scales=Settings::ResolutionInfo::scales;
    for(int i=0;i<256;++i) l.SetAntiAliasPass(d);
#ifdef PS5_NATIVE
    assert(Settings::ResolutionInfo::scales==none_scales);
#else
    assert(Settings::ResolutionInfo::scales==none_scales+512);
#endif
    l.filters.aa=Settings::AntiAliasing::Smaa;
    l.SetAntiAliasPass(d);
    assert(std::holds_alternative<SMAA>(l.anti_alias));
#ifdef PS5_NATIVE
    assert(l.scheduler.finishes==2 && Device::Logical::idles==2);
#else
    assert(l.scheduler.finishes==0 && Device::Logical::idles==0);
#endif
}
""".replace("__ACTUAL_AA_METHOD__",method)
compiler=shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler,"C++20 compiler required"
with tempfile.TemporaryDirectory(prefix="eden-aa-fastpath-") as tmp:
    src=Path(tmp)/"aa.cpp"
    src.write_text(cpp)
    for profile,flags in (("PS5",["-DPS5_NATIVE=1"]),("desktop",[])):
        for opt in ("-O0","-O2"):
            exe=Path(tmp)/(profile+opt[1:])
            subprocess.run([compiler,"-std=c++20",opt,"-pthread","-Wall","-Wextra",
                            "-Werror","-fsanitize=address,undefined",
                            "-fno-sanitize-recover=all",*flags,str(src),"-o",str(exe)],
                           check=True,timeout=40)
            subprocess.run([str(exe)],check=True,timeout=20)
            print(f"VULKAN_AA_DISABLED_FASTPATH_PASS profile={profile} opt={opt}")
print("Host-only branch and resize proof; PS5 GPU latency unmeasured")
