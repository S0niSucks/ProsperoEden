// SPDX-License-Identifier: GPL-3.0-or-later
#include "usb_backup.h"
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <ctime>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <utility>
#if defined(__PROSPERO__)
extern "C" int sceKernelGetdents(int, char*, int);
#endif

namespace Eden::UsbBackup {
namespace {
using Json = nlohmann::json;
constexpr std::size_t kEntries = 100000, kManifest = 16 * 1024 * 1024, kChunk = 256 * 1024;
constexpr std::uint64_t kReserve = 32 * 1024 * 1024;
void Need(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void Check(const std::atomic<bool>& stop) { Need(!stop, "Cancelled. Existing data was not replaced."); }
struct Fd {
    int n = -1;
    explicit Fd(int n = -1) : n(n) {}
    ~Fd() { if (n >= 0) close(n); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : n(std::exchange(other.n, -1)) {}
    Fd& operator=(Fd&& other) noexcept { if (n >= 0) close(n); n = std::exchange(other.n, -1); return *this; }
};
bool Component(std::string_view s) {
    if (s.empty() || s == "." || s == ".." || s.size() > 255) return false;
    for (unsigned char c : s) if (c < 32 || c == 127 || c == '/' || c == '\\' || c == ':') return false;
    return true;
}
std::vector<std::string> Parts(std::string_view path) {
    Need(!path.empty() && path.size() <= 1024, "Invalid or overlong path.");
    std::vector<std::string> parts;
    while (!path.empty()) {
        auto end = path.find('/');
        auto part = path.substr(0, end);
        Need(Component(part) && parts.size() < 32, "Unsafe path component or excessive depth.");
        parts.emplace_back(part);
        if (end == path.npos) break;
        path.remove_prefix(end + 1);
        Need(!path.empty(), "Empty path component.");
    }
    return parts;
}
Fd DirAt(int fd, const std::string& name, bool create = false) {
    Need(Component(name), "Unsafe directory name.");
    if (create) {
        Need(mkdirat(fd, name.c_str(), 0700) == 0 || errno == EEXIST, "Cannot create directory.");
        // Sync the directory ENTRY in its parent as well as later syncing the child. Otherwise
        // power loss after moving an original could lose its newly created recovery ancestor.
        Need(fsync(fd) == 0, "Cannot durably create directory; operation stopped.");
    }
    Fd result{openat(fd, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)};
    Need(result.n >= 0, "Cannot safely open directory (link, missing drive or unsupported filesystem).");
    return result;
}
Fd Root(const std::string& path) {
    Need(path.size() > 1 && path.front() == '/', "An absolute directory is required.");
    auto parts = Parts(std::string_view{path}.substr(1));
    Fd fd{open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC)};
    Need(fd.n >= 0, "Cannot open filesystem root.");
    for (const auto& part : parts) fd = DirAt(fd.n, part);
    return fd;
}
Fd Parent(int root, const std::string& path, bool create = false) {
    auto parts = Parts(path);
    Fd fd{dup(root)};
    Need(fd.n >= 0, "Cannot duplicate directory descriptor.");
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        struct stat parent{}, child{};
        Need(fstat(fd.n, &parent) == 0, "Cannot identify directory filesystem.");
        auto next = DirAt(fd.n, parts[i], create);
        Need(fstat(next.n, &child) == 0 && parent.st_dev == child.st_dev, "Nested mounts are not supported.");
        fd = std::move(next);
    }
    return fd;
}
std::string Leaf(const std::string& path) { return path.substr(path.find_last_of('/') + 1); }
void Sync(int fd) { Need(fsync(fd) == 0, "Filesystem cannot durably sync data; operation stopped."); }
std::uint64_t Available(int fd, std::uint64_t& allocation) {
    struct statvfs info{};
    Need(fstatvfs(fd, &info) == 0 && info.f_frsize && info.f_bavail <= UINT64_MAX / info.f_frsize,
         "Cannot safely determine available space.");
    allocation = std::max<std::uint64_t>(65536, info.f_frsize);
    return info.f_bavail * info.f_frsize;
}
bool Excluded(const std::string& path) {
    // Never open/enumerate these exact credential nodes (including any hostile children).
    // Be conservative across the case-insensitive USB namespace and adapters that alias a
    // trailing dot/space. Dot segments, backslashes and absolute paths are rejected by Parts.
    const auto policy_name = [](std::string_view part, std::string_view expected) {
        while (!part.empty() && (part.back() == '.' || part.back() == ' ')) part.remove_suffix(1);
        if (part.size() != expected.size()) return false;
        for (std::size_t i = 0; i < part.size(); ++i) {
            char c = part[i]; if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            if (c != expected[i]) return false;
        }
        return true;
    };
    const std::string_view relative{path};
    const auto slash = relative.find('/');
    if (slash != relative.npos && policy_name(relative.substr(0, slash), "config")) {
        const auto tail = relative.substr(slash + 1);
        const auto node = tail.substr(0, tail.find('/'));
        if (policy_name(node, "nextendo") || policy_name(node, "network-shares.json")) return true;
    }
    const auto under = [&](std::string_view p) { return path == p || (path.starts_with(p) && path[p.size()] == '/'); };
    for (auto p : {"roms", "updates", "switch", "logs", "cache", "radv-shader-cache", ".remote-downloads",
                   "user/cache", "user/log", "user/shader", "library.json"}) if (under(p)) return true;
    std::string ext = path.substr(path.find_last_of('.') + 1);
    for (auto& c : ext) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return ext == "nsp" || ext == "xci" || ext == "nsz" || ext == "xcz" || ext == "nro";
}
std::vector<std::string> Names(int fd, const std::atomic<bool>& stop, std::size_t limit = kEntries) {
    std::vector<std::string> result;
    auto add = [&](const char* name) {
        Check(stop);
        if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0) return;
        Need(Component(name) && result.size() < limit, "Unsafe name or too many directory entries.");
        result.emplace_back(name);
    };
    // A fresh open, not dup(): enumeration must not share the caller's directory offset.
    Fd copy{openat(fd, ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)};
    Need(copy.n >= 0, "Cannot enumerate directory safely.");
#if defined(__PROSPERO__)
    std::vector<char> buffer(65536);
    for (;;) {
        int n = sceKernelGetdents(copy.n, buffer.data(), buffer.size());
        Need(n >= 0 && n <= static_cast<int>(buffer.size()), "Cannot read native directory.");
        if (!n) break;
        for (std::size_t at = 0; at < static_cast<std::size_t>(n);) {
            constexpr auto offset = offsetof(dirent, d_name);
            Need(n - at > offset, "Invalid native directory record.");
            std::uint16_t length;
            std::memcpy(&length, buffer.data() + at + offsetof(dirent, d_reclen), sizeof(length));
            Need(length > offset && length <= n - at, "Invalid native directory length.");
            const char* name = buffer.data() + at + offset;
            Need(std::memchr(name, 0, length - offset), "Invalid native directory name.");
            add(name); at += length;
        }
    }
#else
    DIR* raw = fdopendir(std::exchange(copy.n, -1));
    Need(raw, "Cannot enumerate directory.");
    std::unique_ptr<DIR, int(*)(DIR*)> dir(raw, closedir);
    for (;;) {
        errno = 0; auto* entry = readdir(dir.get());
        if (!entry) { Need(errno == 0, "Directory enumeration failed."); break; }
        add(entry->d_name);
    }
#endif
    std::sort(result.begin(), result.end());
    Need(std::adjacent_find(result.begin(), result.end()) == result.end(), "Duplicate directory name.");
    return result;
}
struct Entry {
    std::string path, hash;
    bool directory = false, had = false;
    std::uint64_t size = 0;
};
using Entries = std::vector<Entry>;
struct stat Info(int fd) { struct stat s{}; Need(fstat(fd, &s) == 0, "Cannot stat open file."); return s; }
void Scan(int fd, const std::string& prefix, dev_t device, Entries& out, const std::atomic<bool>& stop,
          bool strict, std::size_t& visited) {
    auto names = Names(fd, stop, kEntries - visited); visited += names.size();
    for (const auto& name : names) {
        std::string path = prefix + name;
        if (Excluded(path)) { Need(!strict, "Excluded content was added to a backup payload."); continue; }
        Parts(path);
        Fd child{openat(fd, name.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)};
        Need(child.n >= 0, "Unsafe link, unreadable file or missing drive in app data.");
        auto s = Info(child.n);
        Need(s.st_dev == device && (S_ISDIR(s.st_mode) || (S_ISREG(s.st_mode) && s.st_nlink == 1)) && s.st_size >= 0,
             "Links, special files and nested mounts are not supported.");
        Need(out.size() < kEntries, "Too many files for one backup.");
        const bool directory = S_ISDIR(s.st_mode);
        out.push_back({path, {}, directory, false, directory ? 0 : static_cast<std::uint64_t>(s.st_size)});
        if (directory) Scan(child.n, path + "/", device, out, stop, strict, visited);
    }
}
Entries Scan(int fd, const std::atomic<bool>& stop, bool strict = false) {
    Entries entries; std::size_t visited = 0;
    Scan(fd, "", Info(fd).st_dev, entries, stop, strict, visited); return entries;
}
Estimate Space(const Entries& entries, int target, unsigned copies = 1) {
    Estimate result;
    std::uint64_t allocation{}; result.available = Available(target, allocation);
    for (const auto& e : entries) {
        Need(e.size < UINT64_MAX - result.bytes, "Data size overflow."); result.bytes += e.size;
        result.files += !e.directory;
    }
    // Metadata + conservative allocation rounding; retain a reserve, never consume the drive.
    Need(entries.size() <= (UINT64_MAX - kReserve - kManifest) / allocation / copies, "Allocation size overflow.");
    const auto overhead = entries.size() * allocation * copies;
    Need(result.bytes <= (UINT64_MAX - kReserve - kManifest - overhead) / copies, "Data size overflow.");
    result.required = result.bytes * copies + kReserve + kManifest + overhead;
    return result;
}
std::string Hash(int fd, int output, const Entry& entry, const std::atomic<bool>& stop,
                 Progress& progress, const Report& report) {
    auto before = Info(fd);
    Need(S_ISREG(before.st_mode) && before.st_nlink == 1 && before.st_size >= 0 &&
         static_cast<std::uint64_t>(before.st_size) == entry.size, "Source changed or is not a plain file.");
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    Need(ctx && EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) == 1, "Cannot initialize checksum.");
    std::vector<char> buffer(kChunk); std::uint64_t copied = 0;
    for (;;) {
        Check(stop);
        auto n = read(fd, buffer.data(), buffer.size());
        if (n < 0 && errno == EINTR) continue;
        Need(n >= 0, "Read failed (drive disconnected or file unreadable).");
        if (!n) break;
        Need(static_cast<std::uint64_t>(n) <= entry.size - copied, "Source grew during copy.");
        Need(EVP_DigestUpdate(ctx.get(), buffer.data(), n) == 1, "Checksum failed.");
        for (ssize_t at = 0; output >= 0 && at < n;) {
            Check(stop);
            auto written = write(output, buffer.data() + at, n - at);
            if (written < 0 && errno == EINTR) continue;
            Need(written > 0, "Write failed (drive full, disconnected or read-only)."); at += written;
        }
        copied += n; progress.done += n; progress.path = entry.path;
        if (report) report(progress);
    }
    auto after = Info(fd);
    Need(copied == entry.size && after.st_size == before.st_size && after.st_mtime == before.st_mtime &&
         after.st_ctime == before.st_ctime, "Source changed during copy.");
    std::array<unsigned char, 32> digest{}; unsigned count{};
    Need(EVP_DigestFinal_ex(ctx.get(), digest.data(), &count) == 1 && count == 32, "Checksum failed.");
    std::string hash;
    for (auto c : digest) { hash += "0123456789abcdef"[c >> 4]; hash += "0123456789abcdef"[c & 15]; }
    return hash;
}
Fd FileAt(int root, const std::string& path) {
    auto parent = Parent(root, path);
    Fd fd{openat(parent.n, Leaf(path).c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)};
    Need(fd.n >= 0, "Cannot safely read file."); return fd;
}
void Write(int root, const std::string& name, const std::string& text, bool* created = nullptr) {
    if (created) *created = false;
    Need(text.size() <= kManifest, "Manifest exceeds size limit.");
    Fd out{openat(root, name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600)};
    Need(out.n >= 0, "Cannot exclusively create metadata (existing data was kept).");
    if (created) *created = true;
    for (std::size_t at = 0; at < text.size();) {
        auto n = write(out.n, text.data() + at, std::min(kChunk, text.size() - at));
        if (n < 0 && errno == EINTR) continue;
        Need(n > 0, "Cannot write metadata."); at += n;
    }
    Sync(out.n); Sync(root);
}
std::string Read(int root, const std::string& name) {
    auto fd = FileAt(root, name); auto s = Info(fd.n);
    Need(S_ISREG(s.st_mode) && s.st_nlink == 1 && s.st_size > 0 && s.st_size <= static_cast<off_t>(kManifest),
         "Invalid metadata file.");
    std::string text(s.st_size, '\0'); std::size_t done = 0;
    while (done < text.size()) {
        auto n = read(fd.n, text.data() + done, std::min(kChunk, text.size() - done));
        if (n < 0 && errno == EINTR) continue;
        Need(n > 0, "Truncated metadata."); done += n;
    }
    return text;
}
std::string Digest(const std::string& text) {
    std::array<unsigned char, 32> hash{}; unsigned size{};
    Need(EVP_Digest(text.data(), text.size(), hash.data(), &size, EVP_sha256(), nullptr) == 1 && size == 32,
         "Cannot hash manifest.");
    std::string result;
    for (auto c : hash) { result += "0123456789abcdef"[c >> 4]; result += "0123456789abcdef"[c & 15]; }
    return result;
}
std::string Manifest(const Entries& entries) {
    Json list = Json::array();
    for (const auto& e : entries) list.push_back({{"path", e.path}, {"directory", e.directory}, {"size", e.size},
                                                {"sha256", e.hash}, {"had", e.had}});
    auto text = Json{{"format", "ProsperoEden USB backup"}, {"version", 1}, {"entries", list}}.dump();
    Need(text.size() <= kManifest, "Manifest exceeds size limit."); return text;
}
Json ParseJson(const std::string& text) {
    return Json::parse(text, [](int depth, Json::parse_event_t, Json&) {
        Need(depth < 16, "Excessively nested backup metadata."); return true;
    });
}
Entries Parse(const std::string& text) {
    auto j = ParseJson(text);
    Need(j.at("format") == "ProsperoEden USB backup" && j.at("version") == 1 && j.at("entries").is_array() &&
         j.at("entries").size() <= kEntries, "Unsupported backup manifest.");
    Entries result; std::set<std::string> dirs, paths;
    for (const auto& value : j.at("entries")) {
        Need(value.at("size").is_number_unsigned() && value.at("directory").is_boolean() && value.at("had").is_boolean(),
             "Malformed manifest entry.");
        Entry e{value.at("path").get<std::string>(), value.at("sha256").get<std::string>(),
                value.at("directory").get<bool>(), value.at("had").get<bool>(), value.at("size").get<std::uint64_t>()};
        Parts(e.path);
        Need(!Excluded(e.path) && paths.insert(e.path).second, "Excluded or duplicate manifest path.");
        if (auto slash = e.path.find_last_of('/'); slash != e.path.npos)
            Need(dirs.contains(e.path.substr(0, slash)), "Missing or unordered parent directory.");
        if (e.directory) { Need(e.size == 0 && e.hash.empty(), "Invalid directory entry."); dirs.insert(e.path); }
        else Need(e.hash.size() == 64 && std::all_of(e.hash.begin(), e.hash.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }), "Invalid content checksum.");
        result.push_back(std::move(e));
    }
    return result;
}
void Copy(int source, int target, Entries& entries, const std::atomic<bool>& stop, const Report& report) {
    Progress p{"copying", {}, 0, 0}; for (auto& e : entries) p.total += e.size;
    if (report) report(p);
    for (auto& e : entries) {
        Check(stop); auto parent = Parent(target, e.path, true);
        if (e.directory) { auto dir = DirAt(parent.n, Leaf(e.path), true); Sync(dir.n); }
        else {
            auto in = FileAt(source, e.path);
            Fd out{openat(parent.n, Leaf(e.path).c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600)};
            Need(out.n >= 0, "Cannot create backup file without overwriting.");
            auto hash = Hash(in.n, out.n, e, stop, p, report);
            Need(e.hash.empty() || hash == e.hash, "Source content differs from manifest.");
            e.hash = std::move(hash); Sync(out.n);
        }
        Sync(parent.n);
    }
    Sync(target);
}
void Validate(int payload, const Entries& entries, const std::atomic<bool>& stop, const Report& report) {
    auto actual = Scan(payload, stop, true);
    Need(actual.size() == entries.size(), "Backup content inventory differs from manifest.");
    // Scan is sorted depth-first, as manifests produced here; reject extra files and renamed paths.
    Progress p{"validating", {}, 0, 0}; for (auto& e : entries) p.total += e.size;
    if (report) report(p);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        Check(stop); const auto& e = entries[i]; const auto& a = actual[i];
        Need(e.path == a.path && e.directory == a.directory && e.size == a.size, "Backup inventory mismatch.");
        if (!e.directory) { auto in = FileAt(payload, e.path);
            Need(Hash(in.n, -1, e, stop, p, report) == e.hash, "Backup content checksum mismatch."); }
    }
}
Entries Completed(int folder, const std::atomic<bool>& stop, const Report& report = {},
                  const std::string& expected = {}, std::string* actual = nullptr) {
    auto text = Read(folder, "manifest.json");
    auto hash = Digest(text);
    Need(Read(folder, "COMPLETE") == hash, "Backup has no valid completion marker.");
    Need(expected.empty() || expected == hash, "Backup manifest changed after confirmation. Inspect it again.");
    if (actual) *actual = hash;
    auto entries = Parse(text); auto payload = DirAt(folder, "data"); Validate(payload.n, entries, stop, report);
    return entries;
}
std::string Unique(int root, const std::string& prefix, const std::atomic<bool>& stop) {
    const auto now = std::time(nullptr); std::tm time{}; Need(gmtime_r(&now, &time), "Cannot read backup date.");
    char stamp[32]{}; std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", &time);
    for (unsigned i = 0; i < 1000; ++i) {
        Check(stop);
        auto name = prefix + stamp + "-" + std::to_string(i);
        if (mkdirat(root, name.c_str(), 0700) == 0) { Sync(root); return name; }
        Need(errno == EEXIST, "Cannot create dated folder on drive.");
    }
    throw std::runtime_error("Too many dated folder collisions; no backup was overwritten.");
}
Fd RestoreRoot(const std::string& data) {
    auto slash = data.find_last_of('/');
    Need(slash > 0 && Component(data.substr(slash + 1)), "Invalid data root.");
    auto parent = Root(data.substr(0, slash));
    return DirAt(parent.n, "." + data.substr(slash + 1) + "-restores", true);
}
bool Exists(int root, const std::string& path) {
    // fstatat with NOFOLLOW is mandatory: never downgrade on native EPERM/ENOSYS.
    struct stat s{};
    if (fstatat(root, path.c_str(), &s, AT_SYMLINK_NOFOLLOW) == 0) return true;
    Need(errno == ENOENT, "Filesystem cannot safely check existing data."); return false;
}
void TargetPlan(int data, Entries& entries) {
    for (auto& e : entries) {
        // Missing directories are created at apply, not while active data is open.
        Fd parent{dup(data)}; bool missing = false;
        const auto parts = Parts(e.path);
        for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
            if (!Exists(parent.n, parts[i])) { missing = true; break; }
            auto child = DirAt(parent.n, parts[i]);
            Need(Info(parent.n).st_dev == Info(child.n).st_dev, "Nested restore mounts are not supported.");
            parent = std::move(child);
        }
        e.had = !missing && Exists(parent.n, parts.back());
        if (!e.had) continue;
        if (e.directory) { auto dir = DirAt(parent.n, parts.back()); }
        else { auto file = FileAt(parent.n, parts.back()); auto s = Info(file.n);
            Need(S_ISREG(s.st_mode) && s.st_nlink == 1, "Restore target is a link, directory or special file."); }
    }
}
// Remove only our unpublished staging tree. No path traversal, following links or broad remove_all.
void Cleanup(int root, const std::atomic<bool>& stop, unsigned depth = 0) {
    Need(depth <= 35, "Cleanup depth exceeded.");
    for (const auto& name : Names(root, stop)) {
        Fd child{openat(root, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)};
        if (child.n >= 0) {
            Need(Info(child.n).st_dev == Info(root).st_dev, "Unsafe nested mount in staging cleanup.");
            Cleanup(child.n, stop, depth + 1); Need(unlinkat(root, name.c_str(), AT_REMOVEDIR) == 0, "Cleanup failed."); }
        else Need(unlinkat(root, name.c_str(), 0) == 0, "Cleanup failed.");
    }
}
} // namespace

Estimate Inspect(const std::string& data, const std::string& usb, const std::atomic<bool>& stop) {
    auto source = Root(data), target = Root(usb);
    Need(Info(source.n).st_dev != Info(target.n).st_dev, "Backup target must be a separate USB filesystem.");
    return Space(Scan(source.n, stop), target.n);
}
std::vector<std::string> Drives(const std::string& data, const std::atomic<bool>& stop) {
    auto source = Root(data); const auto device = Info(source.n).st_dev; std::vector<std::string> drives;
    for (unsigned i = 0; i < 8; ++i) {
        Check(stop); auto path = "/mnt/usb" + std::to_string(i);
        try { auto drive = Root(path); if (Info(drive.n).st_dev != device) drives.push_back(path); }
        catch (const std::exception&) {} // absent or unsafe mount; no path-following fallback
    }
    return drives;
}
std::string Backup(const std::string& data, const std::string& usb, const std::atomic<bool>& stop, Report report) {
    auto source = Root(data), target = Root(usb);
    Need(Info(source.n).st_dev != Info(target.n).st_dev, "Backup target must be a separate USB filesystem.");
    auto entries = Scan(source.n, stop); auto estimate = Space(entries, target.n);
    Need(estimate.available >= estimate.required, "Not enough space on the USB drive. No backup started.");
    auto name = Unique(target.n, "ProsperoEden-", stop); auto folder = DirAt(target.n, name);
    auto payload = DirAt(folder.n, "data", true);
    try { Copy(source.n, payload.n, entries, stop, report); }
    catch (const std::exception& e) { throw std::runtime_error(std::string{e.what()} + " Incomplete folder: " + usb + "/" + name); }
    // Rescan source to detect additions/removals during the copy, then independently read all output.
    auto after = Scan(source.n, stop);
    Need(after.size() == entries.size(), "App data changed during backup.");
    for (std::size_t i = 0; i < after.size(); ++i)
        Need(after[i].path == entries[i].path && after[i].size == entries[i].size, "App data changed during backup.");
    Validate(payload.n, entries, stop, report);
    auto text = Manifest(entries); Write(folder.n, "manifest.json", text); Check(stop);
    try { Write(folder.n, "COMPLETE", Digest(text)); } // publish LAST; never an incomplete backup
    catch (...) { unlinkat(folder.n, "COMPLETE", 0); throw; }
    return usb + "/" + name;
}
std::vector<std::string> Backups(const std::string& usb, const std::atomic<bool>& stop) {
    auto drive = Root(usb); std::vector<std::string> result;
    for (const auto& name : Names(drive.n, stop)) {
        if (!name.starts_with("ProsperoEden-")) continue;
        try { auto folder = DirAt(drive.n, name); auto text = Read(folder.n, "manifest.json");
            if (Read(folder.n, "COMPLETE") == Digest(text)) { Parse(text); result.push_back(usb + "/" + name); }
        } catch (const std::exception&) { Check(stop); } // partial/foreign entries are never offers
        Need(result.size() <= 1000, "Too many backups on drive.");
    }
    std::reverse(result.begin(), result.end()); return result;
}
Estimate InspectRestore(const std::string& data, const std::string& backup, const std::atomic<bool>& stop, Report report) {
    auto folder = Root(backup), target = Root(data); std::string fingerprint;
    auto entries = Completed(folder.n, stop, report, {}, &fingerprint);
    TargetPlan(target.n, entries); auto estimate = Space(entries, target.n, 2);
    estimate.manifest_sha256 = fingerprint; return estimate;
}
std::string StageRestore(const std::string& data, const std::string& backup, const std::atomic<bool>& stop, Report report,
                         const std::string& expected_manifest) {
    auto source = Root(backup), target = Root(data); auto entries = Completed(source.n, stop, report, expected_manifest);
    TargetPlan(target.n, entries);
    auto space = Space(entries, target.n, 2);
    Need(space.available >= space.required, "Not enough console space to stage restore and preserve current data.");
    auto transactions = RestoreRoot(data);
    Need(Info(transactions.n).st_dev == Info(target.n).st_dev, "Restore staging must share the data filesystem.");
    Need(!Exists(transactions.n, "pending.json"), "A restore is already pending. Close and reopen ProsperoEden.");
    auto id = Unique(transactions.n, "restore-", stop); auto stage = DirAt(transactions.n, id);
    auto recovery_path = data.substr(0, data.find_last_of('/')) + "/." + Leaf(data) + "-restores/" + id + "/previous";
    bool published = false, pending_created = false;
    try {
        auto payload = DirAt(stage.n, "data", true), input = DirAt(source.n, "data");
        Copy(input.n, payload.n, entries, stop, report); Validate(payload.n, entries, stop, report);
        auto plan = Manifest(entries); Write(stage.n, "plan.json", plan); Check(stop);
        // Pending is small, exclusive, and only published after payload/plan are durable.
        Write(transactions.n, "pending.json", Json{{"id", id}, {"sha256", Digest(plan)}}.dump(), &pending_created);
        published = true;
    } catch (const std::exception& problem) {
        if (!published) {
            // Only remove a pending file this call actually created, never another transaction.
            // If withdrawal is not durable, retain its validated stage and lock the owner.
            if (pending_created) {
                try { Need(unlinkat(transactions.n, "pending.json", 0) == 0, "Cannot withdraw pending restore."); Sync(transactions.n); }
                catch (const std::exception&) { throw RestorePending(std::string{problem.what()} +
                    " Pending restore could not be durably withdrawn. Close the app; retain and inspect the transaction before recovery.", recovery_path); }
            }
            try { std::atomic<bool> never{false}; Cleanup(stage.n, never);
                unlinkat(transactions.n, id.c_str(), AT_REMOVEDIR); } catch (...) {}
        }
        throw;
    }
    return recovery_path;
}
bool ApplyPending(const std::string& data, std::string& error) {
    try {
        // Do not create anything on the ordinary startup path; fail closed if a pending root is hostile.
        auto parent = Root(data.substr(0, data.find_last_of('/')));
        auto name = "." + Leaf(data) + "-restores";
        Fd transactions{openat(parent.n, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)};
        if (transactions.n < 0 && errno == ENOENT) return true;
        Need(transactions.n >= 0, "Cannot safely open restore transactions.");
        // Ordinary startup needs no fstatat/lstat support when there is no pending restore.
        // Some native filesystems deny it: that disables restore, not the rest of the app.
        Fd marker{openat(transactions.n, "pending.json", O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)};
        if (marker.n < 0 && errno == ENOENT) return true;
        Need(marker.n >= 0, "Cannot safely open pending restore.");
        auto pending = ParseJson(Read(transactions.n, "pending.json"));
        auto id = pending.at("id").get<std::string>(); Need(Component(id) && id.starts_with("restore-"), "Invalid restore transaction.");
        auto stage = DirAt(transactions.n, id); auto plan = Read(stage.n, "plan.json");
        Need(pending.at("sha256") == Digest(plan), "Restore plan checksum mismatch; data was not opened.");
        auto entries = Parse(plan); auto payload = DirAt(stage.n, "data"); std::atomic<bool> never{false};
        Validate(payload.n, entries, never, {});
        auto target = Root(data), recovery = DirAt(stage.n, "previous", true);
        Need(Info(stage.n).st_dev == Info(target.n).st_dev, "Restore staging filesystem changed.");
        for (auto& e : entries) {
            auto to = Parent(target.n, e.path, true); auto leaf = Leaf(e.path);
            if (e.directory) { auto dir = DirAt(to.n, leaf, true); Sync(dir.n); Sync(to.n); continue; }
            auto old = Parent(recovery.n, e.path, true);
            bool saved = Exists(old.n, leaf), current = Exists(to.n, leaf);
            if (saved) { auto previous = FileAt(old.n, leaf); auto s = Info(previous.n);
                Need(S_ISREG(s.st_mode) && s.st_nlink == 1, "Unsafe recovery file; restore stopped.");
                Sync(previous.n); Sync(old.n); Sync(to.n); }
            if (saved || !e.had) {
                if (current) { auto in = FileAt(to.n, leaf); Progress p{};
                    Need(Hash(in.n, -1, e, never, p, {}) == e.hash, "Restore interrupted with unexpected target; recovery retained.");
                    // The prior attempt may have stopped at a failed directory sync just after
                    // either rename. Reestablish durability before retiring the pending marker.
                    Sync(in.n); Sync(to.n); Sync(stage.n);
                    continue;
                }
            } else if (current) {
                // Never overwrite previous data. Recovery is private and every ancestor is pinned.
                auto in = FileAt(to.n, leaf); auto s = Info(in.n);
                Need(S_ISREG(s.st_mode) && s.st_nlink == 1, "Unsafe restore target.");
                Sync(in.n); // preserve the original's bytes, not only its recovery name
                Need(renameat(to.n, leaf.c_str(), old.n, leaf.c_str()) == 0, "Cannot preserve existing data.");
                Sync(old.n); Sync(to.n);
            } else Need(!e.had, "Existing data disappeared before restore; recovery retained.");
            // Copy from immutable staging, retaining it across interruption. A private per-file
            // temporary in stage avoids colliding with existing app filenames.
            if (Exists(stage.n, "install.tmp")) Need(unlinkat(stage.n, "install.tmp", 0) == 0, "Cannot clear interrupted temporary.");
            Fd out{openat(stage.n, "install.tmp", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600)};
            Need(out.n >= 0, "Cannot create restore temporary."); auto in = FileAt(payload.n, e.path); Progress p{};
            Need(Hash(in.n, out.n, e, never, p, {}) == e.hash, "Restore source changed."); Sync(out.n);
            Need(!Exists(to.n, leaf), "Restore target appeared unexpectedly.");
            Need(renameat(stage.n, "install.tmp", to.n, leaf.c_str()) == 0, "Cannot install staged file."); Sync(to.n); Sync(stage.n);
        }
        if (!Exists(stage.n, "APPLIED")) Write(stage.n, "APPLIED", "Previous files are in previous/. Staging is retained for recovery.\n");
        Need(unlinkat(transactions.n, "pending.json", 0) == 0, "Cannot finalize restore transaction."); Sync(transactions.n);
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
} // namespace Eden::UsbBackup
