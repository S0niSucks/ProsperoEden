// SPDX-License-Identifier: GPL-3.0-or-later
// A04 / P04 NFS r2 — HOST-ONLY fixture driver for the external candidate.
// Exercises the read-only stable-share contract against the in-memory host
// with injected faults. It performs NO network, socket, DNS, mount, file or
// configuration access and does NOT exercise any real NFS client (blocked).
// r2: N08/N14 expect the SMB-harmonized unseen-name poison (A16 F2); N15
// covers the zero-length request no-op (A16 F1). Build/run commands for the
// central executor are in ../README.md.
#include "../host/nfs_memory_host.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Check(bool ok, const char* label) {
    if (!ok) {
        std::fprintf(stderr, "FAIL nfs-transport-r1: %s\n", label);
        ++failures;
    }
}

std::vector<std::uint8_t> Text(const char* s) {
    const std::size_t n = std::strlen(s);
    return std::vector<std::uint8_t>(s, s + n);
}

bool Same(const std::vector<std::uint8_t>& got, const char* want) {
    const std::size_t n = std::strlen(want);
    if (got.size() != n) return false;
    for (std::size_t i = 0; i < n; ++i)
        if (got[i] != static_cast<std::uint8_t>(want[i])) return false;
    return true;
}

}  // namespace

int main() {
    using Eden::NfsTransport::ErrorCode;
    using Eden::NfsTransport::File;
    using Eden::NfsTransport::MemoryHostTransport;
    using Eden::NfsTransport::Session;
    using Eden::NfsTransport::Stat;

    {  // N01: stable share enumerates completely.
        MemoryHostTransport host;
        host.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        host.AddFile("b.xci", Text("WXYZ"), 1000, 2);
        host.AddFile("c.nro", Text("XY"), 1000, 3);
        std::unique_ptr<Session> session;
        ErrorCode code = ErrorCode::Invalid;
        Check(host.Connect(&session, &code), "N01 connect");
        std::vector<std::string> names;
        Check(session->List(&names, &code), "N01 list-ok");
        Check(names.size() == 3 && names[0] == "a.nsp" && names[1] == "b.xci" &&
                  names[2] == "c.nro",
              "N01 complete-list");
    }

    {  // N02: midway instability fails all-or-nothing and poisons permanently.
        MemoryHostTransport host;
        host.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        host.AddFile("b.xci", Text("WXYZ"), 1000, 2);
        host.AddFile("c.nro", Text("XY"), 1000, 3);
        std::unique_ptr<Session> session;
        ErrorCode code = ErrorCode::Invalid;
        host.Connect(&session, &code);
        host.faults.list_fail_halfway = true;
        std::vector<std::string> names;
        names.push_back("stale");
        Check(!session->List(&names, &code), "N02 midway-fails");
        Check(code == ErrorCode::Timeout, "N02 timeout-reason");
        Check(names.empty(), "N02 nothing-published");
        Check(session->Poisoned(), "N02 poisoned");
        host.faults.list_fail_halfway = false;  // host "recovers"
        Check(!session->List(&names, &code), "N02 still-fails");
        Check(code == ErrorCode::Unavailable, "N02 poison-permanent");
    }

    {  // N03: stat/open/read exact bytes.
        MemoryHostTransport host;
        host.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        std::unique_ptr<Session> session;
        ErrorCode code = ErrorCode::Invalid;
        host.Connect(&session, &code);
        Stat st;
        Check(session->Stat("a.nsp", &st, &code) && st.size == 4 && st.regular,
              "N03 stat");
        std::unique_ptr<File> file;
        Check(session->Open("a.nsp", &file, &code), "N03 open");
        std::vector<std::uint8_t> buffer(4, 0);
        std::size_t got = 0;
        Check(file->Read(0, &buffer, &got, &code), "N03 read-ok");
        Check(got == 4 && Same(buffer, "ABCD"), "N03 exact-bytes");
        Check(file->stat().size == 4, "N03 handle-stat");
    }

    {  // N04: cache serves the session's committed bytes; a same-metadata
        // content swap is NOT detectable (documented limit, not a guarantee).
        MemoryHostTransport host;
        host.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        std::unique_ptr<Session> session;
        ErrorCode code = ErrorCode::Invalid;
        host.Connect(&session, &code);
        std::unique_ptr<File> file;
        session->Open("a.nsp", &file, &code);
        std::vector<std::uint8_t> buffer(4, 0);
        std::size_t got = 0;
        Check(file->Read(0, &buffer, &got, &code) && Same(buffer, "ABCD"),
              "N04 first-read");
        host.MutateContentOnly("a.nsp", Text("XXXX"));
        Check(file->Read(0, &buffer, &got, &code) && Same(buffer, "ABCD"),
              "N04 cache-committed-bytes");
    }

    {  // N05: timeout poisons; poison blocks the warm cache.
        MemoryHostTransport host;
        host.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        std::unique_ptr<Session> session;
        ErrorCode code = ErrorCode::Invalid;
        host.Connect(&session, &code);
        std::unique_ptr<File> file;
        session->Open("a.nsp", &file, &code);
        std::vector<std::uint8_t> buffer(4, 0);
        std::size_t got = 0;
        Check(file->Read(0, &buffer, &got, &code), "N05 warm-cache");
        host.faults.next_timeout = true;
        Check(!file->Read(0, &buffer, &got, &code) && code == ErrorCode::Timeout,
              "N05 timeout");
        host.faults.next_timeout = false;
        Check(!file->Read(0, &buffer, &got, &code) &&
                  code == ErrorCode::Unavailable,
              "N05 cache-blocked-by-poison");
    }

    {  // N06: fresh launch/session reads again; the old handle never rebinds.
        MemoryHostTransport host;
        host.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        std::unique_ptr<Session> s1;
        ErrorCode code = ErrorCode::Invalid;
        host.Connect(&s1, &code);
        std::unique_ptr<File> f1;
        s1->Open("a.nsp", &f1, &code);
        std::vector<std::uint8_t> buffer(4, 0);
        std::size_t got = 0;
        Check(f1->Read(0, &buffer, &got, &code), "N06 old-first-read");
        host.faults.next_timeout = true;
        Check(!f1->Read(0, &buffer, &got, &code), "N06 old-fails");
        host.faults.next_timeout = false;
        std::unique_ptr<Session> s2;
        Check(host.Connect(&s2, &code), "N06 fresh-connect");
        std::unique_ptr<File> f2;
        Check(s2->Open("a.nsp", &f2, &code) && f2->Read(0, &buffer, &got, &code) &&
                  Same(buffer, "ABCD"),
              "N06 new-session-reads");
        Check(!f1->Read(0, &buffer, &got, &code) &&
                  code == ErrorCode::Unavailable,
              "N06 old-handle-never-rebinds");
        Check(s1->Poisoned(), "N06 old-session-still-poisoned");
    }

    {  // N07: detected change poisons the whole session, not just the handle.
        MemoryHostTransport host;
        host.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        host.AddFile("b.xci", Text("WXYZ"), 1000, 2);
        std::unique_ptr<Session> session;
        ErrorCode code = ErrorCode::Invalid;
        host.Connect(&session, &code);
        std::unique_ptr<File> file;
        session->Open("b.xci", &file, &code);
        host.MutateFile("b.xci", Text("12345678"));
        std::vector<std::uint8_t> buffer(4, 0);
        std::size_t got = 0;
        Check(!file->Read(0, &buffer, &got, &code) && code == ErrorCode::Changed,
              "N07 change-detected");
        Check(session->Poisoned(), "N07 session-poisoned");
        std::vector<std::string> names;
        Check(!session->List(&names, &code) &&
                  code == ErrorCode::Unavailable,
              "N07 whole-session-down");
    }

    {  // N08: any name outside the admitted snapshot (removed, new or never
        // seen) is Changed and poisons the whole session (SMB-harmonized, F2).
        MemoryHostTransport host;
        host.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        std::unique_ptr<Session> session;
        ErrorCode code = ErrorCode::Invalid;
        host.Connect(&session, &code);
        host.RemoveFile("a.nsp");
        Stat st;
        Check(!session->Stat("a.nsp", &st, &code) && code == ErrorCode::Changed,
              "N08 known-removed-changed");
        Check(session->Poisoned(), "N08 poisoned");
        MemoryHostTransport other;
        other.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        std::unique_ptr<Session> clean;
        other.Connect(&clean, &code);
        Check(!clean->Stat("zz.nsp", &st, &code) && code == ErrorCode::Changed &&
                  clean->Poisoned(),
              "N08 unseen-name-poisons");
    }

    {  // N09: failed read after a copied prefix is never success.
        MemoryHostTransport host;
        host.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        std::unique_ptr<Session> session;
        ErrorCode code = ErrorCode::Invalid;
        host.Connect(&session, &code);
        std::unique_ptr<File> file;
        session->Open("a.nsp", &file, &code);
        std::vector<std::uint8_t> buffer(4, 0);
        std::size_t got = 99;
        host.faults.next_read_fail_after_prefix = true;
        Check(!file->Read(0, &buffer, &got, &code) && code == ErrorCode::Timeout,
              "N09 not-success");
        Check(got == 0, "N09 no-read-bytes-on-failure");
        Check(buffer[0] == 'A' && buffer[1] == 'B', "N09 prefix-may-be-copied");
        Check(session->Poisoned(), "N09 poisoned");
    }

    {  // N10: exact EOF is success with zero bytes; beyond EOF is Changed.
        MemoryHostTransport host;
        host.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        std::unique_ptr<Session> session;
        ErrorCode code = ErrorCode::Invalid;
        host.Connect(&session, &code);
        std::unique_ptr<File> file;
        session->Open("a.nsp", &file, &code);
        std::vector<std::uint8_t> buffer(4, 0);
        std::size_t got = 99;
        Check(file->Read(4, &buffer, &got, &code) && got == 0, "N10 exact-eof");
        Check(!file->Read(5, &buffer, &got, &code) &&
                  code == ErrorCode::Changed && session->Poisoned(),
              "N10 beyond-eof-changed");
    }

    {  // N11: Close is idempotent; closed handle is Invalid without poison.
        MemoryHostTransport host;
        host.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        std::unique_ptr<Session> session;
        ErrorCode code = ErrorCode::Invalid;
        host.Connect(&session, &code);
        std::unique_ptr<File> file;
        session->Open("a.nsp", &file, &code);
        file->Close();
        file->Close();
        std::vector<std::uint8_t> buffer(4, 0);
        std::size_t got = 0;
        Check(!file->Read(0, &buffer, &got, &code) && code == ErrorCode::Invalid,
              "N11 closed-invalid");
        Check(!session->Poisoned(), "N11 invalid-not-poison");
    }

    {  // N12: an empty stable share is a complete (empty) list.
        MemoryHostTransport host;
        std::unique_ptr<Session> session;
        ErrorCode code = ErrorCode::Invalid;
        host.Connect(&session, &code);
        std::vector<std::string> names;
        Check(session->List(&names, &code) && names.empty(), "N12 empty-complete");
    }

    {  // N13: namespace growth after connect is a detected change.
        MemoryHostTransport host;
        host.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        std::unique_ptr<Session> session;
        ErrorCode code = ErrorCode::Invalid;
        host.Connect(&session, &code);
        host.AddFile("d.nsp", Text("EFGH"), 2000, 4);
        std::vector<std::string> names;
        Check(!session->List(&names, &code) && code == ErrorCode::Changed,
              "N13 growth-changed");
        Check(names.empty() && session->Poisoned(), "N13 nothing-published");
    }

    {  // N14: opening a never-seen name is Changed and poisons (SMB-harmonized).
        MemoryHostTransport host;
        host.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        std::unique_ptr<Session> session;
        ErrorCode code = ErrorCode::Invalid;
        host.Connect(&session, &code);
        std::unique_ptr<File> file;
        Check(!session->Open("zz.nsp", &file, &code) &&
                  code == ErrorCode::Changed && session->Poisoned(),
              "N14 unseen-open-poisons");
    }

    {  // N15: a zero-length request is a successful no-op, not EOF and no
        // poison; exact EOF still reports zero bytes at offset == size (F1).
        MemoryHostTransport host;
        host.AddFile("a.nsp", Text("ABCD"), 1000, 1);
        std::unique_ptr<Session> session;
        ErrorCode code = ErrorCode::Invalid;
        host.Connect(&session, &code);
        std::unique_ptr<File> file;
        session->Open("a.nsp", &file, &code);
        std::vector<std::uint8_t> empty;
        std::size_t got = 99;
        Check(file->Read(0, &empty, &got, &code) && got == 0,
              "N15 empty-request-ok");
        Check(!session->Poisoned(), "N15 no-poison");
        std::vector<std::uint8_t> buffer(4, 0);
        Check(file->Read(4, &buffer, &got, &code) && got == 0,
              "N15 exact-eof-zero");
    }

    if (failures == 0)
        std::puts("PASS: nfs-transport-r2 host-only contract; real NFS client NOT exercised");
    return failures == 0 ? 0 : 1;
}
