#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile the actual PS5 rumble consumer with host stubs and sanitizers.

No PS5 SDK, DualSense or FC27 runtime is exercised. The source-linked test
checks publication, concurrent producer/consumer, and unchanged motor levels.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "headless/pad.cpp").read_text()
start = source.index("bool PadEngine::TakeRumble(")
end = source.index("\nPad::Pad(", start)
method = source[start:end].strip()
assert method.count("std::scoped_lock lock(rumble_mutex);") == 1
lock_start = method.index("    {\n        std::scoped_lock lock(rumble_mutex);")
lock_end = method.index("\n    }\n", lock_start)
critical = method[lock_start:lock_end]
assert "sides = pending;" in critical
assert "pending.changed = false;" in critical
assert "rumble_pending[player].store(false, std::memory_order_release);" in critical
assert "std::pow" not in critical and "std::lround" not in critical
assert method.index("const auto level") > lock_end
assert method.index("out.large") > lock_end and method.index("out.small") > lock_end
producer = source.split("PadEngine::SetVibration(", 1)[1].split(
    "bool PadEngine::TakeRumble(", 1)[0]
assert "std::scoped_lock lock(rumble_mutex);" in producer
assert producer.index("sides.changed = true;") < producer.index(
    "rumble_pending[player].store(true, std::memory_order_release);")

fixture = r"""
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
using u8 = std::uint8_t;
namespace Common::Input {
enum class VibrationAmplificationType { Linear, Exponential };
struct VibrationStatus {
    float low_amplitude{}, high_amplitude{};
    VibrationAmplificationType type{VibrationAmplificationType::Linear};
};
}
class PadEngine {
public:
    static constexpr unsigned kPlayers = 4;
    struct Rumble { u8 large = 0, small = 0; };
    struct Sides {
        Common::Input::VibrationStatus left{}, right{};
        bool changed = false;
    };
    std::mutex rumble_mutex;
    std::array<Sides, kPlayers> rumble{};
    std::array<std::atomic<bool>, kPlayers> rumble_pending{};
    void Publish(unsigned player, Sides sides) {
        std::scoped_lock lock(rumble_mutex);
        rumble[player] = sides;
        rumble[player].changed = true;
        rumble_pending[player].store(true, std::memory_order_release);
    }
    bool TakeRumble(std::size_t player, Rumble& out);
};
__ACTUAL_METHOD__
int main() {
    PadEngine engine;
    PadEngine::Rumble out{};
    assert(!engine.TakeRumble(0, out));
    assert(!engine.TakeRumble(4, out));
    PadEngine::Sides first{};
    first.left.low_amplitude = 1.0f;
    first.right.high_amplitude = 0.25f;
    engine.Publish(0, first);
    assert(engine.TakeRumble(0, out));
    assert(out.large == 255 && out.small > 0 && out.small < 255);
    assert(!engine.TakeRumble(0, out));
    // Two commands before a poll coalesce to the newest state, as before.
    PadEngine::Sides second{};
    second.left.low_amplitude = 0.0f;
    second.right.low_amplitude = 1.0f;
    second.left.high_amplitude = 1.0f;
    engine.Publish(0, first);
    engine.Publish(0, second);
    assert(engine.TakeRumble(0, out));
    assert(out.large == 255 && out.small == 255);
    // Exercise publication/consumption races under the real method body.
    std::thread producer([&] {
        for (int i = 0; i < 30000; ++i) {
            PadEngine::Sides next{};
            next.left.low_amplitude = i % 2 ? 0.5f : 1.0f;
            engine.Publish(1, next);
        }
    });
    for (int i = 0; i < 30000; ++i) engine.TakeRumble(1, out);
    producer.join();
    if (engine.rumble_pending[1].load(std::memory_order_acquire))
        assert(engine.TakeRumble(1, out));
    assert(!engine.rumble_pending[1].load(std::memory_order_acquire));
    assert(!engine.TakeRumble(1, out));
}
""".replace("__ACTUAL_METHOD__", method)
compiler = os.environ.get("CXX") or next(
    (c for c in ("clang++-18", "clang++", "g++") if shutil.which(c)), None)
if not compiler:
    raise SystemExit("C++20 compiler required")
with tempfile.TemporaryDirectory(prefix="eden-ps5-rumble-lock-") as folder:
    src = Path(folder) / "rumble.cpp"
    src.write_text(fixture)
    for opt in ("0", "2"):
        exe = Path(folder) / ("rumble-" + opt)
        subprocess.run([compiler, "-std=c++20", "-O" + opt, "-pthread",
                        "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        str(src), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True, timeout=40)
print("PASS: actual TakeRumble, O0/O2 ASan+UBSan, lock scope, concurrent publication")
print("PS5 rumble latency and FC27 performance: UNMEASURED")
