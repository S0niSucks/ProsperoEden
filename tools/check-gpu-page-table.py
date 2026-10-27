#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the real generated GPU page table with a small simulated flexible-memory pool.

The allocator stand-in is read-only until committed, like the PS5 sparse table. This tests
the table's actual constructor/reservation/destructor, not console memory availability.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
source = build.parent / 'source/src'
original = (source / 'common/multi_level_page_table.inc').read_text()
generated = (build / 'headless/multi_level_page_table.cpp').read_text()
program = r'''
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <sys/mman.h>
static constexpr std::size_t slot = 0x200000;
struct Range { std::size_t size; bool dense; };
static std::map<std::uintptr_t, Range> ranges;
static bool dense = false, fail = false;
static unsigned allocations = 0, commits = 0, soft_asserts = 0;
static void* DeniedMmap(void* address, std::size_t size, int protection, int flags, int fd, off_t offset) {
    if (size > 229u * 1024 * 1024) { errno = ENOMEM; return MAP_FAILED; }
    return mmap(address, size, protection, flags, fd, offset);
}
namespace Common {
void* AllocateSparsePages(std::size_t size) noexcept {
    if (fail) { errno = ENOMEM; return nullptr; }
    void* p = mmap(nullptr, size, dense ? PROT_READ | PROT_WRITE : PROT_READ,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    assert(p != MAP_FAILED);
    ranges.emplace(reinterpret_cast<std::uintptr_t>(p), Range{size, dense}); ++allocations;
    return p;
}
void FreeSparsePages(void* p, std::size_t size) noexcept {
    auto it = ranges.find(reinterpret_cast<std::uintptr_t>(p));
    assert(it != ranges.end() && it->second.size == size);
    assert(munmap(p, size) == 0); ranges.erase(it);
}
void CommitSparsePage(std::uintptr_t p) noexcept {
    auto it = ranges.upper_bound(p);
    assert(it != ranges.begin()); --it;
    assert(p >= it->first && p - it->first < it->second.size);
    if (it->second.dense) return;
    const auto at = it->first + (p - it->first) / slot * slot;
    const auto length = std::min(slot, it->second.size - (at - it->first));
    assert(mprotect(reinterpret_cast<void*>(at), length, PROT_READ | PROT_WRITE) == 0);
    ++commits;
}
}
#define mmap DeniedMmap
SOURCE
#undef mmap
int main(int argc, char** argv) {
    if (argc > 1 && std::string{argv[1]} == "failure") {
        fail = true;
        bool caught = false;
        try { Common::MultiLevelPageTable<std::uint32_t> table(40, 12, 12); }
        catch (const std::bad_alloc&) { caught = true; }
        assert(ranges.empty());
        return caught ? 0 : 12;
    }
    dense = argc > 1;
    for (unsigned cycle = 0; cycle < 5; ++cycle) {
        Common::MultiLevelPageTable<std::uint32_t> table(40, 12, 12);
        if (!table.data()) return 11; // negative control: original soft assert continued with null
        const auto boundary = slot / sizeof(std::uint32_t);
        assert(table[0] == 0 && table[boundary] == 0 && table[(1u << 28) - 1] == 0);
        table.ReserveRange(0, 0x1000);
        table[0] = 7; assert(table[1] == 0);
        table.ReserveRange((boundary - 1ULL) << 12, 0x1000);
        table[boundary - 1] = 11; table[boundary] = 13;
        table.ReserveRange(((1ULL << 28) - 1) << 12, 0);
        table[(1u << 28) - 1] = 17;
        Common::MultiLevelPageTable<std::uint64_t> other(32, 10, 12);
        other.ReserveRange(0, 0x1000); other[0] = 23;
        assert(table[0] == 7 && table[boundary - 1] == 11 && table[boundary] == 13 && other[0] == 23);
    }
    assert(ranges.empty() && allocations == 10 && soft_asserts == 0);
    assert(dense || commits >= 20);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='eden-gpu-page-table-') as tmp:
    temp = Path(tmp)
    (temp / 'common').mkdir()
    (temp / 'common/assert.h').write_text('#pragma once\n#define ASSERT(x) ((x) ? (void)0 : (void)++soft_asserts)\n')
    (temp / 'common/common_types.h').write_text('#pragma once\n#include <cstdint>\nusing u64 = std::uint64_t; using u32 = std::uint32_t;\n')
    (temp / 'common/multi_level_page_table.h').write_text((source / 'common/multi_level_page_table.h').read_text())
    assert 'throw std::bad_alloc{}' in generated
    # A second negative control isolates the fail-closed guard from the sparse-allocation fix.
    guard_start = generated.index('    if (!base) {')
    guard_end = generated.index('    base_ptr = reinterpret_cast<BaseAddr*>(base);', guard_start)
    no_guard = generated[:guard_start] + '    ASSERT(base);\n' + generated[guard_end:]
    for name, text in [('original', original), ('generated', generated), ('no-guard', no_guard)]:
        cpp, exe = temp / f'{name}.cpp', temp / name
        cpp.write_text(program.replace('SOURCE', text))
        subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-O1', '-Wall', '-Wextra',
                        '-Wno-unused-function', '-I', str(temp), str(cpp), '-o', str(exe)], check=True)
        modes = ['sparse', 'dense', 'failure'] if name == 'generated' else ['failure' if name == 'no-guard' else 'sparse']
        for mode in modes:
            result = subprocess.run([str(exe), *([] if mode == 'sparse' else [mode])], capture_output=True, text=True, timeout=30)
            expected = 11 if name == 'original' else 12 if name == 'no-guard' else 0
            assert result.returncode == expected, (name, mode, result.returncode, result.stderr)
            if name == 'generated' and mode == 'failure':
                for field in ['EDEN_GPU_PAGE_TABLE refused stage=reserve', 'address_bits=40', 'page_bits=12', 'bytes=1073741824', 'errno=12']:
                    assert field in result.stderr, result.stderr
print('GPU page table PASS: actual 40-bit constructor, zero reads, 2 MiB boundary commits, '
      'multiple tables, five release/recreate cycles, dense fallback and injected refusal; '
      'original and removed-guard negative controls rejected')
