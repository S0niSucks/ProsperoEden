#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the actual Encore pad implementation and device-fixture recovery checks.

Extracts the complete Pad/PadEngine declarations and implementation verbatim,
the actual mock pad/user ABI functions, and CheckPadRecovery from devices_check.
Only external InputCommon/factory/logging boundaries are stand-ins. No vendor
sources, SDK, guest, native qualification, or full device/audio build is used.
All generated files stay in a temporary directory (TMPDIR may select tmpfs).
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PAD = (ROOT / "headless/pad.cpp").read_text()
HEADER = (ROOT / "headless/devices.h").read_text()
MOCK = (ROOT / "headless/mock_devices.cpp").read_text()
CHECKS = (ROOT / "headless/devices_check.cpp").read_text()


def between(text: str, start: str, end: str) -> str:
    assert text.count(start) == text.count(end) == 1, ("source boundary drift", start, end)
    first, last = text.index(start), text.index(end)
    assert first < last
    return text[first:last]


def replace_once(text: str, old: str, new: str) -> str:
    assert text.count(old) == 1, ("negative-control source drift", old)
    return text.replace(old, new, 1)


declarations = between(HEADER, "namespace Eden {", "\nclass AudioStream final") + "\n}\n"
assert PAD.count("\nnamespace {") == 1
implementation = PAD[PAD.index("\nnamespace {"):]
assert "std::array<ps5::pad::Data, ps5::pad::kMaxSamples> samples;" in implementation
assert "std::array<ps5::pad::Data, ps5::pad::kMaxSamples> samples{};" not in implementation
mock_abi = between(MOCK, "int sceUserServiceInitialize(void*)", "\nint sceAudioOutInit()")
tests = between(CHECKS, "void CheckPadRecovery() {", "\nvoid CheckAudio() {")
assert "CheckPadRecovery();" in CHECKS, "recovery checks must also run in the device fixture"

# No recovery, mapping, rumble, motion, batch or ownership logic is reimplemented here.
# The InputEngine sink records values in place of invoking vendor callbacks.
prelude = r"""
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <time.h>
#include "button_mapping.h"
#include "pad_diagnostics.h"
#include "ps5_guest_system_button_route.h"
#include "ps5_pad.hpp"
#include "mock_devices.h"
using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
namespace Common {
struct UUID {};
namespace Input {
enum class DriverResult { Success, InvalidParameters };
enum class VibrationAmplificationType { Linear, Exponential };
struct VibrationStatus {
    float low_amplitude{}, high_amplitude{};
    VibrationAmplificationType type{VibrationAmplificationType::Exponential};
};
struct MotionStatus {
    float gyro_x{}, gyro_y{}, gyro_z{}, accel_x{}, accel_y{}, accel_z{};
    u64 delta_timestamp{};
};
template<class T> void RegisterInputFactory(const char*, std::shared_ptr<T>) {}
template<class T> void RegisterOutputFactory(const char*, std::shared_ptr<T>) {}
inline void UnregisterInputFactory(const char*) {}
inline void UnregisterOutputFactory(const char*) {}
}
}
struct PadIdentifier { Common::UUID guid{}; std::size_t port{}, pad{}; };
namespace InputCommon {
struct VirtualGamepad {
    enum class VirtualButton {
        ButtonA, ButtonB, ButtonX, ButtonY, StickL, StickR, TriggerL, TriggerR,
        TriggerZL, TriggerZR, ButtonPlus, ButtonMinus, ButtonLeft, ButtonUp,
        ButtonRight, ButtonDown, ButtonSL, ButtonSR, ButtonHome, ButtonCapture
    };
};
class InputEngine {
public:
    explicit InputEngine(std::string) {}
    virtual ~InputEngine() = default;
    virtual Common::Input::DriverResult SetVibration(
        const PadIdentifier&, const Common::Input::VibrationStatus&) = 0;
    virtual bool IsVibrationEnabled(const PadIdentifier&) = 0;
    void PreSetController(const PadIdentifier&) {}
    void SetButton(const PadIdentifier& id, int button, bool value) {
        buttons.at(id.port).at(button) = value;
    }
    bool GetButton(const PadIdentifier& id, int button) const {
        return buttons.at(id.port).at(button);
    }
    void SetAxis(const PadIdentifier& id, int axis, float value) {
        axes.at(id.port).at(axis) = value;
    }
    float GetAxis(const PadIdentifier& id, int axis) const {
        return axes.at(id.port).at(axis);
    }
    void SetMotion(const PadIdentifier& id, int, Common::Input::MotionStatus value) {
        motion.at(id.port) = value;
    }
    Common::Input::MotionStatus GetMotion(const PadIdentifier& id, int) const {
        return motion.at(id.port);
    }
private:
    std::array<std::array<bool, 22>, 10> buttons{};
    std::array<std::array<float, 4>, 10> axes{};
    std::array<Common::Input::MotionStatus, 10> motion{};
};
struct InputFactory { template<class T> explicit InputFactory(T) {} };
struct OutputFactory { template<class T> explicit OutputFactory(T) {} };
}
template<class... Args> void TestLog(const char*, const Args&...) {}
#define LOG_INFO(category, ...) TestLog(__VA_ARGS__)
#define LOG_WARNING(category, ...) TestLog(__VA_ARGS__)
namespace Eden::NativeLogs { inline bool Detailed() { return false; } }
namespace Eden::Performance {
inline unsigned heartbeats = 0;
void ReportInputThreadGpuLiveness() { ++heartbeats; }
}
"""

main = r"""
int main() {
    try {
        CheckPadRecovery();
        CHECK(Eden::Performance::heartbeats > 0);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "PAD_RECOVERY_CHECK_FAILED: %s\n", error.what());
        return 1;
    }
    return 0;
}
"""

compiler = shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler, "C++20 host compiler with ASan/UBSan required (no installation attempted)"
with tempfile.TemporaryDirectory(prefix="encore-pad-recovery-") as tmp:
    folder = Path(tmp)
    source, mocks = folder / "pad.cpp", folder / "mock.cpp"
    mocks.write_text(
        '#include "mock_devices.h"\n#include <algorithm>\n'
        'namespace Eden::Mock { State state; }\nusing Eden::Mock::state;\n'
        'extern "C" {\n' + mock_abi + '\n}\n'
    )

    def build_and_run(name: str, code: str, opt: str, dev: bool = False):
        source.write_text(
            prelude + declarations + code + '\nusing Eden::Mock::state;\n'
            '#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)\n'
            + tests + main
        )
        target = folder / name
        command = [
            compiler, "-std=c++20", opt, "-g", "-pthread", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-parameter", "-Wno-missing-field-initializers",
            "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
            "-I", str(ROOT / "headless"), "-I", str(ROOT / "third_party"),
            str(source), str(mocks), "-o", str(target),
        ]
        if dev:
            command.insert(1, "-DEDEN_DEV_PROFILE=1")
        build = subprocess.run(command, capture_output=True, text=True, timeout=120)
        assert build.returncode == 0, (name, build.stdout, build.stderr)
        return subprocess.run([str(target)], capture_output=True, text=True, timeout=30)

    for dev in (False, True):
        for opt in ("-O0", "-O2"):
            name = ("dev" if dev else "shipping") + opt
            run = build_and_run(name, implementation, opt, dev)
            assert run.returncode == 0, (name, run.stdout, run.stderr)
            assert "handle reuse and cleanup PASS" in run.stdout, (name, run.stdout)
            print(run.stdout.strip(), name, "ASan+UBSan", flush=True)

    # These are actual-code mutants in tmpfs, never changes to the repository.
    mutants = {
        "no-recovery": replace_once(
            implementation, "            if (--slot.polls_to_reopen == 0) ReopenSlot(player);",
            "            --slot.polls_to_reopen;",
        ),
        "no-backoff": replace_once(
            implementation, "slot.polls_to_reopen = kPollsPerScan << std::min(slot.reopens, 5u);",
            "slot.polls_to_reopen = kPollsPerScan;",
        ),
        "unreserved-stale-slot": replace_once(
            implementation, "return slot.handle >= 0 || slot.stale;", "return slot.handle >= 0;",
        ),
        "retained-released-handle": replace_once(
            implementation, "    slot.handle = -1;", "    // Mutant retains a released handle.",
        ),
    }
    for name, code in mutants.items():
        run = build_and_run(name, code, "-O2")
        assert run.returncode == 1 and "PAD_RECOVERY_CHECK_FAILED:" in run.stderr, (
            "negative control did not fail a fixture assertion", name, run.returncode,
            run.stdout, run.stderr,
        )
        print("Negative control rejected:", name, flush=True)

print("PAD_RECOVERY_HOST_PASS actual_pad_and_device_checks=1 negative_controls=4 "
      "vendor_sources_used=0 native_PS5_qualification=0")
