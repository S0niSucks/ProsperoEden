// SPDX-License-Identifier: GPL-3.0-or-later
#include "amiibo_sync.h"
#include <array>
#include <map>
#include <set>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <zlib.h>

namespace Eden {
namespace {
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::string Label(const nlohmann::json& object, const char* key, bool required = true) {
    const auto it = object.find(key);
    if (!required && it == object.end()) return {};
    Require(it != object.end() && it->is_string(), "Invalid Amiibo catalog label");
    auto text = it->get<std::string>();
    Require((!required || !text.empty()) && text.size() <= 128, "Invalid Amiibo catalog label length");
    for (const unsigned char c : text) Require(c >= 32 && c != 127, "Invalid Amiibo catalog label");
    return text;
}
std::uint32_t Big(std::string_view bytes, std::size_t at) {
    return (std::uint32_t(static_cast<unsigned char>(bytes[at])) << 24) |
        (std::uint32_t(static_cast<unsigned char>(bytes[at + 1])) << 16) |
        (std::uint32_t(static_cast<unsigned char>(bytes[at + 2])) << 8) |
        static_cast<unsigned char>(bytes[at + 3]);
}
}
std::string AmiiboSyncId(std::string_view value) {
    if (value.starts_with("0x")) value.remove_prefix(2);
    Require(value.size() == 16, "A model ID must have 16 hexadecimal digits");
    std::string id;
    for (unsigned char c : value) {
        if (c >= 'A' && c <= 'F') c += 'a' - 'A';
        Require((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'), "Invalid Amiibo model ID");
        id += static_cast<char>(c);
    }
    return id;
}
std::vector<AmiiboSyncEntry> ParseAmiiboSyncCatalog(std::string_view text) {
    Require(!text.empty() && text.size() <= 2 * 1024 * 1024, "Amiibo catalog exceeds 2 MiB or is empty");
    std::array<std::set<std::string>, 18> keys;
    std::size_t events = 0;
    const auto data = nlohmann::json::parse(text, [&](int depth, auto event, auto& value) {
        Require(depth <= 16 && ++events <= 65536, "Amiibo catalog nesting/event limit exceeded");
        using Event = nlohmann::json::parse_event_t;
        if (event == Event::object_start) keys[depth + 1].clear();
        if (event == Event::key) Require(keys[depth].insert(value.template get<std::string>()).second,
                                        "Duplicate Amiibo catalog JSON key");
        return true;
    });
    Require(data.is_object(), "Invalid Amiibo catalog root");
    std::map<std::string, AmiiboSyncEntry> unique;
    const auto add = [&](std::string id, const auto& object, std::string series) {
        Require(object.is_object() && unique.size() < 4096, "Amiibo catalog item limit/format is invalid");
        auto name = Label(object, "name");
        Require(unique.emplace(id, AmiiboSyncEntry{id, std::move(name), std::move(series)}).second,
                "Duplicate Amiibo model ID");
    };
    if (const auto table = data.find("amiibos"); table != data.end() && table->is_object()) {
        Require(table->size() <= 4096, "Too many Amiibo catalog entries");
        const auto series = data.find("amiibo_series");
        for (const auto& [key, object] : table->items()) {
            auto id = AmiiboSyncId(key);
            std::string label;
            if (series != data.end() && series->is_object()) {
                const auto name = "0x" + id.substr(12, 2);
                label = Label(*series, name.c_str(), false);
            }
            add(std::move(id), object, std::move(label));
        }
    } else {
        const auto api = data.find("amiibo");
        Require(api != data.end(), "Unsupported Amiibo catalog schema");
        const auto entry = [&](const auto& object) {
            const auto head = Label(object, "head"), tail = Label(object, "tail");
            Require(head.size() == 8 && tail.size() == 8, "Invalid API model ID");
            add(AmiiboSyncId(head + tail), object, Label(object, "amiiboSeries", false));
        };
        if (api->is_array()) {
            Require(api->size() <= 4096, "Too many Amiibo catalog entries");
            for (const auto& object : *api) entry(object);
        } else { Require(api->is_object(), "Invalid Amiibo catalog items"); entry(*api); }
    }
    Require(!unique.empty(), "Amiibo catalog is empty");
    std::vector<AmiiboSyncEntry> result;
    for (auto& [_, entry] : unique) result.push_back(std::move(entry));
    return result;
}

void ValidateAmiiboSyncPng(std::string_view bytes) {
    Require(bytes.size() >= 45 && bytes.size() <= 4 * 1024 * 1024 &&
            bytes.substr(0, 8) == std::string_view("\x89PNG\r\n\x1a\n", 8), "Image is not a bounded PNG");
    bool header = false, ended = false, data = false, data_ended = false, palette = false, transparency = false;
    unsigned color = 0, bits = 0, palette_entries = 0;
    std::string packed;
    std::vector<unsigned> rows;
    std::size_t expected = 0;
    for (std::size_t at = 8; at < bytes.size();) {
        Require(bytes.size() - at >= 12, "Truncated PNG chunk");
        const std::size_t size = Big(bytes, at);
        Require(size <= bytes.size() - at - 12, "Truncated PNG data");
        const auto kind = bytes.substr(at + 4, 4), body = bytes.substr(at + 8, size);
        for (unsigned char c : kind) Require((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'), "Invalid PNG chunk name");
        Require(!(kind[2] & 32), "Invalid PNG reserved bit");
        const auto crc = crc32(0, reinterpret_cast<const Bytef*>(bytes.data() + at + 4), size + 4);
        Require(crc == Big(bytes, at + size + 8), "PNG CRC mismatch");
        Require(header || kind == "IHDR", "PNG must start with IHDR");
        if (kind == "IHDR") {
            Require(!header && size == 13, "Invalid PNG header");
            const auto width = Big(body, 0), height = Big(body, 4);
            bits = static_cast<unsigned char>(body[8]); color = static_cast<unsigned char>(body[9]);
            const unsigned interlace = static_cast<unsigned char>(body[12]);
            const bool depth = color == 0 ? (bits == 1 || bits == 2 || bits == 4 || bits == 8 || bits == 16) :
                color == 3 ? (bits == 1 || bits == 2 || bits == 4 || bits == 8) :
                (color == 2 || color == 4 || color == 6) && (bits == 8 || bits == 16);
            Require(width && width <= 1024 && height && height <= 1024 && depth &&
                    body[10] == 0 && body[11] == 0 && interlace <= 1, "Unsupported PNG dimensions/format");
            const unsigned channels = color == 2 ? 3 : color == 4 ? 2 : color == 6 ? 4 : 1;
            constexpr unsigned passes[7][4]{{0,0,8,8},{4,0,8,8},{0,4,4,8},{2,0,4,4},{0,2,2,4},{1,0,2,2},{0,1,1,2}};
            const auto pass = [&](unsigned x, unsigned y, unsigned dx, unsigned dy) {
                const unsigned w = width > x ? (width - x + dx - 1) / dx : 0;
                const unsigned h = height > y ? (height - y + dy - 1) / dy : 0;
                if (w && h) {
                    const unsigned stride = (w * channels * bits + 7) / 8 + 1;
                    rows.insert(rows.end(), h, stride); expected += std::size_t(stride) * h;
                }
            };
            if (!interlace) pass(0, 0, 1, 1);
            else for (const auto& p : passes) pass(p[0], p[1], p[2], p[3]);
            header = true;
        } else if (kind == "PLTE") {
            Require(!palette && !data && color != 0 && color != 4 && size && size % 3 == 0 && size <= 768,
                    "Invalid PNG palette");
            palette = true; palette_entries = size / 3;
            Require(color != 3 || palette_entries <= (1u << bits), "PNG palette exceeds bit depth");
        } else if (kind == "tRNS") {
            Require(!transparency && !data && color != 4 && color != 6 &&
                    (color != 0 || size == 2) && (color != 2 || size == 6) &&
                    (color != 3 || (palette && size && size <= palette_entries)), "Invalid PNG transparency");
            transparency = true;
        } else if (kind == "IDAT") {
            Require(!data_ended && (color != 3 || palette), "Invalid PNG data order");
            data = true; packed.append(body);
        } else if (kind == "IEND") {
            Require(size == 0 && data && at + 12 == bytes.size(), "Invalid PNG ending"); ended = true;
        } else Require(kind[0] & 32, "Unknown critical PNG chunk");
        if (data && kind != "IDAT") data_ended = true;
        at += size + 12;
    }
    Require(ended && !packed.empty(), "Missing PNG data/ending");
    // Exact IHDR-derived output cap, including Adam7. Worst case < 8.1 MiB, not attacker size.
    std::vector<unsigned char> pixels(expected + 1);
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(packed.data()); stream.avail_in = packed.size();
    stream.next_out = pixels.data(); stream.avail_out = pixels.size();
    Require(inflateInit(&stream) == Z_OK, "Cannot validate PNG compressed data");
    const auto result = inflate(&stream, Z_FINISH);
    const bool valid = result == Z_STREAM_END && stream.total_out == expected && stream.avail_in == 0;
    inflateEnd(&stream);
    Require(valid, "Invalid or oversized PNG compressed data");
    std::size_t offset = 0;
    for (unsigned stride : rows) { Require(pixels[offset] <= 4, "Invalid PNG row filter"); offset += stride; }
}
} // namespace Eden
