#!/usr/bin/env python3
"""Compile the actual PS5 AudioOut diagnostic PCM scan outside its stream mutex.

Only the worker-owned output block is read during this interval. Validate
failed-output accounting, channel OR semantics and released producer lock.
This is a host concurrency/source witness, not an AudioOut device benchmark.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
audio = (ROOT / "headless/audio.cpp").read_text()
begin = "                lock.unlock();\n                const int result = sceAudioOutOutput(handle, block.data());"
stop = "                lock.lock();\n                in_flight = false;"
assert audio.count(begin) == 1
start = audio.index(begin)
end = audio.index(stop, start)
scan = audio[start:end]
assert "u64 nonzero_in_block = 0;" in scan
assert "if (result >= 0)" in scan
assert "for (std::size_t frame = 0; frame < kAudioOutGrain; ++frame)" in scan
assert "nonzero_in_block += block[frame * 2] != 0 ||" in scan
assert "block[frame * 2 + 1] != 0;" in scan
tail = audio[end:audio.index("            }\n        });", end)]
assert tail.index("if (result < 0) {") < tail.index("output_frames += kAudioOutGrain;")
assert tail.index("output_frames += kAudioOutGrain;") < tail.index("nonzero_frames += nonzero_in_block;")
assert "nonzero_frames += block[" not in tail
# Check the scan is strictly outside the production mutex, including error.
scan = scan.replace(
    "                if (result >= 0) {",
    "                assert(!lock.owns_lock());\n                if (result >= 0) {",
    1,
)
compiler = shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler, "C++20 compiler required"
model = r"""
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
using s16 = std::int16_t;
using u64 = std::uint64_t;
constexpr std::size_t kAudioOutGrain = 256;
std::mutex* active_mutex{};
int output_status{};
int sceAudioOutOutput(int handle, const s16* block) {
    assert(handle == 5 && block != nullptr && active_mutex);
    assert(active_mutex->try_lock());  // producer can enter during output
    active_mutex->unlock();
    return output_status;
}
u64 CountOutput(std::array<s16, kAudioOutGrain * 2>& block, int status) {
    std::mutex mutex;
    std::unique_lock lock(mutex);
    active_mutex = &mutex;
    output_status = status;
    const int handle = 5;
__EXTRACTED_SCAN__
    lock.lock();
    assert(lock.owns_lock());
    active_mutex = nullptr;
    return result < 0 ? 0 : nonzero_in_block;
}
int main() {
    std::array<s16, kAudioOutGrain * 2> samples{};
    assert(CountOutput(samples, 0) == 0);
    samples[0] = 1; samples[1] = 2;  // stereo pair counts once
    samples[5] = -1;                 // right-only channel
    samples[(kAudioOutGrain - 1) * 2] = 3;
    assert(CountOutput(samples, 0) == 3);
    assert(CountOutput(samples, -1) == 0); // failed output not counted
    samples.fill(1);
    assert(CountOutput(samples, 0) == kAudioOutGrain);
    std::printf("PS5_AUDIO_OUTPUT_LOCK_SCOPE_PASS pcm_frames=256 stereo=1 failure=1 unlocked=1\\n");
}
""".replace("__EXTRACTED_SCAN__", scan)
with tempfile.TemporaryDirectory(prefix="eden-audio-lock-") as tmp:
    file = Path(tmp) / "audio.cpp"
    file.write_text(model)
    for opt in ("0", "2", "3"):
        exe = Path(tmp) / ("audio-O" + opt)
        subprocess.run(
            [compiler, "-std=c++20", "-O" + opt, "-Wall", "-Wextra",
             "-Werror", "-pthread", "-fsanitize=address,undefined",
             "-fno-sanitize-recover=all", str(file), "-o", str(exe)],
            check=True, timeout=90,
        )
        subprocess.run([str(exe)], check=True, timeout=90)
print("PS5_AUDIO_OUTPUT_LOCK_SCOPE_HOST_PASS O0=1 O2=1 O3=1 sanitizer=asan_ubsan")
