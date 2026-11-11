#!/usr/bin/env python3
"""Source-extracted A32 window page bounds, including overflowing input spans."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/host_memory.cpp").read_text()
for constant in (
    "constexpr std::size_t GuestPage = 0x1000;",
    "constexpr std::size_t GuestPages = std::size_t{1} << 20;",
    "constexpr std::size_t WindowBytes = GuestPages * GuestPage;",
):
    assert constant in source, "requalify changed fastmem window dimensions"
begin = "    static bool Pages("
end = "\n    bool Aliasable("
assert source.count(begin) == source.count(end) == 1
method = begin + source.split(begin, 1)[1].split(end, 1)[0]
fixed = "last = (offset + std::min(length, WindowBytes - offset) - 1) / GuestPage;"
old = "last = (std::min(offset + length, WindowBytes) - 1) / GuestPage;"
assert method.count(fixed) == 1 and old not in method
baseline = method.replace(fixed, old)
cmake = (ROOT / "headless/CMakeLists.txt").read_text()
assert '"${PORT_ROOT}/src/host_memory.cpp"' in cmake
for name in ("Map", "Unmap", "Protect"):
    body = source.split("    void " + name + "(", 1)[1].split("\n    }", 1)[0]
    assert "if (!Pages(offset, length, first, last)) return;" in body
assert "python3 -B tools/check-fastmem-window-bounds-host.py" in (
    ROOT / ".github/workflows/encore-core-preflight.yml").read_text()

cpp = r"""
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
struct Window {
    static constexpr std::size_t GuestPage = 0x1000;
    static constexpr std::size_t GuestPages = std::size_t{1} << 20;
    static constexpr std::size_t WindowBytes = GuestPage * GuestPages;
METHOD
};
int main() {
    using W = Window;
    constexpr auto max = std::numeric_limits<std::size_t>::max();
    std::size_t first = 0, last = 0;
    assert(W::Pages(1, max, first, last));
    assert(first == 0 && last == W::GuestPages - 1);
    unsigned cases = 0;
    for (const auto offset : std::array<std::size_t, 10>{
             0, 1, 4095, 4096, 4097, W::WindowBytes - 4096,
             W::WindowBytes - 1, W::WindowBytes, W::WindowBytes + 1, max}) {
        for (const auto length : std::array<std::size_t, 9>{
                 0, 1, 2, 4095, 4096, 4097, W::WindowBytes, max - offset, max}) {
            first = last = 123;
            const bool valid = W::Pages(offset, length, first, last);
            assert(valid == (offset < W::WindowBytes && length != 0));
            if (valid) {
                assert(first == offset / W::GuestPage);
                assert(first <= last && last < W::GuestPages);
                // Independent oracle: clamp to the final page without adding
                // until the requested final byte is known to be in range.
                const auto expected = length - 1 >= W::WindowBytes - 1 - offset
                    ? W::GuestPages - 1 : (offset + length - 1) / W::GuestPage;
                assert(last == expected);
            } else {
                assert(first == 123 && last == 123);
            }
            ++cases;
        }
    }
    std::printf("FASTMEM_WINDOW_BOUNDS_PASS cases=%u full_span_no_overflow=1\n", cases);
}
"""
compiler = shutil.which("clang++-18") or shutil.which("clang++") or shutil.which("g++")
assert compiler
with tempfile.TemporaryDirectory(prefix="encore-fastmem-bounds-") as temp:
    folder = Path(temp)
    for name, body in (("original", baseline), ("maintained", method)):
        path, executable = folder / (name + ".cpp"), folder / name
        path.write_text(cpp.replace("METHOD", body))
        subprocess.run([compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        str(path), "-o", str(executable)], check=True, timeout=60)
        result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15)
        if name == "original":
            assert result.returncode != 0 and "Assertion" in result.stderr, result.stderr
            print("ORIGINAL_FASTMEM_WRAPPED_PAGE_WITNESS_PASS")
        else:
            assert result.returncode == 0, result.stderr
            print(result.stdout, end="")
print("FASTMEM_WINDOW_HOST_ONLY native_PS5=unverified")

# Execute the real Own/Refresh methods against distinct access/map arrays.
# The production caller holds the window mutex across these stable arrays.
own = '    std::uint8_t Own(' + source.split('    std::uint8_t Own(', 1)[1].split(
    "    // A page's byte", 1)[0]
own = own.replace('const {\n', 'const {\n        ++calls;\n', 1)
refresh_source = (ROOT / 'src/host_memory.cpp').read_text()
refresh = '    void Refresh(' + refresh_source.split('    void Refresh(', 1)[1].split(
    '    void Block(', 1)[0]
previous_refresh = '''    void Before(std::size_t first, std::size_t last) {
        for (std::size_t page = first ? first - 1 : 0; page <= last; ++page)
            map[page] = Own(page) | Own(page + 1);
    }
'''
constants = refresh_source.split('namespace {\nconstexpr std::size_t GuestPage', 1)[1].split(
    'constexpr std::uint64_t ProbeMarker', 1)[0]
constants = 'constexpr std::size_t GuestPage' + constants
constants = '\n'.join(line for line in constants.splitlines() if not line.startswith(
    ('constexpr std::size_t WindowBytes', 'constexpr std::size_t MapBytes', 'constexpr std::size_t ReserveBytes')))
refresh_cpp = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>
CONSTANTS
struct TestWindow {
    std::vector<std::uint8_t> chunk_aliased, page_access, bytes;
    std::uint8_t* map;
    mutable std::size_t calls = 0;
    TestWindow() : chunk_aliased(Chunks), page_access(GuestPages),
                   bytes(GuestPages, 0xa5), map(bytes.data()) {
        for (std::size_t c = 0; c < Chunks; ++c) chunk_aliased[c] = (c % 3) != 0;
        for (std::size_t p = 0; p < GuestPages; ++p) page_access[p] = p % 4;
    }
OWN
BEFORE
AFTER
};
int main() {
    TestWindow old, current;
    const std::array<std::pair<std::size_t, std::size_t>, 6> ranges{{
        {0, 0}, {1, 1}, {3, 12}, {GuestPages-1, GuestPages-1},
        {GuestPages-8, GuestPages-1}, {0, GuestPages-1}}};
    for (const auto& [first, last] : ranges) {
        std::fill(old.bytes.begin(), old.bytes.end(), 0xa5);
        std::fill(current.bytes.begin(), current.bytes.end(), 0xa5);
        old.calls = current.calls = 0;
        old.Before(first, last);
        current.Refresh(first, last);
        assert(old.bytes == current.bytes);
        const auto begin = first ? first - 1 : 0;
        const auto n = last - begin + 1;
        assert(old.calls == 2*n && current.calls == n+1);
        for (std::size_t p = begin; p <= last; ++p) {
            const auto expected = old.Own(p) | old.Own(p+1);
            assert(current.bytes[p] == expected);
        }
    }
    std::printf("FASTMEM_REFRESH_PASS whole_window_before=%zu after=%zu "
                "output_bytes_identical=1\n", 2*GuestPages, GuestPages+1);
}
'''.replace('CONSTANTS', constants).replace('OWN', own).replace(
    'BEFORE', previous_refresh).replace('AFTER', refresh)
with tempfile.TemporaryDirectory(prefix='encore-fastmem-refresh-') as temp:
    path, executable = Path(temp)/'refresh.cpp', Path(temp)/'refresh'
    path.write_text(refresh_cpp)
    subprocess.run([compiler, '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    str(path), '-o', str(executable)], check=True, timeout=60)
    subprocess.run([str(executable)], check=True, timeout=15)

# Exact source-extracted Window::Protect witness: one-page permission no-op
# leaves JIT map bytes unchanged and saves a Refresh; changed and cross-page
# requests continue to publish updated bytes under the same owner mutex.
protect_body = '    void Protect(' + source.split('    void Protect(', 1)[1].split('\n    }', 1)[0] + '\n    }'
assert protect_body.count('if (first == last && page_access[first] == access) return;') == 1
assert 'first / PagesPerGranule == last / PagesPerGranule' in protect_body
assert 'std::all_of(page_access.begin() + first, page_access.begin() + last + 1' in protect_body
assert 'first < last && last - first < 16' in protect_body
assert protect_body.index('std::lock_guard lock{mutex};') < protect_body.index(
    'if (first == last && page_access[first] == access) return;')
protect_cpp = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <vector>
constexpr std::size_t GuestPage = 0x1000;
constexpr std::size_t GuestPages = 64;
constexpr std::size_t PagesPerGranule = 4;
constexpr std::size_t WindowBytes = GuestPage * GuestPages;
struct Counters { std::atomic<unsigned> protect_calls{0}; };
Counters counters;
struct TestWindow {
    std::mutex mutex;
    std::vector<std::uint8_t> page_access = std::vector<std::uint8_t>(GuestPages, 0);
    std::vector<std::uint8_t> map = std::vector<std::uint8_t>(GuestPages, 0);
    unsigned refresh_calls = 0;
PAGES
    void Refresh(std::size_t first, std::size_t last) {
        ++refresh_calls;
        for (std::size_t page = first; page <= last; ++page)
            map[page] = page_access[page];
    }
PROTECT
};
int main() {
    TestWindow w;
    w.Protect(0x1000, 1, 0);
    assert(w.refresh_calls == 0 && counters.protect_calls == 1);
    w.Protect(0x1000, 1, 3);
    assert(w.refresh_calls == 1 && w.map[1] == 3);
    for (int i = 0; i < 20000; ++i) w.Protect(0x1000, 1, 3);
    assert(w.refresh_calls == 1 && counters.protect_calls == 20002);
    w.Protect(0x1000, 1, 1);
    assert(w.refresh_calls == 2 && w.map[1] == 1);
    w.Protect(0x1fff, 2, 2);
    assert(w.refresh_calls == 3 && w.page_access[1] == 2 &&
           w.page_access[2] == 2);
    w.Protect(0x1fff, 2, 2);
    assert(w.refresh_calls == 3); // same-granule repeat is now a no-op
    // A repeated access that crosses 16 KiB retains its original refresh.
    w.Protect(0x3fff, 2, 2);
    assert(w.refresh_calls == 4 && w.page_access[3] == 2 &&
           w.page_access[4] == 2);
    w.Protect(0x3fff, 2, 2);
    assert(w.refresh_calls == 4); // unchanged cross-granule guards
    w.Protect(0x3fff, 2, 1);
    assert(w.refresh_calls == 5); // changed cross-granule guards
    w.Protect(WindowBytes - 1, 1, 3);
    assert(w.refresh_calls == 6 && w.map[GuestPages - 1] == 3);
    w.Protect(WindowBytes, 1, 3);
    w.Protect(WindowBytes - 1, 0, 1);
    assert(w.refresh_calls == 6 && counters.protect_calls == 20009);
    TestWindow large;
    large.Protect(0, 16 * GuestPage, 3);
    assert(large.refresh_calls == 1);
    large.Protect(0, 16 * GuestPage, 3);
    assert(large.refresh_calls == 1); // bounded cross-granule no-op
    large.Protect(0, 17 * GuestPage, 3);
    large.Protect(0, 17 * GuestPage, 3);
    assert(large.refresh_calls == 3); // long requests always republish
    large.Protect(0, 16 * GuestPage, 1);
    assert(large.refresh_calls == 4); // any changed permission republished
    std::printf("FASTMEM_PROTECT_BOUNDED_NOOP_PASS repeated=20000 "
                "same16k=1 cross16k=1 changed=1 bounded16=1 bounds=1\n");
}
'''.replace('PAGES', method).replace('\nPROTECT\n', '\n' + protect_body + '\n')
assert '\nPROTECT\n' not in protect_cpp
with tempfile.TemporaryDirectory(prefix='encore-fastmem-protect-') as temp:
    path = Path(temp)/'protect.cpp'
    path.write_text(protect_cpp)
    for opt in ('-O0', '-O2'):
        executable = Path(temp)/('protect'+opt)
        subprocess.run([compiler, '-std=c++20', opt, '-Wall', '-Wextra', '-Werror',
                        '-pthread', '-fsanitize=address,undefined',
                        '-fno-sanitize-recover=all', str(path), '-o', str(executable)],
                       check=True, timeout=60)
        subprocess.run([str(executable)], check=True, timeout=20)

# Source-extracted fastmem Map/Unmap redundant single-page probes.
# Preserve a kernel retry when an otherwise-identical alias was NOT
# published: a previous fixed-map operation could have failed.
map_body = '    void Map(' + source.split('    void Map(', 1)[1].split('\n    }', 1)[0] + '\n    }'
unmap_body = '    void Unmap(' + source.split('    void Unmap(', 1)[1].split('\n    }', 1)[0] + '\n    }'
assert 'if (first == last && chunk_aliased[first / PagesPerGranule] &&' in map_body
assert "page_backing[first] == static_cast<std::uint32_t>(host + 1)) {" in map_body
assert "page_access[first] = access;\n            Refresh(first, first);\n            return;" in map_body
assert "bool same_backing = true;" in map_body
assert "bool same_access = true;" in map_body
assert "if (same_backing) {" in map_body
assert "for (std::size_t p = first; p <= last; ++p) page_access[p] = access;" in map_body
assert "Refresh(first, last);\n                return;" in map_body
assert map_body.index("Refresh(first, last);") < map_body.index("// Short repeated mappings")
assert map_body.index("std::lock_guard lock{mutex};") < map_body.index("Refresh(first, first);")
assert 'if (first == last && !page_backing[first] && !page_access[first]) return;' in unmap_body
assert 'chunk_aliased[first / PagesPerGranule]) {' in map_body
assert map_body.count('if (same_access) return;') == 2
assert map_body.count('if (live_aliases_match) {') == 1
assert map_body.count('if (same_backing) {') == 1
assert 'const bool short_single_granule = first < last &&' in unmap_body
assert 'const bool short_cross_granule = first < last && last - first < 16' in unmap_body
assert 'if (short_single_granule || short_cross_granule) {' in unmap_body
assert 'if (page_backing[p] != 0 || page_access[p] != 0)' in unmap_body
assert 'if (all_clear) return;' in unmap_body
assert 'std::all_of(page_backing.begin() + first' not in unmap_body
assert 'first / PagesPerGranule == last / PagesPerGranule' in map_body
assert 'first / PagesPerGranule == last / PagesPerGranule' in unmap_body
assert 'first < last && last - first < 16' in map_body
assert 'first < last && last - first < 16' in unmap_body
assert unmap_body.count('for (std::size_t p = first; p <= last; ++p)') == 2
assert '!chunk_aliased[p / PagesPerGranule]' in map_body
assert "bool live_aliases_match = true;" in map_body
assert "if (live_aliases_match) {" in map_body
assert "if (same_access) return;" in map_body
assert map_body.count("Refresh(first, last);") == 2
assert map_body.count("for (std::size_t p = first; p <= last; ++p) page_access[p] = access;") == 2
for body in (map_body, unmap_body):
    assert body.index('std::lock_guard lock{mutex};') < body.index('if (first == last')
map_cpp = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <vector>
constexpr std::size_t GuestPage = 0x1000;
constexpr std::size_t GuestPages = 64;
constexpr std::size_t PagesPerGranule = 4;
constexpr std::size_t WindowBytes = GuestPages * GuestPage;
constexpr std::uint8_t Readable = 1, Writable = 2;
constexpr std::uint8_t ReadBlocked = 1, WriteBlocked = 2, Blocked = 3;
struct Counters {
    std::atomic<unsigned> map_calls{0}, unmap_calls{0}, mapped_pages{0};
};
Counters counters;
struct TestWindow {
    std::mutex mutex;
    std::vector<std::uint32_t> page_backing = std::vector<std::uint32_t>(GuestPages, 0);
    std::vector<std::uint8_t> page_access = std::vector<std::uint8_t>(GuestPages, 0);
    std::vector<std::uint8_t> chunk_aliased = std::vector<std::uint8_t>(GuestPages / PagesPerGranule, 0);
    unsigned updates = 0;
    unsigned guard_refreshes = 0;
    std::vector<std::uint8_t> map = std::vector<std::uint8_t>(GuestPages, 0);
PAGES
    void Update(std::size_t, std::size_t) { ++updates; }
SOURCE_OWN
SOURCE_REFRESH
MAP
UNMAP
};
int main() {
    TestWindow w;
    // A successful prior alias permits exact same-page Map to skip Update.
    w.Map(0, 0, GuestPage, 3);
    assert(w.updates == 1 && counters.mapped_pages == 1);
    for (int i = 0; i < 10000; ++i) w.Map(0, 0, GuestPage, 3);
    assert(w.updates == 10001); // not yet aliased: preserve kernel retry
    w.chunk_aliased[0] = 1;
    for (int i = 0; i < 10000; ++i) w.Map(0, 0, GuestPage, 3);
    assert(w.updates == 10001);
    w.Map(0, GuestPage, GuestPage, 3);
    assert(w.updates == 10002 && w.page_backing[0] == 2);
    w.Map(0, GuestPage, GuestPage, 1);
    assert(w.updates == 10002 && w.page_access[0] == 1);
    assert(w.guard_refreshes == 1);
    assert(w.map[0] == (w.Own(0) | w.Own(1)));
    w.Unmap(0, GuestPage);
    assert(w.updates == 10003 && counters.mapped_pages == 0);
    for (int i = 0; i < 10000; ++i) w.Unmap(0, GuestPage);
    assert(w.updates == 10003 && counters.unmap_calls == 10001);
    // Overlap and the exact end boundary retain ordinary range semantics.
    w.Map(GuestPage - 1, 0, 2, 3);
    assert(w.updates == 10004 && counters.mapped_pages == 2);
    w.Unmap(GuestPage - 1, 2);
    assert(w.updates == 10005 && counters.mapped_pages == 0);
    w.Map(WindowBytes, 0, 1, 3);
    w.Unmap(WindowBytes, 1);
    assert(w.updates == 10005);
    TestWindow two;
    two.Map(GuestPage, GuestPage, 2 * GuestPage, 3);
    assert(two.updates == 1 && counters.mapped_pages == 2);
    // No active fixed alias: an identical request must still reach Update.
    two.Map(GuestPage, GuestPage, 2 * GuestPage, 3);
    assert(two.updates == 2);
    two.chunk_aliased[0] = 1;
    for (int i = 0; i < 10000; ++i)
        two.Map(GuestPage, GuestPage, 2 * GuestPage, 3);
    assert(two.updates == 2);
    two.Map(GuestPage, GuestPage, 2 * GuestPage, 1);
    assert(two.updates == 2 && two.guard_refreshes == 1 &&
           two.page_access[1] == 1 && two.page_access[2] == 1);
    assert(two.map[0] == (two.Own(0) | two.Own(1)));
    assert(two.map[2] == (two.Own(2) | two.Own(3)));
    two.Unmap(GuestPage, 2 * GuestPage);
    assert(two.updates == 3 && counters.mapped_pages == 0);
    for (int i = 0; i < 10000; ++i) two.Unmap(GuestPage, 2 * GuestPage);
    assert(two.updates == 3);
    // A two-page operation that crosses a 16 KiB granule still updates.
    two.Map(3 * GuestPage, 3 * GuestPage, 2 * GuestPage, 3);
    assert(two.updates == 4 && counters.mapped_pages == 2);
    two.Map(3 * GuestPage, 3 * GuestPage, 2 * GuestPage, 3);
    assert(two.updates == 5); // second kernel chunk not aliased: retry
    two.chunk_aliased[1] = 1;
    for (int i = 0; i < 10000; ++i)
        two.Map(3 * GuestPage, 3 * GuestPage, 2 * GuestPage, 3);
    assert(two.updates == 5); // both existing aliases: no-op
    two.Map(3 * GuestPage, 3 * GuestPage, 2 * GuestPage, 1);
    assert(two.updates == 5 && two.guard_refreshes == 2); // only JIT guards
    assert(two.map[3] == (two.Own(3) | two.Own(4)));
    two.Unmap(3 * GuestPage, 2 * GuestPage);
    assert(two.updates == 6 && counters.mapped_pages == 0);
    for (int i = 0; i < 10000; ++i) two.Unmap(3 * GuestPage, 2 * GuestPage);
    assert(two.updates == 6);
    TestWindow short_span;
    short_span.Map(0, 0, 16 * GuestPage, 3);
    assert(short_span.updates == 1 && counters.mapped_pages == 16);
    for (std::size_t chunk = 0; chunk < 4; ++chunk) short_span.chunk_aliased[chunk] = 1;
    short_span.Map(0, 0, 16 * GuestPage, 3);
    assert(short_span.updates == 1);
    short_span.Unmap(0, 16 * GuestPage);
    short_span.Unmap(0, 16 * GuestPage);
    assert(short_span.updates == 2 && counters.mapped_pages == 0);
    // The bound protects large requests from speculative scans.
    short_span.Map(0, 0, 17 * GuestPage, 3);
    short_span.Map(0, 0, 17 * GuestPage, 3);
    assert(short_span.updates == 4 && counters.mapped_pages == 17);
    short_span.Unmap(0, 17 * GuestPage);
    short_span.Unmap(0, 17 * GuestPage);
    assert(short_span.updates == 6 && counters.mapped_pages == 0);
    // A crossed span must retry if any one kernel granule lacks a
    // published alias: changed access alone is not permission to skip it.
    TestWindow missing_cross_alias;
    missing_cross_alias.Map(3 * GuestPage, 3 * GuestPage, 2 * GuestPage, 3);
    missing_cross_alias.chunk_aliased[0] = 1;
    missing_cross_alias.Map(3 * GuestPage, 3 * GuestPage, 2 * GuestPage, 1);
    assert(missing_cross_alias.updates == 2 && missing_cross_alias.guard_refreshes == 0);
    // The first page in a native 16KiB granule changes the preceding
    // page's crossing guard as well as its own guard.
    TestWindow boundary;
    boundary.Map(4 * GuestPage, 4 * GuestPage, GuestPage, 3);
    boundary.chunk_aliased[0] = 1;
    boundary.page_access[3] = 3; // owned predecessor: its guard must change
    boundary.chunk_aliased[1] = 1;
    boundary.Map(4 * GuestPage, 4 * GuestPage, GuestPage, 1);
    assert(boundary.updates == 1 && boundary.guard_refreshes == 1);
    assert(boundary.map[3] == 2); // blocked guest write in page 4
    assert(boundary.map[3] == (boundary.Own(3) | boundary.Own(4)));
    assert(boundary.map[4] == (boundary.Own(4) | boundary.Own(5)));
    // The final page has no successor and must not index beyond the window.
    TestWindow tail;
    tail.Map((GuestPages - 1) * GuestPage, (GuestPages - 1) * GuestPage,
             GuestPage, 3);
    tail.chunk_aliased.back() = 1;
    tail.Map((GuestPages - 1) * GuestPage, (GuestPages - 1) * GuestPage,
             GuestPage, 1);
    assert(tail.updates == 1 && tail.guard_refreshes == 1);
    assert(tail.map.back() == (tail.Own(GuestPages - 1) | tail.Own(GuestPages)));
    // Failed native alias: unchanged backing MUST still retry Update.
    TestWindow missing_alias;
    missing_alias.Map(0, 0, GuestPage, 3);
    missing_alias.Map(0, 0, GuestPage, 1);
    assert(missing_alias.updates == 2 && missing_alias.guard_refreshes == 0);
    std::printf("FASTMEM_MAP_UNMAP_REPEAT_HOST_PASS nonaliased_retry=1 "
                "same_alias_noop=10000 unmap_noop=10000 same16k=1 "
                "cross16k=10000 permission_only=1 permission_batch=1 cross_permission_batch=1 predecessor_guard=1 bound16=1 O0_O2=1\n");
}
'''.replace('PAGES', method).replace('\nMAP\n', '\n' + map_body + '\n').replace(
    '\nUNMAP\n', '\n' + unmap_body + '\n').replace(
    'SOURCE_OWN', '    std::uint8_t Own(' + source.split('    std::uint8_t Own(', 1)[1].split(
        "    // A page's byte", 1)[0].rstrip()).replace(
    'SOURCE_REFRESH', ('    void Refresh(' + source.split('    void Refresh(', 1)[1].split(
        '    void Block(', 1)[0].rstrip()).replace(
            '    void Refresh(std::size_t first, std::size_t last) {',
            '    void Refresh(std::size_t first, std::size_t last) {\n        ++guard_refreshes;', 1))
assert '\nUNMAP\n' not in map_cpp and '\nMAP\n' not in map_cpp
assert 'SOURCE_OWN' not in map_cpp and 'SOURCE_REFRESH' not in map_cpp
with tempfile.TemporaryDirectory(prefix='encore-fastmem-map-unmap-') as temp:
    path = Path(temp)/'map.cpp'
    path.write_text(map_cpp)
    for opt in ('-O0', '-O2'):
        binary = Path(temp)/('map'+opt)
        subprocess.run([compiler, '-std=c++20', opt, '-Wall', '-Wextra',
                        '-Werror', '-pthread', '-fsanitize=address,undefined',
                        '-fno-sanitize-recover=all', str(path), '-o', str(binary)],
                       check=True, timeout=60)
        subprocess.run([str(binary)], check=True, timeout=30)
