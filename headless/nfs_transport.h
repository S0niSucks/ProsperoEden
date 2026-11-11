// SPDX-License-Identifier: GPL-3.0-or-later
// A04 / P04 NFS — r1 EXTERNAL CANDIDATE (AUTH-05). Not integrated, not compiled here.
//
// App-owned, transport-agnostic contract for ONE trusted, stable, read-only
// remote share. It deliberately does NOT model NFS wire details, credentials,
// sockets or versions: the real NFS client is NOT pinned in this repository
// (see ../blockers.json). A future approved client must satisfy this contract;
// nothing here claims it exists yet.
//
// Contract fixed by this header (exercised by the host-only fixture):
//  1. READ-ONLY: no write/resize/rename/create/delete operation exists here.
//     Server-side read-only permission is a separate prerequisite.
//  2. ALL-OR-NOTHING enumeration: List either publishes the complete list or
//     fails having published nothing. There is no partial list.
//  3. PERMANENT POISON: Timeout or detected change (size/mtime/identity of a
//     known object, or namespace growth/shrink) poisons the WHOLE session.
//     A poisoned session fails every later operation, including cache hits.
//     There is no unpoison. Recovery = stop + new launch + new session.
//  4. READERS KEEP THEIR SESSION: a File handle never rebinds to a newer
//     session. Old handles keep failing after a new session connects.
//  5. PARTIAL READ IS NOT SUCCESS: on failure the caller's buffer may contain
//     a copied prefix; success is only what Read reports as success.
//  6. EOF vs ERROR: ok=true with *read_bytes == 0 means either exact EOF
//     (offset == size) within the admitted identity, or a zero-length request
//     (buffer->size() == 0), which is a harmless no-op. Callers that always
//     request at least one byte may treat 0 as EOF. offset > size is Changed,
//     not EOF. (Doc aligned to the tested host behavior; A16 gate F1.)
//  7. A session admits ONLY names present in its connect-time snapshot. Any
//     name outside it — a newly appeared object or a never-seen one — is
//     Changed and poisons the whole session (harmonized with the approved SMB
//     contract, A03 smb-transport.h:64-67 / fixture T04; coordinator decision,
//     A16 gate F2). Invalid (closed handle, local misuse) does NOT poison.
//  8. Connect returns a NEW session per fresh launch; it never cures, reuses
//     or reattaches an old one.
//  9. THREADING: single-threaded per session. A Session and its File handles
//     are used by one owner thread at a time, exactly as exercised host-only;
//     no internal locking is provided or claimed. Cooperative cancellation and
//     cross-thread ownership hand-off belong to the application owner (shared
//     P03/P04 owner gate), not to this contract (A16 gate F3).
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Eden::NfsTransport {

enum class ErrorCode {
    Unavailable,  // poisoned session (reason already latched) or transport refused
    Timeout,      // injected/real deadline exceeded; ALWAYS poisons permanently
    Changed,      // detected size/mtime/identity/namespace change; ALWAYS poisons
    Invalid,      // bad name, closed handle, unknown object; never poisons
};

struct Stat {
    std::uint64_t size = 0;
    std::uint64_t mtime_ns = 0;
    std::uint64_t identity = 0;  // opaque object identity (e.g. fileid/inode)
    bool regular = false;
};

class Session;

class File {
public:
    virtual ~File() = default;

    // Reads up to buffer->size() bytes at `offset`. The request length IS the
    // buffer size; the caller sizes the buffer first. On success returns true
    // and sets *read_bytes (<= buffer->size(); 0 means exact EOF, or a
    // zero-length request — see rule 6). On failure returns false with *code
    // set: *read_bytes is 0 and the buffer contents are unspecified (a prefix
    // may have been copied and MUST be discarded).
    virtual bool Read(std::uint64_t offset, std::vector<std::uint8_t>* buffer,
                      std::size_t* read_bytes, ErrorCode* code) = 0;

    // Identity admitted at open time; readers revalidate against it.
    virtual Stat stat() const = 0;

    // Idempotent. Read after Close reports Invalid and does not poison.
    virtual void Close() = 0;
};

class Session {
public:
    virtual ~Session() = default;

    // ALL-OR-NOTHING: names is cleared first; a complete list is published
    // only in full. Mid-enumeration failure, timeout, or any detected change
    // (including namespace growth/shrink) fails with nothing published.
    virtual bool List(std::vector<std::string>* names, ErrorCode* code) = 0;

    // Only snapshot names are admitted (rule 7): a name outside the snapshot
    // (new or never seen) is Changed and poisons; a snapshot name now
    // absent/changed is Changed and poisons.
    virtual bool Stat(const std::string& name, Stat* out, ErrorCode* code) = 0;

    // The handle keeps its session identity; it never rebinds (rule 4).
    virtual bool Open(const std::string& name, std::unique_ptr<File>* out,
                      ErrorCode* code) = 0;

    virtual bool Poisoned() const = 0;

    // Permanent. Application owners call this after any detected instability;
    // Timeout/Changed poison internally. No method clears it.
    virtual void Poison() = 0;
};

class Transport {
public:
    virtual ~Transport() = default;

    // One NEW session per fresh launch (rule 8). `code` is meaningful only
    // when false is returned.
    virtual bool Connect(std::unique_ptr<Session>* out, ErrorCode* code) = 0;
};

}  // namespace Eden::NfsTransport
