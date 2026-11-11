# Launcher frontend and Eden services

## Overview

Frontend lifecycle and the Eden-backed implementation of the launcher service boundary ([frontend.cpp:SelectProsperoEdenGame](./frontend.cpp), [eden_services.h:EdenServices](./eden_services.h)).

## Interface

- [frontend.h:SelectProsperoEdenGame](./frontend.h) returns a selection, not guest execution. [frontend.cpp:RunApp](./frontend.cpp) owns the launcher loop/resources and rebuilds them after output reopen or language restart; [CMakeLists.txt:EDEN_PS5_OPENGL](../CMakeLists.txt) selects the full frontend versus `frontend_stub.cpp`.
- [eden_services.h:EdenServices](./eden_services.h) implements [pe/ui/services.hpp:Services](./pe/ui/services.hpp): home/library snapshots, media, settings, diagnostics, folders, mods, and save-transfer requests.
- [radio_input.c:radio_input_poll](./radio_input.c) and `queue_push` supply launcher input; [fr_fr_embedded.h:kFrFr](./fr_fr_embedded.h) supplies immutable French catalog data.
- [Components](./pe/EGM.md): UI, core utilities, graphics, platform output, and audio.
- [Host preview](./host/EGM.md): synthetic `FakeServices` and preview rendering, not a native substitute.

## Invariants

- [eden_services.cpp:EdenServices::games](./eden_services.cpp) and `home` use bridge locking and local snapshots with cancellation checkpoints; library export precedes unlock/network enrichment. [eden_services.cpp:EnsureNlibEnrichment](./eden_services.cpp) uses cooperative cancellation, not interruption of an in-flight HTTP request.
- [eden_services.cpp:EdenServices::diagnostics](./eden_services.cpp) leaves disk fields unset and does not call `statfs`/`statvfs`; preserve that PS5 startup restriction.
- [radio_input.c:queue_push](./radio_input.c) drops the oldest event when its 64-entry ring is full. `radio_input_poll` does not neutralize held state on zero/negative reads; do not assume emulator-pad failure handling is identical.
- [eden_services.cpp:EdenServices::save_import](./eden_services.cpp) and `save_export` are gated by `EDEN_SAVE_IMPORT` and delegate save operations. Saves remain user data; UI confirmation alone is not transactional-copy or rollback proof.
- [eden_services.cpp:EdenServices::preferences,set_preferences](./eden_services.cpp) copies saved FSR sharpness, anti-aliasing and independent appearance fields in both directions. The earlier DTO omission is repaired; extracted-provider/settings host fixtures are not selected-core/native or hardware qualification.
- [eden_services.cpp:EdenServices::location_folder,set_location_folder,wallpaper_files,set_wallpaper,wallpaper_image](./eden_services.cpp) exposes independent content only when its provider is available. [eden_services.cpp:EdenServices::load_image](./eden_services.cpp) decodes bounded wallpaper bytes through the existing texture worker; unqualified native content I/O remains unavailable.

## Side Effects

- [eden_services.cpp:EnsureNlibEnrichment](./eden_services.cpp), `EnsureRetailCase`, and `FetchCaseFace` fetch media from `api.nlib.cc` and `images.launchbox-app.com`, write caches, and join download work; staging does not establish power-loss safety.
- [eden_services.cpp:EdenServices::set_preferences](./eden_services.cpp) persists settings before logging/audit cleanup; `clear_shader_caches` removes named cache trees. `set_files_folder` creates layout before saving and is not an all-or-nothing operation.
- [frontend.cpp:RunApp](./frontend.cpp) initializes GL/resources and optional audio/input; [runtime_shims.cpp:pthread_once](./runtime_shims.cpp) changes once state and waits for concurrent initialization.

## Verification

- Repository host entry point, from the repository root: `make test` ([Makefile:test](../../Makefile)); it depends on `deps` and invokes `tools/build-headless-host.sh`.
- Not run here; current source, translation strings, and host preview output are not native/PS5 qualification.
