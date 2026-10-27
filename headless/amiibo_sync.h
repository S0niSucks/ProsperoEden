// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace Eden {
inline constexpr const char* kAmiiboCatalogSource = "https://amiiboapi.org/api/amiibo/";
inline constexpr const char* kAmiiboPhotoSource =
    "https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_{head}-{tail}.png";
struct AmiiboSyncEntry {
    std::string id, name, series;
};
struct AmiiboSyncStatus {
    std::string phase = "idle", error, folder;
    bool busy = false, offline = false;
    std::uint64_t generation = 0, done = 0, total = 0;
    unsigned photos = 0, reused = 0;
    std::vector<AmiiboSyncEntry> entries;
};
struct AmiiboSyncRequest {
    std::string folder;
    std::string source = kAmiiboCatalogSource, image_template = kAmiiboPhotoSource;
    std::vector<std::string> ids; // explicit public IDs only, never inferred from dump bytes
    bool offline = false, confirmed = false;
};
// These validators are also used by focused synthetic fixture tests. Invalid input throws.
std::string AmiiboSyncId(std::string_view);
std::vector<AmiiboSyncEntry> ParseAmiiboSyncCatalog(std::string_view);
void ValidateAmiiboSyncPng(std::string_view);
using AmiiboSyncProgress = std::function<void(std::uint64_t, std::uint64_t)>;
std::string FetchAmiiboSync(const std::string& url, std::size_t limit,
    const std::atomic<bool>& stop, AmiiboSyncProgress progress);

// Owns its worker and all request strings. No bridge/global settings/NFC references on worker.
// Lifecycle methods are serialized; Stop closes admission until AllowStart is explicitly called.
class AmiiboSync final {
public:
    using Fetch = decltype(&FetchAmiiboSync);
    explicit AmiiboSync(Fetch fetch = FetchAmiiboSync) : fetch_(fetch) {}
    ~AmiiboSync() { Stop(); }
    AmiiboSync(const AmiiboSync&) = delete;
    AmiiboSync& operator=(const AmiiboSync&) = delete;
    bool Start(AmiiboSyncRequest request);
    AmiiboSyncStatus Status() const;
    void Cancel() { stop_ = true; }
    void Stop();
    void AllowStart();
private:
    void Run(const AmiiboSyncRequest&);
    void Finish(const char* phase, std::string error = {});
    Fetch fetch_;
    mutable std::mutex status_mutex_;
    std::mutex lifecycle_mutex_;
    AmiiboSyncStatus status_;
    std::atomic<bool> stop_{false};
    std::thread worker_;
    bool closed_ = false;
};
} // namespace Eden
