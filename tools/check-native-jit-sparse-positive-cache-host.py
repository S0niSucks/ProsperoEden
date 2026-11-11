#!/usr/bin/env python3
"""Source-linked host regression for the PS5 sparse JIT positive commit memo.

The production function remains compiled end-to-end by check-jit-sparse-host.py.
This witness compiles the exact cache header and checks interleaved arenas,
growth, MRU eviction, and generation invalidation; no console FPS claim.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/memory_pages.cpp").read_text()
assert '#include "jit-sparse-commit.h"' in source
assert "static thread_local ::Eden::Jit::SparseCommitPositiveCache local;" in source
assert "local.Covers(executable, required, generation)" in source
assert "local.Remember(executable, region.committed," in source
# A newly reserved region cannot invalidate hints for other live arenas.
# Only trimming committed pages or retiring an RX mapping can make an
# existing positive hint unsafe. Check the *real* owner method boundaries.
reserve_body = source.split("void* ReserveSparseJitCode(std::size_t size, void** writable_out) noexcept {", 1)[1].split(
    "bool IsSparseJitCode(", 1)[0]
trim_body = source.split("bool TrimSparseJitCode(void* executable, std::size_t live_code_end) noexcept {", 1)[1].split(
    "void ReleaseSparseJitCode(void* executable) noexcept {", 1)[0]
release_body = source.split("void ReleaseSparseJitCode(void* executable) noexcept {", 1)[1].split(
    "void FreeMemoryPages(void* pointer, std::size_t size) noexcept {", 1)[0]
epoch_bump = "sparse_jit_generation.fetch_add(1, std::memory_order_acq_rel);"
assert epoch_bump not in reserve_body
assert trim_body.count(epoch_bump) == 1
assert release_body.count(epoch_bump) == 1
assert source.count(epoch_bump) == 2

cache_header = (ROOT / "headless/jit-sparse-commit.h").read_text()
assert "if (!entries[i].executable) return false;" in cache_header
assert "if (entries[0].executable == executable) {" in cache_header
assert "return required <= entries[0].committed;" in cache_header
assert "if (required > entries[i].committed) return false;" in cache_header
assert "if (i == 1 && !entries[2].executable) return true;" in cache_header
compiler = shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler, "C++20 compiler required"
FIXTURE = r"""
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <sys/mman.h>
#include <unistd.h>
#include "jit-sparse-commit.h"
int main() {
    Eden::Jit::SparseCommitPositiveCache cache;
    void* arenas[5] = {
        reinterpret_cast<void*>(0x10000000ULL),
        reinterpret_cast<void*>(0x14000000ULL),
        reinterpret_cast<void*>(0x18000000ULL),
        reinterpret_cast<void*>(0x1c000000ULL),
        reinterpret_cast<void*>(0x20000000ULL)};
    constexpr std::size_t committed = 4 * 1024 * 1024;
    for (auto* arena : arenas) assert(!cache.Covers(arena, committed, 7));
    for (unsigned i = 0; i < 4; ++i) cache.Remember(arenas[i], committed, 7);
    std::size_t misses = 0;
    for (unsigned i = 0; i < 100000; ++i)
        if (!cache.Covers(arenas[i % 4], committed, 7)) ++misses;
    assert(misses == 0);
    // End of alternating probes leaves arena 3 hottest; arena 2 is cached
    // but requires another physical page. The failed probe must not promote.
    assert(!cache.Covers(arenas[2], committed + 1, 7));
    assert(cache.Covers(arenas[3], committed, 7));
    // The most frequent single-core hit is strictly read-only. Catch a
    // redundant self-promotion store even at -O0 by protecting the page.
    const long page_size = sysconf(_SC_PAGESIZE);
    assert(page_size > 0 && static_cast<std::size_t>(page_size) >= sizeof(cache));
    void* page = mmap(nullptr, static_cast<std::size_t>(page_size),
                      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(page != MAP_FAILED);
    auto* readonly = new (page) Eden::Jit::SparseCommitPositiveCache{};
    readonly->Remember(arenas[0], committed, 7);
    readonly->Remember(arenas[1], committed, 7);
    assert(mprotect(page, static_cast<std::size_t>(page_size), PROT_READ) == 0);
    for (unsigned i = 0; i < 1000; ++i) {
        assert(readonly->Covers(arenas[1], committed, 7));
        // With exactly two active arenas, secondary hits must also be
        // read-only, even when alternated after a primary-core hit.
        assert(readonly->Covers(arenas[0], committed, 7));
        // Missing arena, hot-arena growth, and non-head arena growth are
        // read-only misses and must not mutate MRU while finding a false hit.
        assert(!readonly->Covers(arenas[2], committed, 7));
        assert(!readonly->Covers(arenas[1], committed + 1, 7));
        assert(!readonly->Covers(arenas[0], committed + 1, 7));
    }
    assert(mprotect(page, static_cast<std::size_t>(page_size), PROT_READ | PROT_WRITE) == 0);
    readonly->~SparseCommitPositiveCache();
    assert(munmap(page, static_cast<std::size_t>(page_size)) == 0);
    assert(!cache.Covers(arenas[2], committed + 1, 7));
    cache.Remember(arenas[2], committed * 2, 7);
    assert(cache.Covers(arenas[2], committed * 2, 7));
    assert(!cache.Covers(arenas[2], committed * 2 + 1, 7));
    // Reserving a third independent RX/RW arena preserves already live
    // positive entries at the unchanged reservation epoch. Physical trim or
    // release, unlike insertion, must invalidate those entries.
    Eden::Jit::SparseCommitPositiveCache preserved;
    preserved.Remember(arenas[0], committed, 70);
    preserved.Remember(arenas[1], committed, 70);
    assert(preserved.Covers(arenas[0], committed, 70));
    assert(preserved.Covers(arenas[1], committed, 70));
    assert(!preserved.Covers(arenas[0], committed, 71));
    assert(!preserved.Covers(arenas[1], committed, 71));
    // A later physical commit refreshes the same arena in-place.
    cache.Remember(arenas[2], committed * 4, 7);
    assert(cache.Covers(arenas[2], committed * 4, 7));
    assert(cache.Covers(arenas[0], committed, 7));
    cache.Remember(arenas[4], committed, 7);
    assert(!cache.Covers(arenas[1], committed, 7)); // LRU evicted
    assert(!cache.Covers(arenas[4], committed, 8)); // trim/release epoch
    assert(!cache.Covers(arenas[2], committed, 8));
    cache.Remember(arenas[4], committed, 8);
    assert(cache.Covers(arenas[4], committed, 8));
    assert(!cache.Covers(arenas[4], committed, 9)); // recycled VA
    // Two active slots after an epoch reset retain MRU order while all
    // negative lookups exit on the first never-populated entry.
    Eden::Jit::SparseCommitPositiveCache partial;
    partial.Remember(arenas[0], committed, 20);
    partial.Remember(arenas[1], committed, 20);
    assert(!partial.Covers(arenas[4], committed, 20));
    assert(partial.Covers(arenas[0], committed, 20));
    assert(partial.Covers(arenas[1], committed, 20));
    assert(!partial.Covers(arenas[2], committed, 21));
    // Deterministic state-machine oracle: four MRU entries, epoch reset,
    // positive hits, capacity changes, evictions and cold probes.
    struct Reference {
        struct Slot { void* key{}; std::size_t bytes{}; };
        std::array<Slot, 4> slots{};
        std::uint64_t epoch{};
        bool Covers(void* ptr, std::size_t bytes, std::uint64_t current) {
            if (epoch != current) return false;
            for (unsigned i = 0; i < slots.size(); ++i) {
                if (!slots[i].key) return false;
                if (slots[i].key != ptr || slots[i].bytes < bytes) continue;
                if (i == 1 && !slots[2].key) return true;
                const Slot hit = slots[i];
                for (unsigned j = i; j > 0; --j) slots[j] = slots[j - 1];
                slots[0] = hit;
                return true;
            }
            return false;
        }
        void Remember(void* ptr, std::size_t bytes, std::uint64_t current) {
            if (epoch != current) { slots = {}; epoch = current; }
            unsigned i = 0;
            while (i < slots.size() && slots[i].key != ptr) ++i;
            if (i == slots.size()) i = slots.size() - 1;
            for (; i > 0; --i) slots[i] = slots[i - 1];
            slots[0] = {ptr, bytes};
        }
    } reference;
    Eden::Jit::SparseCommitPositiveCache verified;
    std::uint64_t seed = 0x9e3779b97f4a7c15ULL, epoch = 1;
    for (unsigned i = 0; i < 250000; ++i) {
        seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
        if ((seed & 0xffu) == 0u) ++epoch;
        void* ptr = reinterpret_cast<void*>(
            std::uintptr_t{0x30000000} + ((seed >> 11) & 15u) * 0x1000000u);
        const std::size_t bytes = ((seed >> 22) & 31u) * 0x200000u;
        if ((seed & 7u) <= 2u) {
            reference.Remember(ptr, bytes, epoch);
            verified.Remember(ptr, bytes, epoch);
        } else {
            assert(reference.Covers(ptr, bytes, epoch) == verified.Covers(ptr, bytes, epoch));
        }
    }
    std::printf("SPARSE_JIT_POSITIVE_CACHE_PASS alternating_misses=%zu epochs=1 growth=1 eviction=1 readonly_hit=1 readonly_two_arenas=1 cold_miss=1\n", misses);
}
"""
with tempfile.TemporaryDirectory(prefix="eden-sparse-jit-mru-") as tmp:
    path = Path(tmp)
    (path / "test.cpp").write_text(FIXTURE)
    for opt in (0, 2, 3):
        exe = path / f"test-O{opt}"
        subprocess.run([compiler, "-std=c++20", f"-O{opt}", "-Wall", "-Wextra",
                        "-Werror", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        "-I", str(ROOT / "headless"), str(path / "test.cpp"),
                        "-o", str(exe)], check=True, timeout=90)
        subprocess.run([str(exe)], check=True, timeout=90)
print("SPARSE_JIT_POSITIVE_CACHE_HOST_PASS O0=1 O2=1 O3=1 source_linked=1 reservation_epoch=1 trim_release_epoch=1")
