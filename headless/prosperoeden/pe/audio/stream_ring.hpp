// ProsperoEden - Lock-free ring of stereo float frames (one producer, one consumer).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <atomic>
#include <cstring>
#include <cstddef>
#include <vector>

namespace pe::audio
{

// The game thread writes decoded music; the audio thread reads it. Neither
// side blocks: a short read means the producer fell behind (silence fills in).
class StreamRing
{
  public:
    explicit StreamRing(std::size_t frames = 1u << 16) : samples_(frames * 2), capacity_(frames)
    {
    }

    std::size_t capacity() const
    {
        return capacity_;
    }
    // Frames buffered and not yet read.
    std::size_t available() const
    {
        return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire);
    }
    std::size_t space() const
    {
        return capacity_ - available();
    }

    // Producer: copies up to frames stereo frames; returns how many fit.
    std::size_t write(const float *stereo, std::size_t frames)
    {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        const std::size_t count = std::min(frames, capacity_ - (head - tail));
        if (!count)
            return 0;
        // Two contiguous copies replace per-frame modulo/address arithmetic.
        // Publishing head remains after both spans, preserving SPSC ordering.
        const std::size_t offset = head % capacity_;
        const std::size_t first = std::min(count, capacity_ - offset);
        std::memcpy(samples_.data() + offset * 2, stereo, first * 2 * sizeof(float));
        if (count != first)
            std::memcpy(samples_.data(), stereo + first * 2,
                        (count - first) * 2 * sizeof(float));
        head_.store(head + count, std::memory_order_release);
        return count;
    }

    // Consumer: copies up to frames stereo frames; returns how many were there.
    std::size_t read(float *stereo, std::size_t frames)
    {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t head = head_.load(std::memory_order_acquire);
        const std::size_t count = std::min(frames, head - tail);
        if (!count)
            return 0;
        const std::size_t offset = tail % capacity_;
        const std::size_t first = std::min(count, capacity_ - offset);
        std::memcpy(stereo, samples_.data() + offset * 2, first * 2 * sizeof(float));
        if (count != first)
            std::memcpy(stereo + first * 2, samples_.data(),
                        (count - first) * 2 * sizeof(float));
        tail_.store(tail + count, std::memory_order_release);
        return count;
    }

    // Consumer side only: drops everything buffered.
    void drain()
    {
        tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release);
    }

  private:
    std::vector<float> samples_;
    std::size_t capacity_;
    std::atomic<std::size_t> head_{0}; // frames written, ever
    std::atomic<std::size_t> tail_{0}; // frames read, ever
};

} // namespace pe::audio
