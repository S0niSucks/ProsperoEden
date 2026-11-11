# Prospero.Eden Encore Blueprint

## System Overview

Architectural contracts for the Encore-owned runtime, native memory/lifecycle
adapters and developer tools. This index complements the human
[README](./README.md), [contribution guide](./CONTRIBUTING.md) and
[security policy](./SECURITY.md); it does not replace them.

Scope: production code in `headless`, `src` and `tools`. External
`vendor`/`third_party`, generated/ignored outputs, test-only packages and
data-only directories are excluded. Build selectors reference external inputs
without establishing their implementation or platform behavior.
[`headless/CMakeLists.txt:core_sources,video_sources`]

## Interface

- Native startup delegates selection to `SelectProsperoEdenGame`, configures a
  title and runs a core session; session failures return to the launcher. The
  host fixture path has separate switches, not a console launcher.
  [`headless/main.cpp:main,PS5_NATIVE`]
- Memory adapters supply backing/window operations, sparse tables, separate
  native sparse JIT arenas and process exit/restart requests.
  [`src/host_memory.cpp:HostMemory::HostMemory`]
  [`src/memory_pages.cpp:AllocateSparsePages,ReserveSparseJitCode`]
  [`src/lifecycle.c:eden_restart_app,eden_exit_app`]
- Root targets include `help`, `package`, `release`, `image`, `prepare`, `deps`,
  `deps-status`, `vendor-verify`, `toolchain`, `test` and `install`. `JOBS` is
  exported as `EDEN_BUILD_JOBS`; FTP installation requires `PS5_HOST`, with
  `PS5_PORT=2121` by default. [`Makefile:help,package,release,image,prepare,deps,deps-status,vendor-verify,toolchain,test,install,JOBS,EDEN_BUILD_JOBS,PS5_HOST,PS5_PORT`]
- `make dev` forwards `DEV_TITLE` to `build-package.sh dev`, but that wrapper
  currently accepts only `release` and `release-stage`. The declared target is
  not an operational development-build guarantee. No code is changed here.
  [`Makefile:dev,DEV_TITLE`] [`tools/build-package.sh:release|release-stage`]

## Subdirectories & System Boundaries

| Contract | Boundary |
| --- | --- |
| [Runtime](./headless/EGM.md) | Startup, core-session orchestration, device/renderer adapters, storage and loader helpers. |
| [Memory and lifecycle](./src/EGM.md) | Backing/fastmem, dense and sparse ownership, heap growth, allocation and exit/restart hooks. |
| [Host tools](./tools/EGM.md) | Dependency/build/package/install operations, source oracles, offline diagnostics and glyph tooling. |

Runtime children cover [elevation](./headless/elevation/EGM.md) and the
[launcher](./headless/prosperoeden/EGM.md), including a separate
[synthetic host preview](./headless/prosperoeden/host/EGM.md). Tool children cover
[release gates](./tools/ci/EGM.md), [launcher asset tools](./tools/launcher/EGM.md)
and the [inert AGC import facade](./tools/stubs/EGM.md).

## Global Invariants

- Elevation rollback failure or changed identity after a failed request is fatal.
  Successful elevation still requires a writable-root probe. Without successful
  filesystem access, storage helpers use sandbox fallback locations; this is
  not a guarantee that elevated execution remains sandboxed.
  [`headless/main.cpp:main,PS5_NATIVE`]
  [`headless/filesystem_probe.h:ProbeWritableRoot`]
  [`headless/storage_paths.h:FilesystemAccess,ConfigDir,UserDir`]
- The release wrapper enables `EDEN_FINAL_RELEASE_PROFILE`; CMake then requires
  native Vulkan/OpenGL/RADV and rejects named development experiments. Raw
  CMake option defaults are not the wrapper's effective release profile.
  [`headless/CMakeLists.txt:EDEN_FINAL_RELEASE_PROFILE`]
  [`tools/build-package.sh:release|release-stage`]
- Native sparse JIT reservation has no automatic dense fallback; sparse table
  allocation can fall back per allocation. JIT trim/release require quiescent
  guest workers: registry locks are not execution barriers.
  [`src/memory_pages.cpp:ReserveSparseJitCode,AllocateSparsePages,TrimSparseJitCode,ReleaseSparseJitCode`]
- Settings, save-transfer copies and caches do not share one transaction.
  Temporary files, best-effort rollback or per-file rename do not imply
  concurrent-writer or power-loss durability.
  [`headless/settings_store.h:Settings::WriteFile`]
  [`headless/ryujinx_saves.h:Import,Export`] [`headless/library_file.h:Write`]
- Distribution preflight rejects named private network configuration and unsafe
  input nodes before output cleanup/image invocation. Staging must stay
  quiescent; this is not a general secret scanner. Direct invocation of the
  image script is a separate boundary.
  [`tools/network_privacy.py:private_network_path,preflight_export_tree`]
  [`tools/ci/make-dist.py:preflight_export_tree,image_only`]
- Source predicates, host previews, counters, hashes and compiled marker strings
  do not establish gameplay progress, scanout, compatibility or PS5 qualification.
  [`headless/prosperoeden/host/fake_services.hpp:FakeServices`]
  [`headless/frame_metrics.h:Sample`]
  [`tools/ci/check-release-experiments.py:verify`]

## Global Side Effects

- Runtime paths can migrate selected legacy data, create directories, persist
  preferences/history, write media/cache/library files and transfer saves.
  Local contracts describe their failure and rollback limits.
  [`headless/main.cpp:MigrateLegacyInstallAssets,MigrateSandboxData,PrepareStorageLayout`]
  [`headless/prosperoeden/eden_services.cpp:EdenServices::games,EdenServices::set_preferences`]
  [`headless/metadata_bridge.cpp:eden_save_import,eden_save_export`]
- Native adapters request mappings/protection, install fault handlers, access
  device APIs and create workers; ambiguous memory ownership can abort.
  [`src/memory_pages.cpp:CommitSparseJitCode,FreeMemoryPages`]
  [`headless/fastmem_handler.cpp:Install,Handle`]
  [`headless/pad.cpp:Pad::Open,Pad::Poll,Pad::Close`]
  [`headless/audio.cpp:AudioStream::Finalize`]
- Launcher enrichment makes HTTP requests and staged cache writes. Cancellation
  is cooperative, not interruption of an in-flight request.
  [`headless/prosperoeden/eden_services.cpp:EnsureNlibEnrichment,EnsureRetailCase`]
- Build/dependency operations can fetch sources, run compilers, write caches and
  replace staging. Distribution recreates `dist`; FTP installation writes the
  fixed remote app path. These are tool behaviors, not bootstrap actions.
  [`tools/deps.py:fetch,clean`] [`tools/build-package.sh:release|release-stage`]
  [`tools/ci/make-dist.py:dist`] [`tools/install-ftp.py:main`]
- `make clean` removes `build` and `dist`; `distclean` also invokes dependency
  cleanup and conditionally removes an owner-stamped cache. These destructive
  maintenance targets are not verification commands. [`Makefile:clean,distclean`]

## System Verification

- Repository target: `make test`, whose `deps` prerequisite can materialize
  dependencies before `tools/build-headless-host.sh` builds/tests the Linux host
  path. It is not documentation-only verification. Local compilation remains
  restricted to an approved Docker workflow, not host toolchain installation.
  [`Makefile:test,deps`]
- Python syntax entrypoint: `python3 -B tools/check-python-source-syntax.py`;
  it parses source without importing project modules.
  [`tools/check-python-source-syntax.py:ast.parse`]
- All commands above are documentary, not executed by this bootstrap. Contract
  verification covers sections, citations, existing local links, actual command
  targets and preservation of the prior working tree. No build, dependency
  installation, console session, Git staging, commit or publication is included.
