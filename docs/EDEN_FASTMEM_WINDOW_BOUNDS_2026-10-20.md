# A32 fastmem window: overflowing page-range clipping

The broader inventory found `src/host_memory.cpp` outside the 478-row contract
graph, although CMake compiles it into `common` and existing bridge checks read
it. The graph must not be described as a complete inventory of Encore code.

Hypothesis: `Window::Pages` clips the end using `min(offset + length,
WindowBytes)`. The addition can wrap before clipping. At offset 1 and length
`SIZE_MAX`, the derived last page becomes far larger than the A32 window.
`Unmap` then iterates over page arrays using that last page; `Protect` derives
an invalid iterator. Public unmap/protect calls do not independently bound it.

Preserve the helper's existing clipping semantics by limiting the length to
`WindowBytes - offset` before addition. A source-extracted ASan/UBSan witness
must reject the original result and verify zero, invalid, ordinary, cross-page,
end-of-window and maximal-length inputs. No whole guest-address-space walk or
console run is needed to reproduce the range error.

This is a bounds correction for the optional A32 fastmem window. It does not
enable that window, alter the A64 JIT, or establish a cause of FC27 freezes.

## Refresh work hypothesis

Under the window mutex, `Refresh` evaluates `Own(page)` and `Own(page+1)`
for every map byte, repeating the same page-access lookup on consecutive
iterations. The access/alias arrays are stable during this loop and distinct
from the map being written. Carry the successor value into the next iteration.
Compare the entire output map against the previous loop across page/alias
boundaries and the final guard page, while counting helper evaluations. N map
bytes should require N+1 evaluations instead of 2N. This is a source-work
measurement, not a PS5 frame-time benchmark.

### 30 October 2026 — redundant single-page fastmem permission no-op

`src/host_memory.cpp::Window::Protect` now skips the redundant `Refresh` for a single guest page whose requested permission matches its current permission. All Map/Unmap/Protect publishers already serialize under the same mutex and refresh their own map-byte modifications, so an unchanged page cannot change its own direct-JIT guard or the preceding page's cross-boundary guard. Existing accounting remains; changed protections and multi-page ranges use the original path. The `tools/check-fastmem-window-bounds-host.py` C++20 source-extracted host ASan/UBSan fixture exercises 20,000 repeated no-op calls under -O0/-O2 plus changed, adjacent and invalid requests. No console FPS claim; F07 still **22/29**.

### 30 October 2026 — fastmem repeat Map/Unmap skip, fail-safe kernel retry

`src/host_memory.cpp::Window::Map/Unmap` bypasses redundant per-page guard refresh for an identical, **already aliased** one-page mapping, or for a one-page Unmap whose backing and permissions are already absent. Map never skips an unaliased chunk, even when metadata is identical, preserving the attempted retry after a refused fixed kernel alias. Calls still increment diagnostics counters, while changed mappings and requests spanning pages retain the original Update and guard publication. The source-extracted C++20 O0/O2 ASan/UBSan witness checks 10,000 repeat unchanged aliases, 10,000 repeat unmaps, nonaliased retries and crossing page boundaries. No PS5 frame-time claim. **F07 remains 22/29**.

### 30 October 2026 — no-op permission updates within one PS5 16 KiB granule

`Window::Protect` now bypasses the metadata fill and JIT guard rebuild for unchanged permissions over **2–4** contiguous guest 4 KiB pages *inside one 16 KiB PS5 granule*. This extends the proven single-page no-op without a full-window search or modifications to native virtual mapping. Checks are bounded to at most four existing permission bytes under the original mutex; a changed byte or any cross-granule range takes the original publication path. The source-extracted O0/O2 ASan/UBSan witness distinguishes repeated same-granule requests from cross-granule requests, plus 20,000 repeated single-page calls. No console FPS measurements. F07 **22/29**.

### 30 October 2026 — repeated Map/Unmap within a 16 KiB kernel granule

`Window::Map` compares at most four existing page records before bypassing a repeated 2–4-page mapping **only if the direct alias is already active**. A rejected fixed-map leaves `chunk_aliased` unset, preserving the next attempt's `Update` retry. `Window::Unmap` likewise skips an already-zeroed 2–4-page range within one granule, checking both the backing and access metadata. All multi-granule and changed mapping operations still issue the original updates, including adjacent JIT crossing-guard refresh. Both checks remain under the existing mutex, with no new allocations. Source-extracted `check-fastmem-window-bounds-host.py` validates cold retry, changed access, 10,000 repeated multi-page mapped/unmapped calls and cross-granule fallback under C++20 -O0/-O2 ASan/UBSan. **F07 remains 22/29**; no console FPS assertion.

### 30 October 2026 — bounded 64 KiB cross-granule map/unmap no-op

`Window::Map` now checks a maximum of 16 guest pages (64 KiB) on an otherwise identical cross-16-KiB mapping; it skips `Update` **only** when every 16-KiB granule has a previously successful active fixed alias and all host backing-page indices and permission bytes match. An unaliased chunk, changed offset/access or longer mapping always takes the original kernel retry and guard update path. `Window::Unmap` skips short cross-granule ranges only if their backing and access arrays are entirely cleared. Existing `Window::mutex` and page counters remain unchanged. Source-extracted O0/O2 ASan/UBSan tests cover 10,000 cross-granule repeats, an unaliased chunk retry, changed access, exactly 16 pages and the 17-page fallback. F07 **22/29**; console performance not measured.

### 30 October 2026 — bounded cross-granule Protect no-op completes short fastmem family

`Window::Protect` now handles unchanged requests up to 16 guest pages (64 KiB) that cross native PS5 16 KiB granule boundaries. An unchanged short span leaves the authoritative JIT map untouched, while *any* changed permission or request of 17+ pages always retains the original `std::fill` + `Refresh` path (including predecessor cross-page guards). The check is bounded to 16 existing metadata bytes under the original mutex, with no kernel calls or allocations. Source-extracted C++20 ASan/UBSan O0/O2 oracle covers changed vs repeated cross-granule updates and the 16/17 page boundary. `Map`, `Unmap`, `Protect` all now share the same bounded unchanged-request policy, while Map still requires actual live native aliases before skipping. **F07 22/29**, native FPS not measured.
