// SPDX-License-Identifier: GPL-3.0-or-later
// Stable settings identifiers shared by the native provider and launcher.
#pragma once
#include <array>
#include <string_view>

namespace Eden::Appearance {
inline constexpr std::array<const char*, 3> kInterfaces{"encore", "switch", "collection"};
inline constexpr std::array<const char*, 3> kInterfaceNames{"Encore", "Switch", "Collection"};
inline constexpr std::array<const char*, 5> kPalettes{"original", "midnight", "canopy", "slate", "ember"};
inline constexpr std::array<const char*, 5> kPaletteNames{"Original", "Midnight", "Canopy", "Slate", "Ember"};
inline constexpr std::array<const char*, 2> kLayouts{"grid", "list"};
inline constexpr std::array<const char*, 2> kLayoutNames{"Grid", "List"};

template <std::size_t N>
inline int Index(std::string_view value, const std::array<const char*, N>& keys) {
    for (std::size_t i = 0; i < N; ++i)
        if (value == keys[i]) return static_cast<int>(i);
    return 0;
}
template <std::size_t N>
inline bool Valid(int value, const std::array<const char*, N>&) {
    return value >= 0 && value < static_cast<int>(N);
}
template <std::size_t N>
inline const char* Name(int value, const std::array<const char*, N>& names) {
    return names[Valid(value, names) ? static_cast<std::size_t>(value) : 0];
}

// Row/column navigation for a paged collection. Never wrap a partial last row
// into a different column or generate an index for an empty library.
inline int Move(int selected, int count, int columns, int dx, int dy) {
    if (count <= 0 || columns <= 0) return 0;
    if (selected < 0 || selected >= count) selected = 0;
    const int column = selected % columns;
    if (dx < 0 && column > 0) return selected - 1;
    if (dx > 0 && column + 1 < columns && selected + 1 < count) return selected + 1;
    if (dy < 0 && selected >= columns) return selected - columns;
    if (dy > 0 && selected + columns < count) return selected + columns;
    return selected;
}
} // namespace Eden::Appearance
