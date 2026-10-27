// SPDX-License-Identifier: GPL-3.0-or-later
#include "amiibo_catalog.h"
#include "file_amiibo.h"
#include "core/hle/service/nfc/common/amiibo_crypto.h"
#include <array>
#include <cstring>
#include <set>
#include <stdexcept>
#include <vector>
#include <nlohmann/json.hpp>

namespace Eden {
namespace {
constexpr size_t kCatalogBytes = 2 * 1024 * 1024;
constexpr size_t kCatalogEntries = 4096;

std::string NormalId(std::string_view id) {
    if (id.starts_with("0x")) id.remove_prefix(2);
    if (id.size() != 16) return {};
    std::string result;
    for (const unsigned char c : id) {
        if (c >= '0' && c <= '9') result += static_cast<char>(c);
        else if (c >= 'a' && c <= 'f') result += static_cast<char>(c);
        else if (c >= 'A' && c <= 'F') result += static_cast<char>(c - 'A' + 'a');
        else return {};
    }
    return result;
}

std::string Text(const nlohmann::json& object, const char* key) {
    const auto field = object.find(key);
    if (field == object.end() || !field->is_string()) return {};
    auto text = field->get<std::string>();
    if (text.size() > 128) return {};
    for (const unsigned char c : text) if (c < 32 || c == 127) return {};
    return text;
}

bool ReadMetadataFile(const std::string& path, size_t limit, std::vector<unsigned char>& bytes) {
    const int fd = OpenAmiiboPath(path, O_RDONLY);
    if (fd < 0) return false;
    struct Close { int fd; ~Close() { close(fd); } } close_fd{fd};
    struct stat info {};
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0 ||
        static_cast<uint64_t>(info.st_size) > limit) return false;
    bytes.resize(static_cast<size_t>(info.st_size));
    size_t done = 0;
    while (done < bytes.size()) {
        const auto n = pread(fd, bytes.data() + done, bytes.size() - done, done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { bytes.clear(); return false; }
        done += static_cast<size_t>(n);
    }
    return true;
}

// Candidate only; the renderer must still decode the PNG under pixel/allocation limits.
bool PngCandidate(const std::string& path) {
    const int fd = OpenAmiiboPath(path, O_RDONLY);
    if (fd < 0) return false;
    struct Close { int fd; ~Close() { close(fd); } } close_fd{fd};
    struct stat info {};
    constexpr std::array<unsigned char, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
    std::array<unsigned char, 8> header{};
    return fstat(fd, &info) == 0 && S_ISREG(info.st_mode) && info.st_size >= 33 &&
        info.st_size <= 4 * 1024 * 1024 && pread(fd, header.data(), header.size(), 0) == 8 &&
        header == signature;
}
} // namespace

AmiiboCatalog AmiiboCatalog::Parse(std::string_view text) {
    AmiiboCatalog catalog;
    if (text.size() > kCatalogBytes) return catalog;
    try {
        size_t events = 0;
        std::array<std::set<std::string>, 18> keys;
        const auto data = nlohmann::json::parse(text, [&](int depth, auto event, auto& value) {
            if (depth > 16 || ++events > 65536) throw std::runtime_error("Amiibo catalog limits");
            using Event = nlohmann::json::parse_event_t;
            if (event == Event::object_start) keys[depth + 1].clear();
            if (event == Event::key && !keys[depth].insert(value.template get<std::string>()).second)
                throw std::runtime_error("Duplicate Amiibo catalog key");
            return true;
        });
        const auto insert = [&](AmiiboMetadata item) {
            // Duplicate IDs (including case aliases) must not pick an arbitrary identity.
            if (item.id.empty() || item.name.empty()) return;
            if (catalog.entries.size() >= kCatalogEntries || !catalog.entries.emplace(item.id, item).second)
                throw std::runtime_error("Ambiguous or oversized Amiibo catalog");
        };
        if (const auto table = data.find("amiibos"); table != data.end() && table->is_object()) {
            if (table->size() > kCatalogEntries) return {};
            const auto series = data.find("amiibo_series");
            for (const auto& [key, value] : table->items()) {
                AmiiboMetadata item{NormalId(key), Text(value, "name"), {}};
                if (item.id.empty()) continue;
                if (series != data.end() && series->is_object()) {
                    const auto series_key = "0x" + item.id.substr(12, 2);
                    item.series = Text(*series, series_key.c_str());
                }
                insert(std::move(item));
            }
        } else if (const auto table = data.find("amiibo"); table != data.end()) {
            // AmiiboAPI's list response or its single-ID object response.
            const auto entry = [&](const auto& value) {
                const auto head = Text(value, "head"), tail = Text(value, "tail");
                if (head.size() != 8 || tail.size() != 8) return;
                insert({NormalId(head + tail), Text(value, "name"), Text(value, "amiiboSeries")});
            };
            if (table->is_array()) {
                if (table->size() > kCatalogEntries) return {};
                for (const auto& value : *table) entry(value);
            } else if (table->is_object()) entry(*table);
        }
    } catch (const std::exception&) { return {}; }
    return catalog;
}

AmiiboCatalog AmiiboCatalog::Read(const std::string& folder) {
    std::vector<unsigned char> text;
    AmiiboCatalog result;
    // Downloaded metadata never replaces a user's catalog; explicit local labels win by ID.
    if (ReadMetadataFile((std::filesystem::path(folder) / "catalog-sync.json").string(), kCatalogBytes, text))
        result = Parse(std::string_view(reinterpret_cast<const char*>(text.data()), text.size()));
    if (ReadMetadataFile((std::filesystem::path(folder) / "amiibo.json").string(), kCatalogBytes, text)) {
        auto local = Parse(std::string_view(reinterpret_cast<const char*>(text.data()), text.size()));
        for (auto& [id, entry] : local.entries) result.entries.insert_or_assign(id, std::move(entry));
    }
    return result;
}

const AmiiboMetadata* AmiiboCatalog::Find(const std::string& id) const {
    const auto found = entries.find(id);
    return found == entries.end() ? nullptr : &found->second;
}

std::string AmiiboModelId(std::span<const unsigned char> bytes) {
    using namespace Service::NFP;
    // These are two different layouts accepted by the pinned core, not interchangeable offsets.
    static_assert(offsetof(NTAG215File, model_info) == 0x1dc);
    static_assert(offsetof(EncryptedNTAG215File, user_memory) + offsetof(EncryptedAmiiboFile, model_info) == 0x54);
    if (bytes.size() != 532 && bytes.size() != 540 && bytes.size() != 572) return {};
    NTAG215File packed{};
    EncryptedNTAG215File raw{};
    const auto count = std::min(bytes.size(), sizeof(raw));
    std::memcpy(&packed, bytes.data(), count);
    std::memcpy(&raw, bytes.data(), count);
    const bool is_packed = AmiiboCrypto::IsAmiiboValid(packed);
    const bool is_raw = AmiiboCrypto::IsAmiiboValid(raw);
    if (is_packed == is_raw) return {}; // neither layout, or a deliberately ambiguous one
    const auto& model = is_packed ? packed.model_info : raw.user_memory.model_info;
    const auto* id = reinterpret_cast<const unsigned char*>(&model);
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (size_t n = 0; n < 8; ++n) { result += hex[id[n] >> 4]; result += hex[id[n] & 15]; }
    return result;
}

AmiiboDescription DescribeAmiibo(const std::string& path, const std::string& collection,
                                 const AmiiboCatalog& catalog) {
    namespace fs = std::filesystem;
    const fs::path file{path}, root{collection};
    AmiiboDescription item;
    item.label = file.parent_path() == root ? file.stem().string() : file.parent_path().filename().string();
    const AmiiboFile dump(path);
    std::array<unsigned char, 540> bytes{};
    if (!dump.IsOpen() || dump.Read(bytes) != std::min(dump.GetSize(), bytes.size())) return item;
    item.id = AmiiboModelId(bytes);
    if (const auto* metadata = catalog.Find(item.id)) {
        item.label = metadata->name;
        if (!metadata->series.empty()) item.label += " / " + metadata->series;
    }
    // Explicitly paired PNG wins. A shared cover is unambiguous only for a one-dump folder.
    auto image = file; image.replace_extension(".png");
    if (PngCandidate(image.string())) item.image_path = image.string();
    if (item.image_path.empty()) {
        const auto entries = AmiiboFolderEntries(file.parent_path().string(), 4097);
        unsigned dumps = 0;
        for (const auto& sibling : entries) {
            const auto name = sibling.filename().string();
            if (!name.empty() && name.front() != '.' && EndsWithNoCase(name, ".bin")) ++dumps;
        }
        image = file.parent_path() / "cover.png";
        if (entries.size() < 4097 && dumps == 1 && PngCandidate(image.string())) item.image_path = image.string();
    }
    if (item.image_path.empty() && !item.id.empty()) {
        image = root / "images" / ("icon_" + item.id.substr(0, 8) + "-" + item.id.substr(8) + ".png");
        if (PngCandidate(image.string())) item.image_path = image.string();
    }
    return item;
}
} // namespace Eden
