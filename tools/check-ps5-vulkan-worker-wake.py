#!/usr/bin/env python3
"""R357: native Vulkan Scheduler drain notifications, exact-source host fixture."""
from pathlib import Path
import subprocess
import shutil
import tempfile
import ast

root = Path(__file__).resolve().parents[1]
header = (root / "vendor/encore-active/eden-vulkan/include/video_core/renderer_vulkan/vk_scheduler.h").read_text()
source = (root / "vendor/encore-active/eden-vulkan/vulkan_scheduler.cpp").read_text()
generator = (root / "tools/prepare-vulkan-port.py").read_text()
ast.parse(generator)
assert "unsigned queue_drain_waiters{};" in header
assert "bool worker_in_flight{};" in header
assert "wake_worker = work_queue.empty() && !worker_in_flight;" in source
assert "if (wake_worker) event_cv.notify_all();" in source
assert "worker_in_flight = false;" in source and "worker_in_flight = true;" in source
assert "bool worker_in_flight{};" in generator
assert "wake_worker = work_queue.empty() && !worker_in_flight;" in generator
assert "++queue_drain_waiters;" in source
assert "--queue_drain_waiters;" in source
assert "if (work_queue.empty() && queue_drain_waiters != 0)" in source
assert "event_cv.notify_all();" in source
assert "queue_drain_waiters" in generator
# Capture the actual production WaitWorker body and queue-pop lambda rather
# than silently testing a separately retyped version.
wait_start = source.index("void Scheduler::WaitWorker() {")
wait_end = source.index("\nvoid Scheduler::DispatchWork()", wait_start)
native_wait = source[wait_start:wait_end]
worker_start = source.index("    const auto TryPopQueue{[this](auto& work) -> bool {")
worker_end = source.index("\n    while (!stop_token.stop_requested())", worker_start)
native_pop = source[worker_start:worker_end]
dispatch_start = source.index("void Scheduler::DispatchWork() {")
dispatch_end = source.index("\nvoid Scheduler::BeginRenderPassImpl(", dispatch_start)
native_dispatch = source[dispatch_start:dispatch_end]
cpp = r"""
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <utility>
#include <thread>
#include <vector>
#include <cstdio>
using namespace std::chrono_literals;
struct CommandChunk {
    unsigned sequence;
    bool Empty() const { return false; }
};
struct MeasuredCV {
    std::condition_variable_any cv;
    std::atomic<unsigned> notifications{0};
    void notify_all() { notifications.fetch_add(1); cv.notify_all(); }
    template<class Lock, class Predicate>
    void wait(Lock& lock, Predicate&& predicate) {
        cv.wait(lock, std::forward<Predicate>(predicate));
    }
    template<class Lock, class Predicate>
    bool wait(Lock& lock, std::stop_token token, Predicate&& predicate) {
        return cv.wait(lock, token, std::forward<Predicate>(predicate));
    }
};
class Scheduler {
public:
    std::queue<std::unique_ptr<CommandChunk>> work_queue;
    std::mutex queue_mutex, execution_mutex;
    MeasuredCV event_cv;
    unsigned queue_drain_waiters{};
    bool worker_in_flight{};
    std::unique_ptr<CommandChunk> chunk;
    std::atomic<bool> pause_worker{false};
    std::atomic<bool> worker_paused{false};
    std::atomic<unsigned> executed{0};
    std::atomic<unsigned> empty_notifications{0};
    std::jthread worker;
    Scheduler() : worker([this](std::stop_token token) { WorkerThread(token); }) {}
    ~Scheduler() {
        worker.request_stop();
        event_cv.notify_all();
        worker.join();
    }
    void AcquireNewChunk() { chunk.reset(); }
    void DispatchWork();
    void WaitWorker();
    void Enqueue(unsigned sequence) {
        assert(!chunk);
        chunk = std::make_unique<CommandChunk>(CommandChunk{sequence});
        DispatchWork();
    }
    void WorkerThread(std::stop_token stop_token) {
__NATIVE_POP__
        while (!stop_token.stop_requested()) {
            std::unique_ptr<CommandChunk> work;
            { std::unique_lock lock{queue_mutex};
              worker_in_flight = false;
              event_cv.wait(lock, stop_token, [&] { return TryPopQueue(work); });
              if (stop_token.stop_requested()) return;
              worker_in_flight = true;
            }
            { std::lock_guard lock{execution_mutex};
              if (pause_worker.load()) {
                  worker_paused.store(true);
                  while (pause_worker.load()) std::this_thread::yield();
              }
              assert(work->sequence == executed.fetch_add(1));
            }
        }
    }
};
__NATIVE_WAIT__
__NATIVE_DISPATCH__
int main() {
    Scheduler scheduler;
    constexpr unsigned rounds = 100, each = 40;
    for (unsigned batch = 0; batch < rounds; ++batch) {
        for (unsigned i=0; i<each; ++i) scheduler.Enqueue(batch*each + i);
        // WaitWorker can start while the worker is actively popping.
        auto completion = std::async(std::launch::async,
                                     [&] { scheduler.WaitWorker(); });
        assert(completion.wait_for(4s) == std::future_status::ready);
        completion.get();
        assert(scheduler.executed.load() == (batch+1)*each);
    }
    // Real native WaitWorker body under overlapping guest sync demand:
    // three independent waiters must all observe the same queue drain,
    // including when the last queued work is already executing.
    for (unsigned wave=0; wave<64; ++wave) {
        for (unsigned i=0; i<64; ++i) scheduler.Enqueue(rounds*each + wave*64 + i);
        std::array<std::future<void>, 3> waiters;
        for (auto& waiter : waiters) {
            waiter = std::async(std::launch::async, [&] { scheduler.WaitWorker(); });
        }
        for (auto& waiter : waiters) {
            assert(waiter.wait_for(4s) == std::future_status::ready);
            waiter.get();
        }
        assert(scheduler.executed.load() == rounds*each + (wave+1)*64);
    }
    // Deterministic burst while the Vulkan worker is executing a chunk:
    // its queue can be empty but it is NOT sleeping on event_cv. Avoid
    // waking drain waiters or broadcasting for each queued successor.
    scheduler.pause_worker.store(true);
    const unsigned before_burst = rounds*each + 64*64;
    scheduler.Enqueue(before_burst);
    for (unsigned spin = 0; spin < 1000000 && !scheduler.worker_paused.load(); ++spin)
        std::this_thread::yield();
    assert(scheduler.worker_paused.load());
    const unsigned wake_before = scheduler.event_cv.notifications.load();
    for (unsigned i = 1; i <= 64; ++i) scheduler.Enqueue(before_burst+i);
    assert(scheduler.event_cv.notifications.load() == wake_before);
    scheduler.pause_worker.store(false);
    scheduler.WaitWorker();
    assert(scheduler.executed.load() == before_burst + 65);
    // WaitWorker must also return when queue already empty, without notification.
    scheduler.WaitWorker();
    assert(scheduler.queue_drain_waiters == 0);
    std::puts("PASS: real WaitWorker exact source, 4000 base tasks + 4096 competing three-waiter tasks, FIFO, no missed wakeups");
}
""".replace("__NATIVE_POP__", native_pop).replace("__NATIVE_WAIT__", native_wait).replace("__NATIVE_DISPATCH__", native_dispatch)
compiler = shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler, "C++20 compiler required"
with tempfile.TemporaryDirectory(prefix="eden-r357-worker-") as tmp:
    path = Path(tmp) / "worker.cpp"
    exe = Path(tmp) / "worker"
    path.write_text(cpp)
    subprocess.run([compiler, "-std=c++20", "-O1", "-pthread",
                    "-Wall", "-Wextra", "-Werror", str(path), "-o", str(exe)],
                   check=True)
    subprocess.run([str(exe)], timeout=30, check=True)
print("Vulkan worker stop-notify remains unconditional; queue drain wakeups are conditional")
