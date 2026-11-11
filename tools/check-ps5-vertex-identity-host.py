#!/usr/bin/env python3
"""C++20 sanitizer regression for actual maintained merged vertex identity."""
import ast
from pathlib import Path
import subprocess
import shutil
import tempfile

root = Path(__file__).resolve().parents[1]
active = (root / "vendor/encore-active/eden-vulkan/vk_pipeline_cache_cost.cpp").read_text()
generator = (root / "tools/prepare-vulkan-port.py").read_text()
header = (root / "headless/dev_vulkan.h").read_text()
shader_costs = None
for node in ast.parse(generator).body:
    if isinstance(node, ast.Assign) and any(
        isinstance(name, ast.Name) and name.id == "shader_costs"
        for name in node.targets
    ):
        shader_costs = ast.literal_eval(node.value)
        break
assert shader_costs is not None
fragment = next(new for old, new in shader_costs
                if old == "GraphicsPipeline* PipelineCache::CurrentGraphicsPipeline() {")
start = "std::mutex eden_attributeless_mutex;"
end = "// A static key without attributes"
helper = active.split(start, 1)[1].split(end, 1)[0]
generated_helper = fragment.split(start, 1)[1].split(end, 1)[0]
assert helper == generated_helper
assert "using EdenVertexIdentity = std::array<u64, 2>;" in helper
assert "std::set<EdenVertexIdentity> eden_attributeless_vertex" in helper
assert "EdenResetStaticVertexInputBinding() noexcept" in helper
assert "struct Bucket {" in helper
assert "std::array<Lookup, 4> ways{}" in helper
assert "static thread_local std::array<Bucket, 16> buckets{}" in helper
assert "bucket.next_slot = (bucket.next_slot + 1) & (bucket.ways.size() - 1);" in helper
assert "lookup.hash == vertex_hash &&" in helper
assert "(lookup.result || lookup.generation == generation)" in helper
# Instrument ONLY the shader memo's locked lookup; do not change any production
# branch. In a warmed four-key collision sequence, all subsequent hits must
# take zero set mutexes, not merely return the same values.
slow_anchor = "    const std::scoped_lock lock{eden_attributeless_mutex};\n    const bool result = eden_attributeless_vertex.contains(vertex_hash);"
assert helper.count(slow_anchor) == 1
instrumented_helper = helper.replace(slow_anchor,
    "    slow_lookups.fetch_add(1, std::memory_order_relaxed);\n" + slow_anchor)
for text in (active, generator):
    assert "EdenAttributeless({graphics_key.unique_hashes[0], graphics_key.unique_hashes[1]})" in text
    assert "EdenRecordAttributeless({key.unique_hashes[0], key.unique_hashes[1]}, programs[1].info)" in text
    assert "EdenResetStaticVertexInputBinding();" in text
assert "static_vertex_input = true;" in header.split("inline void ResetForTitle() noexcept {", 1)[1]
compiler = shutil.which("clang++-18") or shutil.which("clang++")
assert compiler, "clang++ required"
cpp = r"""
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <set>
#include "dev_vulkan.h"
using u64=std::uint64_t; using u32=std::uint32_t;
namespace Shader {
namespace IR { constexpr std::size_t NUM_GENERICS=32; }
struct Info {
 struct Loads {
   u32 bits{};
   bool Generic(std::size_t i) const { return bool(bits & (u32{1} << i)); }
 } loads;
};
}
std::atomic<std::uint64_t> slow_lookups{0};
__EXACT_HELPER__
int main() {
 Shader::Info without{}, with{};
 with.loads.bits=1;
 EdenRecordAttributeless({0x10,0x55}, without);
 assert(EdenAttributeless({0x10,0x55}));
 assert(!EdenAttributeless({0x20,0x55}));
 EdenRecordAttributeless({0x20,0x55}, with);
 assert(!EdenAttributeless({0x20,0x55}));
 assert(EdenAttributeless({0x10,0x55}));
 EdenRecordAttributeless({0x30,0x55}, without);
 assert(EdenAttributeless({0x30,0x55}));
 assert(!EdenAttributeless({0x20,0x55}));
 assert(!EdenAttributeless({0x30,0x56}));
 // A memoized negative answer must turn positive after a worker publishes it.
 assert(!EdenAttributeless({0x70,0x88}));
 EdenRecordAttributeless({0x70,0x88}, without);
 assert(EdenAttributeless({0x70,0x88}));
 // Deliberate 64-slot collisions must never reuse a different shader result.
 for(u64 i=0;i<256;++i) {
   const u64 key=0x1000+i*64;
   if((i & 1)==0) EdenRecordAttributeless({key,0x55}, without);
   assert(EdenAttributeless({key,0x55})==((i&1)==0));
 }
 for(u64 i=0;i<256;++i) {
   const u64 key=0x1000+i*64;
   assert(EdenAttributeless({key,0x55})==((i&1)==0));
 }
 // Four colliding shader keys, all built before warmup: no generation
 // change while the hot draw loop alternates among world/HUD variants.
 for(u64 i=0;i<4;++i) EdenRecordAttributeless({0x99000+i*64,0x55}, without);
 const auto before=slow_lookups.load(std::memory_order_relaxed);
 for(u64 i=0;i<4;++i) assert(EdenAttributeless({0x99000+i*64,0x55}));
 const auto warmed=slow_lookups.load(std::memory_order_relaxed);
 assert(warmed-before<=4);
 for(unsigned j=0;j<20000;++j)
   assert(EdenAttributeless({0x99000+u64(j%4)*64,0x55}));
 assert(slow_lookups.load(std::memory_order_relaxed)==warmed);
 // Positive identities are monotonic: publication of an unrelated shader
 // must not force the same four successful shaders back under the mutex.
 EdenRecordAttributeless({0x12345601,0x7a}, without);
 const auto after_publication=slow_lookups.load(std::memory_order_relaxed);
 for(unsigned j=0;j<20000;++j)
   assert(EdenAttributeless({0x99000+u64(j%4)*64,0x55}));
 assert(slow_lookups.load(std::memory_order_relaxed)==after_publication);
 // A negative answer is NOT monotonic; it must be refreshed when a new
 // shader is registered under that exact key after a generation change.
 constexpr EdenVertexIdentity formerly_negative{0x12349876,0x47};
 assert(!EdenAttributeless(formerly_negative));
 const auto neg_warm=slow_lookups.load(std::memory_order_relaxed);
 assert(!EdenAttributeless(formerly_negative));
 assert(slow_lookups.load(std::memory_order_relaxed)==neg_warm);
 EdenRecordAttributeless(formerly_negative, without);
 assert(EdenAttributeless(formerly_negative));
 assert(slow_lookups.load(std::memory_order_relaxed)>neg_warm);
 // The fifth colliding key may evict a memo entry, but must not alias a
 // different shader or suppress generation-based invalidation.
 assert(!EdenAttributeless({0x99000+u64{4}*64,0x55}));
 EdenRecordAttributeless({0x99000+u64{4}*64,0x55}, without);
 assert(EdenAttributeless({0x99000+u64{4}*64,0x55}));
 eden_static_vertex_input_bound=true;
 EdenResetStaticVertexInputBinding();
 assert(!eden_static_vertex_input_bound);
 Eden::DevVulkan::static_vertex_input=false;
 Eden::DevVulkan::ResetForTitle();
 assert(Eden::DevVulkan::static_vertex_input);
}
""".replace("__EXACT_HELPER__", start + instrumented_helper)
with tempfile.TemporaryDirectory(prefix="encore-vertex-identity-") as folder:
    path = Path(folder)
    src = path / "identity.cpp"
    src.write_text(cpp)
    for opt in ("-O0", "-O2"):
        exe = path / ("identity"+opt)
        subprocess.run([compiler,"-std=c++20",opt,"-g","-pthread","-Wall",
                        "-Wextra","-Werror","-fsanitize=address,undefined",
                        "-fno-sanitize-recover=all","-I",str(root/"headless"),
                        str(src),"-o",str(exe)],check=True,timeout=45)
        subprocess.run([str(exe)],check=True,timeout=30)
print("VERTEX_IDENTITY_HOST_PASS pair=1 collision4_hot_nolock=1 overflow=1 generation=1 monotonic_positive=1 negative_revalidated=1 sanitizer_O0_O2=1 generator_parity=1")
