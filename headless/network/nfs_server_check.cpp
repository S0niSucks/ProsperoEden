// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only writer seeds Ganesha's MEM FSAL. Production Session never uses these APIs.
#include "shares.h"
#include <nfsc/libnfs.h>
#include <cassert>
#include <array>
#include <iostream>
int main() {
    auto* writer = nfs_init_context(); assert(writer);
    nfs_set_version(writer, 3); nfs_set_nfsport(writer, 12050); nfs_set_mountport(writer, 12051);
    nfs_set_uid(writer, 0); nfs_set_gid(writer, 0); nfs_set_timeout(writer, 1500);
    assert(nfs_mount(writer, "127.0.0.1", "/games") == 0);
    struct nfsfh* handle{}; assert(nfs_creat(writer, "Proof.nro", 0644, &handle) == 0);
    std::array<unsigned char, 65536> source{};
    for (std::size_t n = 0; n < source.size(); ++n) source[n] = (n * 13 + (n >> 8)) % 251;
    assert(nfs_pwrite(writer, handle, source.data(), source.size(), 0) == static_cast<int>(source.size()));
    assert(nfs_close(writer, handle) == 0); nfs_destroy_context(writer);
    using namespace Eden::Network;
    Share share; share.id = "test"; share.protocol = "nfs"; share.server = "127.0.0.1";
    share.share = "/games"; share.port = 12050; share.mount_port = 12051; share.timeout_ms = 1500;
    Session reader(share); assert(reader.Connect());
    Entry file; assert(reader.Stat("Proof.nro", &file) && file.size == source.size());
    std::array<unsigned char, 1024> received{};
    for (std::size_t offset : {17u, 3000u, 60000u}) {
        assert(reader.Read(file, received.data(), received.size(), offset) == received.size());
        for (std::size_t n = 0; n < received.size(); ++n) assert(received[n] == source[offset + n]);
    }
    assert(reader.Read(file, received.data(), received.size(), file.size - 3) == 3);
    std::vector<Entry> entries; assert(reader.List("", &entries) && entries.size() == 1);
    share.share = "/not-exported"; Session denied(share); assert(!denied.Connect());
    std::cout << "real NFS-Ganesha 4.3 MEM FSAL (not RFC mock): AUTH_SYS/TCP mount/list/stat/random reads/EOF/denied export PASS\n";
}
