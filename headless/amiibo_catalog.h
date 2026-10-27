// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <map>
#include <span>
#include <string>
#include <string_view>

namespace Eden {
struct AmiiboMetadata {
    std::string id, name, series;
};
// Only public model metadata, never dumps, tag UUIDs, keys or mutable game data.
class AmiiboCatalog {
public:
    static AmiiboCatalog Parse(std::string_view json);
    static AmiiboCatalog Read(const std::string& folder);
    const AmiiboMetadata* Find(const std::string& id) const;
    size_t Size() const { return entries.size(); }
private:
    std::map<std::string, AmiiboMetadata> entries;
};
// Returns an empty ID for unknown/ambiguous layouts. Does not decrypt or authenticate a tag.
std::string AmiiboModelId(std::span<const unsigned char> bytes);
struct AmiiboDescription {
    std::string id, label, image_path;
};
AmiiboDescription DescribeAmiibo(const std::string& path, const std::string& collection,
                                 const AmiiboCatalog& catalog);
} // namespace Eden
