#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Adapted from ProsperoEden 20ef10e6ec2ffb6556fcfd4b61b3ed899f1db26b.
"""Execute the real ASET parser and bridge readers against hostile file layouts.

The VFS/NACP stand-ins only observe reads/construction; this is not a native
core, decryption or PS5 loader qualification. Run inside the existing Docker
toolchain with the repository mounted read-only and a writable /tmp tmpfs.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'headless/metadata_bridge.cpp').read_text()
start = source.index('Eden::NroAssets::Layout NroLayout(')
end = source.index('\n}\n}', source.index('std::unique_ptr<FileSys::NACP> NroControl(', start)) + 2
readers = source[start:end]
code = r'''
#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include "nro_assets.h"
#include "game_files.h"
namespace FileSys {
struct RawNACP { unsigned char bytes[0x4000]; };
struct File {
    std::vector<unsigned char> bytes;
    std::size_t truncate_at = std::numeric_limits<std::size_t>::max();
    unsigned reads = 0;
    virtual ~File() = default;
    virtual std::size_t GetSize() const { return bytes.size(); }
    virtual std::size_t Read(unsigned char* out, std::size_t count, std::uint64_t at) {
        ++reads;
        const auto available = std::min(bytes.size(), truncate_at);
        if (at >= available) return 0;
        count = std::min(count, available - static_cast<std::size_t>(at));
        std::memcpy(out, bytes.data() + at, count);
        return count;
    }
    std::size_t ReadObject(RawNACP* out) {
        return Read(reinterpret_cast<unsigned char*>(out), sizeof(*out), 0);
    }
};
using VirtualFile = std::shared_ptr<File>;
struct OffsetVfsFile : File {
    VirtualFile file;
    std::size_t size;
    std::uint64_t offset;
    OffsetVfsFile(VirtualFile f, std::size_t s, std::uint64_t o, const char*)
        : file(std::move(f)), size(s), offset(o) {}
    std::size_t GetSize() const override { return size; }
    std::size_t Read(unsigned char* out, std::size_t count, std::uint64_t at) override {
        assert(at <= size && count <= size - at);
        return file->Read(out, count, offset + at);
    }
};
struct NACP {
    static inline unsigned constructed = 0;
    explicit NACP(const VirtualFile& file) { assert(file->GetSize() == 0x4000); ++constructed; }
};
}
''' + readers + r'''
using namespace Eden::NroAssets;
void Put(std::vector<unsigned char>& bytes, std::size_t at, std::uint64_t value, unsigned size) {
    for (unsigned n = 0; n < size; ++n) bytes.at(at + n) = static_cast<unsigned char>(value >> (n * 8));
}
std::vector<unsigned char> Fixture() {
    std::vector<unsigned char> bytes(0x80 + 0x38 + 16 + 0x4000 + 32);
    std::memcpy(bytes.data() + 0x10, "NRO0", 4);
    Put(bytes, 0x18, 0x80, 4);
    std::memcpy(bytes.data() + 0x80, "ASET", 4);
    Put(bytes, 0x88, 0x38, 8); Put(bytes, 0x90, 16, 8);
    Put(bytes, 0x98, 0x48, 8); Put(bytes, 0xa0, 0x4000, 8);
    Put(bytes, 0xa8, 0x4048, 8); Put(bytes, 0xb0, 32, 8);
    return bytes;
}
Layout Check(const std::vector<unsigned char>& bytes, unsigned short_read = 0,
             std::uint64_t file_size = 0) {
    unsigned calls = 0;
    auto layout = Read(file_size ? file_size : bytes.size(),
        [&](unsigned char* out, std::size_t count, std::uint64_t at) {
            assert(++calls <= 2 && count <= 0x80); // never read file-sized assets
            if (at > bytes.size() || count > bytes.size() - at) return std::size_t{0};
            std::memcpy(out, bytes.data() + at, count);
            return count - (calls == short_read ? 1 : 0);
        });
    return layout;
}
int main() {
    const auto valid = Fixture();
    const auto layout = Check(valid);
    assert(layout.valid && layout.icon.offset == 0xb8 && layout.icon.size == 16);
    assert(layout.nacp.offset == 0xc8 && layout.nacp.size == 0x4000);
    assert(layout.romfs.offset == 0x40c8 && layout.romfs.size == 32);
    for (unsigned read : {1, 2}) assert(!Check(valid, read).valid);
    for (std::size_t length : {0u, 0x7fu, 0x80u, 0xb7u, 0x40e7u}) {
        auto bytes = valid; bytes.resize(length); assert(!Check(bytes).valid);
    }
    auto bytes = valid;
    bytes[0x10] = 'X'; assert(!Check(bytes).valid);
    for (auto size : {0u, 0x7fu, 0xffffffffu}) {
        bytes = valid; Put(bytes, 0x18, size, 4); assert(!Check(bytes).valid);
    }
    bytes = valid; bytes[0x80] = 'X'; assert(!Check(bytes).valid);
    bytes = valid; Put(bytes, 0x84, 1, 4); assert(!Check(bytes).valid);
    constexpr auto max = std::numeric_limits<std::uint64_t>::max();
    for (auto at : {0x88, 0x98, 0xa8}) {
        for (auto offset : {std::uint64_t{0}, std::uint64_t{0x37}, max}) {
            bytes = valid; Put(bytes, at, offset, 8); assert(!Check(bytes).valid);
        }
        bytes = valid; Put(bytes, at + 8, max, 8); assert(!Check(bytes).valid);
        // Even a virtual file at UINT64_MAX must reject wrapped section ends.
        bytes = valid; Put(bytes, at, max - 0x80, 8); assert(!Check(bytes, 0, max).valid);
    }
    for (auto size : {0x3fffu, 0x4001u}) {
        bytes = valid; Put(bytes, 0xa0, size, 8); assert(!Check(bytes).valid);
    }
    // All three pairs, containment and a one-byte overlap. Adjacency is valid.
    bytes = valid; Put(bytes, 0x98, 0x47, 8); assert(!Check(bytes).valid);
    bytes = valid; Put(bytes, 0xa8, 0x38, 8); assert(!Check(bytes).valid);
    bytes = valid; Put(bytes, 0xa8, 0x4047, 8); assert(!Check(bytes).valid);
    bytes = valid; Put(bytes, 0xa8, 0x100, 8); assert(!Check(bytes).valid);
    bytes = valid; bytes.resize(0xb8 + 1024 * 1024 + 1);
    Put(bytes, 0xa0, 0, 8); Put(bytes, 0xb0, 0, 8);
    Put(bytes, 0x90, 1024 * 1024 + 1, 8); assert(!Check(bytes).valid);
    Put(bytes, 0x90, 1024 * 1024, 8); assert(Check(bytes).valid);
    for (auto at : {0x90, 0xa0, 0xb0}) {
        bytes = valid; Put(bytes, at, 0, 8); assert(Check(bytes).valid);
    }
    bytes = valid;
    for (auto at : {0x88, 0x98, 0xa8}) { Put(bytes, at, max, 8); Put(bytes, at + 8, 0, 8); }
    bytes.resize(0xb8);
    assert(Check(bytes).valid); // empty assets; unused offsets do not cause reads
    // A valid far-end RomFS is file-backed and never allocated/read by the parser.
    Put(bytes, 0xa8, max - 0x80 - 32, 8); Put(bytes, 0xb0, 32, 8);
    auto huge = Check(bytes, 0, max);
    assert(huge.valid && huge.romfs.offset == max - 32 && huge.romfs.size == 32);

    auto file = std::make_shared<FileSys::File>();
    file->bytes = valid;
    auto bridged = NroLayout(file);
    assert(bridged.valid && file->reads == 2);
    assert(NroControl(file, bridged) && FileSys::NACP::constructed == 1);
    file->truncate_at = 0xc8 + 0x3fff;
    assert(!NroControl(file, bridged) && FileSys::NACP::constructed == 1);
    assert(!NroControl(file, {}) && FileSys::NACP::constructed == 1);
    bridged.nacp.size = 0;
    assert(!NroControl(file, bridged) && FileSys::NACP::constructed == 1);
    assert(Eden::EndsWithNoCase("Demo.NRO", ".nro"));
    for (const auto* path : {"Demo.nro.nsp", "Demo.nro/", "Demo.nso", "Demo.nca", "Demo.kip"})
        assert(!Eden::EndsWithNoCase(path, ".nro"));
}
'''
compiler = os.environ.get('CXX') or next(
    (c for c in ('clang++-18', 'clang++', 'g++') if shutil.which(c)), None)
assert compiler, 'C++20 compiler required inside the Docker toolchain'
with tempfile.TemporaryDirectory(prefix='encore-nro-assets-') as tmp:
    for opt in ('-O0', '-O2'):
        exe = Path(tmp) / ('nro-assets' + opt)
        subprocess.run([compiler, '-std=c++20', opt, '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                        '-I', str(root / 'headless'), '-x', 'c++', '-', '-o', str(exe)],
                       input=code, text=True, check=True)
        subprocess.run([str(exe)], check=True)
        print(f'NRO assets: PASS {opt} ASan/UBSan: bounded bridge reads, truncation, '
              'overflow/overlap, 1 MiB icon cap, fixed NACP and optional assets')
