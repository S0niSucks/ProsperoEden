// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace Eden::Network {
// Pure validation: settings/history must not link a client or resolve a server.
inline bool ValidGameUri(std::string_view uri) {
    if (!uri.starts_with("pe-net://") || uri.size() > 2100) return false;
    const auto tail = uri.substr(9);
    const auto slash = tail.find('/');
    if (slash == tail.npos || slash == 0 || slash > 40 || slash + 1 == tail.size()) return false;
    for (std::size_t at = 0; at < tail.size();) {
        const auto end = tail.find('/', at);
        const auto part = tail.substr(at, end == tail.npos ? tail.size() - at : end - at);
        if (part.empty() || part == "." || part == ".." || part.size() > 255) return false;
        for (unsigned char c : part)
            if (c < 32 || c == 127 || c == '\\' || c == ':' || c == '%' || c == '?' || c == '#') return false;
        if (end == tail.npos) break;
        at = end + 1;
        if (at == tail.size()) return false;
    }
    if (tail.size() - slash - 1 > 2048 || uri.size() < 4) return false;
    std::string extension{uri.substr(uri.size() - 4)};
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::tolower(c); });
    return extension == ".nro" || extension == ".nsp" || extension == ".xci";
}
} // namespace Eden::Network
