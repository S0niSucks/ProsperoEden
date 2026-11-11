// ProsperoEden - Launcher colours, type sizes and motion constants.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "pe/gfx/draw_list.hpp"
#include "launcher_appearance.h"

#include <algorithm>

namespace pe::ui
{

using gfx::Align;
using gfx::Color;
using gfx::Rect;

namespace theme
{

// Eden brand palette: dark neutral surfaces with the official violet/pink/blue accents.
// Updated only by the launcher owner thread, before recording a frame. Worker
// services/decoders never consult these roles. Original retains existing colours.
inline Color kBase = Color::rgb(0x07070d);
inline Color kScrim = Color::rgb(0x04040a);
inline Color kGlass = Color::rgb(0x120f1c);
inline Color kPanel = Color::rgb(0x0b0a12);
inline Color kPanelEdge = Color::rgb(0x8c78ad);
inline Color kPanelEdgeSoft = Color::rgb(0x5b506f);
inline Color kRow = Color::rgb(0x12101a);
inline Color kRowEdge = Color::rgb(0x625875);
inline Color kRowFocus = Color::rgb(0x2b1b42);
inline Color kFocusCore = Color::rgb(0xff70e4);
inline Color kFocusBlue = Color::rgb(0xbb59ff);

// Keep the historic names to avoid touching every widget; values are Eden's official accents.
inline Color kLime = Color::rgb(0xbf42f6);
inline Color kLimeDeep = Color::rgb(0x6547c7);
inline Color kLimePale = Color::rgb(0xe1b7ff);
inline Color kSun = Color::rgb(0xff44c4);
inline Color kBlue = Color::rgb(0x62b4ff);
inline Color kAccentTeal = Color::rgb(0xe0b1ff);

// Text
inline const Color kText = Color::rgb(0xf4f2f8);
inline const Color kTitle = Color::rgb(0xffffff);
inline const Color kValue = Color::rgb(0xf0edf7);
inline const Color kBody = Color::rgb(0xdedbe8);
inline const Color kCopy = Color::rgb(0xc5c1d2);
inline const Color kMeta = Color::rgb(0xa9a5b8);
inline const Color kLabel = Color::rgb(0xb7b2c5);
inline const Color kMuted = Color::rgb(0xa19cad);
inline const Color kFaint = Color::rgb(0x7e798b);
inline const Color kWarning = Color::rgb(0xff9f9f);
inline Color kRule = Color::rgb(0x3f3b51);

inline int palette_id = 0;
inline void ApplyPalette(int requested)
{
    struct Palette {
        std::uint32_t base, panel, glass, edge, focus, accent, deep, pale, sun, blue;
    };
    // Role-based native themes inspired by the UI reference's separation of
    // themes from screen designs. No reference renderer/fonts/assets imported.
    static constexpr Palette palettes[] = {
        {0x07070d,0x0b0a12,0x120f1c,0x8c78ad,0xff70e4,0xbf42f6,0x6547c7,0xe1b7ff,0xff44c4,0x62b4ff},
        {0x070f1c,0x101e31,0x14283d,0x7189a7,0x8bdcff,0x52b8ef,0x235984,0xc4ecff,0x9baaf2,0x63d8ed},
        {0x081611,0x10271d,0x183527,0x7c9e85,0xb5e894,0x8ccc7b,0x345f3c,0xd5edbd,0xeac57c,0x80c9b2},
        {0x111318,0x1c2027,0x252a33,0x8b93a2,0xf0f2f8,0xbec8db,0x4f596b,0xe4e9f1,0xb1bad0,0x9fc3df},
        {0x1b100d,0x2d1e18,0x38271e,0xb18e75,0xffd295,0xe9aa6c,0x865334,0xffe2bc,0xee9e89,0xb1cdd5},
    };
    palette_id = Eden::Appearance::Valid(requested, Eden::Appearance::kPalettes) ? requested : 0;
    const Palette& p = palettes[palette_id];
    kBase = Color::rgb(p.base); kPanel = Color::rgb(p.panel); kGlass = Color::rgb(p.glass);
    kPanelEdge = Color::rgb(p.edge); kFocusCore = Color::rgb(p.focus);
    kLime = Color::rgb(p.accent); kLimeDeep = Color::rgb(p.deep); kLimePale = Color::rgb(p.pale);
    kSun = Color::rgb(p.sun); kBlue = Color::rgb(p.blue);
    kScrim = palette_id ? Color::rgb(p.base) : Color::rgb(0x04040a);
    kPanelEdgeSoft = palette_id ? Color::rgb(p.edge) : Color::rgb(0x5b506f);
    kRow = palette_id ? Color::rgb(p.glass) : Color::rgb(0x12101a);
    kRowEdge = palette_id ? Color::rgb(p.edge) : Color::rgb(0x625875);
    kRowFocus = palette_id ? Color::rgb(p.deep) : Color::rgb(0x2b1b42);
    kFocusBlue = palette_id ? Color::rgb(p.accent) : Color::rgb(0xbb59ff);
    kAccentTeal = palette_id ? Color::rgb(p.pale) : Color::rgb(0xe0b1ff);
    kRule = palette_id ? Color::rgb(p.edge) : Color::rgb(0x3f3b51);
}

// Type sizes (Montserrat Medium)
constexpr float kDisplay = 48.0f;
constexpr float kLead = 40.0f;
constexpr float kBrand = 36.0f;
constexpr float kHeading = 32.0f;
constexpr float kClock = 28.0f;
constexpr float kText24 = 24.0f;
constexpr float kSmall = 20.0f;

// Motion (spring responsiveness in rad/s, durations in seconds)
constexpr float kFocusSpring = 20.0f;
constexpr float kCursorSpring = 26.0f;
constexpr float kScrollSpring = 16.0f;
constexpr float kScreenSeconds = 0.30f;
// Allow one visible face change while keeping game admission bounded.
// Front preview 1.2 s, first turn 1.3 s, BACK remains readable 4.5 s,
// return turn 1.3 s, final settle/fade 0.5 s. A truthful staged cinematic
// transition, NOT a proxy for guest/emulation loading percentage.
constexpr float kCaseFrontHold = 1.20f;
constexpr float kCaseTurnSeconds = 0.72f; // rapid pass through spine; never a pause
constexpr float kCaseBackHold = 4.50f;
constexpr float kCaseSettleSeconds = 0.50f;
constexpr float kLaunchSeconds = kCaseFrontHold + 2.0f * kCaseTurnSeconds +
                                 kCaseBackHold + kCaseSettleSeconds;

} // namespace theme

// How the launcher is shown: Settings > Accessibility.
struct Look
{
    bool large_text = false;    // small text is drawn larger
    bool high_contrast = false; // solid panels, brighter text, an outlined highlight
    bool reduce_motion = false; // nothing drifts, slides or zooms: screens fade
};
inline Look &look()
{
    static Look value;
    return value;
}
// 1 normally, 0 with reduced motion: every slide, zoom and drift is multiplied by it.
inline float motion()
{
    return look().reduce_motion ? 0.0f : 1.0f;
}
// The size text of `size` is drawn at. "Larger text" raises everything under 30 to between 26
// and 30 (26 is the least the console accessibility guidelines ask of text on a 1080p TV).
inline float text_size(float size)
{
    return look().large_text && size < 30.0f ? std::clamp(size * 1.3f, 26.0f, 30.0f) : size;
}

// The baseline that centres text of `size` in a line box of height `line` starting at `top`.
inline float baseline(float top, float line, float size)
{
    return top + line * 0.5f + size * 0.35f;
}

inline bool inside(const Rect &r, float x, float y)
{
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

} // namespace pe::ui
