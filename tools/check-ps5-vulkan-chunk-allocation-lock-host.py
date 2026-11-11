#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Vulkan scheduler: source-extracted recycled chunk vs cold allocation lock test.

The slow constructor represents a pageable PS5 host allocation. Its only
purpose is to prove the GPU worker can recycle a chunk concurrently; this
host fixture is NOT an FC27 frame-time or PS5 performance measurement.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
src = (root / "vendor/encore-active/eden-vulkan/vulkan_scheduler.cpp").read_text()
generator = (root / "tools/prepare-vulkan-port.py").read_text()
start = "void Scheduler::AcquireNewChunk() {"
end = "\n} // namespace Vulkan"
assert src.count(start) == 1 and end in src[src.index(start):]
method = src[src.index(start):src.index(end, src.index(start))].rstrip()
assert "chunk_reserve.empty()" in method
assert method.index("std::scoped_lock rl{reserve_mutex};") < method.index("chunk_reserve.back()")
assert method.index("chunk_reserve.pop_back();") < method.index("std::make_unique<CommandChunk>();")
assert "std::make_unique<CommandChunk>();" in method
assert method.count("return;") == 1
assert "The producer can reuse a completed command chunk immediately." in generator
assert "Allocation can page fault or invoke allocator locks." in generator

cpp = r"""
#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
struct CommandChunk {
    static inline std::atomic<bool> block{false};
    static inline std::atomic<bool> entered{false};
    CommandChunk() {
        if (block.load()) {
            entered.store(true);
            while (block.load()) std::this_thread::yield();
        }
    }
};
class Scheduler {
public:
    std::mutex reserve_mutex;
    std::vector<std::unique_ptr<CommandChunk>> chunk_reserve;
    std::unique_ptr<CommandChunk> chunk;
    void AcquireNewChunk();
};
__NATIVE_ACQUIRE__
int main() {
    for (int run=0; run<8; ++run) {
        Scheduler scheduler;
        auto recycled = std::make_unique<CommandChunk>();
        CommandChunk* const recycled_owner = recycled.get();
        CommandChunk::entered.store(false);
        CommandChunk::block.store(true);
        // The producer cannot finish allocation until the fixture releases
        // the simulated allocator. The GPU recycler must remain unblocked.
        std::thread producer([&] { scheduler.AcquireNewChunk(); });
        for (unsigned i=0; i<2000000 && !CommandChunk::entered.load(); ++i)
            std::this_thread::yield();
        assert(CommandChunk::entered.load());
        {
            std::unique_lock<std::mutex> recycle_lock{
                scheduler.reserve_mutex, std::try_to_lock};
            assert(recycle_lock.owns_lock());
            scheduler.chunk_reserve.push_back(std::move(recycled));
        }
        CommandChunk::block.store(false);
        producer.join();
        assert(scheduler.chunk);
        assert(scheduler.chunk_reserve.size()==1);
        scheduler.AcquireNewChunk();
        assert(scheduler.chunk.get()==recycled_owner);
        assert(scheduler.chunk_reserve.empty());
    }
}
""".replace("__NATIVE_ACQUIRE__", method)
compiler = shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler, "C++20 compiler required"
with tempfile.TemporaryDirectory(prefix="eden-vulkan-chunk-allocation-") as tmp:
    source = Path(tmp) / "chunk.cpp"
    source.write_text(cpp)
    for opt in ("-O0", "-O2"):
        binary = Path(tmp) / ("chunk_" + opt[1:])
        subprocess.run([compiler, "-std=c++20", opt, "-pthread", "-Wall", "-Wextra",
                        "-Werror", "-fsanitize=address,undefined",
                        "-fno-sanitize-recover=all", str(source), "-o", str(binary)],
                       check=True, timeout=50)
        subprocess.run([str(binary)], check=True, timeout=25)
        print(f"VULKAN_CHUNK_ALLOC_OUTSIDE_RECYCLER_PASS {opt} no_worker_stall=1 reuse=1")
print("PS5 allocation latency and FPS not measured")
