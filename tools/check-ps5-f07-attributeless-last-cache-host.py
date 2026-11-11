#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Source-extracted PS5 vertex-shader positive/negative fastpath regression."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
vendor = (root / "vendor/encore-active/eden-vulkan/vk_pipeline_cache_cost.cpp").read_text()
generator = (root / "tools/prepare-vulkan-port.py").read_text()
start = "bool EdenAttributeless(EdenVertexIdentity vertex_hash) {"
end = "// A static key without attributes"
assert vendor.count(start) == 1 and vendor.count(end) == 1
fragment = vendor[vendor.index(start):vendor.index(end, vendor.index(start))]
assert generator.count(fragment) == 1, "Pinned vendor and generator must agree exactly"
assert "if (last.result && last.hash == vertex_hash) return true;" in fragment
assert "last.generation == generation" in fragment
assert fragment.count("last = lookup;") == 2
assert fragment.count("auto& bucket = buckets[") == 1
# Observe actual bucket reads, without changing the product implementation.
fragment = fragment.replace("    auto& bucket = buckets[",
                            "    ++bucket_reads;\n    auto& bucket = buckets[")
fixture = r"""
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <set>
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using EdenVertexIdentity = std::array<u64, 2>;
std::mutex eden_attributeless_mutex;
struct TrackingSet {
    std::set<EdenVertexIdentity> keys;
    unsigned lookups{};
    bool contains(const EdenVertexIdentity& key) {
        ++lookups;
        return keys.contains(key);
    }
} eden_attributeless_vertex;
std::atomic<u32> eden_attributeless_generation{0};
unsigned bucket_reads{};
__SOURCE__
int main() {
    const EdenVertexIdentity a{1, 0}, b{17, 0}, c{33, 0}, d{49, 0};
    auto publish = [](const EdenVertexIdentity& key) {
        const std::lock_guard lock{eden_attributeless_mutex};
        eden_attributeless_vertex.keys.insert(key);
        eden_attributeless_generation.fetch_add(1, std::memory_order_release);
    };
    assert(!EdenAttributeless(a));
    for (int i = 0; i < 1024; ++i) assert(!EdenAttributeless(a));
    assert(bucket_reads == 1 && eden_attributeless_vertex.lookups == 1);
    publish(a);
    assert(EdenAttributeless(a)); // Stale negative must not survive publication.
    assert(eden_attributeless_vertex.lookups == 2);
    const auto reads_a = bucket_reads;
    for (int i = 0; i < 1024; ++i) assert(EdenAttributeless(a));
    assert(bucket_reads == reads_a); // Positive hot path skips bucket entirely.
    assert(!EdenAttributeless(b)); // Same four-way bucket, different shader.
    const auto miss_b = eden_attributeless_vertex.lookups;
    for (int i = 0; i < 256; ++i) assert(!EdenAttributeless(b));
    assert(eden_attributeless_vertex.lookups == miss_b);
    publish(b);
    assert(EdenAttributeless(b));
    const auto both = eden_attributeless_vertex.lookups;
    for (int i = 0; i < 512; ++i) {
        assert(EdenAttributeless(a));
        assert(EdenAttributeless(b));
    }
    assert(eden_attributeless_vertex.lookups == both); // Bucket keeps collisions.
    assert(!EdenAttributeless(d));
    const auto old_d = eden_attributeless_vertex.lookups;
    publish(c); // Unrelated publication invalidates negative, not positive.
    assert(!EdenAttributeless(d));
    assert(eden_attributeless_vertex.lookups == old_d + 1);
    assert(EdenAttributeless(a));
    const auto positive_reads = bucket_reads;
    for (int i = 0; i < 1024; ++i) assert(EdenAttributeless(a));
    assert(bucket_reads == positive_reads);
}
""".replace("__SOURCE__", fragment)
compiler = shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler, "Host C++20 compiler required"
with tempfile.TemporaryDirectory(prefix="eden-attributeless-") as folder:
    src = Path(folder) / "source.cpp"
    src.write_text(fixture)
    for opt in ("-O0", "-O2"):
        exe = Path(folder) / ("test_" + opt[1:])
        subprocess.run([compiler, "-std=c++20", opt, "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        str(src), "-o", str(exe)], check=True, timeout=60)
        subprocess.run([str(exe)], check=True, timeout=20)
        print("F07_ATTRIBUTLESS_LAST_CACHE_PASS", opt)
print("PS5 frametime not measured")
