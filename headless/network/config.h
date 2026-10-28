// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "shares.h"
#include <functional>

namespace Eden::Network {
enum class ConfigError { none, unsafe, invalid, conflict, write, cancelled, sync };
// Backend-only. Neither the original bytes nor these shares belong in a UI/Lua snapshot.
struct Configuration {
    std::vector<Share> shares;
    std::string bytes;
    std::uint64_t device = 0, inode = 0;
    bool exists = false;
    ConfigError error = ConfigError::none;
};
Configuration ReadConfiguration(const std::string& file, bool private_mode = false);
std::string ConfigurationError(ConfigError error);
enum class WriteStep { staged, flushed, publish };
ConfigError WriteConfiguration(const std::string& file, const Configuration& expected,
    const std::vector<Share>& shares, const std::atomic<bool>& stop,
    const std::function<void(WriteStep)>& checkpoint = {});
} // namespace Eden::Network
