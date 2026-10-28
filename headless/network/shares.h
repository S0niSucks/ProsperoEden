// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Eden::Network {
// No URLs or ambient credentials: only explicitly configured numeric IPv4 endpoints.
struct Share {
    std::string id, name, protocol = "smb", server, share, folder, updates_dir;
    std::string user, password, domain; // private configuration, never a Status or diagnostic
    int port = 445, mount_port = 0, uid = 65534, gid = 65534;
    int timeout_ms = 5000;
    std::size_t cache_bytes = 16u << 20;
    bool enabled = true, encrypt = false;
};
struct Entry {
    std::string path;
    std::uint64_t size = 0, inode = 0, modified = 0, modified_ns = 0;
    bool directory = false;
};
struct ShareStatus {
    std::string id, name, protocol, error;
    bool connected = false, scanning = false;
    std::size_t games = 0;
};
struct Status {
    std::string file, error;
    std::vector<ShareStatus> shares;
    std::uint64_t generation = 0;
    bool busy = false;
};
struct Game { std::string uri, name, location; std::uint64_t size = 0; };

bool SafePath(std::string_view path, bool allow_empty = false);
bool Valid(const Share& share);
bool IsPath(std::string_view path);
std::string Uri(std::string_view id, std::string_view path);

// One serialized protocol context, a 256 KiB block LRU (read-ahead within each block), and
// at most 16 read-only handles. A failure poisons the entire session, including cached reads.
// Reconnect creates a NEW session; consumers can never silently cross to a changed file.
class Session {
public:
    explicit Session(Share share);
    ~Session();
    Session(const Session&) = delete;
    bool Connect();
    void Cancel(); // nonblocking; a pending operation wakes within the 25 ms poll interval
    bool Stat(std::string_view path, Entry* entry);
    bool List(std::string_view path, std::vector<Entry>* entries);
    std::size_t Read(const Entry& file, unsigned char* data, std::size_t size, std::uint64_t offset);
    bool Healthy() const;
    std::string Error() const; // fixed messages; never library/server error strings
    std::size_t CachedBytes() const;
    std::uint64_t TransportReads() const;
    std::uint64_t TransferredBytes() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
struct Opened { std::shared_ptr<Session> session; Entry entry; };
struct OpenedDirectory { std::shared_ptr<Session> session; std::string id, path; unsigned depth = 0; };

// A per-thread metadata/provider probe budget, independent of the runtime block cache.
// Budget refusal is a metadata fallback, not a transport disconnect. Nested scopes share limits.
class ReadBudget {
public:
    explicit ReadBudget(std::size_t bytes = 32u << 20);
    ~ReadBudget();
    ReadBudget(const ReadBudget&) = delete;
    bool Exhausted() const;
    bool Charge(std::size_t bytes);
private:
    ReadBudget* previous;
    std::size_t remaining;
    std::chrono::steady_clock::time_point deadline;
    bool exhausted = false;
};
bool AllowAllocation(std::size_t bytes);
bool ChargeTransfer(std::size_t bytes);

// The manager owns menu discovery only. Files retain shared session ownership after the menu
// has closed. Reload/disconnect cancels old sessions rather than invalidating their pointers.
void Configure(const std::string& file, bool reconnect = true);
void Refresh();
void Disconnect();
void StopDiscovery();
Status Current();
std::vector<Game> Games();
bool Known(std::string_view uri);
Opened Open(std::string_view uri);
OpenedDirectory OpenDirectory(std::string_view uri);
std::vector<std::string> AddonRoots(std::string_view game);
// Game I/O errors are latched, not thrown across HLE/guest threads. Main stops the session.
void BeginGame();
void EndGame();
std::string GameError();
void CancelGame();
} // namespace Eden::Network
