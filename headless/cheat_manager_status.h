// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Eden {
struct CheatManagerStatus {
    std::string phase{"idle"}, file, source, build, error;
    std::uint64_t generation{}, done{}, total{}, entries{};
    bool busy{}, offline{}, cached{};
    std::vector<std::string> names, other_builds;
    bool other_builds_truncated{};
};
} // namespace Eden
