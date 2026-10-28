// SPDX-License-Identifier: GPL-3.0-or-later
#include "config.h"
#include "prosperoeden/pe/core/strings.hpp" // TR is an identity marker; translate only where displayed.
#include <atomic>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

namespace Eden::Network {
namespace {
using Json = nlohmann::json;
struct Fd {
    int value = -1;
    explicit Fd(int value = -1) : value(value) {}
    ~Fd() { if (value >= 0) close(value); }
    Fd(const Fd&) = delete;
};
// Resolve every ancestor through a pinned directory FD. Never chmod existing ancestors,
// follow a link, or use a path-based temporary-file helper after this walk.
int Parent(const std::string& file, bool create, std::string* leaf) {
    if (file.empty() || file.size() > 4096 || file.find('\0') != file.npos) { errno = EINVAL; return -1; }
    int fd = open(file.front() == '/' ? "/" : ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (fd < 0) return -1;
    for (std::size_t at = file.front() == '/' ? 1 : 0; at < file.size();) {
        const auto slash = file.find('/', at);
        const auto part = file.substr(at, slash == file.npos ? file.size() - at : slash - at);
        if (part.empty() || part == "." || part == ".." || part.size() > 255) { close(fd); errno = EINVAL; return -1; }
        if (slash == file.npos) { *leaf = part; return fd; }
        int next = openat(fd, part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
        if (next < 0 && errno == ENOENT && create) {
            if (mkdirat(fd, part.c_str(), 0700) && errno != EEXIST) { close(fd); return -1; }
            next = openat(fd, part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
        }
        close(fd); if (next < 0) return -1; fd = next; at = slash + 1;
    }
    close(fd); errno = EINVAL; return -1;
}
bool Shape(const Json& value, const std::set<std::string>& allowed) {
    if (!value.is_object()) return false;
    for (auto it = value.begin(); it != value.end(); ++it) if (!allowed.contains(it.key())) return false;
    return true;
}
std::vector<Share> Parse(const std::string& bytes) {
    std::vector<std::set<std::string>> keys;
    auto json = Json::parse(bytes, [&](int depth, Json::parse_event_t event, Json& part) {
        if (depth > 8) throw std::runtime_error("depth");
        if (event == Json::parse_event_t::object_start) keys.emplace_back();
        if (event == Json::parse_event_t::key && !keys.back().insert(part.get<std::string>()).second)
            throw std::runtime_error("duplicate key");
        if (event == Json::parse_event_t::object_end) keys.pop_back();
        return true;
    });
    if (!Shape(json, {"shares"}) || !json.contains("shares") || !json["shares"].is_array() || json["shares"].size() > 4)
        throw std::runtime_error("schema");
    std::vector<Share> shares; std::set<std::string> ids;
    for (const auto& value : json["shares"]) {
        if (!Shape(value, {"id", "name", "protocol", "server", "share", "folder", "updates_dir", "user", "password", "domain",
                          "port", "mount_port", "uid", "gid", "timeout_ms", "cache_mib", "enabled", "encrypt"}))
            throw std::runtime_error("schema");
        for (const auto* key : {"port", "mount_port", "uid", "gid", "timeout_ms", "cache_mib"})
            if (value.contains(key) && (!value[key].is_number_integer() || value[key] < 0 || value[key] > 2147483647))
                throw std::runtime_error("integer");
        for (const auto* key : {"enabled", "encrypt"})
            if (value.contains(key) && !value[key].is_boolean()) throw std::runtime_error("boolean");
        Share s;
        s.id = value.at("id").get<std::string>(); s.name = value.value("name", s.id);
        s.protocol = value.at("protocol").get<std::string>(); s.server = value.at("server").get<std::string>();
        s.share = value.at("share").get<std::string>(); s.folder = value.value("folder", ""); s.updates_dir = value.value("updates_dir", "");
        s.user = value.value("user", ""); s.password = value.value("password", ""); s.domain = value.value("domain", "");
        s.port = value.value("port", s.protocol == "nfs" ? 2049 : 445); s.mount_port = value.value("mount_port", 0);
        s.uid = value.value("uid", 65534); s.gid = value.value("gid", 65534); s.timeout_ms = value.value("timeout_ms", 5000);
        const int cache = value.value("cache_mib", 16);
        if (cache < 1 || cache > 32) throw std::runtime_error("cache");
        s.cache_bytes = static_cast<std::size_t>(cache) << 20;
        s.enabled = value.value("enabled", true); s.encrypt = value.value("encrypt", false);
        const auto clean = [](const std::string& text) {
            return std::none_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; });
        };
        if (!Valid(s) || s.name.empty() || !clean(s.name) || !clean(s.user) || !clean(s.password) || !clean(s.domain) ||
            !ids.insert(s.id).second || std::any_of(shares.begin(), shares.end(), [&](const auto& old) {
                return old.protocol == s.protocol && old.server == s.server && old.port == s.port && old.share == s.share && old.folder == s.folder;
            })) throw std::runtime_error("share");
        shares.push_back(std::move(s));
    }
    return shares;
}
Configuration ReadAt(int parent, const std::string& leaf, bool private_mode) {
    Configuration result;
    Fd fd(openat(parent, leaf.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK));
    if (fd.value < 0) { if (errno != ENOENT) result.error = ConfigError::unsafe; return result; }
    struct stat st{};
    if (fstat(fd.value, &st) || !S_ISREG(st.st_mode) || st.st_nlink != 1 || st.st_size < 0 || st.st_size > 65536 ||
        (private_mode && fchmod(fd.value, 0600))) { result.error = ConfigError::unsafe; return result; }
    result.exists = true; result.device = st.st_dev; result.inode = st.st_ino;
    result.bytes.resize(static_cast<std::size_t>(st.st_size));
    std::size_t at = 0;
    while (at < result.bytes.size()) {
        const auto n = read(fd.value, result.bytes.data() + at, result.bytes.size() - at);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { result.error = ConfigError::unsafe; return result; }
        at += static_cast<std::size_t>(n);
    }
    char extra{};
    if (read(fd.value, &extra, 1) != 0) { result.error = ConfigError::conflict; return result; }
    try { result.shares = Parse(result.bytes); }
    catch (...) { result.error = ConfigError::invalid; } // never expose parser excerpts/credentials
    return result;
}
bool Same(const Configuration& a, const Configuration& b) {
    return a.error == ConfigError::none && b.error == ConfigError::none && a.exists == b.exists &&
        a.device == b.device && a.inode == b.inode && a.bytes == b.bytes;
}
Json Serialize(const std::vector<Share>& shares) {
    Json list = Json::array();
    for (const auto& s : shares) list.push_back({{"id", s.id}, {"name", s.name}, {"protocol", s.protocol}, {"server", s.server},
        {"share", s.share}, {"folder", s.folder}, {"updates_dir", s.updates_dir}, {"user", s.user}, {"password", s.password},
        {"domain", s.domain}, {"port", s.port}, {"mount_port", s.mount_port}, {"uid", s.uid}, {"gid", s.gid},
        {"timeout_ms", s.timeout_ms}, {"cache_mib", s.cache_bytes >> 20}, {"enabled", s.enabled}, {"encrypt", s.encrypt}});
    return Json{{"shares", std::move(list)}};
}
}
Configuration ReadConfiguration(const std::string& file, bool private_mode) {
    std::string leaf; Fd parent(Parent(file, false, &leaf));
    if (parent.value >= 0) return ReadAt(parent.value, leaf, private_mode);
    Configuration result; if (errno != ENOENT) result.error = ConfigError::unsafe; return result;
}
std::string ConfigurationError(ConfigError error) {
    switch (error) {
    case ConfigError::none: return {};
    case ConfigError::invalid: return TR("Invalid network share settings. Fix the imported JSON before editing; the file was not replaced.");
    case ConfigError::unsafe: return TR("Network settings must be a regular file with link-free parent directories (at most 64 KiB).");
    case ConfigError::conflict: return TR("Network settings changed outside this editor. Reopen the editor; nothing was overwritten.");
    case ConfigError::cancelled: return TR("Cancelled. The saved network settings were not changed.");
    case ConfigError::sync: return TR("Saved, but directory sync could not be confirmed.");
    default: return TR("Could not save network settings. The previous file was preserved.");
    }
}
ConfigError WriteConfiguration(const std::string& file, const Configuration& expected, const std::vector<Share>& shares,
    const std::atomic<bool>& stop, const std::function<void(WriteStep)>& checkpoint) {
    if (expected.error != ConfigError::none) return expected.error;
    if (stop) return ConfigError::cancelled;
    std::string bytes;
    try { bytes = Serialize(shares).dump(2) + '\n'; if (bytes.size() > 65536) return ConfigError::invalid; (void)Parse(bytes); }
    catch (...) { return ConfigError::invalid; }
    std::string leaf; Fd parent(Parent(file, true, &leaf));
    if (parent.value < 0 || leaf != "network-shares.json") return ConfigError::unsafe;
    if (!Same(expected, ReadAt(parent.value, leaf, false))) return ConfigError::conflict;
    static std::atomic<unsigned long> sequence{0};
    std::string staging; int created = -1;
    for (unsigned n = 0; n < 32; ++n) {
        staging = "network-shares.json.tmp-" + std::to_string(getpid()) + '-' + std::to_string(++sequence) + '-' +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        // A private, exclusively created directory pins the publication source too. Renaming
        // entries in a writable config parent cannot substitute a symlink for our open file.
        if (mkdirat(parent.value, staging.c_str(), 0700) == 0) {
            created = openat(parent.value, staging.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
            break;
        }
        if (errno != EEXIST) break;
    }
    if (created < 0) return ConfigError::write;
    Fd staging_dir(created);
    struct Cleanup {
        int dir, stage; const std::string& name;
        ~Cleanup() {
            unlinkat(stage, "settings", 0);
            struct stat held{}, entry{};
            if (!fstat(stage, &held) && !fstatat(dir, name.c_str(), &entry, AT_SYMLINK_NOFOLLOW) &&
                held.st_dev == entry.st_dev && held.st_ino == entry.st_ino) unlinkat(dir, name.c_str(), AT_REMOVEDIR);
        }
    } cleanup{parent.value, staging_dir.value, staging};
    Fd out(openat(staging_dir.value, "settings", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_NONBLOCK, 0600));
    if (out.value < 0) return ConfigError::write;
    if (checkpoint) checkpoint(WriteStep::staged);
    if (fchmod(out.value, 0600)) return ConfigError::write;
    for (std::size_t at = 0; at < bytes.size();) {
        if (stop) return ConfigError::cancelled;
        const auto n = write(out.value, bytes.data() + at, bytes.size() - at);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return ConfigError::write;
        at += static_cast<std::size_t>(n);
    }
    if (fsync(out.value)) return ConfigError::write;
    if (checkpoint) checkpoint(WriteStep::flushed);
    std::string again; Fd check(Parent(file, false, &again)); struct stat a{}, b{};
    if (check.value < 0 || fstat(parent.value, &a) || fstat(check.value, &b) || a.st_dev != b.st_dev || a.st_ino != b.st_ino)
        return ConfigError::conflict;
    if (checkpoint) checkpoint(WriteStep::publish);
    if (stop) return ConfigError::cancelled;
    if (!Same(expected, ReadAt(parent.value, leaf, false))) return ConfigError::conflict;
    if (renameat(staging_dir.value, "settings", parent.value, leaf.c_str())) return ConfigError::write;
    // Rename is the commit point: do not report an unchanged/failed save after publication.
    return fsync(parent.value) ? ConfigError::sync : ConfigError::none;
}
} // namespace Eden::Network
