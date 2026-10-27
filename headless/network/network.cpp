// SPDX-License-Identifier: GPL-3.0-or-later
#include "shares.h"
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

namespace Eden::Network {
namespace {
struct Location {
    Share share;
    std::shared_ptr<Session> session;
    ShareStatus status;
    std::vector<Game> games;
};
struct Manager {
    std::mutex mutex;
    std::vector<Location> locations;
    std::vector<std::weak_ptr<Session>> active;
    Status status;
    std::thread worker;
    std::atomic<bool> stop{false};
    bool game = false;
    ~Manager() { stop = true; for (auto& l : locations) l.session->Cancel(); if (worker.joinable()) worker.join(); }
};
Manager& State() { static Manager state; return state; }
bool Split(std::string_view uri, std::string* id, std::string* path) {
    if (!IsPath(uri)) return false;
    const auto tail = uri.substr(9);
    const auto slash = tail.find('/');
    if (slash == tail.npos || !SafePath(tail.substr(0, slash)) || tail.substr(0, slash).find('/') != tail.npos ||
        !SafePath(tail.substr(slash + 1), true)) return false;
    *id = tail.substr(0, slash); *path = tail.substr(slash + 1); return true;
}
bool GameFile(const std::string& path) {
    if (path.size() < 5) return false;
    auto extension = path.substr(path.size() - 4);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::tolower(c); });
    // Solid NSZ/XCZ are intentionally excluded: they may unpack a whole file. Network access
    // promises bounded on-demand reads, never an implicit full-game copy.
    return extension == ".nsp" || extension == ".xci" || extension == ".nro";
}
std::vector<Share> ReadConfig(const std::string& file, std::string* error) {
    // NONBLOCK also prevents a substituted FIFO/device from hanging before fstat rejects it.
    const int fd = open(file.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) { if (errno != ENOENT) *error = "Cannot open network-shares.json."; return {}; }
    struct stat stat{};
    if (fstat(fd, &stat) || !S_ISREG(stat.st_mode) || stat.st_size < 0 || stat.st_size > 65536 ||
        fchmod(fd, 0600)) { close(fd); *error = "Network share settings must be a private regular file (at most 64 KiB)."; return {}; }
    std::string text(static_cast<std::size_t>(stat.st_size), '\0');
    std::size_t done = 0;
    while (done < text.size()) {
        const auto count = read(fd, text.data() + done, text.size() - done);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        done += static_cast<std::size_t>(count);
    }
    close(fd);
    if (done != text.size()) { *error = "Cannot read network share settings."; return {}; }
    try {
        const auto json = nlohmann::json::parse(text);
        const auto& list = json.at("shares");
        if (!list.is_array() || list.size() > 4) throw std::runtime_error("schema");
        std::vector<Share> shares;
        std::set<std::string> ids;
        for (const auto& value : list) {
            Share s;
            s.id = value.at("id").get<std::string>(); s.name = value.value("name", s.id);
            s.protocol = value.at("protocol").get<std::string>(); s.server = value.at("server").get<std::string>();
            s.share = value.at("share").get<std::string>(); s.folder = value.value("folder", "");
            s.user = value.value("user", ""); s.password = value.value("password", ""); s.domain = value.value("domain", "");
            s.port = value.value("port", s.protocol == "nfs" ? 2049 : 445);
            s.mount_port = value.value("mount_port", 0);
            s.uid = value.value("uid", 65534); s.gid = value.value("gid", 65534);
            s.timeout_ms = value.value("timeout_ms", 5000);
            const int cache_mib = value.value("cache_mib", 16);
            if (cache_mib < 1 || cache_mib > 32) throw std::runtime_error("cache");
            s.cache_bytes = static_cast<std::size_t>(cache_mib) << 20;
            s.enabled = value.value("enabled", true); s.encrypt = value.value("encrypt", false);
            if (!Valid(s) || !ids.insert(s.id).second) throw std::runtime_error("share");
            shares.push_back(std::move(s));
        }
        return shares;
    } catch (...) {
        // JSON parser errors can contain passwords and tokens. Never forward them.
        *error = "Invalid network share settings. Use unique IDs, numeric IPv4 servers and safe relative folders; at most four shares.";
        return {};
    }
}
} // namespace

void StopDiscovery() {
    auto& m = State(); m.stop = true;
    {
        std::lock_guard lock(m.mutex);
        for (auto& l : m.locations) if (l.status.scanning) l.session->Cancel();
    }
    if (m.worker.joinable()) m.worker.join();
}
void Configure(const std::string& file) {
    Disconnect();
    auto& m = State();
    std::string error;
    auto shares = ReadConfig(file, &error);
    {
        std::lock_guard lock(m.mutex);
        m.locations.clear(); m.status.file = file; m.status.error = error;
        for (auto& share : shares) {
            ShareStatus s{share.id, share.name, share.protocol, share.enabled ? "" : "Disabled in settings."};
            auto session = std::make_shared<Session>(share);
            m.locations.push_back({std::move(share), std::move(session), std::move(s), {}});
        }
        ++m.status.generation;
    }
    Refresh();
}
void Refresh() {
    StopDiscovery();
    auto& m = State();
    {
        std::lock_guard lock(m.mutex);
        if (m.game) return;
        m.stop = false; m.status.busy = true;
        for (auto& l : m.locations) {
            l.session->Cancel(); l.session = std::make_shared<Session>(l.share);
            l.status.scanning = l.share.enabled; l.status.connected = false; l.games.clear();
        }
        ++m.status.generation;
    }
    m.worker = std::thread([&m] {
        // Locations are stable until this worker is joined, and only the worker writes game lists.
        for (auto& l : m.locations) {
            if (m.stop || !l.share.enabled) continue;
            std::vector<Game> games;
            bool okay = l.session->Connect();
            std::vector<Entry> entries;
            if (okay) okay = l.session->List("", &entries);
            const auto append = [&](const Entry& e) {
                if (e.directory || !GameFile(e.path) || games.size() >= 4096) return;
                const auto slash = e.path.find_last_of('/');
                const auto name = slash == e.path.npos ? e.path : e.path.substr(slash + 1);
                games.push_back({Uri(l.share.id, e.path), name.substr(0, name.size() - 4), l.share.name, e.size});
            };
            for (const auto& e : entries) {
                if (m.stop || !okay) break;
                if (!e.directory) { append(e); continue; }
                std::vector<Entry> children;
                // One level of game folders, as in the local library; no recursive NAS crawl.
                if (!(okay = l.session->List(e.path, &children))) break;
                for (const auto& child : children) append(child);
            }
            std::lock_guard lock(m.mutex);
            l.status.scanning = false; l.status.connected = okay && !m.stop;
            l.status.error = okay ? "" : l.session->Error();
            if (okay && !m.stop) l.games = std::move(games);
            l.status.games = l.games.size(); ++m.status.generation;
        }
        std::lock_guard lock(m.mutex);
        for (auto& l : m.locations) l.status.scanning = false;
        m.status.busy = false; ++m.status.generation;
    });
}
void Disconnect() {
    StopDiscovery();
    auto& m = State(); std::lock_guard lock(m.mutex);
    for (auto& l : m.locations) {
        l.session->Cancel(); l.status.connected = false; l.status.scanning = false;
        l.status.error = "Disconnected. Reconnect to scan and play."; l.games.clear(); l.status.games = 0;
    }
    m.status.busy = false; ++m.status.generation;
}
Status Current() {
    auto& m = State(); std::lock_guard lock(m.mutex);
    Status status = m.status; status.shares.clear();
    for (const auto& l : m.locations) {
        auto s = l.status;
        if (s.connected && !l.session->Healthy()) { s.connected = false; s.error = l.session->Error(); }
        status.shares.push_back(std::move(s));
    }
    return status;
}
std::vector<Game> Games() {
    auto& m = State(); std::lock_guard lock(m.mutex);
    std::vector<Game> games;
    for (const auto& l : m.locations) for (const auto& g : l.games) games.push_back(g);
    return games;
}
bool Known(std::string_view uri) {
    auto& m = State(); std::lock_guard lock(m.mutex);
    for (const auto& l : m.locations) if (l.session->Healthy())
        for (const auto& g : l.games) if (g.uri == uri) return true;
    return false;
}
Opened Open(std::string_view uri) {
    std::string id, path;
    if (!Split(uri, &id, &path) || path.empty()) return {};
    std::shared_ptr<Session> session;
    auto& m = State();
    {
        std::lock_guard lock(m.mutex);
        for (const auto& l : m.locations) if (l.share.id == id && l.share.enabled) { session = l.session; break; }
        if (session && m.game) {
            std::erase_if(m.active, [](const auto& weak) { return weak.expired(); });
            if (std::none_of(m.active.begin(), m.active.end(), [&](const auto& weak) { return weak.lock() == session; }))
                m.active.push_back(session);
        }
    }
    Entry entry;
    if (!session || !session->Stat(path, &entry) || entry.directory) return {};
    return {std::move(session), std::move(entry)};
}
void BeginGame() { auto& m = State(); std::lock_guard lock(m.mutex); m.active.clear(); m.game = true; }
void EndGame() { auto& m = State(); std::lock_guard lock(m.mutex); m.active.clear(); m.game = false; }
std::string GameError() {
    auto& m = State(); std::lock_guard lock(m.mutex);
    for (const auto& weak : m.active) if (auto s = weak.lock(); s && !s->Healthy()) return s->Error();
    return {};
}
void CancelGame() {
    auto& m = State(); std::lock_guard lock(m.mutex);
    for (const auto& weak : m.active) if (auto s = weak.lock()) s->Cancel();
}
} // namespace Eden::Network
