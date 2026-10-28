// SPDX-License-Identifier: GPL-3.0-or-later
#include "shares.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <list>
#include <map>
#include <mutex>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <nfsc/libnfs.h>

namespace Eden::Network {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::size_t kBlock = 256u << 10;
constexpr std::size_t kEntries = 4096;
bool Component(std::string_view value) {
    return !value.empty() && value != "." && value != ".." && value.size() <= 255 &&
        std::none_of(value.begin(), value.end(), [](unsigned char c) {
            return c < 32 || c == 127 || c == '/' || c == '\\' || c == ':' || c == '%' || c == '?' || c == '#';
        });
}
Entry FromSmb(std::string path, const smb2_stat_64& s) {
    return {std::move(path), s.smb2_size, s.smb2_ino, s.smb2_mtime, s.smb2_mtime_nsec,
            s.smb2_type == SMB2_TYPE_DIRECTORY};
}
Entry FromNfs(std::string path, const nfs_stat_64& s) {
    return {std::move(path), s.nfs_size, s.nfs_ino, s.nfs_mtime, s.nfs_mtime_nsec,
            S_ISDIR(s.nfs_mode)};
}
bool Same(const Entry& a, const Entry& b) {
    return a.size == b.size && a.inode == b.inode && a.modified == b.modified &&
           a.modified_ns == b.modified_ns && !b.directory;
}
struct Completion {
    bool done = false;
    int status = -EIO;
    void* data = nullptr;
    nfs_stat_64 stat{};
    static void Smb(smb2_context*, int status, void* data, void* cookie) {
        auto& c = *static_cast<Completion*>(cookie); c.status = status; c.data = data; c.done = true;
    }
    static void Nfs(int status, nfs_context*, void* data, void* cookie) {
        auto& c = *static_cast<Completion*>(cookie); c.status = status; c.data = data; c.done = true;
    }
    static void NfsStat(int status, nfs_context* nfs, void* data, void* cookie) {
        if (status == 0 && data) static_cast<Completion*>(cookie)->stat = *static_cast<nfs_stat_64*>(data);
        Nfs(status, nfs, data, cookie);
    }
};
} // namespace

bool SafePath(std::string_view path, bool allow_empty) {
    if (path.empty()) return allow_empty;
    if (path.size() > 2048) return false;
    for (std::size_t at = 0; at < path.size();) {
        const auto end = path.find('/', at);
        if (!Component(path.substr(at, end == path.npos ? path.size() - at : end - at))) return false;
        if (end == path.npos) return true;
        at = end + 1;
        if (at == path.size()) return false;
    }
    return false;
}
bool Valid(const Share& s) {
    in_addr address{};
    const bool export_ok = s.protocol == "nfs" && s.share.starts_with('/') &&
        (s.share == "/" || SafePath(std::string_view{s.share}.substr(1)));
    const auto bounded = [](const std::string& text) { return text.size() <= 256 && text.find('\0') == text.npos; };
    return Component(s.id) && s.id.size() <= 40 && bounded(s.name) &&
        s.server.size() <= 15 && s.server.find('\0') == s.server.npos &&
        inet_pton(AF_INET, s.server.c_str(), &address) == 1 && address.s_addr != INADDR_ANY &&
        address.s_addr != INADDR_BROADCAST &&
        (s.protocol == "smb" ? Component(s.share) : export_ok) && SafePath(s.folder, true) &&
        SafePath(s.updates_dir, true) &&
        bounded(s.user) && bounded(s.password) && bounded(s.domain) &&
        (s.protocol != "smb" || !s.user.empty() || (s.password.empty() && s.domain.empty() && !s.encrypt)) &&
        (s.protocol != "nfs" || (s.user.empty() && s.password.empty() && s.domain.empty() && !s.encrypt)) &&
        s.port > 0 && s.port <= 65535 && s.mount_port >= 0 && s.mount_port <= 65535 &&
        s.uid >= 0 && s.gid >= 0 && s.timeout_ms >= 250 && s.timeout_ms <= 15000 &&
        s.cache_bytes >= kBlock && s.cache_bytes <= (32u << 20) && s.cache_bytes % kBlock == 0;
}
bool IsPath(std::string_view path) { return path.starts_with("pe-net://"); }
std::string Uri(std::string_view id, std::string_view path) {
    return Component(id) && SafePath(path) ? "pe-net://" + std::string{id} + '/' + std::string{path} : "";
}

struct Session::Impl {
    Share share;
    mutable std::mutex mutex;
    std::atomic<bool> cancelled{false}, healthy{false};
    std::string error;
    smb2_context* smb = nullptr;
    nfs_context* nfs = nullptr;
    struct Handle { smb2fh* smb = nullptr; nfsfh* nfs = nullptr; Entry identity; };
    std::map<std::string, Handle> handles;
    using Key = std::pair<std::string, std::uint64_t>;
    struct Block { std::vector<unsigned char> bytes; std::list<Key>::iterator age; Entry identity; };
    std::map<Key, Block> cache;
    std::list<Key> ages;
    std::size_t cached = 0;
    std::uint64_t reads = 0;
    std::uint64_t transferred = 0;
    explicit Impl(Share s) : share(std::move(s)) {}
    void Destroy() {
        // NFSv3 read-only close is local and immediate (no COMMIT/CLOSE RPC). Release handles
        // before destroying their context. SMB context destruction owns all its handles.
        if (nfs) {
            for (auto& [path, h] : handles) if (h.nfs) {
                Completion c; nfs_close_async(nfs, h.nfs, Completion::Nfs, &c);
            }
            nfs_destroy_context(nfs); nfs = nullptr;
        }
        if (smb) { smb2_destroy_context(smb); smb = nullptr; }
        handles.clear(); cache.clear(); ages.clear(); cached = 0;
        healthy = false;
    }
    bool Fail(const char* message) { if (error.empty()) error = message; Destroy(); return false; }
    bool Check() {
        if (cancelled) return Fail("Network share disconnected or cancelled. Reconnect in Settings.");
        return healthy;
    }
    bool Wait(int queued, Completion& c, Clock::time_point deadline) {
        if (queued < 0) return Fail("Network share request failed. Check access permissions and reconnect.");
        while (!c.done) {
            if (cancelled) return Fail("Network share disconnected or cancelled. Reconnect in Settings.");
            if (Clock::now() >= deadline) return Fail("Network share timed out. Check the connection and reconnect in Settings.");
            pollfd fd{nfs ? nfs_get_fd(nfs) : smb2_get_fd(smb),
                      static_cast<short>(nfs ? nfs_which_events(nfs) : smb2_which_events(smb)), 0};
            const int result = poll(&fd, 1, 25);
            if (result < 0 && errno == EINTR) continue;
            if (result < 0) return Fail("Network share socket failed. Reconnect in Settings.");
            if ((nfs ? nfs_service(nfs, fd.revents) : smb2_service(smb, fd.revents)) < 0)
                return Fail("Network share connection lost. Reconnect in Settings and start the game again.");
        }
        if (c.status < 0) return Fail("Network share read or access failed. Reconnect in Settings.");
        return true;
    }
    Clock::time_point Deadline() const { return Clock::now() + std::chrono::milliseconds(share.timeout_ms); }
    std::string Path(std::string_view relative) const {
        auto path = share.folder;
        if (!relative.empty()) { if (!path.empty()) path += '/'; path += relative; }
        return nfs ? '/' + path : path;
    }
    bool RawStat(const std::string& path, Entry* entry, Clock::time_point deadline) {
        Completion c;
        if (nfs) {
            if (!Wait(nfs_lstat64_async(nfs, path.c_str(), Completion::NfsStat, &c), c, deadline)) return false;
            if (!S_ISREG(c.stat.nfs_mode) && !S_ISDIR(c.stat.nfs_mode))
                return Fail("Network share links and special files are not supported.");
            *entry = FromNfs(path, c.stat);
        } else {
            smb2_stat_64 st{};
            if (!Wait(smb2_stat_async(smb, path.c_str(), &st, Completion::Smb, &c), c, deadline)) return false;
            if (st.smb2_type != SMB2_TYPE_FILE && st.smb2_type != SMB2_TYPE_DIRECTORY)
                return Fail("Network share links and special files are not supported.");
            *entry = FromSmb(path, st);
        }
        return true;
    }
    bool Stat(std::string_view relative, Entry* entry, Clock::time_point deadline) {
        if (!SafePath(relative, true)) return false;
        const std::string path = Path(relative);
        // Check each prefix too: never intentionally follow links out of the configured folder.
        for (std::size_t at = nfs ? 1 : 0; at < path.size();) {
            const auto slash = path.find('/', at);
            if (slash == path.npos) break;
            Entry parent;
            if (!RawStat(path.substr(0, slash), &parent, deadline) || !parent.directory) return false;
            at = slash + 1;
        }
        if (!RawStat(path, entry, deadline)) return false;
        entry->path = relative;
        return true;
    }
    Handle* Open(const Entry& entry, Clock::time_point deadline) {
        if (auto found = handles.find(entry.path); found != handles.end()) {
            if (!Same(entry, found->second.identity)) {
                Fail("Network game changed within this session. Reconnect and rescan."); return nullptr;
            }
            return &found->second;
        }
        if (handles.size() >= 16) {
            auto first = handles.begin(); Completion c;
            const int queued = nfs ? nfs_close_async(nfs, first->second.nfs, Completion::Nfs, &c) :
                smb2_close_async(smb, first->second.smb, Completion::Smb, &c);
            // SMB close consumes the handle even if its reply fails.
            handles.erase(first);
            if (!Wait(queued, c, deadline)) return nullptr;
        }
        Entry current;
        if (!Stat(entry.path, &current, deadline) || !Same(entry, current)) {
            Fail("Network game changed since discovery. Reconnect and rescan before playing."); return nullptr;
        }
        Completion c;
        const std::string path = Path(entry.path);
        const int queued = nfs ? nfs_open_async(nfs, path.c_str(), O_RDONLY | O_NOFOLLOW, Completion::Nfs, &c) :
            smb2_open_async(smb, path.c_str(), O_RDONLY, Completion::Smb, &c);
        if (!Wait(queued, c, deadline) || !c.data) return nullptr;
        Handle h; h.identity = entry;
        if (nfs) h.nfs = static_cast<nfsfh*>(c.data); else h.smb = static_cast<smb2fh*>(c.data);
        auto& held = handles.emplace(entry.path, h).first->second;
        Completion stat;
        Entry opened;
        if (nfs) {
            if (!Wait(nfs_fstat64_async(nfs, h.nfs, Completion::NfsStat, &stat), stat, deadline)) return nullptr;
            opened = FromNfs(entry.path, stat.stat);
        } else {
            smb2_stat_64 st{};
            if (!Wait(smb2_fstat_async(smb, h.smb, &st, Completion::Smb, &stat), stat, deadline)) return nullptr;
            opened = FromSmb(entry.path, st);
        }
        if (!Same(entry, opened)) { Fail("Network game changed while opening. Reconnect and rescan."); return nullptr; }
        return &held;
    }
};

Session::Session(Share share) : impl(std::make_unique<Impl>(std::move(share))) {}
Session::~Session() { impl->Destroy(); }
bool Session::Connect() {
    std::lock_guard lock(impl->mutex);
    auto& i = *impl;
    if (!Valid(i.share) || i.cancelled) return i.Fail("Invalid or cancelled network share configuration.");
    if (i.healthy) return true;
    if (!i.error.empty()) return false;
    Completion c;
    if (i.share.protocol == "smb") {
        i.smb = smb2_init_context();
        if (!i.smb) return i.Fail("Cannot allocate SMB connection.");
        smb2_set_timeout(i.smb, (i.share.timeout_ms + 999) / 1000);
        // libsmb2 6.3 always signs SMB3.1.1 TREE_CONNECT, including guest sessions which have
        // no shared signing key. Explicit guest uses SMB2.x; authenticated shares negotiate 2/3.
        smb2_set_version(i.smb, i.share.user.empty() ? SMB2_VERSION_ANY2 : SMB2_VERSION_ANY);
        // Kerberos/GSSAPI are disabled in our pinned build; the default is NTLMSSP.
        smb2_set_user(i.smb, i.share.user.c_str());
        smb2_set_password(i.smb, i.share.password.c_str());
        smb2_set_domain(i.smb, i.share.domain.c_str());
        // An empty user explicitly selects guest access. Configured credentials require signing;
        // there is no retry as guest or SMB1 if authentication fails.
        smb2_set_sign(i.smb, i.share.user.empty() ? 0 : 1);
        smb2_set_seal(i.smb, i.share.encrypt ? 1 : 0);
        const auto server = i.share.server + ':' + std::to_string(i.share.port);
        // User/password/domain were explicitly set above. Passing a user again makes libsmb2
        // reread NTLM_USER_FILE and overwrite the configured password with ambient credentials.
        if (!i.Wait(smb2_connect_share_async(i.smb, server.c_str(), i.share.share.c_str(),
                    nullptr, Completion::Smb, &c), c, i.Deadline())) return false;
    } else {
        i.nfs = nfs_init_context();
        if (!i.nfs) return i.Fail("Cannot allocate NFS connection.");
        nfs_set_version(i.nfs, 3);
        nfs_set_readonly(i.nfs, 1);
        nfs_set_uid(i.nfs, i.share.uid); nfs_set_gid(i.nfs, i.share.gid);
        nfs_set_debug(i.nfs, 0); nfs_set_autoreconnect(i.nfs, 0); nfs_set_retrans(i.nfs, 0);
        nfs_set_auto_traverse_mounts(i.nfs, 0); nfs_set_dircache(i.nfs, 0);
        nfs_set_readmax(i.nfs, kBlock); nfs_set_timeout(i.nfs, i.share.timeout_ms);
        nfs_set_nfsport(i.nfs, i.share.port);
        if (i.share.mount_port) nfs_set_mountport(i.nfs, i.share.mount_port);
        if (!i.Wait(nfs_mount_async(i.nfs, i.share.server.c_str(), i.share.share.c_str(), Completion::Nfs, &c),
                    c, i.Deadline())) return false;
    }
    i.healthy = true;
    Entry root;
    if (!i.Stat("", &root, i.Deadline()) || !root.directory) return i.Fail("Network game folder is not a directory.");
    return true;
}
void Session::Cancel() { impl->cancelled = true; impl->healthy = false; }
bool Session::Healthy() const { return impl->healthy && !impl->cancelled; }
std::string Session::Error() const {
    std::lock_guard lock(impl->mutex);
    return impl->cancelled ? "Network share disconnected or cancelled. Reconnect in Settings." : impl->error;
}
bool Session::Stat(std::string_view path, Entry* entry) {
    std::lock_guard lock(impl->mutex);
    return entry && impl->Check() && impl->Stat(path, entry, impl->Deadline());
}
bool Session::List(std::string_view path, std::vector<Entry>* entries) {
    std::lock_guard lock(impl->mutex);
    auto& i = *impl;
    if (!entries || !SafePath(path, true) || !i.Check() || !ChargeTransfer(0)) return false;
    entries->clear();
    Entry parent;
    const auto deadline = i.Deadline();
    if (!i.Stat(path, &parent, deadline) || !parent.directory) return false;
    const auto full = i.Path(path);
    Completion c;
    const int queued = i.nfs ? nfs_opendir_async(i.nfs, full.c_str(), Completion::Nfs, &c) :
        smb2_opendir_async(i.smb, full.c_str(), Completion::Smb, &c);
    if (!i.Wait(queued, c, deadline) || !c.data) return false;
    bool overflow = false;
    const auto append = [&](const char* name, Entry e) {
        if (!name || !Component(name)) return;
        if (entries->size() >= kEntries) { overflow = true; return; }
        e.path = path.empty() ? name : std::string{path} + '/' + name;
        entries->push_back(std::move(e));
    };
    if (i.nfs) {
        auto* dir = static_cast<nfsdir*>(c.data);
        while (auto* e = nfs_readdir(i.nfs, dir)) {
            if (e->type != 1 && e->type != 2) continue; // NF3REG, NF3DIR only
            append(e->name, {"", e->size, e->inode, static_cast<std::uint64_t>(e->mtime.tv_sec),
                            e->mtime_nsec, e->type == 2});
        }
        nfs_closedir(i.nfs, dir);
    } else {
        auto* dir = static_cast<smb2dir*>(c.data);
        while (auto* e = smb2_readdir(i.smb, dir)) {
            if (e->st.smb2_type != SMB2_TYPE_FILE && e->st.smb2_type != SMB2_TYPE_DIRECTORY) continue;
            append(e->name, FromSmb("", e->st));
        }
        smb2_closedir(i.smb, dir);
    }
    if (overflow) { entries->clear(); return i.Fail("Network folder exceeds the 4096 entry limit. Use a smaller game folder."); }
    return true;
}
std::size_t Session::Read(const Entry& entry, unsigned char* data, std::size_t size, std::uint64_t offset) {
    std::lock_guard lock(impl->mutex);
    auto& i = *impl;
    if (!i.Check() || !SafePath(entry.path) || entry.directory || (!data && size)) return 0;
    if (offset >= entry.size || size == 0) return 0;
    size = static_cast<std::size_t>(std::min<std::uint64_t>(size, entry.size - offset));
    const auto deadline = i.Deadline(); // entire read, not one timeout per packet
    auto* handle = i.Open(entry, deadline);
    if (!handle) return 0;
    std::size_t done = 0;
    while (done < size) {
        if (!i.Check()) return 0;
        const std::uint64_t block_at = (offset + done) / kBlock * kBlock;
        Impl::Key key{entry.path, block_at};
        auto found = i.cache.find(key);
        if (found == i.cache.end()) {
            const auto bytes = static_cast<std::size_t>(std::min<std::uint64_t>(kBlock, entry.size - block_at));
            if (!ChargeTransfer(bytes)) return 0;
            while (i.cached + bytes > i.share.cache_bytes && !i.ages.empty()) {
                const auto victim = i.cache.find(i.ages.back());
                i.cached -= victim->second.bytes.size(); i.cache.erase(victim); i.ages.pop_back();
            }
            std::vector<unsigned char> block(bytes);
            std::size_t read = 0;
            while (read < bytes) {
                Completion c;
                const auto max_read = i.nfs ? nfs_get_readmax(i.nfs) : smb2_get_max_read_size(i.smb);
                const auto count = std::min<std::size_t>(bytes - read, max_read);
                if (!count) { i.Fail("Network server returned an invalid read limit."); return 0; }
                ++i.reads;
                const int queued = i.nfs ? nfs_pread_async(i.nfs, handle->nfs, block.data() + read, count,
                    block_at + read, Completion::Nfs, &c) : smb2_pread_async(i.smb, handle->smb, block.data() + read,
                    static_cast<std::uint32_t>(count), block_at + read, Completion::Smb, &c);
                if (!i.Wait(queued, c, deadline)) return 0;
                if (c.status <= 0 || static_cast<std::size_t>(c.status) > count) {
                    i.Fail("Network game was truncated or returned an invalid read. Start again after reconnecting."); return 0;
                }
                read += static_cast<std::size_t>(c.status);
                i.transferred += static_cast<std::size_t>(c.status);
            }
            i.ages.push_front(key); i.cached += block.size();
            found = i.cache.emplace(key, Impl::Block{std::move(block), i.ages.begin(), entry}).first;
        }
        if (!Same(entry, found->second.identity)) {
            i.Fail("Network game changed within this session. Reconnect and rescan."); return 0;
        }
        i.ages.splice(i.ages.begin(), i.ages, found->second.age);
        const auto in_block = static_cast<std::size_t>(offset + done - block_at);
        const auto count = std::min(size - done, found->second.bytes.size() - in_block);
        std::memcpy(data + done, found->second.bytes.data() + in_block, count);
        done += count;
    }
    return done;
}
std::size_t Session::CachedBytes() const { std::lock_guard lock(impl->mutex); return impl->cached; }
std::uint64_t Session::TransportReads() const { std::lock_guard lock(impl->mutex); return impl->reads; }
std::uint64_t Session::TransferredBytes() const { std::lock_guard lock(impl->mutex); return impl->transferred; }
} // namespace Eden::Network
