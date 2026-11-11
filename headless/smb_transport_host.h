// SPDX-License-Identifier: GPL-3.0-or-later
// P03 SMB increment r1: HOST-ONLY loopback implementation of smb_transport.h,
// backed by an in-memory tree. It lets host fixtures exercise the real
// read-only/poison contract with no SMB library, socket, DNS or server. Never
// compile this into native or production targets. It has no threads, no I/O
// and no clock: "timeouts" and transport faults are INJECTED. This tests the
// CONTRACT, not latency or wire behavior.
#pragma once
#include "smb_transport.h"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Eden::Smb {

class MemoryTransport final : public Transport {
public:
    struct Node {
        std::string bytes; // payload of a regular file; empty for a directory
        std::uint64_t inode = 0, modified = 0, modified_ns = 0;
        bool directory = false;
    };
    // Key: share-relative path ("" is the root directory).
    using Tree = std::map<std::string, Node>;

    explicit MemoryTransport(Tree tree) : tree_(std::move(tree)) {}

    // ---- host test controls; a production transport has no such knobs ----
    enum class Fault {
        None,
        StatError,          // next Stat fails: share-level error (poisons)
        ListTimeout,        // next List fails before any entry is produced
        ListTimeoutPartial, // next List fails after `after` entries were built
                            // internally; the caller still sees none of them
        ReadError,          // next Read fails before copying anything
        ReadTruncated       // next Read copies count-1 bytes, then fails
    };
    void Inject(Fault fault, std::size_t after = 0) { fault_ = fault; after_ = after; }
    // External writer: replaces the payload AND bumps identity (inode/mtime),
    // so open/read identity comparisons detect the change.
    bool Mutate(const std::string& path, std::string bytes);
    // Limitation probe: same-length payload, identity untouched. Reads then
    // SUCCEED with the new bytes: there is no content-integrity guarantee.
    bool MutateQuiet(const std::string& path, std::string bytes);

    bool Stat(std::string_view path, Entry* out) override;
    bool List(std::string_view path, std::vector<Entry>* out) override;
    Handle Open(std::string_view path, Entry* identity) override;
    std::size_t Read(Handle handle, std::uint64_t offset, unsigned char* data,
                     std::size_t size) override;
    void Close(Handle handle) override;
    bool Healthy() const override { return healthy_; }
    std::string Error() const override { return error_; }

private:
    static constexpr const char* kTimeoutMessage =
        "Network share timed out. Check the connection and reconnect in Settings.";
    static constexpr const char* kAccessMessage =
        "Network share read or access failed. Reconnect in Settings.";

    static bool ChildOf(std::string_view parent, std::string_view key) {
        if (parent.empty()) return !key.empty() && key.find('/') == std::string_view::npos;
        const std::size_t skip = parent.size() + 1;
        if (key.size() <= skip) return false;
        if (key.compare(0, parent.size(), parent) != 0 || key[parent.size()] != '/') return false;
        return key.find('/', skip) == std::string_view::npos;
    }
    static Entry Describe(std::string path, const Node& node);
    bool Fail(const char* message) {
        if (error_.empty()) error_ = message;
        healthy_ = false;
        return false;
    }
    const Node* Find(std::string_view path) const {
        const auto it = tree_.find(std::string{path});
        return it == tree_.end() ? nullptr : &it->second;
    }

    Tree tree_;
    struct Held {
        std::string path;
        Entry identity;
        bool open = true;
    };
    std::vector<Held> holds_;
    std::string error_;
    bool healthy_ = true;
    Fault fault_ = Fault::None;
    std::size_t after_ = 0;
};

inline Entry MemoryTransport::Describe(std::string path, const Node& node) {
    Entry entry;
    entry.path = std::move(path);
    entry.size = node.directory ? 0 : node.bytes.size();
    entry.inode = node.inode;
    entry.modified = node.modified;
    entry.modified_ns = node.modified_ns;
    entry.directory = node.directory;
    return entry;
}

inline bool MemoryTransport::Mutate(const std::string& path, std::string bytes) {
    auto it = tree_.find(path);
    if (it == tree_.end() || it->second.directory) return false;
    it->second.bytes = std::move(bytes);
    it->second.inode += 1;
    it->second.modified += 1;
    it->second.modified_ns += 1;
    return true;
}

inline bool MemoryTransport::MutateQuiet(const std::string& path, std::string bytes) {
    auto it = tree_.find(path);
    if (it == tree_.end() || it->second.directory ||
        it->second.bytes.size() != bytes.size())
        return false;
    it->second.bytes = std::move(bytes);
    return true;
}

inline bool MemoryTransport::Stat(std::string_view path, Entry* out) {
    if (!SafePath(path, true)) return false; // caller error: never poisons
    if (!healthy_) return false;
    if (fault_ == Fault::StatError) {
        fault_ = Fault::None;
        return Fail(kAccessMessage);
    }
    const Node* node = Find(path);
    if (!node) return Fail(kAccessMessage);
    if (out) *out = Describe(std::string{path}, *node);
    return true;
}

inline bool MemoryTransport::List(std::string_view path, std::vector<Entry>* out) {
    if (!out) return false;
    out->clear();
    if (!SafePath(path, true)) return false; // caller error: never poisons
    if (!healthy_) return false;
    const bool timeout_before = fault_ == Fault::ListTimeout;
    const bool timeout_partial = fault_ == Fault::ListTimeoutPartial;
    const std::size_t after = after_;
    fault_ = Fault::None; // R3-04: test knob consumed before node resolution (ordering only)
    const Node* node = Find(path);
    // R3-01 divergence: the reference returns false WITHOUT poison when the
    // path exists as a regular file (its stat succeeds, then the directory
    // check fails cleanly). This host poisons deliberately (fail-closed); the
    // behavior is pinned by fixture T19 and must be resolved before any
    // production transport. See limites.md.
    if (!node || !node->directory) return Fail(kAccessMessage);
    if (timeout_before) return Fail(kTimeoutMessage);
    if (timeout_partial && after == 0) return Fail(kTimeoutMessage);
    // Built completely and published only on total success: a mid-listing
    // timeout can never hand the caller a partial vector.
    std::vector<Entry> complete;
    for (const auto& [key, child] : tree_) {
        if (!ChildOf(path, key)) continue;
        complete.push_back(Describe(key, child));
        if (timeout_partial && complete.size() == after) return Fail(kTimeoutMessage);
    }
    if (timeout_partial) return Fail(kTimeoutMessage); // plan exceeded entry count
    std::sort(complete.begin(), complete.end(),
              [](const Entry& a, const Entry& b) { return a.path < b.path; });
    *out = std::move(complete);
    return true;
}

inline Handle MemoryTransport::Open(std::string_view path, Entry* identity) {
    if (!SafePath(path)) return -1; // caller error: never poisons
    if (!healthy_) return -1;
    const Node* node = Find(path);
    if (!node) {
        Fail(kAccessMessage);
        return -1;
    }
    if (node->directory) return -1; // caller error: never poisons
    const Entry now = Describe(std::string{path}, *node);
    holds_.push_back(Held{std::string{path}, now, true});
    if (identity) *identity = now;
    return static_cast<Handle>(holds_.size() - 1);
}

inline std::size_t MemoryTransport::Read(Handle handle, std::uint64_t offset,
                                         unsigned char* data, std::size_t size) {
    if (handle < 0) return 0;
    if (!healthy_) return 0;
    if (static_cast<std::size_t>(handle) >= holds_.size()) return 0;
    Held& held = holds_[static_cast<std::size_t>(handle)];
    if (!held.open) return 0;
    if (fault_ == Fault::ReadError) {
        fault_ = Fault::None;
        Fail(kAccessMessage);
        return 0;
    }
    const Node* node = Find(held.path);
    if (!node) {
        Fail("Network share connection lost. Reconnect in Settings and start the game again.");
        return 0;
    }
    const Entry now = Describe(held.path, *node);
    if (now.size != held.identity.size || now.inode != held.identity.inode ||
        now.modified != held.identity.modified ||
        now.modified_ns != held.identity.modified_ns) {
        Fail("Network game changed within this session. Reconnect and rescan.");
        return 0;
    }
    if (offset >= now.size || size == 0 || !data) return 0;
    const std::size_t count =
        std::min(size, static_cast<std::size_t>(now.size - offset));
    if (fault_ == Fault::ReadTruncated) {
        fault_ = Fault::None;
        // A real transport may have copied a prefix before failing. Reproduce
        // that trap; only the returned count may ever be treated as success.
        const std::size_t prefix = count - 1;
        if (prefix > 0)
            std::copy_n(node->bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                        prefix, data);
        Fail("Network game was truncated or returned an invalid read. Start again after reconnecting.");
        return 0;
    }
    std::copy_n(node->bytes.begin() + static_cast<std::ptrdiff_t>(offset), count, data);
    return count;
}

inline void MemoryTransport::Close(Handle handle) {
    if (handle < 0 || static_cast<std::size_t>(handle) >= holds_.size()) return;
    holds_[static_cast<std::size_t>(handle)].open = false;
}

} // namespace Eden::Smb
