// SPDX-License-Identifier: GPL-3.0-or-later
#include "cheat_catalog.h"
#include "cheats.h"
#include <algorithm>
#include <cstdio>
#include <exception>
#include <set>
#include <span>
#include <utility>
#include <nlohmann/json.hpp>

namespace Eden {
namespace {
constexpr size_t kCatalogBytes = 16 * 1024 * 1024, kOutputBytes = 1024 * 1024;
constexpr size_t kEntries = 512, kWords = 0x100, kNameBytes = 0x3f;

std::string Hex(std::span<const uint8_t> bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (auto byte : bytes) { result += digits[byte >> 4]; result += digits[byte & 15]; }
    return result;
}
std::string HexKey(std::string_view key, size_t length) {
    if (key.size() != length) return {};
    std::string result;
    for (char c : key) {
        if (c >= 'A' && c <= 'F') c += 'a' - 'A';
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return {};
        result += c;
    }
    return result;
}
bool DecodeEntry(std::string_view title, std::string_view source, CatalogCheat& out) {
    if (title.size() < 3 || title.size() > kNameBytes + 2) return false;
    out.master = title.front() == '{' && title.back() == '}';
    if (!out.master && !(title.front() == '[' && title.back() == ']')) return false;
    title.remove_prefix(1); title.remove_suffix(1);
    for (const unsigned char c : title) if (c < 32 || c == 127 || c == '[' || c == ']' || c == '{' || c == '}') return false;
    while (!title.empty() && title.front() == ' ') title.remove_prefix(1);
    while (!title.empty() && title.back() == ' ') title.remove_suffix(1);
    if (title.empty()) return false;
    out.name = title;
    // A downloaded source cannot insert another header, comment or unsupported text. Opcode
    // semantics are still the real VM's responsibility; syntax alone does not make a cheat safe.
    size_t position = 0, words = 0;
    while (position < source.size()) {
        const auto end = source.find('\n', position);
        auto line = source.substr(position, end == source.npos ? source.size() - position : end - position);
        position = end == source.npos ? source.size() : end + 1;
        bool first = true;
        while (!line.empty()) {
            while (!line.empty() && (line.front() == ' ' || line.front() == '\t' || line.front() == '\r')) line.remove_prefix(1);
            if (line.empty()) break;
            const auto space = line.find_first_of(" \t\r");
            const auto token = line.substr(0, space);
            const auto word = HexKey(token, 8);
            if (word.empty() || ++words > kWords) return false;
            if (!first) out.code += ' ';
            first = false;
            out.code += word;
            line.remove_prefix(space == line.npos ? line.size() : space);
        }
        if (!first) out.code += '\n';
    }
    return words != 0;
}

// SAX avoids building a 16-MiB database DOM on the console: retain only one exact build's
// bounded entries. Only other build labels for this same title are retained as informational
// mismatch diagnostics, not their entries. No unrelated title's data is retained.
class Reader final : public nlohmann::json_sax<nlohmann::json> {
    enum class Place { Root, Title, Build, Alternate, Entry, Other };
    struct Frame { Place place; bool object; std::string key; };
public:
    Reader(std::string title_, std::string build_) : title(std::move(title_)), build(std::move(build_)) {}
    bool null() override { return Scalar(false); }
    bool boolean(bool) override { return Scalar(false); }
    bool number_integer(number_integer_t value) override {
        if (value >= 0) Version(std::to_string(value));
        return Scalar(value >= 0);
    }
    bool number_unsigned(number_unsigned_t value) override { Version(std::to_string(value)); return Scalar(true); }
    bool number_float(number_float_t, const string_t&) override { return Scalar(false); }
    bool binary(binary_t&) override { return false; }
    bool string(string_t& value) override {
        if (!Tick() || frames.empty() || value.size() > kOutputBytes) return false;
        if (frames.back().place == Place::Entry) {
            if (frames.back().key == "title") entry_title = value;
            else if (frames.back().key == "source") entry_source = value;
            else return false;
            return true;
        }
        return AllowedScalar(false);
    }
    bool start_object(std::size_t) override { return Start(true); }
    bool start_array(std::size_t) override { return Start(false); }
    bool key(string_t& value) override {
        if (!Tick() || frames.empty() || value.size() > 128) return false;
        auto& frame = frames.back(); frame.key = value;
        if (frame.place == Place::Root && HexKey(value, 16) == title) return ++titles == 1;
        if (frame.place == Place::Title && HexKey(value, 16) == build) return ++builds == 1;
        if (frame.place == Place::Build) {
            if (value != "version") {
                frame.key = HexKey(value, 64);
                if (frame.key.empty() || entries.size() >= kEntries) return false;
            }
            return row_keys.insert(frame.key).second;
        }
        if (frame.place == Place::Entry) return (value == "title" || value == "source") && fields.insert(value).second;
        if (frame.place == Place::Alternate && value == "version") return ++alternate_versions == 1;
        return true;
    }
    bool end_object() override { return End(true); }
    bool end_array() override { return End(false); }
    bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception&) override { return false; }
    CheatCatalog Take() {
        if (!complete || !frames.empty()) return {};
        if (!titles || !builds || entries.empty()) return {CheatCatalogStatus::NoMatch, {}, std::move(other_builds), other_builds_truncated};
        return {CheatCatalogStatus::Ready, std::move(entries), std::move(other_builds), other_builds_truncated};
    }
private:
    bool Tick() { return ++events <= 1000000; }
    void Version(const std::string& text) {
        if (!frames.empty() && frames.back().place == Place::Alternate && frames.back().key == "version")
            alternate_version = text;
    }
    bool AllowedScalar(bool integer) const {
        if (frames.empty()) return false;
        const auto& f = frames.back();
        switch (f.place) {
        case Place::Root: return HexKey(f.key, 16) != title;
        case Place::Title: return HexKey(f.key, 16) != build;
        case Place::Build: return f.key == "version" && integer;
        case Place::Alternate: return true;
        case Place::Other: return true;
        default: return false;
        }
    }
    bool Scalar(bool integer) { return Tick() && AllowedScalar(integer); }
    bool Start(bool object) {
        if (!Tick() || frames.size() >= 16 || complete) return false;
        Place place = Place::Other;
        if (frames.empty()) {
            if (!object) return false;
            place = Place::Root;
        } else {
            const auto& f = frames.back();
            if (f.place == Place::Root && HexKey(f.key, 16) == title) place = Place::Title;
            if (f.place == Place::Title && HexKey(f.key, 16) == build) place = Place::Build;
            if (f.place == Place::Title && HexKey(f.key, 16) != build && !HexKey(f.key, 16).empty() && object) {
                if (other_builds.size() < 32) {
                    alternate_build = HexKey(f.key, 16);
                    if (!alternate_keys.insert(alternate_build).second) return false;
                    alternate_version.clear(); alternate_versions = 0;
                    place = Place::Alternate;
                } else other_builds_truncated = true;
            }
            if (f.place == Place::Build) {
                if (f.key == "version") return false;
                place = Place::Entry; fields.clear(); entry_title.clear(); entry_source.clear();
            }
            if (f.place == Place::Entry) return false;
            if (!object && place != Place::Other) return false;
        }
        frames.push_back({place, object, {}});
        return true;
    }
    bool End(bool object) {
        if (!Tick() || frames.empty() || frames.back().object != object) return false;
        if (frames.back().place == Place::Entry) {
            CatalogCheat entry;
            if (fields.size() != 2 || !DecodeEntry(entry_title, entry_source, entry) ||
                !names.insert(Cheats::Key("", entry.name)).second || (entry.master && ++masters > 1)) return false;
            output_bytes += entry.name.size() + entry.code.size() + 4;
            if (output_bytes > kOutputBytes || entries.size() >= kEntries) return false;
            entries.push_back(std::move(entry));
        }
        if (frames.back().place == Place::Root) complete = true;
        if (frames.back().place == Place::Alternate)
            other_builds.push_back(alternate_build + (alternate_version.empty() ? "" : " / v" + alternate_version));
        frames.pop_back();
        return true;
    }
    std::string title, build, entry_title, entry_source;
    std::vector<Frame> frames;
    std::set<std::string> row_keys, fields, names;
    std::vector<CatalogCheat> entries;
    std::vector<std::string> other_builds;
    std::set<std::string> alternate_keys;
    std::string alternate_build, alternate_version;
    size_t alternate_versions{};
    bool other_builds_truncated{};
    size_t events{}, output_bytes{}, titles{}, builds{}, masters{};
    bool complete{};
};
} // namespace

CheatCatalog ReadCheatCatalog(std::string_view text, uint64_t program_id,
                             const std::array<uint8_t, 32>& main_build_id) try {
    if (!program_id || text.empty() || text.size() > kCatalogBytes ||
        std::all_of(main_build_id.begin(), main_build_id.begin() + 8, [](auto byte) { return byte == 0; })) return {};
    char title[17]{}; std::snprintf(title, sizeof(title), "%016llx", static_cast<unsigned long long>(program_id));
    Reader reader(title, Hex(std::span{main_build_id}.first<8>()));
    if (!nlohmann::json::sax_parse(text, &reader)) return {};
    return reader.Take();
} catch (const std::exception&) { return {}; }

std::string CheatCatalog::Text() const {
    if (status != CheatCatalogStatus::Ready) return {};
    std::string result;
    // The core gives a master ID zero: keep it ahead of the optional entries.
    for (bool master : {true, false}) for (const auto& entry : entries) if (entry.master == master) {
        result += master ? '{' : '['; result += entry.name; result += master ? "}\n" : "]\n";
        result += entry.code; result += '\n';
    }
    return result;
}
} // namespace Eden
