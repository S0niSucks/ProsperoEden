# Encore headless runtime

## Overview

Encore-owned startup/session orchestration, renderer/input/audio adapters, metadata/storage helpers, and launcher integration ([main.cpp:main](./main.cpp), [CMakeLists.txt:eden-headless](./CMakeLists.txt)). External vendor/third-party implementations are outside this contract's inspected scope.

## Interface

- [main.cpp:main](./main.cpp) handles native startup and launcher/session lifecycle; host controls include `--repeat`, `--soak`, `--devices`, `--game`, `--cpu-pressure`, and `--shutdown-sweep`.
- [CMakeLists.txt:EDEN_PS5_VULKAN](./CMakeLists.txt), `EDEN_PS5_OPENGL`, `EDEN_VULKAN_DRIVER`, and `EDEN_FINAL_RELEASE_PROFILE` select renderer/profile wiring for `eden-headless`, `eden-pad`, and `eden-metadata`. The release-profile guard is opt-in, not a default qualification assertion.
- [graphics.h:GraphicsWindow](./graphics.h) and [graphics.cpp:CreateSharedContext](./graphics.cpp) connect renderer contexts/presentation callbacks; [audio.cpp:AudioSink](./audio.cpp) supplies output streams and [pad.cpp:Pad::Poll](./pad.cpp) publishes virtual input.
- [metadata_bridge.cpp:eden_extract_game_metadata](./metadata_bridge.cpp), `eden_scan_addons`, and `eden_startup_error` bridge metadata/setup; [ncz.cpp:Open](./ncz.cpp) wraps compressed content, with unpacking disabled by default.
- [library_file.h:Text](./library_file.h) and `Write` export the schema-1 `PPSA99008` library; [load_failure.h:Text](./load_failure.h) formats load errors; [nro_assets.h:Read](./nro_assets.h) validates NRO asset ranges before exposing icon/NACP data.
- [settings_store.h:LoadRecentGames](./settings_store.h) and `SaveRecentGame` expose persisted recent selections, separately from `LoadLastGame`/`SaveLastGame`.
- [forwarded_launch.h:ParseForwardedArgs,ResolveForwardedRom](./forwarded_launch.h) admit bounded one-shot requests. [main.cpp:main](./main.cpp) preserves crash/developer precedence and ordinary per-title setup; native argv delivery remains unverified.
- [assets_dir.h:GamesDir,GamePath,WallpapersDir,WallpaperName](./assets_dir.h) freeze independent content choices until process restart. [user_content.h:OpenDirectory,Images,ReadImage,kIoAvailable](./user_content.h) bounds image admission and defaults new descriptor I/O off under PS5_NATIVE without explicit qualification.
- [ps5_net_compat.c:getaddrinfo,freeaddrinfo,gai_strerror,gethostbyname,getnameinfo,__wrap_fcntl](./ps5_net_compat.c) supply native resolver and descriptor compatibility entrypoints, not full libc resolver semantics.
- [Startup elevation](./elevation/EGM.md): single-threaded client/protocol and target-limited helper.
- [Launcher](./prosperoeden/EGM.md): Eden service provider, frontend, reusable components, and host preview.

## Invariants

- [main.cpp:PrepareStorageLayout](./main.cpp), `MigrateLegacyInstallAssets`, and `MigrateSandboxData` preserve existing user roots with selected preflight/skip-existing behavior; rename rollback is best effort and copying has no general rollback. [prosperoeden/eden_services.cpp:EdenServices::diagnostics](./prosperoeden/eden_services.cpp) avoids `statfs`/`statvfs` and leaves disk fields unset.
- [main.cpp:main](./main.cpp) terminates on elevation rollback failure or changed identity after a failed request, and rejects failed sparse JIT alias preflight before launching; [jit-allocator.h:EdenJitAllocator](./jit-allocator.h) has no PS5 dense fallback. Session shutdown neutralizes/joins input and finishes JIT before `ShutdownMainProcess` and fault propagation; guest faults do not trigger automatic retry.
- [gpu_deferred_replay.h:kNativePostStorePublicationReady](./gpu_deferred_replay.h) remains false. [gpu_deferred_write_queue.h:Queue](./gpu_deferred_write_queue.h) models bounded ticket publication, not RAM stores or a GPU fence. Experimental selectors/probe output strings are not shipping or execution evidence.
- [frame_metrics.h:Sample](./frame_metrics.h) and [performance.cpp:Snapshot](./performance.cpp) describe CPU-observed telemetry, not qualified FPS/scanout. [cache_budget.h:TrimShaderCache](./cache_budget.h) trims eligible bins against a free-space reserve deficit, not a cache-size cap.
- Saves are user data: [ryujinx_saves.h:Import](./ryujinx_saves.h) attempts backup/best-effort rollback; `Export` removes its output tree on copy failure, including pre-existing content. Do not generalize these operations into a transactional or destination-symlink-safe save system.
- [settings_store.h:LoadRecentGames](./settings_store.h) retains at most four valid, distinct selections. `SaveRecentGame` moves the chosen selection to the front and truncates to four; a UI's visible slot count does not define persistence capacity.
- [forwarded_launch.h:ResolveForwardedRom](./forwarded_launch.h) rejects outside-root/nested ordinary selections, symlink ancestry and multi-link files; metadata admission is point-in-time, not a pinned loader handle. A request is not replayed after failure.
- [settings_store.h:LoadContentFolder,SaveContentFolder,LoadWallpaper,SaveWallpaper](./settings_store.h) changes only selected content keys, not saves or game-files roots. Unqualified native builds ignore new content overrides and report unavailable rather than linking speculative descriptor operations.
- [settings_store.h:Settings::LoadForMutation,Settings::WriteOwned](./settings_store.h) protects preference/content and mod edits from replacing malformed documents or following pre-existing temporary aliases. `SaveModSelection` combines a named mod and explicit master enable into one document/publication; legacy `SaveModEnabled` does not revive the master. Owned staging uses exclusive no-follow creation; final cancellation follows descriptor close and precedes rename. This is not a retrofit of every legacy writer, full nested-type validation, ReadFile repair, external-writer coordination or power-loss durability.
- [ps5_net_compat.c:getaddrinfo](./ps5_net_compat.c) accepts `AF_UNSPEC`/`AF_INET` and produces one IPv4 result; [ps5_net_compat.c:gethostbyname](./ps5_net_compat.c) uses caller-thread-local storage. `getnameinfo` returns numeric IPv4/port text and ignores flags rather than performing reverse DNS. [ps5_net_compat.c:__wrap_fcntl](./ps5_net_compat.c) forwards to the real implementation first and uses its compatibility fallback only for `EINVAL` on `F_GETFD`, `F_SETFD`, `F_GETFL`, or `F_SETFL`; that fallback does not independently verify descriptor type.

## Side Effects

- [main.cpp:main](./main.cpp) performs startup probes/migrations, starts/stops sessions, and persists successful last/recent selections. [settings_store.h:Settings::WriteFile](./settings_store.h) uses a fixed temporary path without internal locking/fsync; [library_file.h:Write](./library_file.h) uses exclusive no-follow staging/rename without fsync.
- [graphics.cpp:CaptureNextFrame](./graphics.cpp) stages PPM screenshots; [audio.cpp:AudioStream::Stop](./audio.cpp) pauses and waits for in-flight output, while [audio.cpp:AudioStream::Finalize](./audio.cpp) joins/drains/closes it. [crash_report.cpp:Install](./crash_report.cpp) installs signal/report workers and writes retained reports; restart requests are not successful-restart guarantees.
- [network_domain_rules.c:eden_network_filter_load](./network_domain_rules.c) publishes validated immutable policy; [network_audit.c:eden_network_audit_event](./network_audit.c) optionally writes buffered rotating metadata logs. [prosperoeden/eden_services.cpp:EnsureNlibEnrichment](./prosperoeden/eden_services.cpp) fetches/caches launcher media.
- [CMakeLists.txt:eden-headless](./CMakeLists.txt) configures build overlays, target-source replacements, and linkage. Those selectors establish configuration only, not the behavior of excluded external implementations.

## Verification

- From the repository root: `make test` ([Makefile:test](../Makefile)) depends on `deps` and invokes `tools/build-headless-host.sh`; it can fetch dependencies and build host checks. This command is documentary and was not executed.
- [CMakeLists.txt:eden-memory-check](./CMakeLists.txt), `eden-romfs-check`, `eden-scalar-check`, `eden-ryujinx-check`, `eden-shader-cache-check`, `eden-mods-check`, `eden-settings-check`, and `eden-devices-check` are actual host verification executables, not invented Make subtargets. Fixture/test bodies are outside documentation scouting scope.
- All production-source reads were reported complete by root-dispatched fresh scouts. This documentation run performed no builds, native runs, or PS5 hardware qualification; source origin/version alone, including origin 1.006, is not hardware evidence.
