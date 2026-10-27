// ProsperoEden - Bounded Cover Flow navigation and perspective layout.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "pe/gfx/draw_list.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace pe::ui::cover_flow
{
inline constexpr int kRadius = 4; // no more than nine covers, regardless of library size

struct Motion
{
    float position = 0.0f;
    void snap(int selected) { position = static_cast<float>(std::max(0, selected)); }
    void update(float dt, int selected, int count, bool reduced)
    {
        const float target = static_cast<float>(std::clamp(selected, 0, std::max(0, count - 1)));
        if (reduced || count <= 1 || !std::isfinite(position))
            position = target;
        else
        {
            // A rapid burst cannot leave an unbounded animation queue behind the controller.
            position = std::clamp(position, target - 2.0f, target + 2.0f);
            position += (target - position) * (1.0f - std::exp(-16.0f * std::clamp(dt, 0.0f, 0.1f)));
            const float epsilon = std::max(0.001f, target * std::numeric_limits<float>::epsilon() * 2.0f);
            if (std::fabs(target - position) <= epsilon) position = target;
        }
    }
};

struct Card
{
    gfx::PerspectiveQuad quad;
    gfx::Rect bounds;
    float depth = 0.0f;
};

inline Card project(float offset, const gfx::Rect &stage)
{
    const float size = std::min(stage.w * 0.34f, stage.h * 0.52f);
    const float distance = std::fabs(offset);
    const float turn = std::min(distance, 1.0f);
    const float angle = -std::copysign(0.98f * turn, offset);
    const float depth = distance * size * 0.48f;
    const float focal = size * 2.7f;
    const float spread = size * (0.80f * std::min(distance, 1.0f) + 0.38f * std::max(0.0f, distance - 1.0f));
    const float center = std::copysign(spread, offset) * (1.0f + depth / focal);
    const float cx = stage.x + stage.w * 0.5f;
    const float cy = stage.y + stage.h * 0.43f;
    const auto edge = [&](float x) {
        const float w = (focal + depth + std::sin(angle) * x) / focal;
        return std::array<float, 4>{cx + (center + std::cos(angle) * x) / w,
                                   cy - size * 0.5f / w, cy + size * 0.5f / w, w};
    };
    const auto left = edge(-size * 0.5f), right = edge(size * 0.5f);
    const float top = std::min(left[1], right[1]);
    const float bottom = std::max(left[2], right[2]);
    return {{left[0], left[1], left[2], right[0], right[1], right[2], left[3], right[3]},
            {left[0], top, right[0] - left[0], bottom - top}, depth};
}

// Painter order is depth order; the opaque central cover occludes its neighbours.
inline std::array<int, 2 * kRadius + 1> order(int count, float position, int &visible)
{
    std::array<int, 2 * kRadius + 1> out{};
    visible = 0;
    const int center = std::clamp(static_cast<int>(std::round(position)), 0, std::max(0, count - 1));
    for (int i = std::max(0, center - kRadius); i < count && i <= center + kRadius; ++i)
        out[static_cast<std::size_t>(visible++)] = i;
    std::sort(out.begin(), out.begin() + visible, [position](int a, int b) {
        return std::fabs(static_cast<float>(a) - position) > std::fabs(static_cast<float>(b) - position);
    });
    return out;
}
} // namespace pe::ui::cover_flow
