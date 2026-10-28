// SPDX-License-Identifier: GPL-3.0-or-later
// App-owned admission + real process credit. Included at file scope in eden_services.cpp.
#include "storage_paths.h"
#include <fcntl.h>
#include <unistd.h>

std::shared_ptr<void> EdenServices::reserve_theme_import_storage(const std::string& root, std::uint64_t bytes) {
    const std::lock_guard admission(job_admission_mutex_);
    if (root != Eden::kDataDir || !Eden::ThemeStorage::ValidRequest(bytes) ||
        !Eden::FilesystemAccess() || usb_active_ || launch_committed_ ||
        network_mutation_active_ || nextendo_busy_ ||
        cheat_manager_.Status().busy || amiibo_sync_.Status().busy) return {};
    // Conservatively refuse even a completed-but-unjoined sync/pair worker: its final
    // persistence must not be inferred from a UI phase. Do not join or cancel here.
    if (sync_thread_.joinable() || pair_thread_.joinable()) return {};
    const auto update = Eden::UpdateNotice::Poll().phase;
    if (update != Eden::UpdateNotice::Phase::idle && update != Eden::UpdateNotice::Phase::cancelled &&
        update != Eden::UpdateNotice::Phase::failed) return {};
    for (const auto& download : Eden::Remote::Downloads())
        if (download.state != Eden::Remote::State::failed) return {};
    for (const auto& source : Eden::Remote::Current().sources)
        if (source.refreshing) return {};
    const int fd = open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return {};
    struct Close { int fd; ~Close() { close(fd); } } close_root{fd};
    std::uint64_t available = 0;
    if (!Eden::ThemeStorage::AvailableBytes(fd, available)) return {};
    return Eden::ThemeStorage::Reserve(theme_storage_, bytes, available);
}
