# Encore Memory and Lifecycle Adapters

## Overview

Encore-owned C/C++ adapters for backing memory, checked A32 fastmem, dense and
sparse allocation, and application lifecycle requests. `PS5_NATIVE` selects native
memory calls; other memory branches use `mmap` and, for fastmem backing, Linux
`memfd_create`. The fastmem probe contains unguarded x86-64 assembly. Platform
declarations and the included `common/host_memory.h` and `jit-sparse-commit.h` are
external to this directory; source checks do not establish SDK/kernel or hardware
qualification. [host_memory.cpp:PS5_NATIVE] [host_memory.cpp:eden_fastmem_probe_load]
[memory_pages.cpp:PS5_NATIVE] [lifecycle.c:sceSystemServiceLoadExec]

## Interface

- [Fastmem declarations](./fastmem.h): `Eden::Fastmem::Request`, `Requested`,
  `Stats`, `WindowStats`, `Faults`, `ContextRip`, and `ContextRsp`. Request and
  statistics implementations are in [host_memory.cpp](./host_memory.cpp);
  `Faults` is only declared in this directory. [fastmem.h:Request]
  [fastmem.h:Stats] [fastmem.h:Faults] [fastmem.h:ContextRip]
  [fastmem.h:ContextRsp] [host_memory.cpp:Request] [host_memory.cpp:Requested]
  [host_memory.cpp:WindowStats]
- `Common::HostMemory`: construction/move/destruction, `ClearBackingRegion`,
  `Map`, `Unmap`, `Protect`, and `EnableDirectMappedAddress`. A nonzero requested
  virtual size plus `Requested()` selects an attempted fixed 4 GiB fastmem
  window; failed window creation leaves backing only. NCE/direct-mapped-address
  activation throws `std::logic_error`. [host_memory.cpp:HostMemory::HostMemory]
  [host_memory.cpp:HostMemory::Impl] [host_memory.cpp:HostMemory::ClearBackingRegion]
  [host_memory.cpp:HostMemory::Map] [host_memory.cpp:HostMemory::Unmap]
  [host_memory.cpp:HostMemory::Protect] [host_memory.cpp:HostMemory::EnableDirectMappedAddress]
- [Page allocation](./memory_pages.cpp), in `Common`: dense `AllocateMemoryPages`,
  `AllocatedMemoryPagesSpan`, and both `FreeMemoryPages` overloads; sparse-table
  `SparseTablesAvailable`, `AllocateSparsePages`, `FreeSparsePages`,
  `CommitSparsePage`, `DecommitSparsePage`, `SparseCommitSpan`, and `SparseUsage`;
  growing-heap `ReserveMemoryRange`, both `CommitMemoryRange` overloads,
  `RollbackUnpublishedHeapGrowth`, and `AbandonInitialHeapReservation`.
  [memory_pages.cpp:AllocateMemoryPages] [memory_pages.cpp:AllocatedMemoryPagesSpan]
  [memory_pages.cpp:FreeMemoryPages] [memory_pages.cpp:SparseTablesAvailable]
  [memory_pages.cpp:AllocateSparsePages] [memory_pages.cpp:FreeSparsePages]
  [memory_pages.cpp:CommitSparsePage] [memory_pages.cpp:DecommitSparsePage]
  [memory_pages.cpp:SparseCommitSpan] [memory_pages.cpp:SparseUsage]
  [memory_pages.cpp:ReserveMemoryRange] [memory_pages.cpp:CommitMemoryRange]
  [memory_pages.cpp:RollbackUnpublishedHeapGrowth] [memory_pages.cpp:AbandonInitialHeapReservation]
- `PS5_NATIVE`-only `Common` JIT helpers: `DirectMemoryStart`,
  `ExecutableAliasSpan`, `MapExecutableAlias`, `CountDenseJitDirect`,
  `DenseJitDirectBytes`; `ProbeSparseJitAlias`, `ReserveSparseJitCode`,
  `IsSparseJitCode`, `CommitSparseJitCode`, `TrimSparseJitCode`,
  `ReleaseSparseJitCode`, `SparseJitUsage`, and `SparseJitUsageFast`.
  [memory_pages.cpp:DirectMemoryStart] [memory_pages.cpp:ExecutableAliasSpan]
  [memory_pages.cpp:MapExecutableAlias] [memory_pages.cpp:CountDenseJitDirect]
  [memory_pages.cpp:DenseJitDirectBytes] [memory_pages.cpp:ProbeSparseJitAlias]
  [memory_pages.cpp:ReserveSparseJitCode] [memory_pages.cpp:IsSparseJitCode]
  [memory_pages.cpp:CommitSparseJitCode] [memory_pages.cpp:TrimSparseJitCode]
  [memory_pages.cpp:ReleaseSparseJitCode] [memory_pages.cpp:SparseJitUsage]
  [memory_pages.cpp:SparseJitUsageFast]
- [C adapters](./aligned_alloc.c): `__wrap_aligned_alloc`; [lifecycle hooks](./lifecycle.c):
  `catchReturnFromMain`, `eden_restart_app`, `eden_exit_app`, and `__assert`.
  Lifecycle definitions are unconditional and call externally declared platform
  functions; their names alone do not establish callers or linker wrapping.
  [aligned_alloc.c:__wrap_aligned_alloc] [lifecycle.c:catchReturnFromMain]
  [lifecycle.c:eden_restart_app] [lifecycle.c:eden_exit_app] [lifecycle.c:__assert]

## Invariants

- Select fastmem before constructing `HostMemory`; the request defaults false.
  Backing outlives its window. `Map`, `Unmap`, and `Protect` require a window,
  even for zero-length calls; invalid backing map/clear ranges throw. Window
  mutation is mutex-owned: four consecutive 4 KiB guest pages must form an aligned
  16 KiB backing granule before aliasing. Unaliased pages stay blocked; guard
  refresh includes the predecessor for cross-page accesses. `Protect` changes
  guard bytes, not mapping permissions. [fastmem.h:Request]
  [host_memory.cpp:window_requested] [host_memory.cpp:HostMemory::Impl]
  [host_memory.cpp:HostMemory::Map] [host_memory.cpp:HostMemory::Unmap]
  [host_memory.cpp:HostMemory::Protect] [host_memory.cpp:HostMemory::ClearBackingRegion]
  [host_memory.cpp:Window::Aliasable] [host_memory.cpp:Window::Refresh]
- Context layout is selected by `__linux__`, independently of `PS5_NATIVE`:
  Linux uses `REG_RIP`/`REG_RSP`; the other branch uses offsets `0xe0`/`0xf8`.
  These accessors perform no runtime ABI validation. [fastmem.h:ContextRip]
  [fastmem.h:ContextRsp]
- Dense page APIs require their own returned payload pointer and preceding
  `Header`; they do not validate foreign pointers. Sized free checks the recorded
  rounded total, not equality with the original requested payload size. Requests
  of at least 2 MiB normally use a 2 MiB lead, otherwise a system-page lead. Native
  mappings must fit the full checked CPU interval
  `[0x300000000, 0x4000000000)`; `DirectMemoryExtent` is an addressable physical
  extent, not available RAM. [memory_pages.cpp:Header] [memory_pages.cpp:FreeMemoryPages]
  [memory_pages.cpp:lead_size] [memory_pages.cpp:cpu_mapping_range]
  [memory_pages.cpp:DirectMemoryExtent]
- Dense executable aliases share backing owned by the writable allocation:
  unmap the entire `ExecutableAliasSpan` before freeing it, and balance explicit
  dense-JIT accounting. Sparse JIT instead owns stable RX/RW views keyed by the
  exact RX base, committing 2 MiB chunks before emission. Reserve requires a
  nonzero 2 MiB-multiple capacity and writable-output pointer; it bootstraps up
  to 4 MiB and never falls back densely. A failed later commit can retain earlier
  committed chunks. Trim/release require fully quiescent guest workers, not just
  the registry mutex, and invalidate positive caches before retiring backing.
  Both aliases must be retired before physical release.
  [memory_pages.cpp:ExecutableAliasSpan] [memory_pages.cpp:MapExecutableAlias]
  [memory_pages.cpp:CountDenseJitDirect] [memory_pages.cpp:ReserveSparseJitCode]
  [memory_pages.cpp:CommitSparseJitCode] [memory_pages.cpp:TrimSparseJitCode]
  [memory_pages.cpp:ReleaseSparseJitCode]
- Sparse tables are separate from sparse JIT: 2 MiB slots, at most 64 registered
  ranges, and cached availability. Reservations start read-only/zero; native
  slots alias a process-wide shared zero block. Requests below 4 MiB or failed
  sparse allocations fall back densely. Commit before
  writing a slot; decommit only after the whole slot is empty/zero, which this
  implementation does not verify. Registered sparse free requires the exact
  base and ignores its size argument; dense fallback still uses sized dense
  free. `SparseUsage` requires two nonnull output pointers.
  [memory_pages.cpp:sparse_ranges] [memory_pages.cpp:SparseAvailable]
  [memory_pages.cpp:ReserveSparse]
  [memory_pages.cpp:AllocateSparsePages] [memory_pages.cpp:CommitSparsePage]
  [memory_pages.cpp:DecommitSparsePage] [memory_pages.cpp:FreeSparsePages]
  [memory_pages.cpp:SparseUsage]
- Heap-range ownership is caller-managed, with 2 MiB reservation sizes and
  aligned commit segments; commit is not safe over arbitrary live mappings.
  Rollback is only for just-committed, unpublished growth with no live pointers;
  abandonment is only for a failed unpublished first mspace. Preserve exact
  physical ownership and guard/unmap before releasing it.
  [memory_pages.cpp:ReserveMemoryRange] [memory_pages.cpp:CommitMemoryRange]
  [memory_pages.cpp:RollbackUnpublishedHeapGrowth] [memory_pages.cpp:AbandonInitialHeapReservation]
- `WindowStats` is global diagnostic data, not a coherent per-instance or
  lifetime-pinned snapshot; direct-read/write counts describe eligible pages,
  not executed accesses. `SparseJitUsage` is owner-locked; its fast counterpart
  and `SparseUsage` use independent relaxed counters, not an aggregate physical
  memory budget. JIT virtual usage counts one capacity, not both views.
  [host_memory.cpp:WindowStats] [memory_pages.cpp:SparseJitUsage]
  [memory_pages.cpp:SparseJitUsageFast] [memory_pages.cpp:SparseUsage]

## Side Effects

- Memory operations allocate, map, protect, zero, discard, and release owned
  ranges; native branches use declared direct-memory APIs, while non-native
  branches use POSIX mappings. Native heap commit explicitly zeroes new backing;
  non-native heap commit only calls `mprotect`. Alias-withdrawal failure or
  ambiguous native ownership can abort rather than release still-aliased memory.
  [memory_pages.cpp:AllocateMemoryPages] [memory_pages.cpp:CommitMemoryRange]
  [memory_pages.cpp:DecommitSparsePage] [memory_pages.cpp:ReleaseSparseJitCode]
  [host_memory.cpp:Window::Update] [host_memory.cpp:Fatal]
- Fastmem creation temporarily installs `SIGSEGV`/`SIGBUS` handlers and probes
  backing with sentinel writes; sparse-JIT probing creates disposable aliases
  and checks read/write coherence without executing code. These probes do not
  establish general hardware qualification. [host_memory.cpp:Window::SelfTest]
  [host_memory.cpp:Window::SelfTestSteps] [memory_pages.cpp:ProbeSparseJitAlias]
- Reads up to 4095 bytes of `/app0/dev-settings.txt` with substring matching:
  `large_pages=off` changes dense allocation lead; native `sparse_tables=off`
  disables sparse-table availability, and native `heap=whole` refuses heap
  reservation. Native `EDEN_HEAP_GROWTH_TRACE` adds timing logs. Allocator
  diagnostics use kernel debug output and/or stderr; errors have no uniform
  errno policy. [memory_pages.cpp:DevSetting] [memory_pages.cpp:LargePagesEnabled]
  [memory_pages.cpp:SparseAvailable] [memory_pages.cpp:ReserveMemoryRange]
  [memory_pages.cpp:EDEN_HEAP_GROWTH_TRACE] [memory_pages.cpp:Note] [memory_pages.cpp:Report]
- `__wrap_aligned_alloc` requires nonzero power-of-two alignment and a size
  multiple of that original alignment; it delegates to `posix_memalign` with at
  least pointer alignment, returns `NULL` plus errno on failure, and does not
  zero storage. The caller frees successful allocations.
  [aligned_alloc.c:__wrap_aligned_alloc]
- Main-return handling flushes stdio, logs, and requests
  `sceSystemServiceLoadExec("exit", NULL)`; if it returns, logs refusal and sleeps
  forever in 100 ms intervals. Restart requests `/app0/eboot.bin`; explicit exit
  requests `"exit"`; each returns the platform result if the call returns.
  `__assert` logs to stderr and aborts. [lifecycle.c:catchReturnFromMain]
  [lifecycle.c:eden_restart_app] [lifecycle.c:eden_exit_app] [lifecycle.c:__assert]

## Verification

- From the repository root: `make test`. The real [Makefile](../Makefile)
  `test` target depends on `deps` and runs `tools/build-headless-host.sh`, so it
  can fetch dependencies and perform a host build. Documented only; **not run**
  for this documentation change. It is not evidence of native-console behavior.
  [../Makefile:test]
