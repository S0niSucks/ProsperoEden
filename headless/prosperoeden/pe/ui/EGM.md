# Launcher UI

## Overview

Launcher screen state, service-provider boundary, drawing widgets, and asynchronous artwork ownership ([launcher.hpp:Launcher](./launcher.hpp), [services.hpp:Services](./services.hpp), [textures.hpp:Textures](./textures.hpp)).

## Interface

- [launcher.hpp:Launcher](./launcher.hpp) consumes input/frame updates and emits a draw list. [services.hpp:Services](./services.hpp) defines provider operations and UI data structures; [widgets.hpp:Canvas](./widgets.hpp), `ListView`, and `Hint` define drawing/list helpers.
- [library.cpp:start_scan](./library.cpp), `finish_scan`, `apply_games`, and `finish_home_scan` coordinate asynchronous library/home snapshots and filename-based focus.
- [home.cpp:press_home](./home.cpp), [library.cpp:press_game](./library.cpp), `press_mods`, [mapping.cpp:press_mapping](./mapping.cpp), and [settings.cpp:press_settings](./settings.cpp) delegate user changes to services.
- [browse.cpp:press_files](./browse.cpp) applies root selection on the next launch; `press_language` requests an in-process language restart after saving. [switch_theme.cpp:press_switch_home](./switch_theme.cpp) uses the same service boundary for the alternate presentation.
- [textures.cpp:Textures::pump](./textures.cpp), `cover`, `invalidate`, `render_case_page`, and `release` manage decoded/uploaded artwork. [video_presets.hpp:ApplyVideoPreset](./video_presets.hpp) changes values but does not persist them.
- [library.cpp:press_collection,draw_collection](./library.cpp) add a library-first grid/list interface using the same canonical games and actions; Recent retains the existing Home recent surface. Home recovery status remains visible even with ready setup. [theme.hpp:ApplyPalette](./theme.hpp) applies built-in role colours on the single UI owner, separate from interface and controller/video preferences.
- [browse.cpp:enter_files,finish_content_work](./browse.cpp) retains a cancelled directory owner until drainage and only then admits the current screen's deferred navigation reentry; old data and exceptional feedback share the currentness guard. This does not queue mutations. [widgets.cpp:text_shrink](./widgets.cpp) can preserve explicitly paired on-surface ink through high-contrast drawing for Switch's filled selections.
- [settings.cpp:press_dialog,draw_dialog](./settings.cpp) exposes independent Appearance properties and a Storage location chooser. [browse.cpp:press_files,draw_files](./browse.cpp) delegates independent location/image selection to provider capabilities; unsupported builds report refusal.

## Invariants

- [launcher.cpp:Launcher::press](./launcher.cpp) gates input during startup/launch; [home.cpp:press_home](./home.cpp) requires ready setup and a nonempty game file for launching. [launcher.cpp:Launcher::~Launcher](./launcher.cpp) cancels and waits for outstanding futures.
- [browse.cpp:finish_content_work,save_content_location](./browse.cpp) own cancellable new-location queries and writes. Directory results require current screen/kind and no cancellation; a pending content write excludes other UI mutations. A successful publication is reported saved even if cancellation arrived afterward. Cancellation is cooperative, not a hard syscall deadline; teardown joins owners before Services destruction.
- [library.cpp:finish_scan](./library.cpp) rejects canceled local scan snapshots; `apply_games` preserves filename focus. Default cancellation overloads in [services.hpp:Services](./services.hpp) check once, not continuously.
- [textures.cpp:Textures::pump](./textures.cpp) uses one decoder future and rejects stale generations; `render_case_page` owns at most two case pages. `release` waits for decoding before deleting GL textures.
- [textures.cpp:Textures::pump](./textures.cpp) handles at most one completed decode/upload and inspects at most 24 queued decode keys per call. Queued reclamation and cache eviction share a two-texture deletion limit (`kMaxTextureReclaimsPerFrame`, `kMaxCoverQueueLookupsPerFrame`). Its `budget` is a new-decode gate: `budget <= 0` does not suppress completed uploads or reclamation, nor does it specify a time budget.
- [case_media_selection.hpp:PageInputReady](./case_media_selection.hpp) does not make an existing unready page ready merely because time elapsed. [settings.cpp:press_dialog](./settings.cpp) requires nonempty label providers when cycling settings.

## Side Effects

- [settings.cpp:press_settings](./settings.cpp) and `press_dialog` save preference edits immediately and restore local values on failure; [home.cpp:press_home](./home.cpp) commits quick per-title settings locally only after the service save succeeds. Cache cleanup/default reset use confirmation. Service calls, not the drawing code, own the underlying file operations.
- [launcher.cpp:save_preferences](./launcher.cpp) skips ROM rescanning for appearance-only saves. [widgets.cpp:Backdrop::draw,glass](./widgets.cpp) requests a saved custom wallpaper through the existing async texture queue, preserves aspect and avoids sampling stale packaged blur under custom backgrounds.
- [library.cpp:start_scan](./library.cpp) starts asynchronous work; [textures.cpp:Textures::pump](./textures.cpp) schedules decoding and uploads GL textures.
- [library.cpp:press_game](./library.cpp) delegates confirmed save replacement. `press_mods` revives master+named mod through one composite service operation; unsupported providers refuse without two-write fallback. Local mod state/count refresh changes only after success. Source/API-host evidence is not actual UI-event or full-provider runtime qualification; UI confirmation is not a save-copy or rollback guarantee.

## Verification

- Repository host entry point, from the repository root: `make test` ([Makefile:test](../../../../Makefile)); it depends on `deps` and invokes `tools/build-headless-host.sh`.
- Not run here; UI drawing and host previews do not qualify PS5 execution or storage safety.
