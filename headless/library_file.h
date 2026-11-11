// SPDX-FileCopyrightText: Copyright 2026 Oliv3rmoon
// SPDX-License-Identifier: GPL-3.0-or-later
// Library interchange adapted from ProsperoEden PR #292 (library_file.h).
// Encore keeps publication on its local scan worker and never exports a
// canceled/partial scan. No credentials, keys or online catalogs are included.
#pragma once
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <nlohmann/json.hpp>

namespace Eden::LibraryFile {
struct Game {
    std::string file, path, title;
    std::uint64_t title_id = 0;
    std::string cover;
};

inline std::string Text(const std::string& game_files, const std::vector<Game>& games,
                        const std::string& games_folder = {}) {
    auto list = nlohmann::json::array();
    for (const auto& game : games) {
        char id[17];
        std::snprintf(id, sizeof(id), "%016" PRIX64, game.title_id);
        list.push_back({{"file", game.file}, {"path", game.path}, {"title", game.title},
                        {"title_id", game.title_id ? nlohmann::json(id) : nlohmann::json(nullptr)},
                        {"cover", game.cover.empty() ? nlohmann::json(nullptr) : nlohmann::json(game.cover)}});
    }
    nlohmann::json document{{"version", 1}, {"app", "PPSA99008"},
                            {"game_files", game_files}, {"games", std::move(list)}};
    if (!games_folder.empty()) document["games_folder"] = games_folder;
    return document.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) + "\n";
}

// Cache/interchange data, not a save transaction. Bound the old-file comparison
// to the expected text; reject links/special files. Unique O_EXCL staging never
// follows or truncates an attacker-supplied predictable .tmp path. Only existing
// absolute open/read/write/rename APIs are used (no openat/dup/statfs probes).
inline bool Write(const std::string& path, const std::string& text) {
    if (text.size() > (16u << 20)) return false;
    const int old = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (old >= 0) {
        struct stat info{};
        if (fstat(old, &info) != 0 || !S_ISREG(info.st_mode) || info.st_nlink != 1) {
            close(old);
            return false;
        }
        bool same = info.st_size >= 0 && static_cast<std::uint64_t>(info.st_size) == text.size();
        char buffer[4096];
        for (std::size_t at = 0; same && at < text.size();) {
            const auto count = read(old, buffer, std::min(sizeof(buffer), text.size() - at));
            if (count < 0 && errno == EINTR) continue;
            same = count > 0 && text.compare(at, static_cast<std::size_t>(count), buffer,
                                             static_cast<std::size_t>(count)) == 0;
            if (count > 0) at += static_cast<std::size_t>(count);
        }
        close(old);
        if (same) return true;
    } else if (errno != ENOENT) {
        return false;
    }
    static std::atomic<unsigned> sequence{0};
    std::string temporary;
    int fd = -1;
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        temporary = path + ".tmp." + std::to_string(getpid()) + "." +
                    std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
        fd = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644);
        if (fd >= 0 || errno != EEXIST) break;
    }
    if (fd < 0) return false;
    bool ok = true;
    for (std::size_t at = 0; at < text.size();) {
        const auto count = write(fd, text.data() + at, text.size() - at);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ok = false; break; }
        at += static_cast<std::size_t>(count);
    }
    if (close(fd) != 0) ok = false;
    if (ok && std::rename(temporary.c_str(), path.c_str()) == 0) return true;
    (void)std::remove(temporary.c_str());
    return false;
}
} // namespace Eden::LibraryFile
