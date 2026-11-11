// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Physical commitment is a reservation hint, not a new virtual code write.
// Xbyak retains responsibility for checking actual code emission bounds.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Eden::Jit {
struct SparseCommitTarget {
    bool valid;
    bool clamped;
    std::size_t bytes;
};

constexpr SparseCommitTarget PlanSparseCommit(std::size_t written,
                                              std::size_t requested,
                                              std::size_t capacity) noexcept {
    if (written > capacity)
        return {false, false, 0};
    const std::size_t remaining = capacity - written;
    // Dynarmic's 16 MiB PRELUDE_COMMIT_SIZE is a physical precommit hint.
    // Core 3's virtual arena is also 16 MiB. After its 2 MiB constant pool,
    // requiring written + full hint fails despite all actual code fitting.
    // Commit only the address range that exists. Xbyak rejects later writes
    // beyond the allocated arena independently.
    const std::size_t additional = std::min(requested, remaining);
    return {true, requested > remaining, written + additional};
}

// Return only complete, unneeded 2 MiB direct-memory chunks after a FULL
// halted JIT clear. Keep the executable prelude, two bootstrap chunks and
// one chunk of recompilation headroom. Never move the RX/RW virtual aliases.
// This is a DEV-only policy; the caller must prove no old code is executing.
struct SparseTrimTarget {
    bool valid;
    std::size_t keep;
    std::size_t release;
};

constexpr SparseTrimTarget PlanSparseTrim(std::size_t live_code_end,
                                           std::size_t committed,
                                           std::size_t capacity,
                                           std::size_t chunk) noexcept {
    if (chunk == 0 || (chunk & (chunk - 1)) != 0 ||
        capacity % chunk || committed % chunk ||
        committed > capacity || live_code_end > committed)
        return {false, 0, 0};
    const std::size_t headroom = std::min(chunk, capacity - live_code_end);
    const std::size_t bootstrap = capacity / chunk >= 2 ? 2 * chunk : capacity;
    const std::size_t needed = std::max(live_code_end + headroom, bootstrap);
    // capacity is already chunk-aligned, so alignment cannot exceed it.
    const std::size_t padding = (chunk - needed % chunk) % chunk;
    const std::size_t keep = std::min(committed, needed + padding);
    return {true, keep, committed - keep};
}

static_assert(PlanSparseTrim(3u << 20, 128u << 20, 256u << 20, 2u << 20).keep == (6u << 20));
static_assert(PlanSparseTrim(3u << 20, 128u << 20, 256u << 20, 2u << 20).release == (122u << 20));
static_assert(PlanSparseTrim(15u << 20, 16u << 20, 16u << 20, 2u << 20).release == 0);
static_assert(!PlanSparseTrim(17u << 20, 16u << 20, 16u << 20, 2u << 20).valid);

class SparseCommitPositiveCache {
    struct Entry {
        void* executable{};
        std::size_t committed{};
    };
    std::array<Entry, 4> entries{};
    std::uint64_t generation{};

public:
    bool Covers(void* executable, std::size_t required, std::uint64_t current) noexcept {
        if (generation != current) return false;
        // EnsureMemoryCommitted probes this path for every translated block.
        // A same-core cache hit must not write to TLS: the former i=0 MRU
        // promotion wrote entries[0] back to itself on every hot probe.
        if (entries[0].executable == executable) {
            // The current arena cannot also occupy another slot. When the
            // requested code span grows beyond its committed chunk, go
            // directly to the allocator instead of scanning three misses.
            return required <= entries[0].committed;
        }
        for (std::size_t i = 1; i < entries.size(); ++i) {
            // Remember fills from slot zero; MRU promotion never creates
            // holes. A negative lookup on the usual 1-2 live JIT arenas
            // need not probe the remaining unoccupied TLS entries.
            if (!entries[i].executable) return false;
            if (entries[i].executable != executable) continue;
            // An arena is represented at most once. If its required code
            // footprint has grown, no later slot can satisfy this request:
            // take the allocator path now instead of scanning the tail.
            if (required > entries[i].committed) return false;
            // With only two cached arenas, neither can be evicted yet.
            // Keep alternating A32/A64 hits read-only instead of moving
            // two TLS entries on each translated guest block. Once a third
            // arena appears, retain MRU promotion for eviction decisions.
            if (i == 1 && !entries[2].executable) return true;
            const Entry hit = entries[i];
            for (std::size_t j = i; j > 0; --j) entries[j] = entries[j - 1];
            entries[0] = hit;
            return true;
        }
        return false;
    }

    void Remember(void* executable, std::size_t committed, std::uint64_t current) noexcept {
        if (generation != current) {
            entries = {};
            generation = current;
        }
        std::size_t i = 0;
        while (i < entries.size() && entries[i].executable != executable) ++i;
        if (i == entries.size()) i = entries.size() - 1;
        for (; i > 0; --i) entries[i] = entries[i - 1];
        entries[0] = {executable, committed};
    }
};
} // namespace Eden::Jit
