// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cheat_catalog.h"
#include <atomic>
#include <functional>

namespace Eden {
inline constexpr std::string_view kCheatCatalogSource =
    "https://raw.githubusercontent.com/blawar/titledb/master/cheats.json";
enum class CheatDownloadStatus { Failed, Cancelled, NoMatch, Ready };
struct CheatDownload {
    CheatDownloadStatus status{CheatDownloadStatus::Failed};
    CheatCatalog catalog;
    std::string error;
};
bool CheatSourceAllowed(std::string_view url);
// Blocking, inert worker operation: the caller starts/cancels/joins the launcher job. This
// downloads JSON only; it neither installs nor enables code and never changes the running VM.
// Progress counts uncompressed body bytes; zero total means no Content-Length was supplied.
CheatDownload DownloadCheatCatalog(const std::string& https_source, std::uint64_t program,
    const std::array<std::uint8_t, 32>& main_build, const std::atomic<bool>& stop,
    std::function<void(std::uint64_t, std::uint64_t)> progress = {});
} // namespace Eden
