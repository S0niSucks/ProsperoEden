# Eden Encore — performance audit without repeating previous work (26 October 2026)

**Tableau maître à utiliser pour tout prochain bilan :** [FC27_F07_MASTER_TRACKER.md](FC27_F07_MASTER_TRACKER.md). Cet audit contient une chronologie : la section sparse « DEV only » décrit une étape passée, remplacée plus bas par l'unique JIT sparse natif de production. Les preuves sur PS5 13.60 restent nécessaires.

**Branch:** `dev/ps5-sparse-jit`. This is a cross-subsystem execution map, **not** a claim that every emulation source is FPS-optimized. F01–F06 passed host pre-console contracts; F07 remains **OPEN** until same-build console evidence. The 491-source inventory is historical; source-review coverage and measured runtime bottlenecks are different quantities.

| Area / production owner | Already integrated; do not duplicate | Unresolved evidence / next precise investigation |
| --- | --- | --- |
| A32/A64 JIT / `headless/experimental_performance.h`, native Dynarmic | Single native **sparse JIT** RX/RW path with 2 MiB demand commit and quiesced full-clear tail trim; code-space checks and alias memo; prior dense/dev-only note superseded | Firmware 13.60 alias/trim/relaunch qualification; CPU instruction progress and per-core compile/invalidation time during >100 ms frame gaps; no unbounded cache enlargement |
| Shader first-use / `vk_pipeline_cache_cost.cpp` | Asynchronous Vulkan pipeline *build*, release cold-creation timing `graphics_pipeline_create` | Guest IR→SPIR-V preparation remains synchronous in `CreateGraphicsPipeline()`; correlate first-use bursts with waits rather than bypass ordered builds |
| Vulkan worker / `vk_graphics_pipeline_cost.cpp`, `vulkan_scheduler.cpp` | Completion always wakes waiters; speculative optimizer yields to first-use builds when enabled | `ConfigureDraw` queues a build-condition wait in scheduler worker; attribute time/impact before changing worker lock boundaries |
| Graphics pipeline transitions / `vk_graphics_pipeline.h` | **This audit:** PS5-only hot transition index on lists >=8; no changes to pointers, keys or draw order | Host O0/O2 regression required; no hardware frametime improvement claimed |
| Paired cache locks / `vulkan_rasterizer.cpp`, `vk_graphics_pipeline_cost.cpp` | Preflight descriptor reservations; per-draw scratch reuse; 16-entry sampler memo; empty-descriptor acquire shortcut | Buffer cache lock vs texture cache lock *owner* duration and GPU dispatch in same late windows, not just aggregated wait |
| Texture upload / `texture_cache.h`, `vk_texture_cache_cost.cpp` | TIC/TSC guards, null-view revival, alias/invalidation tests; F07 staging/contents/barrier timers | Which upload subphase holds the lock >50ms? Check async decode/unswizzle decisions and avoid any stale texture shortcut |
| HUD/2D correctness / blit scheduler, framebuffer/present | Raw `DrawTexture` blit invalidates cached Vulkan graphics state (source/host) | Must observe actual updated pixel contents vs unchanged host presents and long-lived black HUD panels; host correctness ≠ console |
| DMA / F04 GPU copy path | Conservative overlapping/contiguous remap+upload witnesses and transfer barriers | F04 host replay must remain green after instrumentation changes; compare HUD, jerseys, pitch, not just crash counts |
| NvDisp/HLE guest progress / Binder, BufferQueue, Host1x, KThread | F05 release wake and handoff host fixtures; worker ordered publication | Pair guest PC/thread progress, frame **content** and input response during stalled menus and half-time |
| Input / Pad→NPad→LIFO | F06 synthetic linked source replay and 250 Hz poll | Input-to-visible-frame latency remains unknown; do not alter user-approved mapping as a GPU optimization |
| VRAM/GC/native allocation | Bounded GC scan and budgets, staged safe release, 4× OOM failure logs | Diagnose memory pressure separately from 1440p 1× stutter; no unconditional larger texture/JIT cache |
| Production profile / release vs DEV | Quiet GPU and JIT counters, diagnostics toggle restored on launch; unsafe CPU/DMA/skip invalidation OFF | Make sure instrumentation doesn't change quiet gameplay; compare selected PS5 build only, never unbuilt experiments |
| Launcher OpenGL/cover pipeline | Separate UI source-review ledger and asynchronous artwork decode | Do **not** confuse launcher covers/filters/AA and game Vulkan/HUD performance |

## Closeout criteria and no-repeat rule

1. Identify a **selected active** production source, precise repeatable cost, and an isolated safe edit. Archive changed source symbol, commit and host regression.
2. Source reviews alone never count as completed performance changes; a green host gate is not a matched PS5 improvement.
3. CI fast gate + full preflight + compiled PS5 package must pass **for the same SHA** before asking for a new consolidated console comparison.
4. Benchmark FC27 at 1440p / 1×, same scene, logging OFF, fresh vs retained caches controlled separately. Compare late50/late100/worst, missing HUD pixels, gameplay controls and safe shutdown.
5. Keep F07 open while substantial judder or blank/delayed HUD persists; preserve F01–F06 contracts, no forced 60 FPS or shader skipping.
6. If a source was already inspected and unchanged, do not restart its entire audit without a new regression signal. Prior evidence maps: `FC27_GPU_SOURCE_TARGETS_2026-10-21.md`, `FC27_F07_STUTTER_HUD_PRIORITY_2026-10-26.md`, `EDEN_OPTIMIZATION_WORKLIST_2026-10-20.md`.

## F07: shader completion publication (source fix after the first cross-system pass)

`GraphicsPipeline` sets `is_built=true` with a **release** store after its Vulkan pipeline handle is constructed. Two readers test the status before accessing that non-atomic handle. `CanExecuteDraw()` already used **acquire**, while `IsBuilt()` still used **relaxed**: the latter did not establish the required publication ordering. The maintained PS5 native header and the pinned Vulkan generator now use **acquire** for `IsBuilt()`.

`check-ps5-f07-pipeline-publication-host.py` compiles both methods extracted from the actual native header, verifies the builder's release store, tests threaded release/acquire publication and failed-pipeline behavior under O0/O2. No shader is skipped, no new GPU wait is inserted, and no FPS claim follows from the memory-order fix.

## F07: avoid redundant Vulkan compute builder lock on delayed callback

`vk_compute_pipeline_cost.cpp` queued a scheduler callback if the compute build was unfinished at enqueue time, even though the build could complete before the callback executed. Unlike graphics, its callback always acquired the build mutex and entered the condition-variable path. Compute now performs a release/acquire-compatible `is_built.load(acquire)` first and skips the lock when already complete. Unfinished builds still block in order; failed builds keep their existing rejection behavior. The pinned generator and native TU are updated together; source-extracted O0/O2 tests cover ready-before-callback, unfinished-at-callback and failed-build cases. This is a bounded CPU/lock optimization, **not** an FPS result.

## F07: recurring Vulkan driver-cache flush survives failures

The async disk-cache writer used to clear `flush_in_flight` only after a successful driver `Read()`. An exceptional read (or worker enqueue failure) could leave the title's background cache snapshots permanently suppressed. The production Vulkan source and reproducible generator now use a scoped release of the guard, catch serializer/read exceptions inside the worker and clear the guard when queue admission itself fails. The save frequency, Vulkan cache content, shader compilation, filenames and game-thread ordering remain unchanged. A native-source-extracted host fixture checks failure and recovery at O0/O2. **This protects warm-cache persistence; no FPS gain asserted.**

The enqueue-failure path is deliberately **non-fatal**: logging the rejection and clearing `flush_in_flight` must not turn an optional cache save into a gameplay exception.

## F07: bounded persisted-pipeline prewarm (first step, console unverified)

Previously `headless/main.cpp` requested a *stopped* token at Vulkan title launch. Consequently `PipelineCache::LoadDiskResources()` returned after only setting the per-title filenames and driver blob, **without replaying even one saved guest graphics/compute pipeline**. First-use Maxwells were therefore always rebuilt during gameplay, independently of the driver RADV blob.

PS5 Vulkan now supplies a live token and `LoadDiskResources` uses its own **reader-only** `std::stop_source` to stop parsing after **at most eight persisted pipeline entries** (graphics or compute), then waits for those queued builds with the original never-cancelled worker token. The shader-worker pool remains alive for all later first-use compilations. Invalid old shader metadata, incompatible driver blobs, per-title cache segregation and normal renderer shutdown keep their existing paths. The logged mode changes from `lazy-persistent` to `bounded-prewarm8`. This deliberately trades a limited amount of launch latency for fewer initial game-thread shader compilations; it is not a complete cache preload and cannot yet guarantee less HUD stutter. Monitor whether startup is acceptably fast and whether first-use durations change in the eventual *same-build* PS5 comparison.

## F07: persist every queued first-use shader at shutdown

`PipelineCache::~PipelineCache()` had no explicit drain for the `serialization_thread` that writes `vulkan.bin` shader environments. Because `StatefulThreadWorker` uses stoppable `jthread` instances, destroying the worker with accepted jobs queued could discard unsaved pipelines, making the next title launch repeat expensive guest IR-to-SPIR-V translation. PS5 builds now call `serialization_thread.WaitForRequests()` **only at pipeline-cache destruction** before the final driver-cache save. This has no effect on steady-state draw locks; a title with many pending writes can take longer to close. The pinned Vulkan generator, selected file and source-extracted O0/O2 test all track the behavior. This is a persistence correctness optimization, **not** proof of restored HUD or console FPS.

## F07: reject null native Vulkan pipelines during bounded prewarm

The first eight serialized shader environments were restored synchronously and inserted into graphics/compute maps whenever their C++ pipeline object was non-null. A failed Vulkan build may nevertheless leave a **null native pipeline handle**, which the game would then encounter as a cached unusable draw/dispatch. The PS5-only prewarm now admits a graphics pipeline only if `CanExecuteDraw()`; compute requires `IsBound()`. Failed entries stay absent so the normal first-use path can retry, without adding work or retries to successful draws. No skip, reordering, frame interpolation or cache resizing. The pinned generator includes both safeguards and the O0/O2 source-extracted test checks failed and successful handles. Console HUD recovery remains unverified.

## F07: actionable shader-prewarm health metrics (no frame overhead)

The old prewarm log said `built N/N` even when Vulkan returned a completed-but-unusable pipeline object. The startup worker now counts `valid` only on successfully admitted graphics/compute handles, retaining `completed` for all build attempts. Once all warm builds have joined, PS5 prints exactly one `EDEN_PS5_SHADER_PREWARM queued=... completed=... valid=... rejected=... prebuild_ms=...` line. This separates actual reusable shaders from disk entries scanned, reports startup cost, and makes regressions between runs measurable. It adds **zero frame-loop clocks or counters**, preserves the eight-entry quota and cannot by itself improve frame pacing. The same change is in the pinned generator and in the source-extracted host fixture.

## F07: sparse JIT physical return after a quiesced full cache eviction (DEV only)

The intended dynamic unified-memory policy cannot be achieved by fixed launch
budgets alone. The existing PS5 sparse allocator already reserves stable
RX/RW virtual ranges and commits physical 2 MiB chunks on demand, but previously
held every committed chunk until whole-title teardown. A full JIT cache
evacuation left those direct pages unavailable to subsequent GPU texture/guest
allocation even though all code had been invalidated.

A new **developer-only** path calls `Common::TrimSparseJitCode` after
`BlockOfCode::ClearCache` resets the code cursor. It retains the executable
prelude, at least the initial 4 MiB bootstrap, and one 2 MiB recompilation
chunk; returns ONLY whole unused physical chunks; replaces BOTH their
old RX/RW mappings with inaccessible guards BEFORE releasing backing; and
bumps `sparse_jit_generation` to invalidate thread-local prior commit hints.
It does not relocate the executable VA, allocate outside the reserved arena,
run on partial invalidations, touch the GPU cache, or change W^X.

This is limited by `EDEN_SPARSE_JIT_DEV`. Production CMake still rejects
sparse JIT in `EDEN_FINAL_RELEASE_PROFILE`; no console validation is claimed.
A C++ source-extracted host fixture tests alias guard ordering, exact 2 MiB
return, invalidation epoch, repeat clears, 16 MiB core 3, and demand
recommit policy at O0/O2. Full CPU/GPU/RAM arbitration (priorities, pressure
sampling and safe GPU reclamation) is **not** yet implemented; this change
repairs the missing return path and must not be labeled fully adaptive.

## 26 October — one shipping PS5 sparse JIT; release is the sole integration gate

The developer-only sparse-vs-dense split conflicted with the product's single
native release requirement. Native `PS5_NATIVE` now selects one code allocator:
stable RX/RW virtual arenas + demand-backed 2 MiB physical chunks. Full,
quiesced cache invalidation returns the unused physical tail for unified
guest/GPU allocations, preserving the code prelude and one growth chunk. No
`EDEN_SPARSE_JIT_DEV` CMake selector, runtime atomic sparse mode toggle,
dense native code-JIT fallback, or independently packaged `dev TITLE_ID`
variant remains in this integration. Host-only tests continue to compile
without `PS5_NATIVE`, which is a different target architecture, not an
alternate PS5 user mode.

Native launch verifies the current firmware's RW/RX alias behavior before
creating the CPU JIT; a failed proof **rejects the launch** rather than
silently switching to dense executable backing. Sparse VA size remains bounded
per title; physical pages can grow as the guest compiles and shrink after a
full code-cache clear. This is dynamic RAM reclamation, **not** an already
qualified cross-subsystem CPU/GPU scheduling arbiter. It does not assert
improved FC27 frame times or fix asynchronous shader freshness by itself.

Source ownership and CI updated together: the release installer passes only
the single production profile, architecture/compiled selector tests require
native sparse emission, page-hint epochs invalidate on create/trim/release,
host O0/O2 guards and full native package/build must pass for this exact HEAD.
A green host or cross-compiled PS5 build is not native firmware qualification;
an actual console runtime still needs proof of alias mapping, JIT clear
liveness, recycled physical backing, game frame cadence and HUD correctness.


### Source-review register — native sparse JIT single path

| Source | Concrete change / safety contract |
| --- | --- |
| `headless/experimental_performance.h` | Removed sparse runtime flag; virtual cache admission remains bounded |
| `headless/jit-allocator.h` | Native code arena allocates sparse RX/RW aliases only; no dense retry |
| `headless/jit-sparse-commit.h` | Exact 2 MiB trimming policy and capacity/headroom validation |
| `headless/main.cpp` | One sparse preflight per game, fail-closed firmware alias validation |
| `src/memory_pages.cpp` | Physical commit and whole-cache tail decommit with mapping guards and epoch invalidation |
| `vendor/encore-active/eden-core/src/dynarmic/backend/x64/block_of_code_native.cpp` | Native translation precommit and quiesced cache trim, no opt-in dev mode |

### 30 October 2026 — J01 sparse physical-commit positive memo, source-review refresh

The active `headless/jit-sparse-commit.h` has changed from reviewed Git blob `abf0b4965d9f5927954f7c740428cc0cc86f2233` to `5a26c0201572632a9e47fb4ef2fdd648bc99fb59` during the F07 J01 optimization. Its `SparseCommitPositiveCache` is a four-entry per-thread positive memo keyed by executable virtual alias, physically committed capacity and `sparse_jit_generation`. Epoch changes invalidate old mappings; capacity growth refreshes a memoized entry; MRU eviction is bounded to four slots. It does **not** modify `PlanSparseCommit` or `PlanSparseTrim` semantics and does not authorize memory writes beyond Xbyak's arena bounds. The new source-linked host fixture `tools/check-native-jit-sparse-positive-cache-host.py` compiles the production header under O0/O2 and UBSan and checks alternating cores, demand growth, eviction and generation invalidation. This entry explicitly supersedes the earlier 'reviewed without code edit' blob witness. Exact-source reviewed blob updated in `docs/EDEN_OPTIMIZATION_COVERAGE_2026-10-20.json`; host CI and native FW13.60 qualification remain separate gates.

### 30 October 2026 — J01 sparse positive-cache source drift requalified after CI failure

The prior source review remains historical evidence for `5a26c0201572632a9e47fb4ef2fdd648bc99fb59`. The current **production** Git blob is `a6f4840a124bee3ed9183df4718783e346fb6e60` (HEAD before this ledger repair: `6247ef4fd55e11c8cba2cb1ed4e63d6ba84b410b`). This is a real source change, not a new unreviewed header: `089a1b0d` avoids a redundant write on a slot-zero positive hit; `9cbf0ca9` stops negative scans at the first vacant contiguous slot; `62de8e62` returns directly on a capacity miss for the active arena. `Remember` preserves unique executable keys and contiguous MRU slots, epoch mismatch still rejects stale aliases, and capacity growth still goes through the locked physical-commit owner. `PlanSparseCommit` and `PlanSparseTrim` remain unchanged.

The source-linked `tools/check-native-jit-sparse-positive-cache-host.py` tests read-only hits/cold misses with `mprotect`, four-arena alternation, growth, LRU eviction, generation invalidation and a deterministic 250,000-operation reference oracle under ASan+UBSan `-O0/-O2/-O3`. Earlier local reproductions were reported, but the **GitHub** source-early job `41022353941` failed at `check-eden-optimization-coverage.py:206` because the exact blob pin was stale; that failure is not a proof of a JIT runtime regression. This refresh changes only the exact reviewed-source pin and the audit evidence, not the guard or any runtime source. Re-run the unchanged strict audit on the new HEAD. No PS5 FC27/BOTW or FPS qualification is claimed.

### 30 October 2026 — J01 non-head growth miss, concurrent source update

After the source-review refresh, `main` advanced independently to `e5cbe2226197d933b086cd2068678b3d02aac602`: `SparseCommitPositiveCache::Covers` now returns `false` immediately when a **non-head** matching arena has insufficient committed bytes, rather than scanning subsequent MRU slots. `Remember` maintains unique executable aliases, so no later slot can satisfy the same key. Epoch checks, capacity bounds, mutex-owned physical growth and `PlanSparseCommit/PlanSparseTrim` are unchanged. The exact new source blob is `8be2291eee26afbe44fc0f2c12320474e310cf3d`; the strict reviewed-source pin is advanced only after inspecting this diff. Existing source-linked witness/oracle remains, but no CI or console result for this new SHA is claimed.

### 30 October 2026 — J01 two-arena read-only hot hit (main)

Production `headless/jit-sparse-commit.h` reviewed at Git blob `ec24f22a1144c7d0135c70a0092379a57d98b330` (commit `3ab6c21`): when exactly two executable aliases are cached, a positive hit in slot 1 no longer moves TLS entries. The four-slot cache still checks the generation and committed-byte bound before every hit and preserves its usual MRU promotion with three or four aliases. Physical growth remains under `sparse_jit_mutex`. The source-linked regression now protects the two-entry cache with `mprotect(PROT_READ)` and extends the independent 250,000-step oracle. A separate local C++20 model passed ASan+UBSan in O0/O2/O3. GitHub CI and PS5 13.60 FPS/HUD remain unqualified; F07 is still **22/29** pre-build decisions.

### 30 October 2026 — sparse page-table scan bound after title teardown

Production `src/memory_pages.cpp` (reviewed Git blob `7f08ac8dd6e4ad56396715e4924ab6da662cd938`) now contracts `sparse_used_prefix` only across a **fully vacant tail** when a sparse range is released. All reads, release, new range publication and scans remain serialized by `sparse_mutex`; interior holes are still searched, and a subsequent high-slot registration republishes its bound. The source-extracted C++20 host test covers full 64-slot use, 2,000 tail release/realloc cycles, an empty process, republishing a distant slot and four concurrent readers under O0/O2/O3 + ASan/UBSan. This avoids a permanent 64-entry scan after repeated title sessions; native PS5 frametime and HUD impact remain unmeasured. F07 stays **22/29**.

### 30 October 2026 — sparse table one-entry thread-local lookup memo

Production `SparseRangeOf` in `src/memory_pages.cpp` (reviewed Git blob `98b1fafe1fb633e2fc117e6ab961456b72278133`) now rechecks its last positive range under the same existing `sparse_mutex` before scanning. Neighboring guest/GPU page-table updates within a range now use exactly one begin metadata load, independent of table index. The hint points only into a static array; under the existing owner lock, retirement/reuse and republished begin/end are revalidated, and empty tables return without a probe. Source-extracted C++20 host sanitizer witness checks 10,000 repeated low/high-index hits, tail shrink, 2,000 release/reuse cycles, republish and concurrent readers; no PS5 FPS assertion. F07 stays **22/29** pending complete same-SHA proof.

### 30 October 2026 — two-entry sparse range lookup, alternating tables

`src/memory_pages.cpp::SparseRangeOf` (reviewed Git blob `0bbd4778c12cb313e4a66b50a6e280e76c3add21`) now remembers **two**, not one, recently matched sparse table ranges per guest worker. Both static-array hints are revalidated under the existing `sparse_mutex`, including after release or reuse; no new lock, allocation, extra worker or GPU synchronization. Positive alternating hits do not write to TLS, and a linear fallback skips hints already checked. Source-extracted C++20 ASan/UBSan host witness asserts ≤30,004 begin probes across 10,000 low/high alternating pairs (versus approximately 640,000 probes for an uncached 64-slot high range), plus single-range repeated hits, table-tail release, 2,000 reuse cycles, and four concurrent readers. No console frametime conclusion. F07 remains **22/29** pending full same-SHA suite and hardware qualification.

**Two-entry sparse memo review refinement, 30 October 2026:** In the previously proposed fixed-order search, a worker pinned to the second of two ranges incurred two atomic reads forever after its first insertion. The source `src/memory_pages.cpp` (reviewed Git blob `c3519f91f84a95ecc95c20a538ac2ddb2f7e275e`) now prefers the most recently HIT table (one TLS preference update only when switching), stores that preference when first inserting, and clears stale hints on a negative search. Repeated hits on either single table probe exactly once; two-table alternation stays bounded to two memo probes, plus one initial cold scan. Existing lifetime mutex, end-bound and begin validation remain unchanged. Sanitizer host fixture checks 10,000 hits for each isolated table, up to 40,004 probes across 10,000 alternating pairs, 2,000 reuse cycles and multithread ownership. No measured PS5 FPS, and F07 remains **22/29**.

### 30 October 2026 — alias RX→RW preferred hot-slot lookup (J06/J01)

`headless/jit-allocator.h` (Git blob `ee3ab84c7fdcc4807fd25ce56d0e2878bbd7d590`) now checks the last active RX alias first: repeated guest code emission into the same arena avoids scanning the full eight-slot per-thread positive cache. A hit in another slot updates the preferred index only after three arenas exist; alternating A32/A64 with two arenas remains TLS-read-only. FIFO insertion/eviction, invalidation epoch, ownership mutex on misses and actual executable/writable mapping unchanged. Source-header C++20 ASan/UBSan witness uses read-only protected memo pages for 10,000 hot hits after preference switch and 20,000 two-arena alternations, alongside eight colliding aliases, epoch change, eviction and real RX/RW map checks. FPS/console behavior not qualified; F07 **22/29**.

### 30 October 2026 — Vulkan static-vertex shader memo (V07 / graphics draw hot path)

`EdenAttributeless` in the **active native Vulkan pipeline cache** used one direct-mapped entry per hash slot; shaders with the same low six hash bits repeatedly missed and locked the global `std::set` during alternating world/HUD draws. The exact 64-entry budget is retained as **16 buckets × 4 ways** in per-GPU-thread TLS. No allocation, mutex or TLS write on a hot positive/negative hit; a miss still takes the original mutex and generation-validated tree lookup. Shader-builder generation publication remains release/acquire. The maintained selected vendor source and `prepare-vulkan-port.py` generator are synchronized; `check-ps5-vertex-identity-host.py` source-extracts and compiles both with O0/O2 ASan/UBSan, instruments slow mutex lookups and asserts **20,000 alternating collision-four hits with zero warm mutex acquisitions**. The fifth collision tests bounded eviction; inserting a previously negative shader tests generation invalidation. No FC27 frame improvement claimed; F07 still **22/29**.

### 30 October 2026 — preserve positive static-vertex shader identities across shader growth

The global `eden_attributeless_vertex` set only receives inserts and never erases a confirmed vertex shader identity. On PS5 a known `true` can therefore remain in the 4-way GPU-thread memo across unrelated shader builder generation increments, avoiding redundant global mutex/tree lookups during first-use shader bursts. A cached `false` still requires exact generation match and correctly becomes `true` on later insertion. No new allocations, races or title-specific resets; the source and generator are synchronized. The source-extracted C++20 O0/O2 ASan/UBSan witness adds 20,000 positive hits *after* new shader publication with zero new slow lookups, and asserts a published formerly-negative key is rechecked. No PS5 FPS claim. F07 **22/29**.

### 30 October 2026 — four-key positive compute pipeline memo

`PipelineCache::CurrentComputePipeline` used to fast-path only the **last** healthy compute key. Alternating compute dispatches (e.g., streaming passes and HUD) still hashed and probed `compute_cache` on almost every call. The PS5 native cache now keeps **four** successful keys/pointers in a per-`PipelineCache`, fixed-size, allocation-free array; repeated dispatches skip the map while the cache objects' lifetime remains unchanged. Failed/null translations are never memoized, retain the bounded delayed-retry path, and do not evict healthy hints; `trace_pipelines` always retains its regular map lookup. Exact generated Vulkan delta and selected vendor fork are synchronized. The source-extracted `tools/check-ps5-f07-compute-key-positive-cache-host.py` fixture now verifies 20,000 four-key alternating hits, fifth-key eviction, null retries, traced path and per-owner isolation in native/desktop O0/O2 UBSan variants. F07 **22/29**, no console FPS measurements.

### 30 October 2026 — preferred compute-key way, fixed four-way fast cache

The fixed four-way compute memo now checks the *most recently used* successful slot first. A consecutive repeated dispatch hits after **one key comparison**, even when four distinct shaders are already resident; a different resident key takes the bounded scan and becomes preferred without a map lookup or allocation. A newly admitted positive pipeline becomes preferred; null failures and trace mode retain their existing paths. The generator and native vendor remain synchronized. The source-extracted O0/O2 desktop/native UBSan witness instruments key equality and checks 10,000 hot probes each use exactly one comparison and zero additional map lookups. No console FPS claim; **F07 22/29**.
