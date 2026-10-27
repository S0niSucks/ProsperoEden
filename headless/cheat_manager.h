// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cheat_manager_status.h"
#include "cheat_catalog.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

namespace Eden {
struct CheatManagerRequest {
    std::string file, path, game_folder, source, cache_folder, title_mods_folder;
    std::uint64_t program{};
    bool operator==(const CheatManagerRequest&) const = default;
};
// Start/Install/Stop belong to the launcher owner thread. Status and Cancel are thread safe.
// Every probe and the complete install/publication share the metadata bridge mutex. No lock
// is held during HTTP. The owner must stop/join this job before boot or destroying the bridge.
class CheatManager {
public:
    using Build = std::array<std::uint8_t, 32>;
    using Probe = std::function<Build(const CheatManagerRequest&)>; // empty on unknown/mismatched program
    CheatManager(std::mutex& bridge, Probe probe);
    ~CheatManager();
    bool Start(CheatManagerRequest request, bool offline);
    bool Install(const CheatManagerRequest& request);
    CheatManagerStatus Status() const;
    void Cancel();
    void Stop();
private:
    void Finish(const char* phase, std::string error = {});
    bool Begin(const CheatManagerRequest& request, bool offline, const char* phase);
    std::mutex& bridge_;
    Probe probe_;
    mutable std::mutex mutex_;
    CheatManagerStatus status_;
    std::thread worker_;
    std::atomic<bool> stop_{false};
    CheatManagerRequest pending_;
    Build build_{};
    CheatCatalog catalog_;
};
} // namespace Eden
