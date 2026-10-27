#!/usr/bin/env python3
"""Host check of guest core idle polling (performance.h AdaptIdlePoll, the Idle body that
headless/CMakeLists.txt writes) with Eden's pinned PhysicalCore::Interrupt, and of the kernel
spin lock (headless/kernel_spin_lock.h).

Usage: check-guest-idle.py PINNED_PHYSICAL_CORE_CPP [--generated GENERATED_PHYSICAL_CORE_CPP]
       [--idle-only] [--sanitizer all|none|address|thread]

It checks the polling rule, that no wake-up is lost, that the time counted as polling is what
the core spent polling (the overlay leaves it out of the core's load), that a core which waits long stops polling
and one that is handed work often keeps polling, and the lock under ThreadSanitizer. It does not
measure a game's frame rate on the console.
"""
import argparse
import hashlib
from pathlib import Path
import subprocess, tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('pinned', type=Path)
parser.add_argument('--generated', type=Path, help='also verify the saved build uses these exact bodies')
parser.add_argument('--idle-only', action='store_true', help='skip the unrelated kernel spin lock check')
parser.add_argument('--sanitizer', choices=('all', 'none', 'address', 'thread'), default='all')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
pinned = args.pinned.read_text()
cmake = (root / 'headless/CMakeLists.txt').read_text()
idle = cmake.split('string(REPLACE "${idle_old}" [=[', 1)[1].split(']=] physical_core_source', 1)[0]
interrupts = pinned[pinned.index('void PhysicalCore::Interrupt() {'):pinned.index('\n} // namespace Kernel')]
# What the build changes in Interrupt and ClearInterrupt, as CMakeLists.txt writes it.
unquote = lambda text: text.replace('\\n', '\n').replace('\\"', '"')
for name in ('interrupt_old', 'clear_old'):
    before = unquote(cmake.split('set(' + name + ' "', 1)[1].split('")', 1)[0])
    after = unquote(cmake.split('string(REPLACE "${' + name + '}"\n    "', 1)[1].split('"\n', 1)[0])
    assert interrupts.count(before) == 1, name
    interrupts = interrupts.replace(before, after)

# Check the actual saved generated source, not just an assumed CMake expansion.
if args.generated:
    generated = args.generated.read_text()
    actual_idle = generated[generated.index('void PhysicalCore::Idle() {'):generated.index('\nbool PhysicalCore::IsInterrupted()')]
    actual_interrupts = generated[generated.index('void PhysicalCore::Interrupt() {'):generated.index('\n} // namespace Kernel')]
    assert actual_idle.strip() == idle.strip(), 'Generated Idle differs from the tested CMake body'
    assert actual_interrupts.strip() == interrupts.strip(), 'Generated Interrupt/ClearInterrupt differ'
for path in (Path(__file__), args.pinned, root / 'headless/CMakeLists.txt', root / 'headless/performance.h', args.generated):
    if path:
        print('SOURCE', hashlib.sha256(path.read_bytes()).hexdigest(), path, flush=True)

# Test-only, nonblocking observation immediately AFTER Idle captures its actual start timestamp.
# A signal before calling Idle would have the same scheduling hole as the original kick.
# Wakeups leaves the hook disabled: its before/during/after-entry races remain unsynchronized.
entry_line = '    const long long start = ::Eden::Performance::NowNs();'
assert idle.count(entry_line) == 1
timed_idle = idle.replace(entry_line, entry_line + '\n    ObserveIdleEntry(start);')

clock = r'''
#include <chrono>
namespace Common {
struct WallClock {
    std::chrono::nanoseconds GetTimeNS() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
    }
    long long GetCNTPCT() const { return 0; }
};
inline WallClock g_wall_clock;
}
'''

idle_code = r'''
#include "performance.h"
#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <limits>
#include <mutex>
#include <semaphore>
#include <thread>
#include <time.h>
#include <vector>
using namespace std::chrono_literals;
namespace P = Eden::Performance;
struct Thread {};
struct Interface { void SignalInterrupt(Thread*) {} };
class PhysicalCore {
public:
    explicit PhysicalCore(std::size_t index) : m_core_index{index} {}
    void Idle();
    void Interrupt();
    void ClearInterrupt();
private:
    std::size_t m_core_index;
    std::mutex m_guard;
    std::condition_variable m_on_interrupt;
    bool m_is_interrupted{};
    Interface* m_arm_interface{};
    Thread* m_current_thread{};
};
static thread_local std::atomic<long long>* idle_entry = nullptr;
static void ObserveIdleEntry(long long start) {
    if (idle_entry) idle_entry->store(start, std::memory_order_release);
}
''' + timed_idle + '\n' + interrupts + r'''
static long long ThreadCpuNs() {
    timespec now{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
    return now.tv_sec * 1'000'000'000LL + now.tv_nsec;
}
static void Reset() {
    for (auto& poll : P::idle_poll_ns) poll = 0;
    for (auto& kept : P::idle_poll_kept_ns) kept = 0;
    for (auto& run : P::idle_long_run) run = 0;
    for (auto& core : P::guest_idle) core.calls = core.nanoseconds = core.sleeps = core.spin_ns = 0;
    P::idle_poll_most_ns = 500'000;
    P::idle_poll_adaptive = true;
}

// The rule alone.
static void Policy() {
    Reset();
    assert(P::IdlePollBudget(0) == 0);                // A core starts by sleeping at once.
    P::AdaptIdlePoll(0, 30'000, true);                // A 30 us sleep: polling would have caught it.
    assert(P::IdlePollBudget(0) == 60'000);           // Twice the wait (at least a tenth).
    P::AdaptIdlePoll(0, 10'000, true);                // Shorter than the poll: no change.
    assert(P::IdlePollBudget(0) == 60'000);
    P::AdaptIdlePoll(0, 70'000, true);                // At least double.
    assert(P::IdlePollBudget(0) == 140'000);
    P::AdaptIdlePoll(0, 300'000, true);
    assert(P::IdlePollBudget(0) == 500'000);          // Never beyond the longest.
    P::AdaptIdlePoll(0, 20'000, false);               // Caught while polling: kept.
    assert(P::IdlePollBudget(0) == 500'000);
    // Long waits between handoffs keep the poll: only a run of them reduces it.
    for (int round = 0; round < 50; ++round) {
        for (unsigned i = 0; i < P::kIdleLongRun; ++i) P::AdaptIdlePoll(0, 5'000'000, true);
        P::AdaptIdlePoll(0, 150'000, false);
    }
    assert(P::IdlePollBudget(0) == 500'000);
    for (unsigned i = 0; i < P::kIdleLongRun; ++i) P::AdaptIdlePoll(0, 5'000'000, true);
    assert(P::IdlePollBudget(0) == 500'000);
    P::AdaptIdlePoll(0, 5'000'000, true);             // Past the run, each long wait halves it ...
    assert(P::IdlePollBudget(0) == 250'000);
    P::AdaptIdlePoll(0, 5'000'000, true);
    P::AdaptIdlePoll(0, 5'000'000, true);
    assert(P::IdlePollBudget(0) == 62'500);
    P::AdaptIdlePoll(0, 5'000'000, true);             // ... to nothing below a tenth.
    assert(P::IdlePollBudget(0) == 0);
    P::AdaptIdlePoll(0, 40'000, true);                // A short sleep again: back to the poll it had.
    assert(P::IdlePollBudget(0) == 500'000);
    P::AdaptIdlePoll(0, 5'000'000, true);             // The run starts over.
    assert(P::IdlePollBudget(0) == 500'000);
    assert(P::IdlePollBudget(1) == 0);                // Each core has its own.
    P::AdaptIdlePoll(9, 30'000, true);                // A core the counters do not have: ignored.
    assert(P::IdlePollBudget(9) == 500'000);
    P::idle_poll_adaptive = false;                    // dev-settings idle_spin_us: fixed.
    P::idle_poll_most_ns = 100'000;
    P::AdaptIdlePoll(0, 30'000, true);
    assert(P::IdlePollBudget(0) == 100'000);
}

// Interrupts before, during and after entry into the wait; none is lost.
static void Wakeups() {
    Reset();
    PhysicalCore core{0};
    core.Interrupt();                                 // Remains until cleared.
    core.Idle();
    core.Idle();
    assert(P::guest_idle[0].calls == 2 && P::guest_idle[0].sleeps == 0);
    core.ClearInterrupt();
    constexpr unsigned rounds = 3000;
    std::binary_semaphore kick{0}, done{0};
    std::thread worker([&] {
        for (unsigned i = 0; i < rounds; ++i) {
            kick.acquire();
            core.Idle();
            done.release();
        }
    });
    for (unsigned i = 0; i < rounds; ++i) {
        core.ClearInterrupt();
        kick.release();
        if ((i % 4) == 1) std::this_thread::yield();
        if ((i % 4) == 2) std::this_thread::sleep_for(100us);
        if ((i % 4) == 3) std::this_thread::sleep_for(1ms);
        core.Interrupt();
        assert(done.try_acquire_for(2s));
    }
    worker.join();
    assert(P::guest_idle[0].calls == rounds + 2);
    // Independent cores do not wake each other.
    PhysicalCore other{1};
    std::binary_semaphore other_done{0};
    core.ClearInterrupt();
    std::thread parked([&] { other.Idle(); other_done.release(); });
    core.Interrupt();
    assert(!other_done.try_acquire_for(30ms));
    other.Interrupt();
    assert(other_done.try_acquire_for(2s));
    parked.join();
}

enum class Handshake { idle_entry, legacy_kick };
struct WaitEvidence {
    long long intended_min = std::numeric_limits<long long>::max(), intended_max = 0;
    long long observed_min = std::numeric_limits<long long>::max(), observed_max = 0;
    long long entry_lag_max = 0;
    unsigned preraised = 0;
};
static void Until(long long deadline) {
    while (P::NowNs() < deadline) Eden::CpuRelax();
}
static long long Entered(const std::atomic<long long>& entry) {
    const long long deadline = P::NowNs() + 2'000'000'000LL;
    long long start;
    while ((start = entry.load(std::memory_order_acquire)) == 0) {
        assert(P::NowNs() < deadline);
        Eden::CpuRelax();
    }
    return start;
}

// Arm each interval against the timestamp captured INSIDE the actual Idle body. The hook
// does not park the worker or move the runtime timestamp. Record raised-start separately
// from the requested gap: a slow producer can still overshoot, and that must be visible.
// late_round gates one entry until the old producer deadline has passed (deterministic scheduler
// delay). legacy_kick is retained ONLY to prove that the old timing assertion then fails.
// Returns worker CPU time; slept[i] retains the original Cost/Spin assertion inputs.
static long long Waiting(std::vector<std::chrono::microseconds> gaps, unsigned rounds,
                         std::vector<bool>& slept, Handshake handshake = Handshake::idle_entry,
                         int late_round = -1, WaitEvidence* evidence = nullptr) {
    PhysicalCore core{2};
    std::binary_semaphore kick{0}, done{0}, delayed_entry{0};
    std::atomic<long long> entry{0};
    WaitEvidence result;
    long long cpu = 0;
    slept.assign(rounds, false);
    std::thread worker([&] {
        idle_entry = &entry;
        const long long begin = ThreadCpuNs();
        for (unsigned i = 0; i < rounds; ++i) {
            kick.acquire();
            if (static_cast<int>(i) == late_round) delayed_entry.acquire();
            core.Idle();
            done.release();
        }
        cpu = ThreadCpuNs() - begin;
    });
    for (unsigned i = 0; i < rounds; ++i) {
        const auto sleeps = P::guest_idle[2].sleeps.load();
        core.ClearInterrupt();
        entry.store(0, std::memory_order_relaxed);
        const long long kicked = P::NowNs();
        kick.release();
        const long long armed = P::NowNs();             // Original timer starts after kick.release().
        const long long intended = std::chrono::duration_cast<std::chrono::nanoseconds>(gaps[i % gaps.size()]).count();
        const bool delayed = static_cast<int>(i) == late_round;
        if (delayed && handshake == Handshake::idle_entry) {
            Until(armed + intended);
            delayed_entry.release();
        }
        const long long start = handshake == Handshake::idle_entry ? Entered(entry) : 0;
        Until((handshake == Handshake::idle_entry ? start : armed) + intended);
        core.Interrupt();
        if (delayed && handshake == Handshake::legacy_kick) delayed_entry.release();
        assert(done.try_acquire_for(2s));
        slept[i] = P::guest_idle[2].sleeps.load() != sleeps;
        const long long actual_start = entry.load(std::memory_order_acquire);
        const long long raised = P::RaisedNs(2);
        assert(actual_start != 0);
        const long long observed = std::max(0LL, raised - actual_start);
        if (handshake == Handshake::idle_entry) assert(observed >= intended);
        result.intended_min = std::min(result.intended_min, intended);
        result.intended_max = std::max(result.intended_max, intended);
        result.observed_min = std::min(result.observed_min, observed);
        result.observed_max = std::max(result.observed_max, observed);
        result.entry_lag_max = std::max(result.entry_lag_max, actual_start - kicked);
        result.preraised += raised <= actual_start;
    }
    worker.join();
    if (evidence) *evidence = result;
    std::printf("waits=%u handshake=%s late-round=%d intended=[%.2f,%.2f] us "
                "observed=[%.2f,%.2f] us preraised=%u entry-lag-max=%.2f us CPU=%.2f ms\n",
                rounds, handshake == Handshake::idle_entry ? "idle-entry" : "legacy-kick", late_round,
                result.intended_min / 1e3, result.intended_max / 1e3,
                result.observed_min / 1e3, result.observed_max / 1e3, result.preraised,
                result.entry_lag_max / 1e3, cpu / 1e6);
    return cpu;
}

static void LateWorkerRegression() {
    constexpr unsigned rounds = P::kIdleLongRun + 8;
    constexpr int late_round = rounds / 2;
    std::vector<bool> slept;
    WaitEvidence legacy, synchronized;
    Reset();
    P::idle_poll_ns[2] = 500'000;
    (void)Waiting({4ms}, rounds, slept, Handshake::legacy_kick, late_round, &legacy);
    // Just ONE pre-raised interrupt breaks the run of long waits. The original Cost
    // assertion (budget == 0) is FALSE even if every other round really waited 4 ms.
    assert(legacy.preraised >= 1 && legacy.observed_min == 0);
    assert(P::IdlePollBudget(2) == 500'000 && P::idle_long_run[2] < P::kIdleLongRun);
    std::printf("late worker, old handshake: unchanged budget==0 criterion FAILS (budget %lld ns)\n",
                P::IdlePollBudget(2));
    Reset();
    P::idle_poll_ns[2] = 500'000;
    (void)Waiting({4ms}, rounds, slept, Handshake::idle_entry, late_round, &synchronized);
    assert(synchronized.preraised == 0 && synchronized.observed_min >= 4'000'000);
    assert(P::IdlePollBudget(2) == 0);
    std::puts("late worker: PASS old handshake fails, actual-Idle-entry handshake stops polling");
}
static unsigned Count(const std::vector<bool>& slept, unsigned from = 0, unsigned step = 1) {
    unsigned count = 0;
    for (unsigned i = from; i < slept.size(); i += step) count += slept[i];
    return count;
}

static void Cost() {
    std::vector<bool> slept;
    // A core that only waits 4 ms each time (most of a frame): the fixed poll spins every time.
    Reset();
    P::idle_poll_adaptive = false;
    const long long fixed = Waiting({4ms}, 60, slept);
    Reset();
    const long long adaptive = Waiting({4ms}, 60, slept);
    std::printf("4 ms waits: fixed 500 us poll %.2f ms CPU, adaptive %.2f ms CPU (poll now %lld ns)\n",
                fixed / 1e6, adaptive / 1e6, P::IdlePollBudget(2));
    assert(P::IdlePollBudget(2) == 0);
    assert(adaptive * 2 < fixed);
    // A core that was polling fully stops once its waits keep running long.
    Reset();
    P::idle_poll_ns[2] = 500'000;
    (void)Waiting({4ms}, P::kIdleLongRun + 8, slept);
    std::printf("4 ms waits after polling fully: poll now %lld ns after %u waits\n", P::IdlePollBudget(2),
                P::kIdleLongRun + 8);
    assert(P::IdlePollBudget(2) == 0);
    // Short handoffs mixed with long waits, as most cores have them: the handoffs keep the poll.
    Reset();
    (void)Waiting({100us, 3ms, 3ms, 150us, 3ms}, 400, slept);
    const unsigned short_slept = Count(slept, 50, 5) + Count(slept, 53, 5);
    std::printf("100/150 us handoffs between 3 ms waits: %u of %u handoffs slept after warm-up "
                "(poll now %lld ns)\n", short_slept, 2 * 70, P::IdlePollBudget(2));
    assert(short_slept < 2 * 70 / 5);
    // Work handed over every 50 us: the poll grows to catch it, as the fixed one did ...
    Reset();
    (void)Waiting({50us}, 400, slept);
    const unsigned handoff_slept = Count(slept);
    std::printf("50 us handoffs: %u of 400 waits slept (poll now %lld ns)\n", handoff_slept,
                P::IdlePollBudget(2));
    assert(P::IdlePollBudget(2) >= 50'000 && handoff_slept < 80);
    // ... where never polling (idle_spin_us=0) sleeps on them.
    Reset();
    P::idle_poll_adaptive = false;
    P::idle_poll_most_ns = 0;
    (void)Waiting({50us}, 400, slept);
    std::printf("50 us handoffs without polling: %u of 400 waits slept\n", Count(slept));
    assert(Count(slept) > 200);
    assert(P::IdleSpinNs(2) == 0);                    // Nothing polled, nothing counted.
}

// The overlay leaves a core's polling out of its load (GuestCoreLoads): the time Idle counts as
// polled must be the CPU time it spent waiting, and only that.
static void Spin() {
    std::vector<bool> slept;
    // Work handed over every 50 us, caught while polling: nearly all of the waiting thread's CPU
    // time is polling, so leaving it out leaves little.
    Reset();
    const long long handoffs = Waiting({50us}, 400, slept);
    const long long polled = static_cast<long long>(P::IdleSpinNs(2));
    std::printf("50 us handoffs: %.2f ms CPU, %.2f ms of it polling\n", handoffs / 1e6, polled / 1e6);
    assert(polled > 0 && handoffs - polled < handoffs / 2);
    // 4 ms waits with a fixed 500 us poll: only the poll counts, not the sleep after it.
    Reset();
    P::idle_poll_adaptive = false;
    const long long waits = Waiting({4ms}, 60, slept);
    const long long poll_only = static_cast<long long>(P::IdleSpinNs(2));
    std::printf("4 ms waits, 500 us poll: %.2f ms CPU, %.2f ms polling\n", waits / 1e6, poll_only / 1e6);
    assert(poll_only >= 60 * 450'000LL && poll_only <= 60 * 1'500'000LL);
    assert(poll_only <= waits + 10'000'000);
}

int main(int argc, char**) {
    std::setbuf(stdout, nullptr);                       // Keep interval evidence if an assert aborts.
    Policy();
    Wakeups();
    LateWorkerRegression();
    if (argc == 1) {                                  // Timing: not under ThreadSanitizer.
        Cost();
        Spin();
    }
    std::puts("idle: PASS");
}
'''

lock_code = r'''
#include "kernel_spin_lock.h"
#include <cassert>
#include <cstdio>
#include <semaphore>
#include <thread>
#include <vector>
int main() {
    Eden::KernelSpinLock lock;
    long long counter = 0;
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&, t] {
            for (int i = 0; i < 50'000; ++i) {
                if (t % 2) {
                    lock.lock();
                } else {
                    while (!lock.try_lock()) Eden::CpuRelax();   // As the scheduler retries it.
                }
                ++counter;
                lock.unlock();
            }
        });
    for (auto& thread : threads) thread.join();
    assert(counter == 200'000);
    assert(lock.try_lock());
    assert(!lock.try_lock());
    // Taken on one thread and released on another, as a thread's context guard is.
    std::binary_semaphore taken{0};
    std::thread releaser([&] { taken.acquire(); lock.unlock(); });
    taken.release();
    releaser.join();
    assert(lock.try_lock());
    lock.unlock();
    std::puts("kernel spin lock: PASS");
}
'''

with tempfile.TemporaryDirectory() as tmp:
    (Path(tmp) / 'common').mkdir()
    (Path(tmp) / 'common' / 'cpu_features.h').write_text(clock)
    flags = ['c++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror', '-pthread',
             '-I' + tmp, '-I' + str(root / 'headless'), '-x', 'c++', '-']
    runs = []
    if args.sanitizer in ('all', 'none', 'address'):
        runs.append(('idle', idle_code, '' if args.sanitizer == 'none' else 'address,undefined', []))
    if args.sanitizer in ('all', 'thread'):
        runs.append(('idle-tsan', idle_code, 'thread', ['--no-timing']))
        if not args.idle_only:
            runs.append(('lock', lock_code, 'thread', []))
    for name, code, sanitizer, arguments in runs:
        exe = Path(tmp) / name
        instrumentation = ['-fsanitize=' + sanitizer] if sanitizer else []
        subprocess.run(flags + instrumentation + ['-o', str(exe)], input=code, text=True, check=True)
        subprocess.run([str(exe)] + arguments, check=True, timeout=180)
print('Guest idle: PASS selected checks; sanitizer=' + args.sanitizer +
      ', kernel spin lock=' + ('included' if args.sanitizer in ('all', 'thread') and not args.idle_only else 'not run'))
