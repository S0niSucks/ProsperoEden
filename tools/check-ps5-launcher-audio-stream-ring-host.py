#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Source-linked PS5 launcher SPSC audio ring copy/wrap and publication regression."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
header = (root / "headless/prosperoeden/pe/audio/stream_ring.hpp").read_text()
assert header.count("std::memcpy(") == 4
assert "head_.store(head + count, std::memory_order_release)" in header
assert "tail_.store(tail + count, std::memory_order_release)" in header
assert header.count("if (!count)") == 2
fixture = r"""
#include "pe/audio/stream_ring.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <deque>
#include <thread>
#include <vector>

void model(std::size_t cap) {
    pe::audio::StreamRing ring(cap);
    std::deque<float> reference;
    std::uint32_t seed = 0x35432u;
    std::size_t produced = 0;
    for (int iteration = 0; iteration < 4000; ++iteration) {
        seed = seed * 1664525u + 1013904223u;
        const std::size_t n = (seed >> 17) % 71;
        const unsigned mode = (seed >> 8) % 9;
        if (mode <= 4) {
            std::vector<float> input(n * 2);
            for (std::size_t i = 0; i < n; ++i) {
                input[i * 2] = float(produced + i);
                input[i * 2 + 1] = -float(produced + i);
            }
            const std::size_t expected = std::min(n, cap - reference.size() / 2);
            assert(ring.write(input.data(), n) == expected);
            for (std::size_t i = 0; i < expected * 2; ++i) reference.push_back(input[i]);
            produced += expected;
        } else if (mode <= 7) {
            std::vector<float> out(n * 2 + 2, -987654.0f);
            const std::size_t expected = std::min(n, reference.size() / 2);
            assert(ring.read(out.data(), n) == expected);
            for (std::size_t i = 0; i < expected * 2; ++i) {
                assert(out[i] == reference.front());
                reference.pop_front();
            }
            for (std::size_t i = expected * 2; i < out.size(); ++i)
                assert(out[i] == -987654.0f);
        } else {
            ring.drain();
            reference.clear();
        }
        assert(ring.available() == reference.size() / 2);
        assert(ring.space() == cap - reference.size() / 2);
    }
}
void concurrent() {
    pe::audio::StreamRing ring(257);
    constexpr std::size_t total = 100000;
    std::thread producer([&] {
        std::size_t index = 0;
        while (index != total) {
            std::size_t n = std::min<std::size_t>(11, total - index);
            std::vector<float> data(n * 2);
            for (std::size_t i = 0; i < n; ++i) {
                data[2 * i] = float(index + i);
                data[2 * i + 1] = -float(index + i);
            }
            const std::size_t done = ring.write(data.data(), n);
            index += done;
            if (done == 0) std::this_thread::yield();
        }
    });
    std::size_t index = 0;
    while (index != total) {
        float data[26]{};
        const std::size_t done = ring.read(data, 13);
        for (std::size_t i = 0; i < done; ++i) {
            assert(data[i * 2] == float(index + i));
            assert(data[i * 2 + 1] == -float(index + i));
        }
        index += done;
        if (done == 0) std::this_thread::yield();
    }
    producer.join();
    assert(ring.available() == 0 && ring.space() == 257);
}
int main() {
    for (auto cap : {0u, 1u, 3u, 7u, 64u, 257u, 65536u})
        model(cap);
    concurrent();
}
"""
compiler = shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler, "C++20 host compiler required"
with tempfile.TemporaryDirectory(prefix="eden-audio-ring-") as work:
    src = Path(work) / "test.cpp"
    src.write_text(fixture)
    for opt in ("-O0", "-O2"):
        exe = Path(work) / ("ring" + opt[1:])
        subprocess.run([compiler, "-std=c++20", opt, "-Wall", "-Wextra", "-Werror",
                        "-pthread", "-fsanitize=address,undefined",
                        "-fno-sanitize-recover=all",
                        "-I", str(root / "headless/prosperoeden"),
                        str(src), "-o", str(exe)], check=True, timeout=60)
        subprocess.run([str(exe)], check=True, timeout=30)
        print("PS5_LAUNCHER_AUDIO_RING_PASS", opt)
print("Audio copy throughput on PS5 not yet measured")
