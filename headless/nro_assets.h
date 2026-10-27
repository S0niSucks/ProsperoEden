// SPDX-License-Identifier: GPL-3.0-or-later
// Bounds for an NRO's optional ASET metadata, before allocating or parsing its contents.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Eden::NroAssets {
struct Section {
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};
struct Layout {
    bool valid = false;
    Section icon, nacp, romfs;
};
inline std::uint64_t Little(const unsigned char* bytes, unsigned count) {
    std::uint64_t value = 0;
    for (unsigned n = 0; n < count; ++n) value |= std::uint64_t{bytes[n]} << (8 * n);
    return value;
}
// read(buffer, count, offset) returns the number of bytes actually read. No file-sized allocation.
template <typename Reader>
Layout Read(std::uint64_t file_size, Reader read) {
    std::array<unsigned char, 0x80> header{};
    if (file_size < header.size() || read(header.data(), header.size(), 0) != header.size() ||
        std::memcmp(header.data() + 0x10, "NRO0", 4) != 0) return {};
    const auto image = Little(header.data() + 0x18, 4);
    if (image < header.size() || image > file_size || file_size - image < 0x38) return {};
    std::array<unsigned char, 0x38> asset{};
    if (read(asset.data(), asset.size(), image) != asset.size() ||
        std::memcmp(asset.data(), "ASET", 4) != 0 || Little(asset.data() + 4, 4) != 0) return {};
    Layout result;
    Section* sections[] = {&result.icon, &result.nacp, &result.romfs};
    for (unsigned n = 0; n < 3; ++n) {
        const auto offset = Little(asset.data() + 8 + n * 16, 8);
        const auto size = Little(asset.data() + 16 + n * 16, 8);
        if (!size) continue;
        if (offset < asset.size() || offset > file_size - image || size > file_size - image - offset)
            return {};
        *sections[n] = {image + offset, size};
    }
    // Icons are small JPEG/PNG images. NACP v0 is a fixed-size record; RomFS stays file-backed.
    if (result.icon.size > 1024 * 1024 || (result.nacp.size && result.nacp.size != 0x4000)) return {};
    for (unsigned a = 0; a < 3; ++a)
        for (unsigned b = a + 1; b < 3; ++b)
            if (sections[a]->size && sections[b]->size &&
                sections[a]->offset < sections[b]->offset + sections[b]->size &&
                sections[b]->offset < sections[a]->offset + sections[a]->size) return {};
    result.valid = true;
    return result;
}
} // namespace Eden::NroAssets
