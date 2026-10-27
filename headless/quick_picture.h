// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace Eden {
// Immutable, letterboxed RGBA pixels shared by input and presentation; no decoder on a GPU thread.
struct QuickPicture {
    static constexpr unsigned kSide = 256;
    static constexpr unsigned kBox = 224, kPad = 12, kGap = 8; // reference rows at 1080p
    std::array<uint8_t, kSide * kSide * 4> rgba{};
};
std::shared_ptr<const QuickPicture> LoadQuickPicture(const std::string& path);
} // namespace Eden
