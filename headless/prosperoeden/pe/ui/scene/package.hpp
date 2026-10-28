// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pe/ui/scene/model.hpp"
#include "pe/ui/runtime_filesystem.hpp"
#include "pe/gfx/image.hpp"
#include "pe/gfx/font_format.hpp"
#include "pe/audio/wav.hpp"
#include <memory>
#include <cstring>

namespace pe::ui::scene {
// Root and every path component stay pinned by descriptors. No check-then-fopen resource reads.
class Reader {
public:
    explicit Reader(const std::string& root) : root_(root, runtime_fs::Kind::directory) {}
    std::string read(const std::string& name, std::size_t limit) {
        require(runtime_fs::relative_path(name), "unsafe resource path");
        // A fresh descriptor to the same pinned inode; no pathname fallback or native libc dup.
        int fd = openat(root_.get(), ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); require(fd >= 0, "cannot pin package directory");
        for (std::size_t at = 0; at < name.size();) {
            const auto end = name.find('/', at); const auto component = name.substr(at, end == name.npos ? name.size() - at : end - at);
            const auto next = openat(fd, component.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | (end == name.npos ? 0 : O_DIRECTORY));
            close(fd); fd = next; require(fd >= 0, "missing resource or symbolic link: " + name);
            at = end == name.npos ? name.size() : end + 1;
        }
        struct Close { int fd; ~Close() { close(fd); } } close_file{fd};
        struct stat st{};
        require(fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size >= 0 && static_cast<std::uint64_t>(st.st_size) <= limit, "invalid/oversized resource: " + name);
        const auto size = static_cast<std::size_t>(st.st_size);
        require(total_ + size <= 128 * 1024 * 1024 && ++files_ <= 256, "package resource budget exceeded"); total_ += size;
        std::string result(size, '\0'); std::size_t at = 0;
        while (at < size) {
            const auto n = ::read(fd, result.data() + at, size - at);
            if (n < 0 && errno == EINTR) continue;
            require(n > 0, "short resource read"); at += static_cast<std::size_t>(n);
        }
        return result;
    }
    Json json(const std::string& path) {
        const auto bytes = read(path, 512 * 1024);
        // nlohmann's DOM parser is recursive; refuse excessive depth before constructing the DOM.
        int depth = 0; bool quoted = false, escaped = false;
        for (char c : bytes) {
            if (quoted) { if (escaped) escaped = false; else if (c == '\\') escaped = true; else if (c == '"') quoted = false; }
            else if (c == '"') quoted = true;
            else if (c == '[' || c == '{') require(++depth <= 32, "JSON depth exceeded");
            else if (c == ']' || c == '}') --depth;
        }
        // Duplicate members are ambiguous across authoring tools: refuse them, do not last-win.
        std::vector<std::set<std::string>> keys;
        return Json::parse(bytes, [&](int, Json::parse_event_t event, Json& parsed) {
            if (event == Json::parse_event_t::object_start) keys.emplace_back();
            if (event == Json::parse_event_t::key) require(keys.back().insert(parsed.get<std::string>()).second, "duplicate JSON key");
            if (event == Json::parse_event_t::object_end) keys.pop_back();
            return true;
        });
    }
private:
    runtime_fs::Descriptor root_;
    std::size_t total_{}, files_{};
};
inline gfx::Image tga(const std::string& bytes, std::size_t remaining) {
    require(bytes.size() >= 18, "truncated TGA");
    const auto* p = reinterpret_cast<const unsigned char*>(bytes.data());
    const int width = p[12] | (p[13] << 8), height = p[14] | (p[15] << 8), pixel = p[16] / 8;
    require(p[1] == 0 && p[2] == 2 && (p[16] == 24 || p[16] == 32) && (p[17] & 0xd0) == 0 && width > 0 && height > 0 && width <= 4096 && height <= 4096, "unsupported TGA");
    const auto count = static_cast<std::size_t>(width) * height;
    require(count <= remaining / 4 && bytes.size() == 18 + p[0] + count * pixel, "TGA size/resource budget exceeded");
    gfx::Image out; out.width = width; out.height = height; out.rgba.resize(count * 4);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const int source_y = p[17] & 32 ? y : height - 1 - y;
        const auto* in = p + 18 + p[0] + (static_cast<std::size_t>(source_y) * width + x) * pixel;
        auto* dst = out.rgba.data() + (static_cast<std::size_t>(y) * width + x) * 4;
        dst[0] = in[2]; dst[1] = in[1]; dst[2] = in[0]; dst[3] = pixel == 4 ? in[3] : 255;
    }
    return out;
}
struct Package {
    Model model;
    Json description;
    std::map<std::string, gfx::Image> images;
    std::map<std::string, std::unique_ptr<audio::DecodedWav>> audio;
    std::string font;
};
inline void validate_font(std::string_view bytes) {
    namespace ff = gfx::font_format;
    require(bytes.size() >= sizeof(ff::Header), "truncated font");
    ff::Header h{}; std::memcpy(&h, bytes.data(), sizeof(h));
    require(h.magic == ff::kMagic && h.version == ff::kVersion && h.atlas_width > 0 && h.atlas_width <= 4096 && h.atlas_height > 0 && h.atlas_height <= 4096 && h.glyph_count > 0 && h.glyph_count <= 65536 && h.kern_count <= 262144, "font resource budget exceeded");
    for (float metric : {h.pixel_size, h.sdf_range, h.ascent, h.descent, h.line_gap}) require(std::isfinite(metric) && std::abs(metric) <= 1024, "invalid font metric");
    require(h.pixel_size >= 1 && h.sdf_range > 0 && h.ascent > h.descent, "invalid font dimensions");
    const auto glyph_bytes = static_cast<std::size_t>(h.glyph_count) * sizeof(ff::Glyph);
    const auto kern_bytes = static_cast<std::size_t>(h.kern_count) * sizeof(ff::Kern);
    require(bytes.size() == sizeof(h) + glyph_bytes + kern_bytes + static_cast<std::size_t>(h.atlas_width) * h.atlas_height, "font size mismatch");
    std::uint32_t previous = 0;
    for (std::size_t i = 0; i < h.glyph_count; ++i) {
        ff::Glyph g{}; std::memcpy(&g, bytes.data() + sizeof(h) + i * sizeof(g), sizeof(g));
        require((i == 0 || g.codepoint > previous) && g.codepoint <= 0x10ffff && g.x + g.w <= h.atlas_width && g.y + g.h <= h.atlas_height, "invalid font glyph"); previous = g.codepoint;
        for (float metric : {g.offset_x, g.offset_y, g.advance}) require(std::isfinite(metric) && std::abs(metric) <= 4096, "invalid glyph metric");
    }
    std::pair<std::uint32_t, std::uint32_t> previous_kern{};
    for (std::size_t i = 0; i < h.kern_count; ++i) {
        ff::Kern k{}; std::memcpy(&k, bytes.data() + sizeof(h) + glyph_bytes + i * sizeof(k), sizeof(k));
        const auto key = std::make_pair(k.first, k.second);
        require((i == 0 || key > previous_kern) && std::isfinite(k.amount) && std::abs(k.amount) <= 1024, "invalid font kerning"); previous_kern = key;
    }
}
inline audio::DecodedWav wav(std::string_view bytes, std::size_t remaining) {
    const auto u16 = [&](std::size_t at) { return std::uint32_t(static_cast<unsigned char>(bytes[at])) | (std::uint32_t(static_cast<unsigned char>(bytes[at + 1])) << 8); };
    const auto u32 = [&](std::size_t at) { return u16(at) | (u16(at + 2) << 16); };
    require(bytes.size() >= 12 && bytes.substr(0,4) == "RIFF" && bytes.substr(8,4) == "WAVE" && std::uint64_t(u32(4)) + 8 == bytes.size(), "invalid RIFF/WAV extent");
    std::uint32_t channels = 0, bits = 0, block = 0, data = 0; bool format = false, samples = false;
    for (std::size_t at = 12; at < bytes.size();) {
        require(bytes.size() - at >= 8, "truncated WAV chunk"); const auto size = u32(at + 4); const auto start = at + 8;
        require(size <= bytes.size() - start && size + (size & 1ULL) <= bytes.size() - start, "WAV chunk outside file");
        if (bytes.substr(at,4) == "fmt ") {
            require(!format && size >= 16 && u16(start) == 1, "only PCM WAV is supported"); format = true;
            channels = u16(start + 2); bits = u16(start + 14); block = u16(start + 12);
            require((channels == 1 || channels == 2) && (bits == 16 || bits == 24) && u32(start + 4) == 48000 && block == channels * (bits / 8) && u32(start + 8) == 48000 * block, "unsupported PCM WAV format");
        } else if (bytes.substr(at,4) == "data") {
            require(!samples && size > 0, "invalid/duplicate WAV data"); samples = true; data = size;
        }
        at = start + size + (size & 1);
    }
    require(format && samples && block > 0 && data % block == 0 && std::uint64_t(data / block) * 8 <= remaining, "decoded WAV budget exceeded");
    auto result = audio::decode_wav(bytes); require(result.ok(), "invalid WAV: " + result.error); return result;
}
inline Package load(const std::string& folder, const std::string& path = "theme.json") {
    Reader reader(folder); Package package; auto& j = package.description; j = reader.json(path);
    Json screens = Json::object(), animations = Json::object();
    require(j.at("screens").is_object() && j.at("screens").size() <= 16, "invalid screen table");
    for (auto it = j.at("screens").begin(); it != j.at("screens").end(); ++it) screens[it.key()] = reader.json(it.value().get<std::string>());
    const auto table = j.value("animations", Json::object()); require(table.is_object() && table.size() <= 32, "invalid animation table");
    for (auto it = table.begin(); it != table.end(); ++it) animations[it.key()] = reader.json(it.value().get<std::string>());
    package.model = parse(j, screens, animations);
    const auto images = j.value("images", Json::object()); std::size_t decoded = 0;
    for (auto it = images.begin(); it != images.end(); ++it) {
        require(identifier(it.key()), "invalid image ID");
        auto image = tga(reader.read(it.value().get<std::string>(), 64 * 1024 * 1024), 96 * 1024 * 1024 - decoded);
        decoded += image.rgba.size(); package.images.emplace(it.key(), std::move(image));
    }
    const auto audio = j.value("audio", Json::object()); require(audio.is_object() && audio.size() <= 32, "invalid audio table");
    std::size_t pcm = 0;
    for (auto it = audio.begin(); it != audio.end(); ++it) {
        require(identifier(it.key()), "invalid audio ID");
        auto recording = std::make_unique<audio::DecodedWav>(wav(reader.read(it.value().get<std::string>(), 8 * 1024 * 1024), 32 * 1024 * 1024 - pcm));
        pcm += recording->samples.size() * sizeof(float);
        package.audio.emplace(it.key(), std::move(recording));
    }
    const auto font = text(j, "font");
    if (!font.empty()) {
        package.font = reader.read(font, 16 * 1024 * 1024); validate_font(package.font);
    }
    return package;
}
} // namespace pe::ui::scene
