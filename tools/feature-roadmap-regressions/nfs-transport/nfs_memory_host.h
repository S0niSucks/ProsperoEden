// SPDX-License-Identifier: GPL-3.0-or-later
// HOST-ONLY synthetic share for the r1 fixture. No sockets, no threads, no
// clock, no NFS: every failure is injected deterministically between calls by
// the test. Public state on purpose — this is a fault-injection harness, not
// production code, and must never be linked into the application.
#pragma once

#include "../interface/nfs_transport.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Eden::NfsTransport {

struct MemoryFile {
    std::vector<std::uint8_t> bytes;
    std::uint64_t mtime_ns = 0;
    std::uint64_t identity = 0;
};

struct MemoryFaults {
    // Mid-enumeration instability: List fails halfway (persists until cleared,
    // so a test can also verify the poison outlives the injection).
    bool list_fail_halfway = false;
    // Next operation of any kind reports Timeout (consumed once).
    bool next_timeout = false;
    // Next Read copies half the requested prefix into the caller's buffer and
    // then fails with Timeout (consumed once). Demonstrates rule 5.
    bool next_read_fail_after_prefix = false;
};

class MemoryHostTransport final : public Transport {
public:
    std::map<std::string, MemoryFile> files;
    MemoryFaults faults;

    void AddFile(const std::string& name, std::vector<std::uint8_t> bytes,
                 std::uint64_t mtime_ns, std::uint64_t identity);
    // Detectable change: size, mtime and identity all move together.
    void MutateFile(const std::string& name, std::vector<std::uint8_t> bytes);
    // Same size/mtime/identity, different bytes: NOT detectable by this
    // contract. Documents the stable-share precondition; it is a limitation,
    // not a guarantee (see fixture/scenarios.json).
    void MutateContentOnly(const std::string& name, std::vector<std::uint8_t> bytes);
    void RemoveFile(const std::string& name);

    // Always succeeds on this host; real-client Connect gates stay blocked.
    bool Connect(std::unique_ptr<Session>* out, ErrorCode* code) override;
};

}  // namespace Eden::NfsTransport
