// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include "metadata_bridge.h"

namespace Eden::Network {
struct Metadata {
    std::string title, cover, update, error;
    std::uint64_t title_id = 0;
    unsigned dlc = 0;
    int content = EDEN_CONTENT_UNKNOWN;
};
// Uses the pinned real metadata bridge through RealVfs, never a filename/title-ID guess.
// Call under the same bridge serialization as the local library. Output is local, staged/atomic.
Metadata InspectGame(const std::string& uri, const std::string& keys, const std::string& cover);
// Home reads the asynchronous Library result without network I/O on the controller thread.
Metadata CachedMetadata(const std::string& uri);
} // namespace Eden::Network
