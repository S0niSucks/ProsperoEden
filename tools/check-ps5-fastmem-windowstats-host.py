#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile the real PS5 A32 fastmem diagnostic census; preserve every bit."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/host_memory.cpp").read_text()
header = (ROOT / "src/fastmem.h").read_text()
assert "std::uint64_t direct_reads, direct_writes" in header
assert source.count("Stats WindowStats() noexcept {") == 1
method = "Stats WindowStats() noexcept {" + source.split(
    "Stats WindowStats() noexcept {", 1)[1].split(
    "\n}\n} // namespace Eden::Fastmem", 1)[0] + "\n}"
assert "std::memcpy(&guards, map + page, sizeof(guards));" in method
assert "std::popcount(guards & lane_bits)" in method
assert "std::popcount((guards >> 1) & lane_bits)" in method
assert "GuestPages % sizeof(std::uint64_t) == 0" in method
assert "for (std::size_t page = 0; page < GuestPages; ++page)" not in method
assert "python3 -B tools/check-ps5-fastmem-windowstats-host.py" in (
    ROOT / ".github/workflows/encore-core-fast-gate.yml").read_text()
assert "python3 -B tools/check-ps5-fastmem-windowstats-host.py" in (
    ROOT / ".github/workflows/encore-core-preflight.yml").read_text()

cpp = r"""
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
constexpr std::size_t GuestPages = std::size_t{1} << 20;
constexpr std::uint8_t ReadBlocked = 1, WriteBlocked = 2;
struct Stats {
    std::uint64_t window, mapped_pages, aliased_chunks;
    std::uint64_t direct_reads, direct_writes;
    std::uint64_t map_calls, unmap_calls, protect_calls;
    std::uint64_t kernel_calls, kernel_ns, failures;
};
struct Counters {
    std::atomic<std::uint64_t> window{}, mapped_pages{}, aliased_chunks{};
    std::atomic<std::uint64_t> map_calls{}, unmap_calls{}, protect_calls{};
    std::atomic<std::uint64_t> kernel_calls{}, kernel_ns{}, failures{};
} counters;
std::atomic<const std::uint8_t*> current_map{nullptr};
namespace Eden::Fastmem {
METHOD
}
int main() {
    using Eden::Fastmem::WindowStats;
    const auto empty = WindowStats();
    assert(empty.direct_reads == 0 && empty.direct_writes == 0);
    // Force the source's 64-bit memcpy to work on deliberately misaligned
    // guest map bytes, and check every blocked bit against the old oracle.
    std::vector<std::uint8_t> storage(GuestPages + 9);
    auto* const map = storage.data() + 1;
    current_map.store(map, std::memory_order_release);
    counters.window.store(42);
    counters.mapped_pages.store(91);
    counters.aliased_chunks.store(9);
    counters.kernel_calls.store(3);
    std::uint64_t seed = 0x9e3779b97f4a7c15ULL;
    for (unsigned mode = 0; mode < 8; ++mode) {
        for (std::size_t i = 0; i < GuestPages; ++i) {
            seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
            map[i] = mode == 0 ? 0 : mode == 1 ? 3 :
                     mode == 2 ? 1 : mode == 3 ? 2 :
                     mode == 4 ? 0xff : mode == 5 ? 0xfc :
                     mode == 6 ? (i & 3) : static_cast<std::uint8_t>(seed >> 32);
        }
        std::uint64_t reads = 0, writes = 0;
        for (std::size_t i = 0; i < GuestPages; ++i) {
            reads += !(map[i] & ReadBlocked);
            writes += !(map[i] & WriteBlocked);
        }
        const auto stats = WindowStats();
        assert(stats.direct_reads == reads && stats.direct_writes == writes);
        assert(stats.window == 42 && stats.mapped_pages == 91 &&
               stats.aliased_chunks == 9 && stats.kernel_calls == 3);
    }
    current_map.store(nullptr, std::memory_order_release);
    const auto after = WindowStats();
    assert(after.direct_reads == 0 && after.direct_writes == 0);
    std::puts("FASTMEM_WINDOW_STATS_PASS patterns=8 pages=1048576 misaligned=1 counters=preserved");
}
""".replace("METHOD", method)
compiler = shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler
with tempfile.TemporaryDirectory(prefix="f07-fastmem-windowstats-") as tmp:
    path = Path(tmp) / "windowstats.cpp"
    path.write_text(cpp)
    for opt in ("-O0", "-O2", "-O3"):
        executable = Path(tmp) / ("test_" + opt[1:])
        subprocess.run([compiler, "-std=c++20", opt, "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        str(path), "-o", str(executable)], check=True, timeout=60)
        subprocess.run([str(executable)], check=True, timeout=30)
print("FASTMEM_WINDOW_STATS_SOURCE_LINKED_PASS O0=1 O2=1 O3=1 console=unverified")
