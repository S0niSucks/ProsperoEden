// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Eden {
enum class CheatCatalogStatus { Invalid, NoMatch, Ready };
struct CatalogCheat { std::string name, code; bool master{}; };
struct CheatCatalog {
    CheatCatalogStatus status{CheatCatalogStatus::Invalid};
    std::vector<CatalogCheat> entries;
    // Informational only: up to 32 other build prefixes/version integers for this exact title.
    // Never feed these to the installer or use them as a version fallback.
    std::vector<std::string> other_builds{};
    bool other_builds_truncated{};
    std::string Text() const;
};
// TitleDB schema: exact program ID and the standard first eight main-NSO build-ID bytes.
// The caller must provide the effective (patched/updated) main module, never a version label.
// This is parsing only: it does not fetch, install, enable or execute any cheat.
CheatCatalog ReadCheatCatalog(std::string_view json, uint64_t program_id,
                             const std::array<uint8_t, 32>& main_build_id);
} // namespace Eden
