// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#if !defined(__PROSPERO__)
#include <sys/statvfs.h>
#endif

namespace Eden::ThemeStorage {
inline constexpr std::uint64_t MiB = 1024 * 1024;
inline constexpr std::uint64_t MinRequest = MiB;
// nx::storage_bytes: 2 * total + largest + metadata, total <= 128 MiB.
inline constexpr std::uint64_t MaxRequest = 385 * MiB;
inline constexpr std::uint64_t MaxHeld = MaxRequest;
inline constexpr std::uint64_t Headroom = 64 * MiB;
inline bool ValidRequest(std::uint64_t bytes) { return bytes >= MinRequest && bytes <= MaxRequest; }

// A capacity preflight, NOT an OS/cross-process disk reservation. The console has no
// qualified safe free-space import here (see remote.cpp's FreeSpace): unknown denies.
inline bool AvailableBytes(int fd, std::uint64_t& bytes) {
    bytes = 0;
#if defined(__PROSPERO__)
    (void)fd;
    return false;
#else
    struct statvfs status{};
    if (fstatvfs(fd, &status) != 0 || (status.f_flag & ST_RDONLY)) return false;
    const auto block = status.f_frsize ? status.f_frsize : status.f_bsize;
    if (!block || status.f_bavail > UINT64_MAX / block) return false;
    bytes = std::uint64_t(status.f_bavail) * block;
    return true;
#endif
}

struct Ledger {
    std::mutex mutex;
    std::atomic<std::uint64_t> held{0}; // parent guards read this while holding job_admission_mutex_
    bool busy() const { return held.load() != 0; }
};
inline std::shared_ptr<Ledger> ProcessLedger() {
    static const auto ledger = std::make_shared<Ledger>();
    return ledger;
}
struct Credit {
    std::shared_ptr<Ledger> ledger;
    std::uint64_t bytes = 0;
    explicit Credit(std::shared_ptr<Ledger> value) : ledger(std::move(value)) {}
    ~Credit() {
        if (!bytes) return;
        const std::lock_guard lock(ledger->mutex);
        ledger->held.store(ledger->held.load() - bytes);
    }
    Credit(const Credit&) = delete;
    Credit& operator=(const Credit&) = delete;
};
// Admission is checked by EdenServices first. Release only takes the ledger lock,
// never admission/bridge, and the token remains safe even after its service is gone.
inline std::shared_ptr<void> Reserve(const std::shared_ptr<Ledger>& ledger,
    std::uint64_t bytes, std::uint64_t available) {
    if (!ValidRequest(bytes)) return {};
    auto token = std::make_shared<Credit>(ledger); // throwing allocation cannot strand credit
    const std::lock_guard lock(ledger->mutex);
    const auto held = ledger->held.load();
    if (held > MaxHeld || bytes > MaxHeld - held || available < Headroom ||
        held > available - Headroom || bytes > available - Headroom - held) return {};
    ledger->held.store(held + bytes);
    token->bytes = bytes;
    return token;
}
} // namespace Eden::ThemeStorage
