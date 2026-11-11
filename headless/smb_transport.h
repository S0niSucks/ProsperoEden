// SPDX-License-Identifier: GPL-3.0-or-later
// P03 SMB increment r1: the minimal read-only transport surface a network VFS
// needs. Pure interface: no SMB library, socket, DNS, thread or Eden core type
// appears here. A production SMB client (still BLOCKED: this repository pins
// no SMB dependency) and the host test loopback (smb_transport_host.h) must
// both implement exactly this contract.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Eden::Smb {

// One entry as the caller sees it. inode/modified/modified_ns are the share's
// own identity: they are compared between open and every read to detect a
// share that changed under an active reader.
struct Entry {
    std::string path; // share-relative, '/'-separated, no leading '/'; "" is the root
    std::uint64_t size = 0, inode = 0, modified = 0, modified_ns = 0;
    bool directory = false;
};

using Handle = int; // open regular file; negative means failure

// Share-relative path grammar admitted by the reference application
// (network/transport.cpp SafePath): non-empty components of at most 255
// printable bytes, no '.', '..', backslash, ':', '%', '?' or '#', and no
// trailing '/'. Unsafe paths are a CALLER error: implementations reject them
// without poisoning the session.
inline bool SafePath(std::string_view path, bool allow_empty = false) {
    if (path.empty()) return allow_empty;
    if (path.size() > 2048) return false;
    for (std::size_t at = 0; at < path.size();) {
        const auto end = path.find('/', at);
        const auto part = path.substr(at, end == path.npos ? path.size() - at : end - at);
        if (part.empty() || part == "." || part == ".." || part.size() > 255) return false;
        for (unsigned char c : part)
            if (c < 32 || c == 127 || c == '\\' || c == ':' || c == '%' || c == '?' || c == '#')
                return false;
        if (end == path.npos) return true;
        at = end + 1;
        if (at == path.size()) return false;
    }
    return false;
}

// Read-only transport over ONE stable share. Every implementation obeys:
//
// - Read-only: there is no write, resize, rename or create anywhere in this
//   interface; share content can only be admitted read-only.
// - Stable enumeration or nothing: List either returns the complete
//   path-sorted entry set of one directory level or fails. A timeout or error
//   during enumeration must leave the caller's vector empty and poison the
//   session: a silent partial listing is never produced.
// - Failure poisons the whole session: after any share-level failure the
//   transport stays unhealthy forever; every later call fails, including reads
//   of bytes an earlier call already delivered or copied. Error() returns one
//   fixed user-facing message; raw library or server strings never cross this
//   interface. There is deliberately no un-poison or reset: recovery is a NEW
//   Transport (a fresh launch), which must never silently reattach readers of
//   the poisoned session.
// - Caller errors never poison: an unsafe path, opening a directory or the
//   root, and bad/closed handles are refused without touching session health.
//   Refusing Open-on-directory as a caller error is a deliberate divergence
//   from the reference poison path (R3-03; parity list in limites.md).
//   A MISSING path is a share-level failed lookup and poisons, matching the
//   reference stat round-trip behavior. Listing a path that exists as a
//   regular file also poisons here: deliberate divergence (R3-01), pinned by
//   fixture T19 and to be resolved before any production transport.
// - Mutation invalidates readers: if a file's identity changes between Open
//   and Read, the read fails and the session is poisoned. Same-metadata
//   content replacement is NOT detected: this is not an immutable snapshot.
// - A Read may have copied a prefix into `data` before returning 0. Callers
//   must trust only the returned count, never the buffer after a failure.
// - Implementations are not required to be thread-safe; the owner that mounts
//   a share serializes access (the reference Session holds one mutex).
class Transport {
public:
    virtual ~Transport() = default;
    Transport() = default;
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;

    // Complete identity of one file or directory ("" is the root directory).
    virtual bool Stat(std::string_view path, Entry* out) = 0;

    // Complete path-sorted listing of one directory level. On any share-level
    // failure (timeout, error, missing path, or a path that exists as a
    // regular file): false, `out` cleared, session poisoned. The regular-file
    // case diverges from the reference, which returns false without poison:
    // deliberate (R3-01), pinned by fixture T19, revisit before production.
    virtual bool List(std::string_view path, std::vector<Entry>* out) = 0;

    // Opens one regular file for reading and reports the identity Reads are
    // checked against. Caller errors (unsafe path, directory, root) return -1
    // without poison; a missing path poisons like any share-level failure.
    virtual Handle Open(std::string_view path, Entry* identity) = 0;

    // Up to `size` bytes at `offset` of an open file. Returns the number of
    // bytes read; 0 on EOF, empty request, unsafe/closed handle, any
    // share-level failure or poison.
    virtual std::size_t Read(Handle handle, std::uint64_t offset,
                             unsigned char* data, std::size_t size) = 0;

    // Always safe: unknown, negative and already-closed handles, and closes on
    // a poisoned session, are no-ops; nothing leaks across poison.
    virtual void Close(Handle handle) = 0;

    virtual bool Healthy() const = 0;
    virtual std::string Error() const = 0; // fixed message; empty while healthy
};

} // namespace Eden::Smb
