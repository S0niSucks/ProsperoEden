// SPDX-License-Identifier: GPL-3.0-or-later
// In-memory session for ONE distinct local Amiibo .bin dump with atomic
// same-size writeback. NOT a dump-format parser: no UID/crypto/checksum
// validation exists in permitted app code (see agents/A01.md for that
// blocker). Acquired bytes stay UNVALIDATED; a guest/core contract must be
// separately established before anything here is called "supported".
#pragma once
#include "amiibo_candidate.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

// Must stay in sync with user_content.h:20-26 (native descriptor I/O remains
// default-off; this header adds no speculative native imports of its own).
#if !defined(PS5_NATIVE) || (defined(EDEN_NATIVE_USER_CONTENT_IO_QUALIFIED) && EDEN_NATIVE_USER_CONTENT_IO_QUALIFIED == 1)
#define EDEN_AMIIBO_SESSION_IO 1
#else
#define EDEN_AMIIBO_SESSION_IO 0
#endif

namespace Eden::AmiiboSession {

inline constexpr bool kIoAvailable = AmiiboCandidate::kIoAvailable;

// Admission-time identity (dev/ino/size/mtime/ctime) of the exact file whose
// bytes this session owns. WriteBack refuses to publish over any file that no
// longer matches it: stale selections, aliases and in-flight external edits
// fail closed instead of being overwritten (cf. A13 finding A13-02).
struct Identity {
    dev_t dev = 0;
    ino_t ino = 0;
    off_t size = 0;
    time_t mtime = 0;
    time_t ctime = 0;
};
inline Identity Identify(const struct stat& st) {
    return Identity{st.st_dev, st.st_ino, st.st_size, st.st_mtime, st.st_ctime};
}
inline bool SameFile(const struct stat& st, const Identity& id) {
    return st.st_dev == id.dev && st.st_ino == id.ino && st.st_size == id.size &&
           st.st_mtime == id.mtime && st.st_ctime == id.ctime;
}

struct Session {
    std::string folder;                 // absolute; re-verified no-follow on every use
    std::string name;                   // admitted .bin leaf name
    std::vector<unsigned char> bytes;   // unvalidated acquisition snapshot
    Identity identity;                  // the exact admitted file
};

// Acquire-only session open: bounded UNVALIDATED bytes plus the identity they
// came from. True is acquisition success, never dump validity. Identity is
// pinned only when the file is observably the same before and after the read;
// anything else means an active external writer and is refused.
inline bool Open(const std::string& folder, const std::string& name, Session& out,
                 const std::atomic<bool>* cancel = nullptr) {
#if !EDEN_AMIIBO_SESSION_IO
    out = Session{}; // refusal leaves an empty session, like the acquisition helper
    (void)folder; (void)name; (void)cancel;
    return false; // native descriptor I/O stays default-off
#else
    out = Session{};
    if (UserContent::Cancelled(cancel) || !AmiiboCandidate::BinName(name)) return false;
    UserContent::Descriptor first(UserContent::OpenDirectory(folder, cancel));
    if (first.value < 0) return false;
    struct stat admitted{};
    if (::fstatat(first.value, name.c_str(), &admitted, AT_SYMLINK_NOFOLLOW) != 0 ||
        !AmiiboCandidate::CandidateStat(admitted))
        return false;
    Session session;
    session.folder = folder;
    session.name = name;
    if (!AmiiboCandidate::ReadUnvalidatedBin(folder, name, session.bytes, cancel)) return false;
    UserContent::Descriptor second(UserContent::OpenDirectory(folder, cancel));
    if (second.value < 0) return false;
    struct stat after{};
    // Identity comparison must go stat -> Identity (Identify) so SameFile's
    // (const stat&, const Identity&) overload resolves; passing a second raw
    // stat does not compile (A50 amiibo-session-r1-01 compile-plain.log).
    const Identity admitted_identity = Identify(admitted);
    if (::fstatat(second.value, name.c_str(), &after, AT_SYMLINK_NOFOLLOW) != 0 ||
        !AmiiboCandidate::CandidateStat(after) || !SameFile(after, admitted_identity))
        return false;
    session.identity = admitted_identity;
    out = std::move(session);
    return true;
#endif
}

// Atomically replace the admitted file's CONTENTS with `replacement`.
// Mirrors Settings::WriteOwned (headless/settings_store.h): exclusive no-follow
// staging created in the same verified directory, bounded writes, cancellation
// cutoff AFTER staging close and BEFORE rename, rename publication, and removal
// of only the staging node this writer created. The original file is never
// opened for writing; on any failure it stays byte-identical and no staging
// survives (cf. A13 finding A13-03: no in-place partial writes, no rollback
// promises). Same-size only: a writeback may never truncate or extend the
// distinct dump. No fsync: close/rename is not a power-loss durability
// guarantee, exactly like the existing owned writers.
inline bool WriteBack(Session& session, const std::vector<unsigned char>& replacement,
                      const std::atomic<bool>* cancel = nullptr) {
#if !EDEN_AMIIBO_SESSION_IO
    (void)session; (void)replacement; (void)cancel;
    return false; // native descriptor I/O stays default-off
#else
    if (UserContent::Cancelled(cancel) || !AmiiboCandidate::BinName(session.name)) return false;
    if (replacement.empty() || replacement.size() != session.bytes.size() ||
        replacement.size() > AmiiboCandidate::kMaxCandidateBytes)
        return false;
    UserContent::Descriptor directory(UserContent::OpenDirectory(session.folder, cancel));
    if (directory.value < 0) return false;
    struct stat target{};
    if (::fstatat(directory.value, session.name.c_str(), &target, AT_SYMLINK_NOFOLLOW) != 0 ||
        !AmiiboCandidate::CandidateStat(target) || !SameFile(target, session.identity))
        return false; // mutated, aliased or removed since admission: refuse
    static std::atomic<unsigned> sequence{0};
    std::string staging;
    int fd = -1;
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        if (UserContent::Cancelled(cancel)) return false;
        staging = session.name + ".tmp." + std::to_string(::getpid()) + "." +
                  std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
        fd = ::openat(directory.value, staging.c_str(),
                      O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (fd >= 0 || errno != EEXIST) break;
    }
    if (fd < 0) return false; // never remove a pre-existing staging node
    bool ok = true;
    for (std::size_t at = 0; at < replacement.size();) {
        if (UserContent::Cancelled(cancel)) { ok = false; break; }
        const auto count = ::write(fd, replacement.data() + at,
                                   std::min<std::size_t>(65536, replacement.size() - at));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ok = false; break; }
        at += static_cast<std::size_t>(count);
    }
    struct stat staged{};
    if (ok && (::fstat(fd, &staged) != 0 || !S_ISREG(staged.st_mode) || staged.st_nlink != 1 ||
               staged.st_size != static_cast<off_t>(replacement.size())))
        ok = false;
    if (::close(fd) != 0) ok = false;
    // Final decision/cancellation cutoff: after close, before rename. After it,
    // publication is non-cancellable and its success must be reported as saved.
    if (ok && !UserContent::Cancelled(cancel)) {
        struct stat again{};
        if (::fstatat(directory.value, session.name.c_str(), &again, AT_SYMLINK_NOFOLLOW) == 0 &&
            AmiiboCandidate::CandidateStat(again) && SameFile(again, session.identity) &&
            ::renameat(directory.value, staging.c_str(), directory.value, session.name.c_str()) == 0) {
            session.bytes = replacement;
            struct stat published{};
            // Re-pin the published inode so chained writebacks keep working. If
            // this refresh fails, the stale identity makes the NEXT writeback
            // refuse (fail-closed); this one is already published.
            if (::fstatat(directory.value, session.name.c_str(), &published,
                          AT_SYMLINK_NOFOLLOW) == 0 &&
                AmiiboCandidate::CandidateStat(published)) {
                session.identity = Identify(published);
            }
            return true;
        }
    }
    (void)::unlinkat(directory.value, staging.c_str(), 0); // only our own staging node
    return false;
#endif
}

} // namespace Eden::AmiiboSession
#undef EDEN_AMIIBO_SESSION_IO
