#!/usr/bin/env python3
"""Compile the actual sparse-table lookup and publication bound with sanitizers.

Host-only lifecycle oracle: no PS5 mapping or throughput conclusion.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/memory_pages.cpp").read_text()
header = "SparseRange* SparseRangeOf(std::uintptr_t address) {"
assert source.count(header) == 1
start = source.index(header)
stop = source.index("\n}\n", start) + len("\n}")
lookup = source[start:stop]
assert "i < sparse_used_prefix" in lookup
assert "range.begin.load(std::memory_order_acquire)" in lookup
assert "address < range.end" in lookup
assert "static thread_local std::array<SparseRange*, 2> last_ranges{};" in lookup
assert "static thread_local unsigned next_slot = 0;" in lookup
assert "static thread_local unsigned preferred_slot = 0;" in lookup
assert "if (probe != 0) preferred_slot = slot;" in lookup
assert "last_ranges = {};" not in lookup
assert "retired or reused slot cannot return a stale range." in lookup
assert "next_slot ^= 1u;" in lookup
assert "if (&range == last_ranges[0] || &range == last_ranges[1]) continue;" in lookup
assert "if (sparse_used_prefix != 0)" in lookup
assert "std::size_t sparse_used_prefix = 0;" in source
assert "std::uint64_t sparse_range_generation = 1;" in source
assert source.count("++sparse_range_generation;") == 2
assert "static thread_local std::array<std::uintptr_t, 4> last_negative_addresses{};" in lookup
assert "last_negative_generation == sparse_range_generation" in lookup
assert "last_negative_addresses[i] == address" in lookup
assert "negative_count = 0;" in lookup and "next_negative_slot = 0;" in lookup
assert "next_negative_slot = (next_negative_slot + 1u) & 3u;" in lookup
assert lookup.index("last_negative_addresses[i] == address") < lookup.index("i < sparse_used_prefix")
assert lookup.index("last_negative_addresses[next_negative_slot] = address;") > lookup.index("i < sparse_used_prefix")
assert source.count("sparse_used_prefix = (std::max)(") == 1
pub_start = source.index("    sparse_used_prefix = (std::max)(")
pub_end = source.index(";", pub_start) + 1
publish = source[pub_start:pub_end]
assert publish.endswith("free_range - sparse_ranges.data()) + 1);")
assert source.index("free_range->begin.store(") < pub_start
assert source.index("free_range->end =") < pub_start
allocate = source.split("void* AllocateSparsePages(std::size_t size) noexcept {", 1)[1]
choose_start = allocate.index("    SparseRange* free_range = nullptr;")
choose_end = allocate.index("    const std::size_t slots =", choose_start)
selection = allocate[choose_start:choose_end]
assert "i < sparse_used_prefix" in selection
assert "sparse_used_prefix < sparse_ranges.size()" in selection
assert "free_range = &sparse_ranges[sparse_used_prefix];" in selection
release = source.split("void ReleaseSparse(SparseRange& range) {", 1)[1].split("\n}", 1)[0]
shrink = """    while (sparse_used_prefix > 0 &&
           sparse_ranges[sparse_used_prefix - 1].begin.load(std::memory_order_relaxed) == 0)
        --sparse_used_prefix;"""
assert release.count(shrink) == 1
assert release.index("range.begin.store(0, std::memory_order_release);") < release.index("++sparse_range_generation;") < release.index(shrink)
assert source.index("sparse_used_prefix = (std::max)(") < source.index("++sparse_range_generation; // publish exact")
for function in ("void FreeSparsePages(", "void CommitSparsePage(", "void DecommitSparsePage("):
    body = source[source.index(function):]
    assert body.index("const std::lock_guard lock{sparse_mutex};") < body.index(
        "SparseRangeOf("), function
compiler = shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler, "C++20 host compiler required"
fixture = r"""
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

inline std::atomic<unsigned> scans{0};
struct CountAtomic {
    std::atomic<std::uintptr_t> value{0};
    std::uintptr_t load(std::memory_order order) const noexcept {
        scans.fetch_add(1, std::memory_order_relaxed);
        return value.load(order);
    }
    void store(std::uintptr_t v, std::memory_order order) noexcept {
        value.store(v, order);
    }
};
struct SparseRange {
    CountAtomic begin;
    std::uintptr_t end{};
};
std::array<SparseRange, 64> sparse_ranges{};
std::mutex sparse_mutex;
std::size_t sparse_used_prefix = 0;
std::uint64_t sparse_range_generation = 1;
__LOOKUP__
SparseRange* ChooseFreeRange() {
    std::lock_guard lock{sparse_mutex};
__SELECT_FREE__
    return free_range;
}
void Register(unsigned slot, std::uintptr_t begin) {
    std::lock_guard lock{sparse_mutex};
    assert(slot < sparse_ranges.size() && begin != 0);
    auto* free_range = &sparse_ranges[slot];
    assert(free_range->begin.load(std::memory_order_relaxed) == 0);
    free_range->end = begin + 4096;
    free_range->begin.store(begin, std::memory_order_release);
__PUBLISH__
    ++sparse_range_generation;
}
void Release(unsigned slot) {
    std::lock_guard lock{sparse_mutex};
    sparse_ranges[slot].begin.store(0, std::memory_order_release);
__SHRINK__
    ++sparse_range_generation;
}
SparseRange* Lookup(std::uintptr_t address) {
    std::lock_guard lock{sparse_mutex};
    return SparseRangeOf(address);
}
int main() {
    scans = 0;
    assert(!Lookup(0x1000) && scans.load() == 0);
    assert(ChooseFreeRange() == &sparse_ranges[0] && scans.load() == 0);
    Register(0, 0x1000);
    scans = 0;
    assert(ChooseFreeRange() == &sparse_ranges[1] && scans.load() == 1);
    scans = 0;
    assert(Lookup(0x1000) == &sparse_ranges[0]);
    assert(scans.load() == 1 && sparse_used_prefix == 1);
    scans = 0;
    assert(!Lookup(0x2000));
    assert(scans.load() == 1);
    Register(1, 0x2000);
    // Same exact address was negatively cached before registration.
    assert(Lookup(0x2000) == &sparse_ranges[1]);
    assert(Lookup(0x2001) == &sparse_ranges[1]);
    assert(sparse_used_prefix == 2);
    scans = 0;
    for (int i = 0; i < 10000; ++i)
        assert(Lookup(0x2000 + (i % 64)) == &sparse_ranges[1]);
    assert(scans.load() == 10000); // one load per adjacent in-range page
    Release(0);
    scans = 0;
    assert(ChooseFreeRange() == &sparse_ranges[0] && scans.load() == 1);
    assert(!Lookup(0x1001));
    assert(Lookup(0x2001) == &sparse_ranges[1]);
    assert(sparse_used_prefix == 2); // holes may not end the scan
    Register(0, 0x3000);
    assert(Lookup(0x3001) == &sparse_ranges[0]);
    assert(Lookup(0x2001) == &sparse_ranges[1]);
    Register(63, 0x5000);
    scans = 0;
    assert(ChooseFreeRange() == &sparse_ranges[2] && scans.load() == 3);
    assert(sparse_used_prefix == 64);
    assert(Lookup(0x5001) == &sparse_ranges[63]);
    scans = 0;
    for (int i = 0; i < 10000; ++i)
        assert(Lookup(0x5000 + (i % 64)) == &sparse_ranges[63]);
    assert(scans.load() == 10000); // previously 64 probes per high-slot hit
    // A worker alternating low/high page tables cannot afford a 64-slot
    // scan on every second access. Two hints stabilize after warmup without
    // mutating TLS on hits; both pointers stay range-locked and revalidated.
    scans = 0;
    for (int i = 0; i < 10000; ++i) {
        assert(Lookup(0x3001) == &sparse_ranges[0]);
        assert(Lookup(0x5001) == &sparse_ranges[63]);
    }
    assert(scans.load() <= 40004); // <= 4 probes per alternating pair
    // Full-prefix misses cost up to 64 atomics; repeat the exact same
    // negative lookup without scanning the full table again.
    scans = 0;
    assert(!Lookup(0xdead0000));
    assert(scans.load() >= 3);
    scans = 0;
    for (int i = 0; i < 10000; ++i) assert(!Lookup(0xdead0000));
    assert(scans.load() <= 20000); // only two positive hints per miss
    // Four alternating exact dense addresses must remain negative-cache
    // hits, without four repeated 64-entry scans each cycle.
    const std::uintptr_t dense_addresses[4]{
        0xcafe1000, 0xcafe2000, 0xcafe3000, 0xcafe4000
    };
    for (const auto address : dense_addresses) assert(!Lookup(address));
    scans = 0;
    for (int i = 0; i < 10000; ++i)
        assert(!Lookup(dense_addresses[i & 3]));
    assert(scans.load() <= 20000); // only the two retained positive hints
    // A fifth distinct miss can evict the oldest negative hint safely.
    assert(!Lookup(0xcafe5000));
    assert(!Lookup(dense_addresses[0]));
    // A dense-region miss must not erase valid sparse-table hints. The
    // subsequent high-slot lookup needs at most two revalidated probes, not
    // a 64-slot scan. Stale aliases are still checked after release/reuse.
    assert(!Lookup(0xdead0000));
    scans = 0;
    assert(Lookup(0x5001) == &sparse_ranges[63]);
    assert(scans.load() <= 2);
    Release(63);
    assert(!Lookup(0x5001));
    assert(sparse_used_prefix == 2); // freed high tail is no longer scanned
    scans = 0;
    // A negative lookup now rechecks two retained hints (one retired)
    // plus the one live unhinted table. Preserve the hints for later hits.
    assert(!Lookup(0x5001) && scans.load() <= 2); // negative memo, hints only
    for (int j = 0; j < 2000; ++j) {
        Register(63, 0x5000);
        assert(Lookup(0x5001) == &sparse_ranges[63]);
        Release(63);
        assert(sparse_used_prefix == 2);
    }
    // The owner mutex serializes lookup with slot reuse and teardown.
    std::vector<std::thread> readers;
    for (int worker = 0; worker < 4; ++worker)
        readers.emplace_back([] {
            for (int j = 0; j < 10000; ++j) {
                const auto* p = Lookup(0x2001);
                assert(p == &sparse_ranges[1]);
            }
        });
    for (auto& worker : readers) worker.join();
    assert(Lookup(0x2001) == &sparse_ranges[1]);
    Release(1);
    assert(sparse_used_prefix == 1); // keep slot zero
    Release(0);
    assert(sparse_used_prefix == 0); // empty process, zero probe cost
    scans = 0;
    assert(!Lookup(0x2001) && scans.load() == 0);
    Register(63, 0x8000); // a later new high allocation re-expands safely
    assert(sparse_used_prefix == 64);
    assert(Lookup(0x8000) == &sparse_ranges[63]);
    assert(!Lookup(0x5001)); // former mapping must not match reused slot
    scans = 0;
    assert(Lookup(0x8001) == &sparse_ranges[63] && scans.load() == 1);
    Release(63);
    assert(sparse_used_prefix == 0);
    // Exercise every slot: first-free allocation, a hole in a full prefix,
    // saturation, and complete tail release without any kernel mappings.
    for (unsigned i = 0; i < 64; ++i) {
        assert(ChooseFreeRange() == &sparse_ranges[i]);
        Register(i, 0x100000 + i * 0x2000);
    }
    scans = 0;
    assert(ChooseFreeRange() == nullptr && scans.load() == 64);
    Release(40);
    scans = 0;
    assert(ChooseFreeRange() == &sparse_ranges[40] && scans.load() == 41);
    Register(40, 0xdead0000);
    assert(Lookup(0xdead0000) == &sparse_ranges[40]); // epoch invalidated miss
    for (int i = 63; i >= 0; --i) Release(i);
    scans = 0;
    assert(sparse_used_prefix == 0 && ChooseFreeRange() == &sparse_ranges[0] && scans.load() == 0);
}
""".replace("__LOOKUP__", lookup).replace("__SELECT_FREE__", selection).replace("__PUBLISH__", publish).replace("__SHRINK__", shrink)
with tempfile.TemporaryDirectory(prefix="eden-sparse-prefix-") as tmp:
    src = Path(tmp) / "sparse.cpp"
    src.write_text(fixture)
    for opt in ("0", "2", "3"):
        binary = Path(tmp) / ("sparse-O" + opt)
        command = [
            compiler, "-std=c++20", "-O" + opt, "-Wall", "-Wextra",
            "-Werror", "-pthread", "-fsanitize=address,undefined",
            "-fno-sanitize-recover=all", str(src), "-o", str(binary)
        ]
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, (opt, result.stderr[-4000:])
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (opt, result.stderr[-4000:])
print("SPARSE_RANGE_PREFIX_HOST_PASS source_extracted=1 empty=1 holes=1 reuse=1 full64=1 tail_reclaim=1 republish=1 tls_dual_positive=1 preferred_hot=1 allocation_prefix=1 saturation=1 miss_preserves_hints=1 exact_negative_ring4=1 publish_release_epoch=1 recycled_slot_safe=1 alternating_10000=1 threads=4 O0_O2_O3=1")
