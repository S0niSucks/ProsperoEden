// SPDX-License-Identifier: GPL-3.0-or-later
// P09 restore barrier: explicit, confirmed, validated restore applied on the NEXT
// startup, never in line with staging.
//
// Staging copies a fully validated package into a private transaction root and
// publishes a durable "pending" record LAST; presence of that record is the
// barrier. Only ApplyPending() mutates the data root, and it is a startup-time,
// single-owner operation: the caller must admit it before any other writer of
// the data root runs (migrations, layout creation, settings, caches, logs) and
// must refuse ordinary startup (fail closed) while it does not succeed.
//
// Durability order everywhere: file fsync -> close -> rename -> parent fsync.
// The rename of a record is its COMMIT POINT: once renamed, a later directory
// fsync failure is reported as "published, sync degraded" and is never undone
// (r2/B3). Installs are never rolled back under a published completion marker.
// The pending record is removed only after everything else.
//
// Rollback model (r2/B2): ALL recovery copies of replaced originals are
// materialized and durably verified BEFORE the first install. A mid-apply
// failure then restores the exact previous file set and contents. A rollback
// line whose recovery copy is missing (only possible after a crash during the
// eager phase) is a no-op when the target still matches the plan's recorded
// previous bytes. Created empty parent directories may remain after rollback;
// the restored state is defined over files and their contents (documented).
//
// Temporary files use exclusive "<name>.restore-new/.restore-rollback.<pid>.<seq>"
// names and are never unlinked before creation: a pre-existing user file with a
// similar name is never touched (r2/M1). Crashed record staging residues
// (pending.tmp/plan.tmp/completed.tmp/inconsistent.tmp) are swept before use
// and tolerated on EEXIST when they are plain regular files (r2/M2).
//
// Checksums are FNV-1a 64: corruption/truncation detection only. They are NOT
// authorship, authentication or a malicious-package defense (A05/USB-01).
//
// Native PS5 filesystem semantics (openat/fstatat/fsync/rename/no-follow) are
// NOT qualified: research/native-inputs.md and sources/storage-network.md keep
// them unproven. Host checks only; a native integration must stay behind its
// own qualification gate, off by default.
//
// Single-owner contract: no internal locking. Concurrent writers (internal or
// external FTP/file managers) are outside this header and must be stopped by
// the integrator for staging and for the whole startup apply.
#pragma once

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Eden::Restore {

inline constexpr std::size_t kMaxEntries = 4096;
inline constexpr std::size_t kMaxPathBytes = 240;
inline constexpr std::size_t kMaxComponentBytes = 96;
inline constexpr std::size_t kMaxComponents = 32; // also bounds RemoveTree depth needs
inline constexpr std::size_t kMaxManifestBytes = 4u << 20;
inline constexpr std::size_t kMaxRecordBytes = 4096;
inline constexpr std::size_t kChunkBytes = 256u * 1024u;
inline constexpr std::size_t kMaxDepth = kMaxComponents + 16; // > any legal tree below tx roots
inline constexpr const char* kManifestName = "restore-manifest.txt";
inline constexpr const char* kTxPending = "pending";
inline constexpr const char* kTxStaged = "staged";
inline constexpr const char* kTxStaging = "staged.tmp";
inline constexpr const char* kTxRecovery = "recovery";
inline constexpr const char* kTxPlan = "plan";
inline constexpr const char* kTxCompleted = "completed";
inline constexpr const char* kTxInconsistent = "inconsistent";

enum class Reason {
    Ok = 0,
    Cancelled,          // cooperative cancellation; pending state kept
    PendingExists,      // stage refused: a restore is already pending
    NoPending,          // apply/cancel found no pending record
    InvalidPackage,     // manifest malformed (parse failure)
    UnsafePath,         // manifest entry path rejected by the schema
    TooManyEntries,
    PathTooLong,
    UnsortedPaths,      // duplicates or non-ascending order
    ManifestTooLarge,
    SizeMismatch,       // truncation/corruption caught by size
    ChecksumMismatch,   // corruption caught by content checksum
    NotRegularFile,
    LinkInPath,         // symlink anywhere on the package path walk
    StaleConfirmation,  // package changed since the inspected digest
    ValidationRefused,  // staged tree failed full revalidation; nothing applied
    UnsafeTarget,       // existing data-root target is not a plain regular file,
                        // or no longer matches the plan's previous bytes
    StageFailed,        // I/O failure while staging (nothing published)
    PendingWriteFailed, // the pending record could not be published (r2/B1)
    PendingInvalid,     // unreadable/corrupt pending record: fail closed
    InconsistentState,  // rollback previously failed; manual recovery required
    ApplyFailed,        // mid-apply failure; rollback restored previous state
    RollbackFailed,     // rollback failed; recovery data retained, fail closed
    IoError,
};

inline const char* ToString(Reason reason) {
    switch (reason) {
    case Reason::Ok: return "ok";
    case Reason::Cancelled: return "cancelled";
    case Reason::PendingExists: return "pending-exists";
    case Reason::NoPending: return "no-pending";
    case Reason::InvalidPackage: return "invalid-package";
    case Reason::UnsafePath: return "unsafe-path";
    case Reason::TooManyEntries: return "too-many-entries";
    case Reason::PathTooLong: return "path-too-long";
    case Reason::UnsortedPaths: return "unsorted-paths";
    case Reason::ManifestTooLarge: return "manifest-too-large";
    case Reason::SizeMismatch: return "size-mismatch";
    case Reason::ChecksumMismatch: return "checksum-mismatch";
    case Reason::NotRegularFile: return "not-regular-file";
    case Reason::LinkInPath: return "link-in-path";
    case Reason::StaleConfirmation: return "stale-confirmation";
    case Reason::ValidationRefused: return "validation-refused";
    case Reason::UnsafeTarget: return "unsafe-target";
    case Reason::StageFailed: return "stage-failed";
    case Reason::PendingWriteFailed: return "pending-write-failed";
    case Reason::PendingInvalid: return "pending-invalid";
    case Reason::InconsistentState: return "inconsistent-state";
    case Reason::ApplyFailed: return "apply-failed";
    case Reason::RollbackFailed: return "rollback-failed";
    case Reason::IoError: return "io-error";
    }
    return "unknown";
}

struct Result {
    bool ok = false;
    Reason reason = Reason::IoError;
    std::string detail; // bounded; relative paths only, never file contents
    explicit operator bool() const { return ok; }
};

struct Entry {
    std::string path;
    std::uint64_t size = 0;
    std::uint64_t checksum = 0; // FNV-1a 64 of the file bytes
};

struct PackageInfo {
    std::uint64_t manifest_checksum = 0; // FNV-1a 64 of the manifest bytes
    std::uint64_t total_bytes = 0;       // overflow-checked payload sum
    std::vector<Entry> entries;          // strictly ascending by path bytes
};

struct Options {
    // Provided by the integrator; this header never discovers roots or USB.
    std::string data_root;        // tree being restored
    std::string transaction_root; // private state; keep it in a dot-directory
                                  // inside data_root: the manifest schema forbids
                                  // dot-leading components, so no valid package
                                  // can ever name it.
};

// Test-only fault hooks. Production callers pass nullptr.
struct Faults {
    int fail_apply_after = -1;       // fail after N successful installs
    int fail_rollback_after = -1;    // fail after N rollback restores/no-ops
    bool fail_completed_sync = false; // simulate a directory fsync failure AFTER
                                      // the completed marker rename (r2/B3)
};

struct PendingState {
    bool active = false;   // barrier: a pending restore exists (or must be
                           // assumed to exist when unreadable)
    bool readable = false; // the record parsed cleanly
    std::uint64_t manifest_checksum = 0;
    std::uint64_t entries = 0;
};

namespace detail {

inline std::string Bounded(std::string text) {
    if (text.size() > 180) text.resize(180);
    return text;
}

inline Result Fail(Reason reason, std::string detail) {
    return Result{false, reason, Bounded(std::move(detail))};
}

inline Result Okay(std::string detail = {}) {
    return Result{true, Reason::Ok, Bounded(std::move(detail))};
}

inline std::string Join(const std::string& base, const std::string& relative) {
    return base + "/" + relative;
}

inline std::uint64_t FnvUpdate(std::uint64_t state, const void* data, std::size_t size) {
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        state ^= bytes[i];
        state *= 1099511628211ULL;
    }
    return state;
}

inline std::uint64_t Fnv(const void* data, std::size_t size) {
    return FnvUpdate(14695981039346656037ULL, data, size);
}

inline std::string Hex16(std::uint64_t value) {
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
    return text;
}

inline bool ParseHex16(const std::string& text, std::uint64_t* value) {
    if (text.size() != 16) return false;
    std::uint64_t parsed = 0;
    for (char c : text) {
        int digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else return false;
        parsed = (parsed << 4) | static_cast<std::uint64_t>(digit);
    }
    *value = parsed;
    return true;
}

inline bool ParseDecimal(const std::string& text, std::uint64_t* value) {
    if (text.empty() || text.size() > 20) return false;
    if (text.size() > 1 && text[0] == '0') return false;
    std::uint64_t parsed = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        const unsigned digit = static_cast<unsigned>(c - '0');
        if (parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) return false;
        parsed = parsed * 10 + digit;
    }
    *value = parsed;
    return true;
}

inline bool Cancelled(const std::atomic<bool>* cancel) {
    return cancel && cancel->load(std::memory_order_acquire);
}

inline bool WriteFdAll(int fd, const char* data, std::size_t size) {
    for (std::size_t at = 0; at < size;) {
        const ssize_t wrote = ::write(fd, data + at, size - at);
        if (wrote < 0 && errno == EINTR) continue;
        if (wrote <= 0) return false;
        at += static_cast<std::size_t>(wrote);
    }
    return true;
}

inline bool SyncFd(int fd) { return ::fsync(fd) == 0; }

inline bool SyncDirectory(const std::string& path) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    const bool synced = SyncFd(fd);
    const bool closed = ::close(fd) == 0;
    return synced && closed;
}

// Reads whole files that must stay small (manifest/records). No-follow.
inline bool ReadSmall(const std::string& path, std::size_t limit, std::string* out) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    out->clear();
    char buffer[4096];
    for (;;) {
        const ssize_t got = ::read(fd, buffer, sizeof(buffer));
        if (got < 0 && errno == EINTR) continue;
        if (got < 0) { ::close(fd); return false; }
        if (got == 0) break;
        out->append(buffer, static_cast<std::size_t>(got));
        if (out->size() > limit) { ::close(fd); return false; }
    }
    return ::close(fd) == 0;
}

// Streams a no-follow regular file, computing size and FNV checksum.
inline bool ChecksumFile(const std::string& path, std::uint64_t* size, std::uint64_t* checksum) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    std::vector<char> buffer(kChunkBytes);
    std::uint64_t state = 14695981039346656037ULL;
    std::uint64_t total = 0;
    for (;;) {
        const ssize_t got = ::read(fd, buffer.data(), buffer.size());
        if (got < 0 && errno == EINTR) continue;
        if (got < 0) { ::close(fd); return false; }
        if (got == 0) break;
        state = FnvUpdate(state, buffer.data(), static_cast<std::size_t>(got));
        total += static_cast<std::uint64_t>(got);
    }
    if (::close(fd) != 0) return false;
    *size = total;
    *checksum = state;
    return true;
}

inline bool VerifyFileBytes(const std::string& path, std::uint64_t expected_size,
                            std::uint64_t expected_checksum) {
    std::uint64_t size = 0;
    std::uint64_t checksum = 0;
    return ChecksumFile(path, &size, &checksum) && size == expected_size && checksum == expected_checksum;
}

// r2/B3: the rename is the commit point. NotPublished means the record is
// absent and callers may recover freely. PublishedSyncDegraded means the
// record is in place but its directory sync was not confirmed: callers must
// treat it as published and never undo work under it.
enum class RecordOutcome { NotPublished, Published, PublishedSyncDegraded };

inline RecordOutcome WriteRecord(const std::string& path, const std::string& body,
                                 bool simulate_post_rename_sync_failure = false) {
    const std::string staging = path + ".tmp";
    int fd = ::open(staging.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0 && errno == EEXIST) {
        // r2/M2: a crashed earlier write of this same record left its exclusive
        // staging node behind. It is ours by construction (exclusive no-follow
        // create inside the private transaction root); remove a plain regular
        // residue and retry once. Anything else stays and fails closed.
        struct stat residue {};
        if (::lstat(staging.c_str(), &residue) == 0 && S_ISREG(residue.st_mode) &&
            ::unlink(staging.c_str()) == 0)
            fd = ::open(staging.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    }
    if (fd < 0) return RecordOutcome::NotPublished;
    const bool written = WriteFdAll(fd, body.data(), body.size());
    const bool synced = written && SyncFd(fd);
    const bool closed = ::close(fd) == 0;
    if (!written || !synced || !closed) {
        (void)::unlink(staging.c_str()); // exclusively ours
        return RecordOutcome::NotPublished;
    }
    if (::rename(staging.c_str(), path.c_str()) != 0) {
        (void)::unlink(staging.c_str()); // exclusively ours
        return RecordOutcome::NotPublished;
    }
    if (simulate_post_rename_sync_failure) return RecordOutcome::PublishedSyncDegraded;
    const std::size_t slash = path.find_last_of('/');
    const std::string parent = slash == std::string::npos ? std::string{"."} : path.substr(0, slash);
    return SyncDirectory(parent) ? RecordOutcome::Published : RecordOutcome::PublishedSyncDegraded;
}

// r2/M2: crash residues of the four records. Regular files only; a foreign
// node (link/directory) is left in place so the exclusive create fails closed.
inline void ClearRecordResidues(const std::string& tx) {
    for (const char* record : {kTxPending, kTxPlan, kTxCompleted, kTxInconsistent}) {
        const std::string residue = Join(tx, std::string{record} + ".tmp");
        struct stat info {};
        if (::lstat(residue.c_str(), &info) == 0 && S_ISREG(info.st_mode))
            (void)::unlink(residue.c_str());
    }
}

inline bool Exists(const std::string& path) {
    struct stat info {};
    return ::lstat(path.c_str(), &info) == 0;
}

// mkdir one component; existing is fine only when it is a plain directory.
inline bool MkDirPlain(const std::string& path) {
    if (::mkdir(path.c_str(), 0777) == 0) return true;
    if (errno != EEXIST) return false;
    struct stat info {};
    return ::lstat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode) && !S_ISLNK(info.st_mode);
}

// Creates base/<a>/<b>/... parents for a relative entry path (base must exist),
// refusing symlinked or non-directory components. Fsyncs each parent that
// creates a new directory.
inline bool MkDirTree(const std::string& base, const std::string& relative, std::string* detail) {
    std::string current = base;
    std::size_t start = 0;
    while (start < relative.size()) {
        const std::size_t slash = relative.find('/', start);
        const std::string part = relative.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (part.empty()) return false;
        const bool fresh = !Exists(Join(current, part));
        current = Join(current, part);
        if (!MkDirPlain(current)) {
            *detail = current;
            return false;
        }
        if (fresh) {
            const std::size_t parent_slash = current.find_last_of('/');
            if (parent_slash != std::string::npos && !SyncDirectory(current.substr(0, parent_slash)))
                return false;
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return true;
}

// Splits a validated relative path; caller already ran ValidateEntryPath.
inline std::vector<std::string> Components(const std::string& relative) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= relative.size()) {
        const std::size_t slash = relative.find('/', start);
        const std::string part = relative.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (part.empty()) return parts;
        parts.push_back(part);
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return parts;
}

// No-follow walk: every ancestor must be a plain directory, the leaf a regular
// file. TOCTOU between walk and later open is bounded by the single-owner
// contract; final opens still use O_NOFOLLOW.
inline bool StatRegular(const std::string& base, const std::string& relative, struct stat* out,
                        Reason* reason, std::string* detail) {
    std::string current = base;
    const std::vector<std::string> parts = Components(relative);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        current = Join(current, parts[i]);
        struct stat info {};
        if (::lstat(current.c_str(), &info) != 0) {
            *reason = Reason::IoError;
            *detail = current;
            return false;
        }
        if (S_ISLNK(info.st_mode)) {
            *reason = i + 1 == parts.size() ? Reason::NotRegularFile : Reason::LinkInPath;
            *detail = current;
            return false;
        }
        if (i + 1 == parts.size()) {
            if (!S_ISREG(info.st_mode)) {
                *reason = Reason::NotRegularFile;
                *detail = current;
                return false;
            }
            *out = info;
            return true;
        }
        if (!S_ISDIR(info.st_mode)) {
            *reason = Reason::NotRegularFile;
            *detail = current;
            return false;
        }
    }
    *reason = Reason::UnsafePath;
    return false;
}

// r2/M1: collision-free exclusive temp name; nothing pre-existing is unlinked.
inline std::string UniqueTempName(const std::string& base, const char* suffix) {
    static std::atomic<unsigned> sequence{0};
    return base + suffix + "." + std::to_string(::getpid()) + "." +
           std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
}

// Streams src (no-follow) into an exclusively created destination, verifying the
// expected size and checksum before the caller renames anything into place.
// `collision` reports an EEXIST on the destination so callers can retry with a
// fresh exclusive name; no pre-existing node is ever removed here.
inline bool CopyFileVerified(const std::string& src, const std::string& destination,
                             std::uint64_t expected_size, std::uint64_t expected_checksum,
                             std::string* detail, bool* collision = nullptr) {
    if (collision) *collision = false;
    const int in = ::open(src.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (in < 0) {
        *detail = src;
        return false;
    }
    const int out = ::open(destination.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (out < 0) {
        if (collision && errno == EEXIST) *collision = true;
        ::close(in);
        *detail = destination;
        return false;
    }
    std::vector<char> buffer(kChunkBytes);
    std::uint64_t state = 14695981039346656037ULL;
    std::uint64_t total = 0;
    bool ok = true;
    while (ok) {
        const ssize_t got = ::read(in, buffer.data(), buffer.size());
        if (got < 0 && errno == EINTR) continue;
        if (got < 0) { ok = false; break; }
        if (got == 0) break;
        state = FnvUpdate(state, buffer.data(), static_cast<std::size_t>(got));
        total += static_cast<std::uint64_t>(got);
        if (total > expected_size) { ok = false; break; }
        if (!WriteFdAll(out, buffer.data(), static_cast<std::size_t>(got))) { ok = false; break; }
    }
    const bool synced = ok && SyncFd(out);
    const bool closed = ::close(out) == 0;
    const bool in_closed = ::close(in) == 0;
    if (!ok || !synced || !closed || !in_closed || total != expected_size || state != expected_checksum) {
        (void)::unlink(destination.c_str()); // the node this call created
        *detail = src;
        return false;
    }
    return true;
}

// Removes only plain directories and regular files below base/relative.
// Anything else (symlink, special node) aborts: this tree is exclusively ours.
inline bool RemoveTree(const std::string& base, const std::string& relative, std::size_t depth) {
    if (depth > kMaxDepth) return false;
    const std::string current = Join(base, relative);
    struct stat info {};
    if (::lstat(current.c_str(), &info) != 0) return errno == ENOENT;
    if (S_ISLNK(info.st_mode) || !(S_ISDIR(info.st_mode) || S_ISREG(info.st_mode))) return false;
    if (S_ISREG(info.st_mode)) return ::unlink(current.c_str()) == 0;
    const int fd = ::open(current.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    std::vector<std::string> names;
    DIR* stream = ::fdopendir(::dup(fd));
    if (!stream) { ::close(fd); return false; }
    struct dirent* entry;
    while ((entry = ::readdir(stream)) != nullptr) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        names.push_back(name);
    }
    ::closedir(stream);
    ::close(fd);
    for (const std::string& name : names)
        if (!RemoveTree(current, name, depth + 1)) return false;
    return ::rmdir(current.c_str()) == 0;
}

} // namespace detail

// Manifest schema (all bytes, LF-terminated):
//   EDEN-RESTORE-1\n
//   <count decimal, 1..kMaxEntries>\n
//   <16-hex-checksum> <size-decimal> <relative-path>\n   (exactly count lines,
//   strictly ascending by path bytes, which also forbids duplicates)
inline bool ValidateEntryPath(const std::string& path, Reason* reason) {
    if (path.empty() || path.size() > kMaxPathBytes) { *reason = Reason::PathTooLong; return false; }
    if (path.front() == '/' || path.back() == '/') { *reason = Reason::UnsafePath; return false; }
    std::size_t components = 0;
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t slash = path.find('/', start);
        const std::string part = path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (part.empty() || part == "." || part == "..") { *reason = Reason::UnsafePath; return false; }
        if (part.size() > kMaxComponentBytes) { *reason = Reason::PathTooLong; return false; }
        if (part.front() == '.' || part.back() == '.') { *reason = Reason::UnsafePath; return false; }
        for (unsigned char c : part)
            if (c < 32 || c == 127 || c == '\\') { *reason = Reason::UnsafePath; return false; }
        ++components;
        if (components > kMaxComponents) { *reason = Reason::PathTooLong; return false; } // r2/n1
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return true;
}

inline bool ParseManifest(const std::string& text, PackageInfo* out, Reason* reason, std::string* detail) {
    if (text.size() > kMaxManifestBytes) { *reason = Reason::ManifestTooLarge; return false; }
    if (text.empty() || text.back() != '\n') { *reason = Reason::InvalidPackage; *detail = "missing final newline"; return false; }
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t nl = text.find('\n', start);
        lines.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    if (lines.size() < 3 || lines[0] != "EDEN-RESTORE-1") {
        *reason = Reason::InvalidPackage;
        *detail = "header";
        return false;
    }
    std::uint64_t count = 0;
    if (!detail::ParseDecimal(lines[1], &count) || count < 1 || count > kMaxEntries) {
        *reason = Reason::InvalidPackage;
        *detail = "count";
        return false;
    }
    if (lines.size() != count + 2) { *reason = Reason::InvalidPackage; *detail = "line count"; return false; }
    PackageInfo parsed;
    for (std::uint64_t i = 0; i < count; ++i) {
        const std::string& line = lines[2 + static_cast<std::size_t>(i)];
        const std::size_t space1 = line.find(' ');
        const std::size_t space2 = space1 == std::string::npos ? std::string::npos : line.find(' ', space1 + 1);
        if (space1 == std::string::npos || space2 == std::string::npos) {
            *reason = Reason::InvalidPackage;
            *detail = "entry fields";
            return false;
        }
        Entry entry;
        if (!detail::ParseHex16(line.substr(0, space1), &entry.checksum) ||
            !detail::ParseDecimal(line.substr(space1 + 1, space2 - space1 - 1), &entry.size)) {
            *reason = Reason::InvalidPackage;
            *detail = "entry checksum/size";
            return false;
        }
        entry.path = line.substr(space2 + 1);
        Reason path_reason = Reason::Ok;
        if (!ValidateEntryPath(entry.path, &path_reason)) { *reason = path_reason; *detail = entry.path; return false; }
        if (!parsed.entries.empty() && !(parsed.entries.back().path < entry.path)) {
            *reason = Reason::UnsortedPaths;
            *detail = entry.path;
            return false;
        }
        if (parsed.total_bytes > std::numeric_limits<std::uint64_t>::max() - entry.size) {
            *reason = Reason::InvalidPackage;
            *detail = "size overflow";
            return false;
        }
        parsed.total_bytes += entry.size;
        parsed.entries.push_back(std::move(entry));
    }
    *out = std::move(parsed);
    return true;
}

// Full package validation: manifest parse + no-follow walk + per-file size and
// checksum. Reads only; never writes anywhere.
inline Result InspectPackage(const std::string& package_root, PackageInfo* out) {
    if (out) out->entries.clear();
    const std::string manifest = detail::Join(package_root, kManifestName);
    std::string text;
    if (!detail::ReadSmall(manifest, kMaxManifestBytes, &text)) {
        if (!detail::Exists(manifest)) return detail::Fail(Reason::InvalidPackage, "manifest missing");
        return detail::Fail(Reason::InvalidPackage, "manifest unreadable");
    }
    PackageInfo info;
    Reason reason = Reason::Ok;
    std::string failure;
    if (!ParseManifest(text, &info, &reason, &failure)) return detail::Fail(reason, failure);
    for (const Entry& entry : info.entries) {
        struct stat info_stat {};
        Reason stat_reason = Reason::Ok;
        std::string stat_detail;
        if (!detail::StatRegular(package_root, entry.path, &info_stat, &stat_reason, &stat_detail))
            return detail::Fail(stat_reason, stat_detail);
        if (static_cast<std::uint64_t>(info_stat.st_size) != entry.size)
            return detail::Fail(Reason::SizeMismatch, entry.path);
        std::uint64_t size = 0;
        std::uint64_t checksum = 0;
        if (!detail::ChecksumFile(detail::Join(package_root, entry.path), &size, &checksum))
            return detail::Fail(Reason::IoError, entry.path);
        if (size != entry.size) return detail::Fail(Reason::SizeMismatch, entry.path);
        if (checksum != entry.checksum) return detail::Fail(Reason::ChecksumMismatch, entry.path);
    }
    info.manifest_checksum = detail::Fnv(text.data(), text.size());
    if (out) *out = std::move(info);
    return detail::Okay();
}

inline PendingState InspectPending(const Options& options) {
    PendingState state;
    const std::string pending = detail::Join(options.transaction_root, kTxPending);
    if (!detail::Exists(pending)) return state;
    state.active = true; // an unreadable record still keeps the barrier on
    std::string text;
    if (!detail::ReadSmall(pending, kMaxRecordBytes, &text) || text.empty() || text.back() != '\n')
        return state;
    if (!text.starts_with("EDEN-RESTORE-PENDING-1\n")) return state;
    // Strict four-line record: header, manifest=, entries=, bytes=.
    std::size_t at = text.find('\n') + 1;
    std::vector<std::string> lines;
    while (at < text.size()) {
        const std::size_t nl = text.find('\n', at);
        if (nl == std::string::npos) return state;
        lines.push_back(text.substr(at, nl - at));
        at = nl + 1;
    }
    if (lines.size() != 3 || !lines[0].starts_with("manifest=") ||
        !lines[1].starts_with("entries=") || !lines[2].starts_with("bytes="))
        return state;
    std::uint64_t manifest_checksum = 0;
    std::uint64_t entries = 0;
    std::uint64_t bytes = 0;
    if (!detail::ParseHex16(lines[0].substr(9), &manifest_checksum) ||
        !detail::ParseDecimal(lines[1].substr(8), &entries) ||
        !detail::ParseDecimal(lines[2].substr(6), &bytes) || entries < 1 || entries > kMaxEntries)
        return state;
    state.readable = true;
    state.manifest_checksum = manifest_checksum;
    state.entries = entries;
    return state;
}

// Stages an explicitly confirmed restore. `confirmed_manifest_checksum` must be
// the digest the user inspected; a package that changed since then is refused
// (stale confirmation). NEVER touches the data root. The durable pending record
// is the last thing written: its absence after any failure means no barrier and
// no restore will happen.
inline Result StageRestore(const Options& options, const std::string& package_root,
                           std::uint64_t confirmed_manifest_checksum,
                           const std::atomic<bool>* cancel = nullptr) {
    if (detail::Cancelled(cancel)) return detail::Fail(Reason::Cancelled, {});
    if (InspectPending(options).active)
        return detail::Fail(Reason::PendingExists, "cancel or apply the existing restore first");
    if (detail::Exists(detail::Join(options.transaction_root, kTxInconsistent)))
        return detail::Fail(Reason::InconsistentState, "manual recovery required");
    PackageInfo info;
    Result inspected = InspectPackage(package_root, &info);
    if (!inspected) return inspected;
    if (info.manifest_checksum != confirmed_manifest_checksum)
        return detail::Fail(Reason::StaleConfirmation, "package changed since inspection");

    const std::string& tx = options.transaction_root;
    struct stat tx_info {};
    if (::lstat(tx.c_str(), &tx_info) == 0) {
        if (S_ISLNK(tx_info.st_mode) || !S_ISDIR(tx_info.st_mode))
            return detail::Fail(Reason::StageFailed, tx);
    } else if (::mkdir(tx.c_str(), 0777) != 0) {
        return detail::Fail(Reason::StageFailed, tx);
    }
    // Orphans from previously crashed attempts (no pending record was published).
    for (const char* name : {kTxStaging, kTxStaged, kTxRecovery, kTxPlan})
        if (!detail::RemoveTree(tx, name, 0)) return detail::Fail(Reason::StageFailed, name);
    if (detail::Exists(detail::Join(tx, kTxCompleted)) &&
        ::unlink(detail::Join(tx, kTxCompleted).c_str()) != 0)
        return detail::Fail(Reason::StageFailed, kTxCompleted);
    detail::ClearRecordResidues(tx); // r2/M2

    const std::string staging = detail::Join(tx, kTxStaging);
    if (!detail::MkDirPlain(staging)) return detail::Fail(Reason::StageFailed, kTxStaging);
    std::string manifest_text;
    if (!detail::ReadSmall(detail::Join(package_root, kManifestName), kMaxManifestBytes, &manifest_text))
        return detail::Fail(Reason::StageFailed, "manifest reread");
    const std::string staged_manifest = detail::Join(staging, kManifestName);
    const int mfd = ::open(staged_manifest.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (mfd < 0) return detail::Fail(Reason::StageFailed, kManifestName);
    const bool m_ok = detail::WriteFdAll(mfd, manifest_text.data(), manifest_text.size()) && detail::SyncFd(mfd);
    const bool m_closed = ::close(mfd) == 0;
    if (!m_ok || !m_closed) return detail::Fail(Reason::StageFailed, kManifestName);
    for (const Entry& entry : info.entries) {
        if (detail::Cancelled(cancel)) {
            (void)detail::RemoveTree(tx, kTxStaging, 0);
            return detail::Fail(Reason::Cancelled, {});
        }
        const std::size_t slash = entry.path.find_last_of('/');
        if (slash != std::string::npos) {
            std::string mk_detail;
            if (!detail::MkDirTree(staging, entry.path.substr(0, slash), &mk_detail)) {
                (void)detail::RemoveTree(tx, kTxStaging, 0);
                return detail::Fail(Reason::StageFailed, mk_detail);
            }
        }
        std::string copy_detail;
        if (!detail::CopyFileVerified(detail::Join(package_root, entry.path),
                                      detail::Join(staging, entry.path), entry.size, entry.checksum,
                                      &copy_detail)) {
            (void)detail::RemoveTree(tx, kTxStaging, 0);
            return detail::Fail(Reason::StageFailed, copy_detail);
        }
    }
    if (!detail::SyncDirectory(staging)) return detail::Fail(Reason::StageFailed, "staged sync");
    if (::rename(staging.c_str(), detail::Join(tx, kTxStaged).c_str()) != 0) {
        (void)detail::RemoveTree(tx, kTxStaging, 0);
        return detail::Fail(Reason::StageFailed, "staged publish");
    }
    if (!detail::SyncDirectory(tx)) return detail::Fail(Reason::StageFailed, "tx sync");

    // r2/B3: the pending record's rename is the commit point. A degraded
    // directory sync is reported, never hidden, and the barrier stands.
    const std::string body = "EDEN-RESTORE-PENDING-1\nmanifest=" + detail::Hex16(info.manifest_checksum) +
                             "\nentries=" + std::to_string(info.entries.size()) +
                             "\nbytes=" + std::to_string(info.total_bytes) + "\n";
    const detail::RecordOutcome published = detail::WriteRecord(detail::Join(tx, kTxPending), body);
    if (published == detail::RecordOutcome::NotPublished) {
        (void)detail::RemoveTree(tx, kTxStaged, 0);
        return detail::Fail(Reason::PendingWriteFailed, "pending record");
    }
    if (published == detail::RecordOutcome::PublishedSyncDegraded)
        return detail::Okay("pending published; directory sync not confirmed");
    return detail::Okay();
}

namespace detail {

struct PlanLine {
    bool had_previous = false;
    std::uint64_t size = 0;
    std::uint64_t checksum = 0; // previous bytes, for rollback verification
    std::string path;
};

inline bool ParsePlan(const std::string& text, std::vector<PlanLine>* out) {
    if (text.empty() || text.back() != '\n' || !text.starts_with("EDEN-RESTORE-PLAN-1\n"))
        return false;
    std::size_t at = text.find('\n') + 1;
    while (at < text.size()) {
        const std::size_t nl = text.find('\n', at);
        if (nl == std::string::npos) return false;
        const std::string line = text.substr(at, nl - at);
        at = nl + 1;
        PlanLine parsed;
        const std::size_t space1 = line.find(' ');
        if (space1 == std::string::npos) return false;
        const std::string had = line.substr(0, space1);
        if (had == "1") {
            const std::size_t space2 = line.find(' ', space1 + 1);
            const std::size_t space3 = space2 == std::string::npos ? std::string::npos : line.find(' ', space2 + 1);
            if (space2 == std::string::npos || space3 == std::string::npos) return false;
            std::uint64_t size = 0;
            if (!ParseDecimal(line.substr(space1 + 1, space2 - space1 - 1), &size) ||
                !ParseHex16(line.substr(space2 + 1, space3 - space2 - 1), &parsed.checksum))
                return false;
            parsed.had_previous = true;
            parsed.size = size;
            parsed.path = line.substr(space3 + 1);
        } else if (had == "0") {
            parsed.path = line.substr(space1 + 1);
        } else {
            return false;
        }
        Reason reason = Reason::Ok;
        if (!ValidateEntryPath(parsed.path, &reason)) return false;
        out->push_back(std::move(parsed));
    }
    return !out->empty();
}

inline std::string ParentDirectoryOf(const Options& options, const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    const std::string parent = slash == std::string::npos ? std::string{} : path.substr(0, slash);
    return parent.empty() ? options.data_root : Join(options.data_root, parent);
}

// Restores the pre-apply state from the plan + recovery tree. Replaced files
// are copied back (size+checksum verified); created files are removed. A line
// with no recovery copy (crash during the eager phase, r2/B2) is a no-op when
// the target still matches the plan's recorded previous bytes, and a hard
// failure otherwise. Recovery copies are kept until final cleanup so rollback
// stays re-runnable. Created empty parent directories may remain (documented).
inline Result RollbackFromPlan(const Options& options, const Faults* faults, std::string* detail) {
    const std::size_t plan_limit = kMaxEntries * (kMaxPathBytes + 48) + 64;
    std::string plan_text;
    if (!ReadSmall(Join(options.transaction_root, kTxPlan), plan_limit, &plan_text))
        return Fail(Reason::IoError, "plan read");
    std::vector<PlanLine> plan;
    if (!ParsePlan(plan_text, &plan)) return Fail(Reason::IoError, "plan parse");
    int restored = 0;
    for (std::size_t i = plan.size(); i-- > 0;) {
        const PlanLine& line = plan[i];
        struct stat info {};
        const std::string target = Join(options.data_root, line.path);
        const std::string parent_dir = ParentDirectoryOf(options, line.path);
        if (line.had_previous) {
            const std::string recovery = Join(Join(options.transaction_root, kTxRecovery), line.path);
            struct stat recovery_info {};
            const bool have_recovery = ::lstat(recovery.c_str(), &recovery_info) == 0;
            if (have_recovery) {
                if (!S_ISREG(recovery_info.st_mode) ||
                    static_cast<std::uint64_t>(recovery_info.st_size) != line.size)
                    return Fail(Reason::RollbackFailed, line.path);
                bool done = false;
                for (unsigned attempt = 0; attempt < 8 && !done; ++attempt) {
                    const std::string back = UniqueTempName(target, ".restore-rollback");
                    bool collision = false;
                    if (CopyFileVerified(recovery, back, line.size, line.checksum, detail, &collision)) {
                        if (::rename(back.c_str(), target.c_str()) == 0 && SyncDirectory(parent_dir)) {
                            done = true;
                        } else {
                            (void)::unlink(back.c_str()); // ours from this call
                            return Fail(Reason::RollbackFailed, line.path);
                        }
                    } else if (!collision) {
                        return Fail(Reason::RollbackFailed, *detail);
                    }
                }
                if (!done) return Fail(Reason::RollbackFailed, "rollback temp collision");
            } else if (::lstat(target.c_str(), &info) != 0 || !S_ISREG(info.st_mode) ||
                       !VerifyFileBytes(target, line.size, line.checksum)) {
                // r2/B2: never applied must mean "still exactly the previous
                // bytes"; anything else is a real inconsistency.
                return Fail(Reason::RollbackFailed, line.path);
            }
        } else {
            if (::lstat(target.c_str(), &info) == 0) {
                if (!S_ISREG(info.st_mode)) return Fail(Reason::RollbackFailed, line.path);
                if (::unlink(target.c_str()) != 0 || !SyncDirectory(parent_dir))
                    return Fail(Reason::RollbackFailed, line.path);
            } else if (errno != ENOENT) {
                return Fail(Reason::RollbackFailed, line.path);
            }
        }
        ++restored;
        if (faults && faults->fail_rollback_after >= 0 && restored > faults->fail_rollback_after)
            return Fail(Reason::RollbackFailed, "fault hook");
    }
    return Okay();
}

inline Result MarkInconsistent(const Options& options) {
    if (WriteRecord(Join(options.transaction_root, kTxInconsistent),
                    "EDEN-RESTORE-INCONSISTENT-1\n") == RecordOutcome::NotPublished)
        return Fail(Reason::IoError, "inconsistent marker");
    return Okay();
}

} // namespace detail

// Applies a pending restore. Startup-only, single owner: no other writer of the
// data root may run concurrently, and the caller must fail closed (refuse
// ordinary startup) unless this returns Ok with reason Ok.
inline Result ApplyPending(const Options& options, const Faults* faults = nullptr,
                           const std::atomic<bool>* cancel = nullptr) {
    const std::string& tx = options.transaction_root;
    const std::string pending_path = detail::Join(tx, kTxPending);
    detail::ClearRecordResidues(tx); // r2/M2: crash residues of record staging
    // A completed apply whose cleanup was interrupted: completion wins. The
    // completion marker is only ever renamed after every install succeeded and
    // was synced, and installs are never rolled back under a published marker,
    // so this state always means the data root holds the applied package.
    if (detail::Exists(detail::Join(tx, kTxCompleted))) {
        if (detail::Exists(detail::Join(tx, kTxInconsistent)))
            return detail::Fail(Reason::InconsistentState, "completed+inconsistent");
        (void)detail::RemoveTree(tx, kTxStaged, 0);
        (void)detail::RemoveTree(tx, kTxRecovery, 0);
        (void)::unlink(detail::Join(tx, kTxPlan).c_str());
        (void)::unlink(pending_path.c_str());
        // r3/F1: finish the interrupted cleanup by retiring the completion
        // marker too (best-effort: a crash before this unlink simply re-runs
        // this idempotent branch on the next boot).
        (void)::unlink(detail::Join(tx, kTxCompleted).c_str());
        (void)detail::SyncDirectory(tx);
        return detail::Okay();
    }
    if (detail::Exists(detail::Join(tx, kTxInconsistent)))
        return detail::Fail(Reason::InconsistentState, "manual recovery required");
    if (!detail::Exists(pending_path)) return detail::Fail(Reason::NoPending, {});
    const PendingState pending = InspectPending(options);
    if (!pending.active) return detail::Fail(Reason::NoPending, {});
    if (!pending.readable)
        return detail::Fail(Reason::PendingInvalid, "pending record unreadable; nothing modified");

    // An interrupted previous attempt: restore the pre-apply state first.
    if (detail::Exists(detail::Join(tx, kTxPlan))) {
        std::string rollback_detail;
        if (!detail::RollbackFromPlan(options, nullptr, &rollback_detail)) {
            (void)detail::MarkInconsistent(options);
            return detail::Fail(Reason::RollbackFailed, rollback_detail);
        }
        if (::unlink(detail::Join(tx, kTxPlan).c_str()) != 0 || !detail::SyncDirectory(tx))
            return detail::Fail(Reason::IoError, "plan retire");
        (void)detail::RemoveTree(tx, kTxRecovery, 0);
    }

    // Full revalidation of the staged copy: a truncated or corrupted staged
    // package is refused with nothing applied; pending and staged are kept.
    PackageInfo staged;
    Result validated = InspectPackage(detail::Join(tx, kTxStaged), &staged);
    if (!validated) return detail::Fail(Reason::ValidationRefused, validated.detail);
    if (staged.manifest_checksum != pending.manifest_checksum)
        return detail::Fail(Reason::ValidationRefused, "staged manifest digest mismatch");
    if (staged.entries.size() != pending.entries)
        return detail::Fail(Reason::ValidationRefused, "staged entry count mismatch");
    if (detail::Cancelled(cancel)) return detail::Fail(Reason::Cancelled, {});

    // Plan: which targets exist and their current bytes, so a mid-apply failure
    // can restore the exact previous state.
    std::vector<detail::PlanLine> plan_lines;
    std::string plan_body = "EDEN-RESTORE-PLAN-1\n";
    for (const Entry& entry : staged.entries) {
        detail::PlanLine line;
        line.path = entry.path;
        struct stat info {};
        const std::string target = detail::Join(options.data_root, entry.path);
        if (::lstat(target.c_str(), &info) == 0) {
            if (S_ISLNK(info.st_mode) || !S_ISREG(info.st_mode))
                return detail::Fail(Reason::UnsafeTarget, entry.path);
            std::uint64_t size = 0;
            std::uint64_t checksum = 0;
            if (!detail::ChecksumFile(target, &size, &checksum) ||
                size != static_cast<std::uint64_t>(info.st_size))
                return detail::Fail(Reason::IoError, entry.path);
            line.had_previous = true;
            line.size = size;
            line.checksum = checksum;
            plan_body += "1 " + std::to_string(size) + " " + detail::Hex16(checksum) + " " + entry.path + "\n";
        } else {
            plan_body += "0 " + entry.path + "\n";
        }
        plan_lines.push_back(std::move(line));
    }
    const detail::RecordOutcome plan_written = detail::WriteRecord(detail::Join(tx, kTxPlan), plan_body);
    if (plan_written == detail::RecordOutcome::NotPublished)
        return detail::Fail(Reason::IoError, "plan write");
    // A degraded plan sync still converges: a crash now replays rollback (all
    // lines are no-ops or verified restores) and revalidates before retrying.

    // r2/B2: materialize EVERY recovery copy, durably and verified, BEFORE the
    // first install. The recovery copy equals the plan's previous bytes; a
    // mismatch means the target changed under the plan and neither applying nor
    // rolling back to stale data is safe -> fail closed without touching data.
    const std::string recovery_root = detail::Join(tx, kTxRecovery);
    if (!detail::MkDirPlain(recovery_root)) return detail::Fail(Reason::IoError, "recovery root");
    for (const detail::PlanLine& line : plan_lines) {
        if (!line.had_previous) continue;
        const std::size_t slash = line.path.find_last_of('/');
        if (slash != std::string::npos) {
            std::string mk_detail;
            if (!detail::MkDirTree(recovery_root, line.path.substr(0, slash), &mk_detail))
                return detail::Fail(Reason::IoError, mk_detail);
        }
        const std::string recovery_copy = detail::Join(recovery_root, line.path);
        if (detail::Exists(recovery_copy)) {
            // Ours from an earlier interrupted attempt (kept through a failed
            // apply): reuse it when it still matches the plan's previous
            // bytes; anything else in this private tree fails closed.
            if (!detail::VerifyFileBytes(recovery_copy, line.size, line.checksum)) {
                (void)detail::MarkInconsistent(options);
                return detail::Fail(Reason::UnsafeTarget, recovery_copy);
            }
            continue;
        }
        std::string copy_detail;
        if (!detail::CopyFileVerified(detail::Join(options.data_root, line.path), recovery_copy,
                                      line.size, line.checksum, &copy_detail)) {
            (void)detail::MarkInconsistent(options);
            return detail::Fail(Reason::UnsafeTarget, copy_detail);
        }
    }

    const auto fail_path = [&](Reason reason, const std::string& why) {
        std::string rollback_detail;
        if (!detail::RollbackFromPlan(options, faults, &rollback_detail)) {
            (void)detail::MarkInconsistent(options);
            return detail::Fail(Reason::RollbackFailed, rollback_detail);
        }
        if (::unlink(detail::Join(tx, kTxPlan).c_str()) != 0 || !detail::SyncDirectory(tx))
            return detail::Fail(Reason::IoError, "plan retire after rollback");
        return detail::Fail(reason, why);
    };

    int installed = 0;
    for (const Entry& entry : staged.entries) {
        if (detail::Cancelled(cancel)) return fail_path(Reason::Cancelled, entry.path);
        const std::string target = detail::Join(options.data_root, entry.path);
        const std::size_t slash = entry.path.find_last_of('/');
        const std::string parent = slash == std::string::npos ? std::string{} : entry.path.substr(0, slash);
        if (!parent.empty()) {
            std::string mk_detail;
            if (!detail::MkDirTree(options.data_root, parent, &mk_detail))
                return fail_path(Reason::IoError, mk_detail);
        }
        // Install: exclusive unique sibling temp (r2/M1), checksum verified
        // before the rename publishes it.
        bool installed_ok = false;
        for (unsigned attempt = 0; attempt < 8 && !installed_ok; ++attempt) {
            const std::string temp = detail::UniqueTempName(target, ".restore-new");
            bool collision = false;
            std::string copy_detail;
            if (detail::CopyFileVerified(detail::Join(detail::Join(tx, kTxStaged), entry.path), temp,
                                         entry.size, entry.checksum, &copy_detail, &collision)) {
                if (::rename(temp.c_str(), target.c_str()) == 0 &&
                    detail::SyncDirectory(detail::ParentDirectoryOf(options, entry.path))) {
                    installed_ok = true;
                } else {
                    (void)::unlink(temp.c_str()); // ours from this call
                    return fail_path(Reason::IoError, entry.path);
                }
            } else if (!collision) {
                return fail_path(Reason::ChecksumMismatch, copy_detail);
            }
        }
        if (!installed_ok) return fail_path(Reason::IoError, "install temp collision");
        ++installed;
        if (faults && faults->fail_apply_after >= 0 && installed > faults->fail_apply_after)
            return fail_path(Reason::ApplyFailed, entry.path);
    }

    // Commit: everything installed and synced; completion marker first, pending
    // record removed last. r2/B3: once the completion marker is renamed, its
    // outcome is final — a degraded directory sync is reported in the result,
    // and the installs are NEVER rolled back under a published marker.
    const detail::RecordOutcome committed = detail::WriteRecord(
        detail::Join(tx, kTxCompleted), "EDEN-RESTORE-COMPLETED-1\n",
        faults && faults->fail_completed_sync);
    if (committed == detail::RecordOutcome::NotPublished)
        return fail_path(Reason::IoError, "completed marker");
    (void)detail::RemoveTree(tx, kTxStaged, 0);
    (void)::unlink(detail::Join(tx, kTxPlan).c_str());
    (void)detail::RemoveTree(tx, kTxRecovery, 0);
    if (::unlink(pending_path.c_str()) != 0 || !detail::SyncDirectory(tx))
        return detail::Fail(Reason::IoError, "pending retire");
    // r3/F1: retire the completion marker after the pending record, so success
    // leaves no transaction state behind. A crash before this unlink keeps the
    // crash-window semantics: "completed wins" resolves it on the next boot.
    // A failed unlink here reports IoError with the barrier already resolved
    // (pending gone); the orphan marker is plain residue cleaned by the next
    // StageRestore.
    if (::unlink(detail::Join(tx, kTxCompleted).c_str()) != 0 || !detail::SyncDirectory(tx))
        return detail::Fail(Reason::IoError, "completed retire");
    if (committed == detail::RecordOutcome::PublishedSyncDegraded)
        return detail::Okay("completed published; directory sync not confirmed");
    return detail::Okay();
}

// Explicitly cancels a staged restore (user action before startup apply).
// Refuses an unreadable pending record or an inconsistent state: those need
// manual recovery and are reported, not silently discarded.
inline Result CancelPending(const Options& options, const Faults* faults = nullptr) {
    const std::string& tx = options.transaction_root;
    if (detail::Exists(detail::Join(tx, kTxInconsistent)))
        return detail::Fail(Reason::InconsistentState, "manual recovery required");
    const PendingState pending = InspectPending(options);
    if (!pending.active) return detail::Fail(Reason::NoPending, {});
    if (!pending.readable)
        return detail::Fail(Reason::PendingInvalid, "pending record unreadable; nothing removed");
    if (detail::Exists(detail::Join(tx, kTxPlan))) {
        std::string rollback_detail;
        if (!detail::RollbackFromPlan(options, faults, &rollback_detail)) {
            (void)detail::MarkInconsistent(options);
            return detail::Fail(Reason::RollbackFailed, rollback_detail);
        }
        if (::unlink(detail::Join(tx, kTxPlan).c_str()) != 0 || !detail::SyncDirectory(tx))
            return detail::Fail(Reason::IoError, "plan retire");
    }
    (void)detail::RemoveTree(tx, kTxStaged, 0);
    (void)detail::RemoveTree(tx, kTxRecovery, 0);
    if (::unlink(detail::Join(tx, kTxPending).c_str()) != 0 || !detail::SyncDirectory(tx))
        return detail::Fail(Reason::IoError, "pending retire");
    detail::ClearRecordResidues(tx);
    return detail::Okay();
}

} // namespace Eden::Restore
