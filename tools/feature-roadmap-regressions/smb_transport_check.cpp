// SPDX-License-Identifier: GPL-3.0-or-later
// P03 SMB increment r1: HOST fixture for the transport contract using the
// in-memory loopback. This is NOT the real SMB client: no library, socket,
// DNS or server is involved. Execution state: NOT_RUN by A03; the run command
// for the central host executor is in the workspace README.
#include "../patch/headless/smb_transport.h"
#include "../patch/headless/smb_transport_host.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {
int failures = 0;
void Check(bool ok, const char* name) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", name);
        ++failures;
    }
}
std::vector<std::string> Paths(const std::vector<Eden::Smb::Entry>& entries) {
    std::vector<std::string> paths;
    for (const auto& entry : entries) paths.push_back(entry.path);
    return paths;
}
Eden::Smb::MemoryTransport::Tree MakeTree() {
    Eden::Smb::MemoryTransport::Tree tree;
    const auto add = [&tree](const char* path, bool directory, std::string bytes,
                             std::uint64_t inode) {
        Eden::Smb::MemoryTransport::Node node;
        node.directory = directory;
        node.bytes = std::move(bytes);
        node.inode = inode;
        node.modified = 1000;
        node.modified_ns = 7;
        tree.emplace(path, std::move(node));
    };
    add("", true, "", 1);
    add("roms", true, "", 2);
    add("docs", true, "", 3);
    add("Top.nsp", false, "TOPLEVEL-PAYLOAD", 4);
    add("roms/Game A.nsp", false, std::string(300, 'A'), 5);
    add("roms/Homebrew.nro", false, "NRO-PAYLOAD", 6);
    add("docs/readme.txt", false, "readme", 7);
    return tree;
}
} // namespace

int main() {
    using Eden::Smb::Entry;
    using Eden::Smb::MemoryTransport;

    { // T01 stat of the root and a file keeps the session healthy
        MemoryTransport transport{MakeTree()};
        Entry root{}, file{};
        Check(transport.Stat("", &root) && root.directory && root.path.empty(),
              "T01 stat root");
        Check(transport.Stat("Top.nsp", &file) && file.size == 16 && file.inode == 4 &&
                  !file.directory,
              "T01 stat file identity");
        Check(transport.Healthy() && transport.Error().empty(), "T01 healthy");
    }
    { // T02 unsafe paths are caller errors: refused, never poisoned
        MemoryTransport transport{MakeTree()};
        std::vector<Entry> listed;
        for (const char* bad : {"..", "a/../b", "/abs", "a//b", "a\\b", "a:b", "a%b",
                                "a?b", "a#b", "a/", "rom\ns"}) {
            Check(!transport.Stat(bad, nullptr), "T02 stat refuses unsafe path");
            Check(!transport.List(bad, &listed) && listed.empty(),
                  "T02 list refuses unsafe path");
        }
        Check(transport.Healthy(), "T02 unsafe paths do not poison");
    }
    { // T03 listing is complete, sorted, one level, stable when repeated
        MemoryTransport transport{MakeTree()};
        std::vector<Entry> root, roms, repeat;
        Check(transport.List("", &root) && root.size() == 3, "T03 root complete");
        Check(Paths(root) == std::vector<std::string>({"Top.nsp", "docs", "roms"}),
              "T03 root sorted");
        Check(!root[0].directory && root[1].directory && root[2].directory,
              "T03 kinds");
        Check(transport.List("roms", &roms) &&
                  Paths(roms) ==
                      std::vector<std::string>({"roms/Game A.nsp", "roms/Homebrew.nro"}),
              "T03 subdir full paths one level");
        Check(transport.List("", &repeat) && Paths(repeat) == Paths(root),
              "T03 stable repeat");
        Check(transport.Healthy(), "T03 healthy");
    }
    { // T04 a missing path is a share-level failure: no listing, poisoned
        MemoryTransport transport{MakeTree()};
        std::vector<Entry> listed;
        listed.push_back(Entry{}); // sentinel: must be cleared
        Check(!transport.List("missing", &listed) && listed.empty(),
              "T04 missing clears output");
        Check(!transport.Healthy() && !transport.Error().empty(),
              "T04 missing poisons");
        Check(!transport.Stat("Top.nsp", nullptr), "T04 poisoned session refuses stat");
    }
    { // T05 timeout DURING enumeration never yields a silent partial list
        MemoryTransport transport{MakeTree()};
        transport.Inject(MemoryTransport::Fault::ListTimeoutPartial, 1);
        std::vector<Entry> listed;
        Check(!transport.List("roms", &listed) && listed.empty(),
              "T05 partial timeout returns empty listing");
        Check(!transport.Healthy(), "T05 partial timeout poisons");
        Check(!transport.List("roms", &listed) && listed.empty(),
              "T05 later listing refused");
    }
    { // T06 timeout BEFORE enumeration
        MemoryTransport transport{MakeTree()};
        transport.Inject(MemoryTransport::Fault::ListTimeout);
        std::vector<Entry> listed;
        Check(!transport.List("roms", &listed) && listed.empty() && !transport.Healthy(),
              "T06 early timeout refuses whole listing");
    }
    { // T07 open/read happy path, offsets, EOF and empty reads
        MemoryTransport transport{MakeTree()};
        Entry identity{};
        const auto handle = transport.Open("roms/Game A.nsp", &identity);
        Check(handle >= 0 && identity.size == 300, "T07 open identity");
        std::string payload(300, '\0');
        auto* buffer = reinterpret_cast<unsigned char*>(payload.data());
        Check(transport.Read(handle, 0, buffer, 300) == 300, "T07 full read");
        Check(payload == std::string(300, 'A'), "T07 payload bytes");
        Check(transport.Read(handle, 10, buffer, 5) == 5 &&
                  payload.compare(10, 5, "AAAAA") == 0,
              "T07 offset read");
        Check(transport.Read(handle, 300, buffer, 16) == 0, "T07 EOF read is zero");
        Check(transport.Read(handle, 0, nullptr, 0) == 0, "T07 empty read is zero");
        Check(transport.Healthy(), "T07 healthy");
        transport.Close(handle);
        Check(transport.Read(handle, 0, buffer, 4) == 0 && transport.Healthy(),
              "T07 closed handle refused without poison");
    }
    { // T08 invalid handles are caller errors, not poison
        MemoryTransport transport{MakeTree()};
        unsigned char byte = 0;
        Check(transport.Read(-1, 0, &byte, 1) == 0, "T08 negative handle");
        Check(transport.Read(999, 0, &byte, 1) == 0, "T08 unknown handle");
        Check(transport.Healthy(), "T08 no poison");
    }
    { // T09 opening a directory or the root is a caller refusal
        MemoryTransport transport{MakeTree()};
        Check(transport.Open("roms", nullptr) == -1, "T09 directory refused");
        Check(transport.Open("", nullptr) == -1, "T09 root refused");
        Check(transport.Healthy(), "T09 no poison");
    }
    { // T10 a read failure poisons the session permanently
        MemoryTransport transport{MakeTree()};
        const auto handle = transport.Open("Top.nsp", nullptr);
        transport.Inject(MemoryTransport::Fault::ReadError);
        unsigned char byte = 0;
        Check(transport.Read(handle, 0, &byte, 1) == 0, "T10 failing read");
        Check(!transport.Healthy() && !transport.Error().empty(), "T10 poisoned");
        Check(transport.Read(handle, 0, &byte, 1) == 0, "T10 read refused after poison");
        Check(!transport.Stat("Top.nsp", nullptr), "T10 stat refused after poison");
        Check(transport.Open("Top.nsp", nullptr) < 0, "T10 open refused after poison");
        transport.Close(handle); // still safe on a poisoned session
    }
    { // T11 truncated read: a prefix may be copied, only the count is truth
        MemoryTransport transport{MakeTree()};
        const auto handle = transport.Open("Top.nsp", nullptr);
        unsigned char buffer[16];
        std::fill(buffer, buffer + sizeof(buffer), 0xAA);
        transport.Inject(MemoryTransport::Fault::ReadTruncated);
        Check(transport.Read(handle, 0, buffer, sizeof(buffer)) == 0,
              "T11 truncated reports zero");
        Check(!transport.Healthy(), "T11 truncated poisons");
        // Deliberately NOT asserting buffer contents: after failure the buffer
        // is meaningless by contract, even though a prefix may have landed in
        // it. Guest error gating must rely on the count/result, never bytes.
    }
    { // T12 mutation between open and read invalidates the open reader
        MemoryTransport transport{MakeTree()};
        const auto handle = transport.Open("Top.nsp", nullptr);
        Check(transport.Mutate("Top.nsp", "SHORTER"), "T12 external mutation applied");
        unsigned char byte = 0;
        Check(transport.Read(handle, 0, &byte, 1) == 0, "T12 mutated read fails");
        Check(!transport.Healthy() && !transport.Error().empty(),
              "T12 mutation poisons");
        transport.Close(handle);
    }
    { // T13 discovery-to-open staleness is the CALLER's comparison to make
        MemoryTransport transport{MakeTree()};
        std::vector<Entry> listed;
        Check(transport.List("", &listed) && listed.size() == 3, "T13 listing");
        const Entry& discovered = listed[0]; // "Top.nsp"
        Check(transport.Mutate("Top.nsp", "MUTATED-PAYLOAD!!"), "T13 mutation");
        Entry opened{};
        const auto handle = transport.Open("Top.nsp", &opened);
        Check(handle >= 0 && opened.inode != discovered.inode &&
                  opened.size != discovered.size,
              "T13 open reports the NEW identity");
        Check(transport.Healthy(),
              "T13 transport stays truthful; the VFS adapter must compare");
        transport.Close(handle);
    }
    { // T14 recovery is a NEW session over the same share, never a reset
        MemoryTransport transport{MakeTree()};
        transport.Inject(MemoryTransport::Fault::StatError);
        Check(!transport.Stat("Top.nsp", nullptr) && !transport.Healthy(),
              "T14 poisoned");
        MemoryTransport fresh{MakeTree()};
        Entry entry{};
        std::vector<Entry> listed;
        Check(fresh.Stat("Top.nsp", &entry) && fresh.List("", &listed) &&
                  listed.size() == 3,
              "T14 fresh session works");
        Check(!transport.Healthy(), "T14 old session stays poisoned forever");
    }
    { // T15 same-metadata content replacement is NOT detected (no snapshot)
        MemoryTransport transport{MakeTree()};
        const auto handle = transport.Open("Top.nsp", nullptr);
        Check(transport.MutateQuiet("Top.nsp", "QQQQQQQQQQQQQQQQ"),
              "T15 quiet mutation");
        std::string payload(16, '\0');
        auto* buffer = reinterpret_cast<unsigned char*>(payload.data());
        Check(transport.Read(handle, 0, buffer, 16) == 16 &&
                  payload == "QQQQQQQQQQQQQQQQ",
              "T15 quiet mutation reads through undetected");
        Check(transport.Healthy(), "T15 documented limitation, not a guarantee");
        transport.Close(handle);
    }
    { // T16 close is always safe
        MemoryTransport transport{MakeTree()};
        const auto handle = transport.Open("Top.nsp", nullptr);
        transport.Close(handle);
        transport.Close(handle); // double close
        transport.Close(-1);
        transport.Close(999);
        Check(transport.Healthy(), "T16 close never disturbs a healthy session");
    }
    { // T17 a stat fault poisons like any share-level failure
        MemoryTransport transport{MakeTree()};
        transport.Inject(MemoryTransport::Fault::StatError);
        Check(!transport.Stat("Top.nsp", nullptr) && !transport.Healthy(),
              "T17 stat failure poisons");
    }
    { // T18 mutation of another file does not disturb an open reader
        MemoryTransport transport{MakeTree()};
        const auto handle = transport.Open("Top.nsp", nullptr);
        Check(transport.Mutate("docs/readme.txt", "other!"),
              "T18 other file mutated");
        std::string payload(16, '\0');
        auto* buffer = reinterpret_cast<unsigned char*>(payload.data());
        Check(transport.Read(handle, 0, buffer, 16) == 16 && transport.Healthy(),
              "T18 reader of an untouched file survives");
        transport.Close(handle);
    }
    { // T19 List over an existing regular FILE poisons (documented R3-01)
        MemoryTransport transport{MakeTree()};
        std::vector<Entry> listed;
        Check(!transport.List("Top.nsp", &listed) && listed.empty(),
              "T19 list over file clears output");
        Check(!transport.Healthy() && !transport.Error().empty(),
              "T19 list over file poisons (R3-01 divergence, pinned)");
    }

    if (failures == 0)
        std::puts("PASS: SMB transport contract on the host loopback; real client NOT_TESTED");
    return failures == 0 ? 0 : 1;
}
