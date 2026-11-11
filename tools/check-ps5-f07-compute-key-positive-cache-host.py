#!/usr/bin/env python3
"""F07: compile actual GPU-owner compute-key positive lookup and publication.

A same-key healthy shader skips the CPU map hash/probe on PS5. Null entries
keep their retry path; developer tracing remains accurate; no cache survives
a PipelineCache instance. This does not measure game FPS.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
src = (root / "vendor/encore-active/eden-vulkan/vk_pipeline_cache_cost.cpp").read_text()
header = (root / "vendor/encore-active/eden-vulkan/include/video_core/renderer_vulkan/vk_pipeline_cache.h").read_text()
gen = (root / "tools/prepare-vulkan-port.py").read_text()

begin = "#ifdef PS5_NATIVE\n    // Alternating successful compute shaders need no unordered-map hash"
end = "    const auto [pair, is_new]{compute_cache.try_emplace(key)};"
assert src.count(begin) == 1 and src.count(end) == 1
lookup = src[src.index(begin):src.index(end, src.index(begin)) + len(end)]
publish_begin = "#ifdef PS5_NATIVE\n    // Only successful pipelines enter the bounded owner-local memo."
publish_end = "#endif\n    return pipeline.get();"
assert src.count(publish_begin) == 1 and src.count(publish_end) == 1
publish = src[src.index(publish_begin):src.index(publish_end, src.index(publish_begin)) + len(publish_end)]
fields = ("struct ComputePositive {",
          "std::array<ComputePositive, 4> recent_compute_pipelines{};",
          "unsigned next_compute_positive{};",
          "unsigned preferred_compute_positive{};")
for s in fields:
    assert s in header
for marker in ("F07 compute positive owner cache fields",
               "F07 compute same-key skip map probe",
               "F07 compute null-safe positive memo publication"):
    assert marker in gen, marker
assert "::Eden::DevVulkan::trace_pipelines && pipeline" in src
assert "failed_compute_retries.find(key)" in src
assert "if (!recent.pipeline) break;" in lookup
assert "if (recent_compute_pipelines[2].pipeline)" in lookup
assert lookup.index("if (!recent.pipeline) break;") < lookup.index("if (recent.key == key)")
assert "pipeline = CreateComputePipeline(key, shader);" in src

cpp = r"""
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <map>
using u64 = std::uint64_t;
using u32 = std::uint32_t;
namespace Eden::DevVulkan { inline bool trace_pipelines = false; }
struct ComputePipeline { unsigned serial{}; };
struct ComputePipelineCacheKey {
    u64 unique_hash{};
    u32 shared_memory_size{};
    u32 workgroup_size[3]{};
    static inline unsigned equality_checks = 0;
    bool operator==(const ComputePipelineCacheKey& b) const noexcept {
        ++equality_checks;
        return unique_hash == b.unique_hash &&
               shared_memory_size == b.shared_memory_size &&
               workgroup_size[0] == b.workgroup_size[0] &&
               workgroup_size[1] == b.workgroup_size[1] &&
               workgroup_size[2] == b.workgroup_size[2];
    }
    bool operator<(const ComputePipelineCacheKey& b) const noexcept {
        if (unique_hash != b.unique_hash) return unique_hash < b.unique_hash;
        if (shared_memory_size != b.shared_memory_size) return shared_memory_size < b.shared_memory_size;
        for (int i=0;i<3;++i)
            if (workgroup_size[i] != b.workgroup_size[i]) return workgroup_size[i] < b.workgroup_size[i];
        return false;
    }
};
struct Map {
    std::map<ComputePipelineCacheKey, std::unique_ptr<ComputePipeline>> slots;
    unsigned probes{};
    auto try_emplace(ComputePipelineCacheKey key) {
        ++probes;
        return slots.try_emplace(key);
    }
};
struct Cache {
    Map compute_cache;
#ifdef PS5_NATIVE
    struct ComputePositive {
        ComputePipelineCacheKey key{};
        ComputePipeline* pipeline{};
    };
    std::array<ComputePositive, 4> recent_compute_pipelines{};
    unsigned next_compute_positive{};
    unsigned preferred_compute_positive{};
#endif
    ComputePipeline* Get(ComputePipelineCacheKey key) {
__LOOKUP__
        auto& pipeline{pair->second};
        if (is_new && key.unique_hash != 0xbad) {
            pipeline = std::make_unique<ComputePipeline>();
            pipeline->serial = static_cast<unsigned>(key.unique_hash);
        }
        // The real failure path is bounded/retryable; this miniature tests
        // only that an unsuccessful entry must NEVER be returned from memo.
__PUBLISH__
    }
};
int main() {
    Cache cache;
    ComputePipelineCacheKey a{17,32,{8,2,1}};
    ComputePipelineCacheKey b{18,32,{8,2,1}};
    ComputePipelineCacheKey same_shader_other_group{17,32,{16,1,1}};
    ComputePipelineCacheKey failed{0xbad,32,{8,2,1}};
    auto* first = cache.Get(a);
    assert(first && first->serial == 17 && cache.compute_cache.probes == 1);
    for (unsigned i=0;i<5000;++i) assert(cache.Get(a)==first);
#ifdef PS5_NATIVE
    assert(cache.compute_cache.probes == 1);
#else
    assert(cache.compute_cache.probes == 5001);
#endif
    const auto before = cache.compute_cache.probes;
    auto* second = cache.Get(b);
    assert(second && second != first && cache.compute_cache.probes == before+1);
    assert(cache.Get(same_shader_other_group) != first);
    assert(cache.Get(a)==first);
    // Two alternating keys never need to mutate the cache's preferred slot;
    // after the second insertion one remains the one-probe preferred key
    // and the other is only one additional read-only comparison away.
    Cache two_way;
    auto* one = two_way.Get(a);
    auto* other = two_way.Get(b);
    assert(one && other && one != other);
    for (unsigned i = 0; i < 20000; ++i)
        assert(two_way.Get((i & 1) ? b : a) == ((i & 1) ? other : one));
#ifdef PS5_NATIVE
    assert(two_way.preferred_compute_positive == 1);
    assert(two_way.compute_cache.probes == 2);
#else
    assert(two_way.compute_cache.probes == 20002);
#endif
    // Four alternating live compute keys (including dispatch geometry)
    // must hit without repeated unordered-map probes once warmed.
    Cache alternating;
    ComputePipelineCacheKey keys[4]{
        {700,32,{8,2,1}}, {701,32,{8,2,1}},
        {700,32,{16,1,1}}, {702,64,{8,2,1}},
    };
    ComputePipeline* outputs[4]{};
    for (unsigned i=0; i<4; ++i) outputs[i]=alternating.Get(keys[i]);
    assert(alternating.compute_cache.probes == 4);
    for (unsigned i=0; i<20000; ++i)
        assert(alternating.Get(keys[i%4]) == outputs[i%4]);
#ifdef PS5_NATIVE
    assert(alternating.compute_cache.probes == 4);
    // Once the active shader is preferred, even after four keys were
    // admitted, a repeated dispatch must compare exactly one key.
    const auto comparisons = ComputePipelineCacheKey::equality_checks;
    for (unsigned i=0; i<10000; ++i)
        assert(alternating.Get(keys[3]) == outputs[3]);
    assert(ComputePipelineCacheKey::equality_checks == comparisons + 10000);
    assert(alternating.compute_cache.probes == 4);
#else
    assert(alternating.compute_cache.probes == 20004);
#endif
    // A fifth key may evict a positive hint, but must never return the wrong
    // shader. Keeping failed entries OUT of the memo protects healthy ones.
    ComputePipelineCacheKey fifth{703,32,{8,2,1}};
    auto* new_shader=alternating.Get(fifth);
    assert(new_shader && new_shader!=outputs[0]);
    const unsigned old_probes=alternating.compute_cache.probes;
    assert(alternating.Get(keys[0]) == outputs[0]);
#ifdef PS5_NATIVE
    assert(alternating.compute_cache.probes == old_probes+1);
#else
    assert(alternating.compute_cache.probes == old_probes+1);
#endif
    // A new PipelineCache must never inherit a raw pointer from the old one.
    Cache separate;
    assert(separate.Get(a) != first);
    // No memoization of failed shader entries: they still reach the map.
    const auto missed = cache.compute_cache.probes;
    assert(cache.Get(failed)==nullptr);
    assert(cache.Get(failed)==nullptr);
    assert(cache.compute_cache.probes == missed+2);
    // DEV pipeline first-use trace must keep the ordinary map path.
    Eden::DevVulkan::trace_pipelines = true;
    const auto traced = cache.compute_cache.probes;
    assert(cache.Get(a)==first);
    assert(cache.Get(a)==first);
    assert(cache.compute_cache.probes==traced+2);
}
""".replace("__LOOKUP__",lookup).replace("__PUBLISH__",publish)
compiler = shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler, "C++20 compiler required"
with tempfile.TemporaryDirectory(prefix="f07-compute-hot-key-") as td:
    source = Path(td)/"test.cpp"
    source.write_text(cpp)
    for native in (False, True):
        for opt in ("-O0","-O2"):
            exe = Path(td)/(f"compute-{int(native)}-{opt[1:]}")
            cmd = [compiler,"-std=c++20",opt,"-Wall","-Wextra","-Werror",
                   "-fsanitize=undefined","-fno-sanitize-recover=all",
                   *(["-DPS5_NATIVE=1"] if native else []),
                   str(source),"-o",str(exe)]
            built = subprocess.run(cmd,capture_output=True,text=True,timeout=50)
            assert built.returncode==0,(native,opt,built.stderr[-4000:])
            run = subprocess.run([str(exe)],capture_output=True,text=True,timeout=25)
            assert run.returncode==0,(native,opt,run.stderr[-4000:])
            print(f"F07_COMPUTE_KEY_POSITIVE_CACHE_PASS native={int(native)} opt={opt} "
                  "same_key=1 alternating_2_readonly=1 sparse_miss=1 alternating_4=1 preferred_hot=1 overflow_5=1 failure=1 key_change=1 dev_trace=1 owner_scope=1")
