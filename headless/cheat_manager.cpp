// SPDX-License-Identifier: GPL-3.0-or-later
#include "cheat_manager.h"
#include "cheat_download.h"
#include "cheat_store.h"
#include <algorithm>
#include <exception>
#include <utility>

namespace Eden {
namespace {
bool Known(const CheatManager::Build& build) {
    return std::any_of(build.begin(), build.begin() + 8, [](auto byte) { return byte != 0; });
}
std::string BuildText(const CheatManager::Build& build) {
    std::string text;
    for (auto byte : build) { text += "0123456789ABCDEF"[byte >> 4]; text += "0123456789ABCDEF"[byte & 15]; }
    return text;
}
}
CheatManager::CheatManager(std::mutex& bridge, Probe probe) : bridge_(bridge), probe_(std::move(probe)) {}
CheatManager::~CheatManager() { Stop(); }
CheatManagerStatus CheatManager::Status() const { const std::lock_guard lock(mutex_); return status_; }
void CheatManager::Cancel() { stop_ = true; }
void CheatManager::Stop() { Cancel(); if (worker_.joinable()) worker_.join(); }
void CheatManager::Finish(const char* phase, std::string error) {
    const std::lock_guard lock(mutex_);
    status_.phase = phase; status_.error = std::move(error); status_.busy = false; ++status_.generation;
}
bool CheatManager::Begin(const CheatManagerRequest& request, bool offline, const char* phase) {
    if (Status().busy) return false;
    if (worker_.joinable()) worker_.join();
    stop_ = false;
    const std::lock_guard lock(mutex_);
    const auto generation = status_.generation + 1;
    status_ = {}; status_.generation = generation;
    status_.file = request.file; status_.source = request.source;
    status_.offline = offline; status_.phase = phase; status_.busy = true;
    return true;
}
bool CheatManager::Start(CheatManagerRequest request, bool offline) {
    if (!Begin(request, offline, "checking")) return false;
    catalog_ = {}; build_ = {}; pending_ = {};
    if (!request.program || request.file.empty() || request.path.empty() || !CheatSourceAllowed(request.source)) {
        Finish("failed", "Choose a local game and a valid HTTPS catalog source"); return true;
    }
    try {
        worker_ = std::thread([this, request = std::move(request), offline] {
            try {
                Build build;
                {
                    const std::lock_guard lock(bridge_);
                    if (stop_) { Finish("cancelled"); return; }
                    build = probe_(request);
                }
                if (stop_) { Finish("cancelled"); return; }
                if (!Known(build)) { Finish("failed", "Cannot identify this game's effective main build; no fallback was used"); return; }
                {
                    const std::lock_guard lock(mutex_);
                    status_.build = BuildText(build); status_.phase = offline ? "checking" : "downloading";
                }
                CheatCatalog catalog;
                if (offline) {
                    catalog = LoadCheatCache(request.cache_folder, request.source, request.program, build);
                    if (catalog.status != CheatCatalogStatus::Ready) {
                        Finish(stop_ ? "cancelled" : "failed", "No valid offline cache for this exact source and build"); return;
                    }
                } else {
                    auto downloaded = DownloadCheatCatalog(request.source, request.program, build, stop_, [this](auto done, auto total) {
                        const std::lock_guard lock(mutex_); status_.done = done; status_.total = total;
                    });
                    if (stop_ || downloaded.status == CheatDownloadStatus::Cancelled) { Finish("cancelled"); return; }
                    if (downloaded.status != CheatDownloadStatus::Ready) {
                        if (downloaded.status == CheatDownloadStatus::NoMatch) {
                            const std::lock_guard lock(mutex_);
                            status_.other_builds = std::move(downloaded.catalog.other_builds);
                            status_.other_builds_truncated = downloaded.catalog.other_builds_truncated;
                        }
                        Finish(downloaded.status == CheatDownloadStatus::NoMatch ? "no_match" : "failed", downloaded.error); return;
                    }
                    catalog = std::move(downloaded.catalog);
                }
                if (stop_) { Finish("cancelled"); return; }
                const bool cached = offline || SaveCheatCache(request.cache_folder, request.source, request.program, build, catalog);
                if (stop_) { Finish("cancelled"); return; }
                pending_ = request; build_ = build; catalog_ = std::move(catalog);
                {
                    const std::lock_guard lock(mutex_);
                    status_.entries = std::count_if(catalog_.entries.begin(), catalog_.entries.end(), [](const auto& e) { return !e.master; });
                    status_.cached = cached;
                    for (const auto& entry : catalog_.entries) if (!entry.master) status_.names.push_back(entry.name);
                }
                Finish("ready", cached ? "" : "The offline cache could not be saved; this preview is available for this session only");
            } catch (const std::exception&) { Finish(stop_ ? "cancelled" : "failed", "The cheat catalog could not be checked"); }
        });
    } catch (const std::exception&) { Finish("failed", "Cannot start the cheat catalog worker"); }
    return true;
}
bool CheatManager::Install(const CheatManagerRequest& request) {
    const auto previous = Status();
    if (previous.busy || previous.phase != "ready" || previous.entries == 0) return false;
    if (worker_.joinable()) worker_.join();
    if (request != pending_) { catalog_ = {}; Finish("failed", "The game or catalog configuration changed; check again"); return false; }
    if (!Begin(request, previous.offline, "installing")) return false;
    {
        const std::lock_guard lock(mutex_);
        status_.build = BuildText(build_); status_.entries = previous.entries; status_.cached = previous.cached;
        status_.names = previous.names;
    }
    try {
        worker_ = std::thread([this, request] {
            try {
                const std::lock_guard lock(bridge_);
                auto result = InstallCheatCatalog(request.title_mods_folder, request.source, request.program,
                    build_, catalog_, stop_, [&] { return probe_(request) == build_; });
                const bool installed = result.status == CheatInstallStatus::Installed || result.status == CheatInstallStatus::AlreadyPresent;
                Finish(installed ? "installed" : (result.status == CheatInstallStatus::Cancelled ? "cancelled" : "failed"), result.error);
            } catch (const std::exception&) { Finish(stop_ ? "cancelled" : "failed", "The cheat catalog could not be installed"); }
        });
    } catch (const std::exception&) { Finish("failed", "Cannot start the cheat installation worker"); }
    return true;
}
} // namespace Eden
