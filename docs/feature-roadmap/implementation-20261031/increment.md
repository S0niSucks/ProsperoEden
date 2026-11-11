# Encore implementation increment — P10, P11 and bounded P05

Status: application implementation; host evidence only; not full roadmap closure.
Date: 2026-10-31. No native package or console operation is qualified here.

## Problem and scope

Players need genuinely different launcher interfaces and independent visual
themes without losing video settings, saves, multilingual text, controller
recovery or the existing game handoff. Forwarders must not bypass current
library/history admission. Content-location options must not silently migrate
data or introduce unknown native imports into a working console path.

This increment implements the application side of those boundaries. It does
NOT implement NFC, live/per-code cheats, SMB/NFS transports, Nextendo online,
native keyboard, wired Pro Controller, USB backup/restore or Switch 2. Those
remain explicit work items in the parent twelve-group design. Host/guest firmware
are separate axes; neither has gained universal support. Absence of permitted
core/SDK/provider evidence is not an API to fabricate.

## Preserved boundaries

- Keep the existing native session, Safe Launch, per-title profile/input/glyph
  setup, history and teardown. No guest-fault retry, SDK offset change, renderer
  replacement, wholesale core copy or user-data reset.
- Builds only in Docker. Repository read-only during tests/previews; logs,
  objects, executables and screenshots external. No staging/commits/publication.
- Prior uncommitted owned files preserved in an external 1,526-file baseline.
  Each intentional subsequent change is explicit; no assertion all old bytes
  remain unchanged where this increment deliberately extends their owners.

## Architecture and chosen alternatives

### P10

One native Launcher still owns screens/modals/input and canonical games_. Three
interfaces are Encore, Switch and Collection. Five palette choices are Original,
Midnight, Canopy, Slate and Ember; Original keeps earlier interface colours.
Collection has a paged four-column grid or one-column list, eight items per
page and a shared selected-game detail/sleeve pane. It draws only that page.
Arrow movement clamps at edges and does not invent cells on short last rows.
At the top, Up focuses the existing navigation. L1/R1 remain main-tab shortcuts.
Recent deliberately uses the existing true Recent surface. R2 enters the
existing rear-jacket viewer; viewer Cross must not launch. Triangle/Options/
Square/Cross delegate existing game settings/Safe Launch/mod/launch behavior.

Appearance uses the existing immediate settings save and local rollback.
Colour edits no longer rescan all ROMs or change docked/video defaults. The
native provider now reads saved FSR sharpness and AA instead of DTO defaults,
preventing an unrelated appearance save from overwriting these video values.

Stable keys are in headless/launcher_appearance.h. Canonical appearance.interface
is authoritative when present; absent reads the old appearance.theme. A present
invalid canonical value falls back to Encore, not a stale alias. Save maintains
theme=switch only for Switch, otherwise encore, for downgrade compatibility.
palette and library_layout have independent fallback/validation. Unknown JSON
fields survive successful writes. Invalid values do not write.
For SavePreferences, SaveContentFolder and SaveWallpaper, existing malformed
scalar/array/null top-level documents, invalid JSON and unreadable documents are
refused without publication. Malformed named sections are also refused. Reads
retain defaulting behavior. Missing documents may import legacy values in memory;
mutation loading does not itself publish a legacy migration. This is not a
retrofit of all other legacy settings writers. Bool failure restores local look.

The existing renderer/text stack is retained. Native role colours are updated
only by the one UI owner through apply_look before drawing; services/image workers
must not read them. Static legacy Plate identity is preserved (artwork focus
remains edge-only); palette-aware drawing resolves new surface colours at use.
No fonts, sounds, assets, renderer or component binary layouts are imported from
the ps5-homebrew-ui reference. Its theme/design separation is adapted conceptually.

Alternative: explicit per-Canvas palette/context everywhere. It avoids the
existing single-UI global-look convention but would require broad invasive edits
through previously working screen/case/widget code. This increment keeps the
single-owner convention, does not support concurrently rendered Launchers, and
retains immutable content/image ink. Runtime packs and a new renderer are not
selected: their load/trust/lifetime surface is unnecessary for built-in choices.
Selected Switch categories, Files rows and language names/OK markers use dark
ink on the non-Original/high-contrast bright focus role. Their text_shrink calls
explicitly preserve that on-surface ink instead of applying text_ink's generic
high-contrast neutral lift; other text retains its existing transform. Original
non-high-contrast retains its legacy pair. Focused advanced editors stay dark
with light ink. Raw-role checks do not prove drawn contrast: the combined preview
must inspect actual DrawList glyph RGB and require at least4.5:1 against the focus
role for all five palettes with high contrast both off and on (excluding the
preserved Original/off legacy pair). Language's OK marker is checked separately.
This is not certification of every legacy Original accessibility surface.
Switch dialog labels reserve a32px gutter before the fixed294px value column,
including long pt-BR labels with larger text; this is a local layout refinement.

### P11

Parse at most 64 readable C-string arguments, at most 1,024 bytes each, accepting
--rom X, --rom=X and --exit-after-game. Invalid/duplicate known flags invalidate
the whole request. Unknown bounded tokens are ignored. Do not log raw argv.

Admit only current persisted identities: supported top-level files or exactly
Folder/main with main.npdm. Absolute spellings must be under the selected games
root. Reject unsafe spelling, any symlink ancestry, wrong types, multi-link files
and missing files. No moved-file fallback: nested ordinary ROMs are not existing
history identities. Metadata checks are point-in-time, not a pinned loader handle;
the storage tree must remain quiescent until normal load.

main.cpp consumes once after storage setup, suppresses unattended forwarding if
a previous crash report exists, preserves explicit developer-boot precedence,
and passes the selection through the ordinary per-title path. Failed admission
opens the launcher with a generic message. Exceptions return through existing
failure handling without replay. The exit modifier affects only a normally
completed forwarded session after teardown, not a manual later game.

Alternative: arbitrary absolute paths/moved fallback would broaden admission
and risk an unrelated title. Direct core loader calls would bypass current
session guards. Neither is selected. Native argv delivery remains unverified.

### P05

Additional settings are storage.games_folder, wallpapers_folder and wallpaper
(one relative filename). Empty roots follow assets/roms and assets/wallpapers.
Paths use the existing /data or /mnt user-data grammar. Process-frozen GamesDir,
GamePath, WallpapersDir and WallpaperName keep all readers bound until restart.
All content fields are captured together from one settings document in ContentDirs;
first use of a previously unused wallpaper getter cannot activate a later save.
Discovery/metadata/presence/launch/forwarder/history use the same games root.
library.json preserves its existing game_files field and adds games_folder;
existing absolute paths/fields and schema-1 identity are retained.

The existing root browser is reached through a three-property Storage modal.
Files still uses its original action. Games/Wallpapers use independent providers;
the wallpaper browser offers images alongside directories, explicit Cross selects,
Triangle saves the directory and clears image choice, Square resets only that
property. Failures preserve current settings. No content or saves are copied.

UserContent walks no-follow directory descriptors, opens only regular single-link
files, bounds encoded images to 16 MiB, and checks metadata again after reading.
Independent content_entries uses one UserContent::Entries walk for both lists:
at most 4,096 entries, 256 sorted folders and 128 sorted images from those entries.
It does not first call legacy folders/ListEntries. Every entry has a cancellation
check; failure/cancellation/unavailability clears both lists, including prepopulated
outputs at both the default Services and native-provider boundaries. Legacy Files enumeration remains unchanged.
The fdopendir stream has immediate scope ownership, including its duplicate
descriptor. Lists are built privately and moved to caller outputs only after
the successful bounded walk/sort and final cancellation check. Exceptional
allocation/iteration returns failure with empty caller outputs and releases both
directory owners; the UI future catch is not relied on for resource cleanup.
This does not authenticate
content or detect every concurrent in-place write. Stop external writers.
The wallpaper codec reads private bounded bytes, probes dimensions before full
decode (4,096 per side, 8,388,608 total pixels), then uses the existing async
texture queue. Missing/corrupt art falls back; high contrast may suppress art.
Custom wallpaper preserves aspect and suppresses sampling stale packaged blur.

Discovery/admission run on owned cancellable futures, not the input/render
owner. finish_content_work applies a completed directory snapshot only while
its screen/kind/cancel identity remains current; exceptional completion feedback
has the same guard. Only one query exists at a time. Circle cancels discovery
and returns to settings without blocking a frame. Reentering Games/Wallpapers
before drainage records only the latest current screen's navigation intent,
never starts a second query or clears the old token. Once the old owner drains,
the current browse_dir/kind is queried exactly once; leaving again discards this
deferred intent. Successful/exceptional old completion cannot replace newer
screen feedback. This is navigation reentry, not a mutation queue.
Writers reject all new mutations
while an admitted save is pending; Circle requests cancellation. Native reads
check cancellation between path components, entries and 64 KiB read chunks, and
the token is carried through settings staging. Noninterruptible syscall
latency is not bounded by a byte cap; cancellation and teardown remain cooperative.
Launcher destruction cancels and joins both owners before Services dies.
These three protected mutations publish through Settings::WriteOwned, not the
legacy predictable .tmp/fopen helper used by other writers. It exclusively creates
one same-directory .tmp.<pid>.<sequence> with O_EXCL|O_NOFOLLOW, mode0600, trying
at most8 candidates; collisions fail without following/removing others' nodes.
It uses the already-present LibraryFile open/write/close import family, no new
native fdopen/fsync API. External writers must remain stopped during publication.
Only its own created temporary is removed on failure; a pre-existing legacy .tmp
symlink/hardlink is untouched. Existing files are never opened for truncation.
The cutoff is the final acquire-load of the cancellation token after temporary
descriptor writes/close and immediately before the rename decision in WriteOwned.
If observed, remove the temporary and do not publish. After that decision rename
is non-cancellable; a successful publication wins over a later request and reports
Saved, never "unchanged". Tests inject cancellation during actual write/close
and at rename entry, using real libc I/O with deterministic wrappers. This is
not a timing/power-loss/native durability claim. No mutation queue or detached writer.

CRITICAL NATIVE GATE: new descriptor I/O is unavailable by default under
PS5_NATIVE unless EDEN_NATIVE_USER_CONTENT_IO_QUALIFIED equals 1 and native
qualification is externally established. Defining it as 0 remains unavailable.
Default native code compiles no new openat/fdopendir/readdir/fstatat references;
new settings are inactive and UI reports Unavailable. No CMake/release option
silently sets that gate. Existing game-files storage remains available. Host
fixtures prove no new descriptor imports for the unqualified header path, NOT
that qualified native APIs are available. Do not enable the gate based on them.

Alternative: unconditionally linking speculative native filesystem functions
could break startup even with the new UI unused. Replacing descriptor traversal
with check-then-open path calls would weaken the promised ancestry checks.
Neither is selected. Thus P05 is application/host implementation, native-blocked.

### Firmware labels

Guest setup says Guest firmware checks passed; a folder NCA count says files
found, not compatibility. Diagnostics says PS5 host version not observed/current
build unqualified when no adapter supplies a real observation. No host probe,
firmware offset, guest-version range or automatic key/firmware download is added.

## Explicit changed-file list

Create:
- headless/launcher_appearance.h: stable IDs/names, range validation and grid move.
- headless/forwarded_launch.h: argument/admission boundary (worker-owned creation).
- headless/user_content.h: bounded descriptor reader and default-off native gate.
- tools/check-appearance.py: actual settings/provider-body/nav fixture.
- tools/check-forwarded-rom.py: actual forwarder/validator fixture.
- tools/check-content-roots.py: actual roots/descriptor/codec/reopen/native-gate fixture.
- docs/APPEARANCE.md, CONTENT_LOCATIONS.md, FIRMWARE_COMPATIBILITY.md, FORWARDER.md:
  durable behavior/limits; not temporary execution reports.

Modify:
- headless/settings_store.h: additive appearance/content JSON validation/persistence.
- headless/assets_dir.h: process-frozen independent root resolution.
- headless/library_file.h: optional games_folder field, old field meaning retained.
- headless/main.cpp: one-shot native forwarder and common games-root path/history.
- headless/dev_launch_policy.h: explicit launcher=first/autoboot=off suppresses
  forwarding as well as developer autoboot; default explicit forwarded intent is
  still permitted when neither crash nor force-launcher intent exists.
- headless/prosperoeden/eden_services.h/.cpp: DTO round trip, paths, providers,
  image codec and truthful guest-setup label.
- headless/prosperoeden/pe/ui/services.hpp: additive properties/provider defaults.
- headless/prosperoeden/pe/ui/launcher.hpp/.cpp: dispatch, palette application,
  wallpaper request, appearance-save scan avoidance and Storage browser state.
- headless/prosperoeden/pe/ui/library.cpp: Collection input/drawing, old actions reused.
- headless/prosperoeden/pe/ui/settings.cpp: independent Appearance/Storage rows,
  refusal/rollback, compatibility diagnostics.
- headless/prosperoeden/pe/ui/browse.cpp: one selected location and image picker.
- headless/prosperoeden/pe/ui/home.cpp: existing Files shortcut resets browser kind.
- headless/prosperoeden/pe/ui/switch_theme.cpp: matching controls/palette surface
  versus neutral image ink; new location browser uses shared native composition.
- headless/prosperoeden/pe/ui/theme.hpp: single-UI-owner palette role application.
- headless/prosperoeden/pe/ui/widgets.hpp/.cpp: native palette surfaces/backdrop.
- headless/prosperoeden/host/fake_services.hpp/.cpp, host/preview_main.cpp:
  synthetic provider controls and real C++ rendered/action-based scenarios.
- tools/check-recent-games.py, tools/check-library-file.py: intentional source
  seams follow GamesDir while keeping earlier identity/schema assertions.
- headless/EGM.md, headless/prosperoeden/EGM.md,
  headless/prosperoeden/pe/ui/EGM.md and docs/LIBRARY_INTERCHANGE.md: synchronize
  repaired DTO, separate ROM identity base, actual helper/provider/UI boundaries.
- headless/prosperoeden/ui/lang/pt-BR.po: new property/status translations only;
  existing translations, font shaping and saved identifiers are preserved.

No deletions. Full selected-core/native compilation remains an independent gate.

## Ordered stable implementation checkpoints

These are the ordered replay/validation checkpoints for a fresh implementer, not
invented historical commits; current edits evolved together under the preserved
baseline. A checkpoint is not complete until its listed assertions pass.

1. **Preference safety first.** Create launcher_appearance.h and user_content.h
   (standalone bounded helpers/native gate, not yet wired to UI); change settings_store.h,
   both Preferences DTOs, eden_services.cpp conversions and theme.hpp's standalone
   role/palette helpers as one coherent unit. The fixture's ApplyPalette assertions
    therefore have their implementation in this checkpoint, before UI wiring.
    Keep existing interface defaults and downgrade aliases. Add check-appearance.py;
    run it with --preferences-only here, excluding the later sidebar source seams.
    The final full invocation without that flag requires checkpoint2 UI wiring.
   Acceptance: all 3×5×2 round trips, old Switch-only config, invalid canonical
   versus stale alias, invalid ranges/no writes, scalar/array/null containers,
   unrelated JSON/history/FSR/AA preservation. Existing interfaces remain usable.
2. **Coherent application/provider/UI bundle.** P10 UI and P05 providers/owners
   are one checkpoint because the real preview intentionally exercises both.
   Do not require a P10-only gate before its mock/provider APIs exist. Change
   assets_dir.h, content persistence in settings_store.h, library_file.h, main's
   ROM/history seams, services.hpp, eden_services.h/.cpp, launcher.hpp/.cpp,
   library.cpp, settings.cpp, browse.cpp, home.cpp, switch_theme.cpp and
   widgets.hpp/.cpp together with both FakeServices files and preview_main.cpp.
   Use user_content.h from checkpoint1. Add check-content-roots.py and update
   recent/library source seams. Both provider additions and all owned futures
   therefore exist before Stage::restart and the combined fixture compile.
   No new .cpp/CMake entry is required. Acceptance: real C++ preview of all
   interface/palette pairs; Collection grid/list/partial pages, true Recents,
   shared settings/Safe Launch/jacket actions, save rollback, no-ROM/setup recovery,
   and focused Switch game/mods/mapping rows in all five palettes. With Collection
   and valid setup, forwarder refusal, previous crash and returned session error
   must render recovery status, including after controller navigation to/from
   Settings. Existing case
   renderer/input/font owners survive. FakeServices.games_calls must not increase
   during appearance-only edits. For a ten-item grid, controller sequences must
   clamp to index7 when its lower column is missing, and index9 at the last-row
   edge; Options must call the observable arm_safe_launch once and select that
   exact file. Square must call set_mods_enabled for the selected mod-bearing
   title and toggle it. On Recent, Cross on first/second provider recents first
   focuses the existing Home Hero without launching; a second Cross must select
   that file in provider order, not alphabetical library order.
   Content acceptance: no-follow ancestry/types/links/size/cancel; exactly4,096 examined
   entries across the actual provider, 256 sorted folders and 128 sorted offered
   images; cancellation after fourth entry and injected metadata error both clear
    both lists. Per-side AND total-pixel refusal must leave a wrapper counter of
   actual stbi_load_from_memory entries unchanged, not merely return false from
   malformed tiny TGA data. Independently selected roots on fresh process; resetting
   one preserves other properties, legacy root/history and unknown fields; active
   roots remain frozen until reopen. Both absent and explicitly-zero native gates
   have no new descriptor imports. UI remains renderable during stalled discovery/
   admission; cancellation observed at the defined final pre-rename check preserves
   saved bytes, while cancellation after that decision does not undo publication. Unavailable
    providers preserve saved state and render no selected custom wallpaper.
   Prepopulated discovery outputs must be empty after unavailable default/native/
   mock calls. A mock noninterruptible query must remain active after Circle;
   reopen the same and a different storage kind BEFORE releasing it. No overlap
   is allowed; after release exactly one replacement query must publish a usable
   parent entry. A cancelled old query that throws on release must not alter the
   later unavailable-provider explanation; compare actual rendered glyph identity.
   Separately run actual content_entries on an owned future with a bad_alloc
   injected after stream acquisition and partial private discovery. Repeat8 times:
   each failure must close exactly its acquired stream, leave both prepopulated
   output lists empty, and make fcntl(F_GETFD) on its duplicate fail with EBADF.
3. **Forwarder bundle.** Add forwarded_launch.h, dev_launch_policy predicate,
   main one-shot selection and check-forwarded-rom.py. GamesDir already exists
   from checkpoint2. Acceptance: malformed/bounded argv, all current formats,
   narrow ExeFS, containment/nodes, no moved fallback, previous crash and BOTH
   force-launcher tokens. Preserve developer autoboot precedence, ordinary title
   setup/history and post-teardown exit; native argv/core compile still NOT_RUN.
4. **Documentation/localization/evidence.** Synchronize named contracts and four
   feature documents, library field precedence and pt-BR messages. Run all ten
   focused regressions and Python syntax checks in read-only-source Docker;
   capture current real C++ images and inspect them. Record each source/target
   hash and any failure without erasing old receipts. Keep native gate off and
   all unrelated roadmap/core/firmware qualification states explicitly blocked.

## Verification and limits

Actual Docker fixtures already passed before this review: appearance settings/
provider-body extraction/nav ASan+UBSan; forwarder ASan+UBSan; content descriptor/
codec/reopen ASan+UBSan and an nm check of unqualified native header imports.
Preference-provider bodies redirect only their default settings pathname.
Content discovery bodies are unchanged; the codec body's single full-decoder
entry is wrapped for counting and forwards the actual distribution stb decoder.
None of these extracted-body fixtures is a native provider/core compile.

The launcher-only C++/EGL/GL preview compiled and rendered the interface/palette
matrix and real key-driven save/refusal/empty-library/viewer actions. It uses
distribution HarfBuzz 8.3 and existing checked-in assets, not native's pinned
HarfBuzz 12.3.2 or regenerated canonical package art. Some optional Switch print
art is absent in this host asset set. Logs separate those limits from passes.
Later edits are being rebuilt; exact execution receipts live separately.

## Executable acceptance gate

Use Docker image `prosperoeden-host:ui-features` (Ubuntu 24.04 host compiler,
Mesa/EGL/GL, distribution stb and HarfBuzz 8.3, nlohmann/json). The external
`preview-image/Dockerfile` derives it from `prosperoeden-host:network-features`;
it is not a native SDK image. No host compiler install is needed.

The ten checks, in order, are tools/check-appearance.py, check-content-roots.py,
check-forwarded-rom.py, check-recent-games.py, check-library-file.py,
check-loader-message.py, check-nro-assets.py, check-metadata-image-bounds-host.py,
check-pad-recovery-host.py and check-export-privacy.py. Run with Python3 -B from
/repo; every exit code must be zero. Content fixtures require an empty writable
disposable /data tmpfs, not real user data. Parse all tools/headless Python with
ast.parse without writing bytecode. The external run-focused-checks.py performs
this exact sequence. It writes a receipt before starting, updates it after each
test and records timeout/start-failure status with partial output in that log.
This Linux Docker runner owns each check in a new process group, uses a regular
log file rather than a captured pipe, enables child-subreaping, and SIGKILLs the
group on timeout or direct-parent completion. Direct-child wait and adopted-group
reaping each have a2s bound; cleanup failure is a terminal failed check, not PASS.
This owns normal check/compiler descendants, not malicious session-escaping jobs
or a kernel task stuck indefinitely in noninterruptible I/O.
Syntax errors are collected with file/error and leave terminal failed result.json;
completed results survive. Any such failure exits nonzero. Unattempted checks are
not_run until attempted. Existing output directories are refused, not overwritten.
check-runner-receipts.py separately tests real timeout/partial output, missing
interpreter, syntax failure and refusal to overwrite a previous receipt. A second
timeout fixture creates a descendant retaining stdout; terminal timeout, its
partial log and no surviving/unreaped descendant are required. The same self-check
must verify preview evidence collisions are rejected BEFORE any compilation and
that old logs/screenshots remain byte-identical.
For that self-check, use the same Docker/security/evidence mount flags below,
changing the command to python3 -B /evidence/check-runner-receipts.py. Checkpoint1
uses the first invocation's flags with command python3 -B
/repo/tools/check-appearance.py --preferences-only. These are supplemental to,
not replacements for, the final ten-test combined gate.

PowerShell, from the source root (set E to this external implementation directory):

```powershell
$E='C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/encore-feature-ports-20261030/implementation-20261031'
$R='D:/leonardommello/PS5/ProsperoEden-Encore'
docker run --rm --pull never --network none --read-only --cap-drop ALL --security-opt no-new-privileges --user 65534:65534 --tmpfs /tmp:rw,exec,size=1024m --tmpfs /data:rw,mode=1777,size=128m --env CHECK_RUN=unique-run-name --mount "type=bind,source=$R,target=/repo,readonly" --mount "type=bind,source=$E,target=/evidence" --workdir /repo prosperoeden-host:ui-features python3 -B /evidence/run-focused-checks.py
docker run --rm --pull never --network none --read-only --cap-drop ALL --security-opt no-new-privileges --user 65534:65534 --tmpfs /tmp:rw,exec,size=512m --env PREVIEW_RUN=unique-preview-name --env PREVIEW_WIDTH=1920 --env PREVIEW_HEIGHT=1080 --mount "type=bind,source=$R,target=/repo,readonly" --mount "type=bind,source=$E,target=/evidence" --workdir /repo prosperoeden-host:ui-features python3 -B /evidence/build-preview.py
```

build-preview.py compiles host/*.cpp, pe/core/*.cpp, pe/gfx/*.cpp and pe/ui/*.cpp
except harfbuzz.cpp with C++20, -O1, -Wall -Wextra, -pthread and GL prototypes;
include source/host/headless plus system stb/HarfBuzz; link EGL/GL/HarfBuzz/m.
It runs pe_preview with checked-in ui assets and PE_PREVIEW_APPEARANCE=1 (not the
default gallery). Require REAL_NATIVE_CPP_UI_PREVIEW_PASS, the inner
APPEARANCE_NATIVE_UI_FIXTURE_PASS, no APPEARANCE_ASSERT_FAIL and GL error 0x0
for all screenshots. Inspect actual PNGs. Repeat with --env PE_LANG=pt-BR and
--env PE_LOOK=large,contrast,calm AND a different fresh --env PREVIEW_RUN value.
For example use preview-<checkpoint>-normal and preview-<checkpoint>-ptbr-accessible;
replace <checkpoint> for every retry rather than rerun an old name. build-preview.py
requires PREVIEW_RUN and exclusively reserves that evidence directory BEFORE
compilation; an existing directory must fail without touching logs/images or
starting workers. Do not reuse CHECK_RUN either. These are still host captures.
Snapshot packages include the external runners under harness/ and the ten test
sources. Never run concurrent previews sharing the build output directory.
The immutable snapshot is an audit view, not a standalone native SDK/source
checkout. Replay commands use the existing source root and approved image.
Owned root headless C++ inputs and the export-privacy helpers are included for
fixture source-seam inspection; protected core/SDK/dependencies remain excluded.

Host mock semantics are explicit: Stage restart drains the old launcher and
captures saved content choices as a synthetic full-process reopen. It is not
native in-process language-restart evidence. FakeServices exposes separate saved
and active roots; active choices change only at that snapshot, and disabling
content I/O suppresses custom wallpaper while preserving saved values. Wallpaper
pixels are generated fixture cover art, not arbitrary host filesystem input.
The mock's state mutex protects preference/game-setting/docked/mod snapshots,
saved Files root and error strings; public scalar controls are atomics. Its
separate content mutex protects saved/active content roots and image choices.
The generated games/art/catalog vectors are immutable after construction; label
catalog use stays within the initialized UI language lifetime. This is a defined
access contract, not ASan/UBSan proof of race freedom. A concurrent coherent-pair
preference stress witness complements static review (no TSan claim). A pauseable provider
permits deterministic cancellation-before-publication tests. The UI refusal
scenario asserts unchanged games/wallpaper roots and image selection; a PNG alone
is not its acceptance test. Actual filesystem/codec assertions are separate.

Switch Diagnostics presents the same unobserved/current-build-unqualified host
label in both category and dialog as Encore/Collection. Host capture acceptance
requires both surfaces in each palette and pt-BR/large/high-contrast/reduced-motion;
completion of current execution receipts is tracked separately.

Still required: selected native/core compile, real argv/handoff, native codec/
filesystem qualification, system fonts/audio, PS5 focus/input/frame/resource
behavior and firmware/title tests. No screenshot, compile or metadata claim
promotes any of these to PASS. The other roadmap features remain unimplemented.
