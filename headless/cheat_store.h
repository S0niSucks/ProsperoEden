// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cheat_catalog.h"
#include <atomic>
#include <functional>

namespace Eden {
// App-owned cache, one bounded record per exact source/program/full build. Offline reads are
// explicit; a failed transfer must not be represented as a fresh catalog or erase this record.
bool SaveCheatCache(const std::string& cache_folder, const std::string& source,
    uint64_t program, const std::array<uint8_t, 32>& build, const CheatCatalog& catalog);
CheatCatalog LoadCheatCache(const std::string& cache_folder, const std::string& source,
    uint64_t program, const std::array<uint8_t, 32>& build);

enum class CheatInstallStatus { Failed, Cancelled, StaleIdentity, Conflict, Installed, AlreadyPresent };
struct CheatInstallation {
    CheatInstallStatus status{CheatInstallStatus::Failed};
    std::string mod_name, error;
};
// Caller passes that title's mods folder and a serialized, fresh identity recheck. Only a
// content-named managed mod is touched, never loose cheats or another mod. Publication is
// no-clobber, at the final .txt; incomplete/cancelled work has no executable .txt. The caller
// joins this job before starting a game. Newly installed code remains OFF by Mods::Switches.
CheatInstallation InstallCheatCatalog(const std::string& title_mods_folder, const std::string& source,
    uint64_t program, const std::array<uint8_t, 32>& build, const CheatCatalog& catalog,
    const std::atomic<bool>& stop, const std::function<bool()>& identity_still_current);
} // namespace Eden
