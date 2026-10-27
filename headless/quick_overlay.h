// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <mutex>
#include <string_view>
#include "quick_picture.h"

namespace Eden::QuickOverlay {
// The existing in-game chip has 111 baked glyphs on both renderers. Its input producer and
// renderer exchange a complete copy; no UI/toolkit work is performed on the guest GPU thread.
inline std::mutex mutex;
inline std::array<char, 112> line{};
inline std::shared_ptr<const QuickPicture> picture;
inline std::atomic<bool> visible{false};
inline void Show(std::string_view text, std::shared_ptr<const QuickPicture> image = {}) {
    const std::lock_guard lock(mutex);
    line.fill(0);
    for (size_t n = 0; n < text.size() && n + 1 < line.size(); ++n) {
        unsigned char ch = text[n];
        // This baked HUD font has uppercase Latin, numbers and punctuation, not lowercase.
        if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
        line[n] = (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == ' ' || ch == '.' || ch == '/' || ch == '-' || ch == '%' ? static_cast<char>(ch) : '-';
    }
    picture = std::move(image);
    visible.store(true, std::memory_order_release);
}
inline void Hide() {
    const std::lock_guard lock(mutex);
    visible.store(false, std::memory_order_release);
    picture.reset();
}
struct Snapshot {
    std::array<char, 112> text{};
    std::shared_ptr<const QuickPicture> image;
};
inline Snapshot Read() {
    if (!visible.load(std::memory_order_acquire)) return {};
    const std::lock_guard lock(mutex);
    return visible.load(std::memory_order_acquire) ? Snapshot{line, picture} : Snapshot{};
}
inline std::array<char, 112> Text() {
    return Read().text;
}
} // namespace Eden::QuickOverlay
