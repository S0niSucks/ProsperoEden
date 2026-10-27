// SPDX-License-Identifier: GPL-3.0-or-later
#include "amiibo_sync.h"
#include "amiibo_file.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <sys/file.h>
#include <nlohmann/json.hpp>

namespace Eden {
namespace {
constexpr std::size_t kCatalogLimit = 2 * 1024 * 1024, kImageLimit = 4 * 1024 * 1024;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct File {
    int fd = -1;
    explicit File(int value) : fd(value) { Require(fd >= 0, "Cannot open a safe Amiibo cache path"); }
    ~File() { close(fd); }
    File(const File&) = delete;
};
int OpenRoot(const std::string& path, bool create) {
    namespace fs = std::filesystem;
    const fs::path root(path);
    Require(path.size() <= 4095 && path.find('\0') == path.npos && root.is_absolute() &&
            root.filename() != "." && root.filename() != ".." && !root.filename().empty() && root.filename().string().size() <= 255,
            "Amiibo cache needs an absolute collection folder");
    File parent(root.parent_path() == "/" ? open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC) :
                OpenAmiiboPath(root.parent_path().string(), O_RDONLY | O_DIRECTORY));
    const auto leaf = root.filename().string();
    if (create && mkdirat(parent.fd, leaf.c_str(), 0777) && errno != EEXIST)
        throw std::runtime_error("Cannot create the Amiibo cache folder");
    return openat(parent.fd, leaf.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
}
// No dump/key paths are accepted here. All names are generated internally from validated IDs.
std::string Read(int dir, const char* name, std::size_t limit, bool optional = false) {
    const int descriptor = openat(dir, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (descriptor < 0 && optional && errno == ENOENT) return {};
    File file(descriptor);
    struct stat info{};
    Require(fstat(file.fd, &info) == 0 && S_ISREG(info.st_mode) && info.st_size >= 0 &&
            std::uint64_t(info.st_size) <= limit, "Amiibo cache target is not a bounded regular file");
    std::string result(static_cast<std::size_t>(info.st_size), '\0');
    std::size_t done = 0;
    while (done < result.size()) {
        const auto n = read(file.fd, result.data() + done, result.size() - done);
        if (n < 0 && errno == EINTR) continue;
        Require(n > 0, "Cannot read the Amiibo cache"); done += n;
    }
    return result;
}
struct CatalogSource {
    std::vector<AmiiboSyncEntry> entries;
    std::string error;
};
CatalogSource ReadCatalog(int dir, const char* name) {
    try {
        const auto text = Read(dir, name, kCatalogLimit, true);
        Require(!text.empty(), "missing or empty");
        return {ParseAmiiboSyncCatalog(text), {}};
    } catch (const std::exception& e) {
        // A bad/unsafe source cannot suppress the other safe catalog. Read never follows links.
        return {{}, e.what()};
    }
}
std::vector<AmiiboSyncEntry> MergeCatalogs(std::vector<AmiiboSyncEntry> downloaded,
                                         std::vector<AmiiboSyncEntry> local) {
    std::map<std::string, AmiiboSyncEntry> unique;
    for (auto& entry : downloaded) unique.emplace(entry.id, std::move(entry));
    // Replace the complete label pair, including an explicitly empty/absent local series,
    // just like AmiiboCatalog::Read. Each source is bounded to 4,096 entries (union <= 8,192).
    for (auto& entry : local) unique.insert_or_assign(entry.id, std::move(entry));
    std::vector<AmiiboSyncEntry> result;
    for (auto& [_, entry] : unique) result.push_back(std::move(entry));
    return result;
}
struct Staged {
    int dir;
    std::string name, target;
    bool replace;
    Staged(int folder, std::string final, std::string_view bytes, bool overwrite)
        : dir(folder), target(std::move(final)), replace(overwrite) {
        static std::atomic<unsigned> serial{0};
        int fd = -1;
        for (unsigned attempt = 0; attempt < 64 && fd < 0; ++attempt) {
            name = ".amiibo-sync-" + std::to_string(getpid()) + "-" + std::to_string(++serial);
            fd = openat(dir, name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
            if (fd < 0 && errno != EEXIST) break;
        }
        File file(fd);
        try {
            std::size_t done = 0;
            while (done < bytes.size()) {
                const auto n = write(file.fd, bytes.data() + done, bytes.size() - done);
                if (n < 0 && errno == EINTR) continue;
                Require(n > 0, "Cannot write the Amiibo cache (check free space)"); done += n;
            }
            Require(fsync(file.fd) == 0, "Cannot flush the Amiibo cache");
        } catch (...) { unlinkat(dir, name.c_str(), 0); throw; }
    }
    ~Staged() { unlinkat(dir, name.c_str(), 0); }
    Staged(const Staged&) = delete;
    bool Commit() {
        struct stat info{};
        if (replace) {
            const int exists = fstatat(dir, target.c_str(), &info, AT_SYMLINK_NOFOLLOW);
            Require((exists == 0 && S_ISREG(info.st_mode)) || (exists < 0 && errno == ENOENT),
                    "Refusing a nonregular Amiibo metadata target");
            Require(renameat(dir, name.c_str(), dir, target.c_str()) == 0, "Cannot publish Amiibo metadata");
        } else {
            // linkat is atomic create-only publication: a photo appearing during HTTP wins too.
            if (linkat(dir, name.c_str(), dir, target.c_str(), 0) != 0) {
                Require(errno == EEXIST && fstatat(dir, target.c_str(), &info, AT_SYMLINK_NOFOLLOW) == 0 &&
                        S_ISREG(info.st_mode), "Cannot publish Amiibo photo; target changed or is unsafe");
                return false; // custom regular art that appeared during HTTP is left untouched
            }
        }
        Require(fsync(dir) == 0, "Cannot flush the Amiibo cache directory; some files may already be saved");
        return true;
    }
};
std::string PhotoName(const std::string& id) { return "icon_" + id.substr(0, 8) + "-" + id.substr(8) + ".png"; }
std::string PhotoUrl(std::string pattern, const std::string& id) {
    for (const auto& [field, value] : {std::pair{"{head}", id.substr(0, 8)}, {"{tail}", id.substr(8)}}) {
        for (auto at = pattern.find(field); at != pattern.npos; at = pattern.find(field)) pattern.replace(at, 6, value);
    }
    Require(pattern.find_first_of("{}") == pattern.npos, "Image templates support only {head} and {tail}");
    return pattern;
}
std::string Encode(const std::vector<AmiiboSyncEntry>& entries) {
    nlohmann::json array = nlohmann::json::array();
    for (const auto& e : entries) array.push_back({{"head", e.id.substr(0, 8)}, {"tail", e.id.substr(8)},
                                                {"name", e.name}, {"amiiboSeries", e.series}});
    auto result = nlohmann::json{{"amiibo", std::move(array)}}.dump();
    Require(result.size() <= kCatalogLimit, "Normalized Amiibo catalog exceeds limits");
    return result;
}
}
AmiiboSyncStatus AmiiboSync::Status() const { const std::lock_guard lock(status_mutex_); return status_; }
void AmiiboSync::Finish(const char* phase, std::string error) {
    const std::lock_guard lock(status_mutex_);
    status_.phase = phase; status_.error = std::move(error); status_.busy = false; ++status_.generation;
}
void AmiiboSync::Stop() {
    const std::lock_guard lock(lifecycle_mutex_);
    closed_ = true; Cancel(); if (worker_.joinable()) worker_.join();
}
void AmiiboSync::AllowStart() { const std::lock_guard lock(lifecycle_mutex_); closed_ = false; }
bool AmiiboSync::Start(AmiiboSyncRequest request) {
    const std::lock_guard lock(lifecycle_mutex_);
    if (closed_ || Status().busy || (!request.offline && !request.confirmed)) return false;
    if (worker_.joinable()) worker_.join();
    stop_ = false;
    {
        const std::lock_guard status_lock(status_mutex_);
        const auto generation = status_.generation + 1;
        auto previous_entries = request.folder == status_.folder ? std::move(status_.entries) : std::vector<AmiiboSyncEntry>{};
        status_ = {}; status_.generation = generation; status_.folder = request.folder;
        status_.entries = std::move(previous_entries); // failed/cancelled refresh keeps the last valid offline view
        status_.offline = request.offline; status_.busy = true; status_.phase = "checking";
    }
    try {
        worker_ = std::thread([this, request = std::move(request)] {
            try { Run(request); Finish("ready"); } // Run's descriptors/staging are gone before busy clears
            catch (const std::exception& e) { Finish(stop_ ? "cancelled" : "failed", stop_ ? "" : e.what()); }
            catch (...) { Finish(stop_ ? "cancelled" : "failed", "Amiibo synchronization failed"); }
        });
    } catch (...) { Finish("failed", "Cannot start the Amiibo synchronization worker"); }
    return true;
}
void AmiiboSync::Run(const AmiiboSyncRequest& request) {
    const auto cancelled = [&] { Require(!stop_, "Cancelled"); };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(10);
    const auto progress = [&](auto done, auto total) {
        Require(std::chrono::steady_clock::now() < deadline, "Amiibo batch time limit exceeded");
        const std::lock_guard lock(status_mutex_); status_.done = done; status_.total = total;
    };
    std::set<std::string> ids;
    Require(request.source.size() <= 2048 && request.image_template.size() <= 2048, "Amiibo source URL exceeds limits");
    Require(request.ids.size() <= 64, "Select at most 64 Amiibo photo IDs");
    for (const auto& id : request.ids) ids.insert(AmiiboSyncId(id));
    (void)PhotoUrl(request.image_template, "0000000000000000");
    File root(OpenRoot(request.folder, !request.offline));
    if (request.offline) {
        auto downloaded = ReadCatalog(root.fd, "catalog-sync.json");
        auto local = ReadCatalog(root.fd, "amiibo.json");
        if (downloaded.entries.empty() && local.entries.empty())
            throw std::runtime_error("No usable offline Amiibo catalog; catalog-sync.json: " + downloaded.error +
                                     "; amiibo.json: " + local.error);
        auto entries = MergeCatalogs(std::move(downloaded.entries), std::move(local.entries));
        const std::lock_guard lock(status_mutex_);
        cancelled(); status_.entries = std::move(entries); return;
    }
    // This lock is independent of live NFC descriptors. Only cache publishers take it.
    File file_lock(openat(root.fd, ".amiibo-sync.lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600));
    struct stat info{};
    Require(fstat(file_lock.fd, &info) == 0 && S_ISREG(info.st_mode) && flock(file_lock.fd, LOCK_EX | LOCK_NB) == 0,
            "The Amiibo cache is locked or unsafe");
    (void)Read(root.fd, "catalog-sync.json", kCatalogLimit, true); // preflight target, never touch amiibo.json
    { const std::lock_guard lock(status_mutex_); status_.phase = "catalog"; }
    cancelled();
    auto entries = ParseAmiiboSyncCatalog(fetch_(request.source, kCatalogLimit, stop_, progress));
    for (const auto& id : ids) Require(std::any_of(entries.begin(), entries.end(), [&](const auto& e) { return e.id == id; }),
                                     "A selected model ID is not in the catalog; nothing was changed");
    cancelled();
    const auto encoded = Encode(entries);
    Staged metadata(root.fd, "catalog-sync.json", encoded, true);
    // Keep the images descriptor alive until staged files have been cleaned up.
    if (!ids.empty() && mkdirat(root.fd, "images", 0777) && errno != EEXIST)
        throw std::runtime_error("Cannot create the Amiibo images cache");
    const int image_fd = ids.empty() ? dup(root.fd) : openat(root.fd, "images", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    File images(image_fd);
    std::vector<std::unique_ptr<Staged>> staged;
    for (const auto& id : ids) {
        cancelled(); progress(0, 0);
        const auto name = PhotoName(id);
        const auto cached = Read(images.fd, name.c_str(), kImageLimit, true);
        struct stat existing{};
        if (fstatat(images.fd, name.c_str(), &existing, AT_SYMLINK_NOFOLLOW) == 0) {
            // Existing art is NEVER overwritten, even corrupt/empty/custom art.
            ValidateAmiiboSyncPng(cached);
            const std::lock_guard lock(status_mutex_); ++status_.reused;
            continue;
        }
        Require(errno == ENOENT, "Cannot check the Amiibo image cache");
        { const std::lock_guard lock(status_mutex_); status_.phase = "photo"; }
        auto bytes = fetch_(PhotoUrl(request.image_template, id), kImageLimit, stop_, progress);
        ValidateAmiiboSyncPng(bytes); cancelled();
        staged.push_back(std::make_unique<Staged>(images.fd, name, bytes, false));
    }
    // Only the completed view gets local overrides. Neither downloaded bytes nor the user's
    // file is rewritten with merged labels, and failures/cancellation keep the old snapshot.
    auto display_entries = MergeCatalogs(std::move(entries), ReadCatalog(root.fd, "amiibo.json").entries);
    // All requested responses are valid before publication. Per-file atomic, not a batch transaction.
    { const std::lock_guard lock(status_mutex_); status_.phase = "saving"; }
    for (auto& photo : staged) {
        cancelled(); progress(0, 0); const bool saved = photo->Commit();
        const std::lock_guard lock(status_mutex_);
        if (saved) ++status_.photos; else ++status_.reused;
    }
    cancelled(); progress(0, 0); metadata.Commit();
    {
        const std::lock_guard lock(status_mutex_); status_.entries = std::move(display_entries);
    }
}
} // namespace Eden
