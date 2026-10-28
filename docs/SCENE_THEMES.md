# External scene themes — experimental implementation

This workstream adds a data-only scene renderer, a bounded native offline NXTheme importer,
and separate PC **lite** author tooling. It does **not** complete #173/#239: full firmware
material/texture reconstruction, reference parity and PS5 qualification remain outstanding.
Neither artwork conversion nor the diagnostic geometry preview is faithful Switch parity.

## Players

Scene packs share `/data/prosperoeden/launchers/<id>/` and **Appearance > Launcher** with Lua
packs. They do not alter the compiled **Theme** selector. `manifest.json` retains the
`launcher-v1.0` contract/API 1.1, declares `themes.scene.v1`, and names `"scene":"theme.json"`
instead of `entry`. Old hosts refuse the required capability rather than trying to run the
pack as Lua. Existing Lua packs do not need to change.

`examples/canopy-scene` is an installable 11-screen reference increment using the existing
Canopy palette, rail and host typography. Library, profiles, Settings, video/language, audio,
accessibility, game options, game video overrides, save transfer, mods and cheats have
scene-owned screens backed by native services. Advanced built-in tools remain reachable;
not every built-in workflow or reference artwork has parity. No Nintendo artwork, games,
keys or firmware are included.

Install an unpacked scene folder on a POSIX authoring host (or Docker) with the compiled native
validator:

```sh
python3 -B tools/launcher/scene/themezer.py install examples/canopy-scene /path/to/test-data \
  --validator /work/scene-tests/scene-check
```

The installer copies into a private staging folder, refuses links/special files, scripts and
shaders, bounds file counts/bytes, validates the copy with the same reader used at runtime,
then atomically publishes a **new** pack. It never overwrites an installed pack. Existing
HTTPS launcher-update feeds, staged replacement and rollback can update scene packs; the
updater now validates their complete scene/resources before replacement. A feed is not
created by the example or the importer. Scene updates require a scene-capable destination
host. `launcher-runtime.json` advertises the additive capability without changing the old API.

Selection prepares all scene, image, font and audio resources before retiring the previous
launcher. A rejected selection leaves the previous launcher intact. Startup/runtime failures
recover to the built-in launcher; `config/force-builtin-launcher` still works. **Options** is
always a host-owned route to the launcher picker unless a native confirmation/launch dialog
needs an answer first.

## Author contract (scene schema 1)

`theme.json` contains `schema`, matching `id`, `name`, `entry` (screen ID), and maps:

| Field | Value |
| --- | --- |
| `screens` | screen ID → relative JSON filename |
| `animations` | animation ID → relative JSON filename containing a track array |
| `images` | logical image ID → uncompressed 24/32-bit true-color TGA |
| `font` | optional relative `.pefont`; omitted uses the host's shaped/fallback font |
| `audio` | cue/music ID → 48 kHz, 16/24-bit PCM mono/stereo WAV |
| `cues` | optional `focus`, `select`, `back`, `error` → audio IDs |

Screens contain `nodes`, optional `focus`, `input`, `music`, `transition` (0–2 seconds) and
`animations` (array of IDs). The canvas is the incumbent 1920×1080, scaled/letterboxed by
the existing OpenGL renderer. A screen switch has a host fade; node tracks carry authored
motion. `music` repeats a bounded decoded recording on the existing menu-volume bus.
Playback is host-scheduled, **not sample-accurate/gapless streaming**. No new audio thread or
decoder worker is introduced. Existing mixer stop fences retain retired clip owners.

Nodes require unique per-screen lowercase `id` and a `type`:

| Type | Meaning |
| --- | --- |
| `group` | translated child subtree |
| `image` | named image, optional normalized `uv`, rounded image via `radius` |
| `text` | host-shaped/fitted text; static text is localized through the host |
| `nineslice` | named image; `slice:[left,top,right,bottom]` source fractions <0.5, `border` in canvas pixels |
| `rect`, `circle`, `ring`, `line`, `triangle` | existing host shape primitives |
| `mask` | intersecting **rectangular** clip; not an arbitrary alpha/stencil mask |
| `focus` | host focus ring and translated children, optional `activate` and `nav` |
| `list`, `grid` | virtualized bound collection; `source`, `rows`, `columns`, `cell_w`, `cell_h`, `gap`, child template |
| `cover` | host cover cache by bound current library handle |
| `particles` | deterministic bounded host motes; removed entirely with Reduce motion |

Common geometry uses `x,y,w,h`; `alpha`, `scale`, `color` (24-bit RGB), `radius`, `visible`
and optional `state` control presentation. Group/focus children use local coordinates. Mask
children keep the parent's coordinate system. `list` uses one column; `grid` defaults to four.
Rows default to two. Nested collections are refused. The host supplies focused item framing;
selection follows stable handles when the model changes and is clamped if an item disappears.
Empty collections cannot dispatch their item action.

Text/cover `bind` and collection `source` use JSON pointers rooted at `library`, `home`,
`profiles`, `status`, `preferences`, `game`, `item` (template row), or `selected` (focused collection
row). `game` retains the last library selection across workflow screens; its settings, mods
and cheats are queried by a current host-validated handle, not a theme-supplied file path.
Examples: `/library/games`, `/item/name`, `/item/handle`, `/game/handle`, `/status/clock`,
`/status/controllers`. The same immutable host model vocabulary as Lua packs is used. Missing
bindings render empty rather than exposing native pointers or arbitrary paths.

`nav` maps `up/down/left/right` to node IDs. D-pad navigation inside collections is host-owned;
edge navigation follows explicit neighbors, with bounded document-order fallback. `activate`
or `input` has exactly one `screen`, `state`, or allowed `action`. `input` uses semantic keys
`select/back/secondary/settings/options/l1/r1`; physical menu remapping remains host-owned.
Action arguments can bind a selected field with `{"$bind":"/selected/handle"}`. Expressions,
dynamic resource names, shaders, scripts and native modules are unsupported and rejected.

Allowed actions are enumerated in `pe/ui/scene/model.hpp`; they reuse existing validated
Services operations. Launch requests use the existing profile/library checks and save-sync
lifecycle. The host shows save-conflict/failure choices and only commits a ready launch.
Destructive, save, profile-management and mod/cheat actions require a **host-drawn, host-input confirmation**;
the theme cannot supply `confirmed` or answer that dialog. Profile deletion/rename captures
and re-resolves its stable handle before confirmation. A selected game handle is revalidated
by the existing host action at execution. Themes cannot invoke `launch.commit` themselves.
`preferences.adjust`/`game_settings.adjust` are host-bounded, immediate setting controls:
toggle booleans or cycle a whitelisted native choice, with fixed volume steps and native
labels. They cannot edit paths, arbitrary fields or confirmation state. Text `labels` maps
at most 32 primitive values to localized display strings (for example boolean On/Off).

Each animation track has `node`, `property` (`x/y/alpha/scale`), `trigger` (`enter`, `focus`, or
`state:<id>`), `duration`, optional `loop`, and ordered `keys:[{"at":0,"value":...},...]`
including 0 and 1 endpoints. `interpolation` defaults to `linear`; `step` holds the previous
key; `hermite` uses each key's bounded `tangent` per normalized duration. Projection clamps
curve values to the property's resource bounds. Reduce motion always chooses the
last key, removes particles and makes screen transitions immediate, including after a live
preference change. Larger text and High contrast override theme text/focus presentation;
high contrast adds solid backing behind text. Themes cannot turn those preferences off as
presentation data. Optional Lua event hooks are **not implemented**.

## Limits and trust boundary

Resource reads pin the package directory and each component using `openat`/`O_NOFOLLOW`,
then read from the verified regular-file descriptor. There is no check-then-`fopen` image
path in the scene loader. Paths must be relative, without parent components, backslashes,
colons or control characters. JSON duplicate members and nesting over 32 are refused.

- At most 16 screens, 512 static nodes total, node depth 12, 128 tracks and 64 keys/track.
- At most 64 visible collection items, 8,192 rows per model and 8,192 projected commands/frame.
  Expanded template cost is checked before load; runtime enforces the command bound again.
- At most 256 referenced resource reads and 128 MiB referenced encoded resources.
- JSON files ≤512 KiB; up to 32 images, each ≤4096², 96 MiB decoded RGBA total.
- Up to 32 WAVs, each ≤8 MiB, 32 MiB decoded PCM total; font ≤16 MiB, bounded atlas/counts,
  finite metrics and sorted/in-bounds glyph/kerning tables before the existing font loader.
- Text ≤4 KiB/node and ≤32 KiB/frame before glyph projection, binding pointers ≤256 bytes,
  action arguments ≤8 KiB/depth 8. Runtime text overflow falls back rather than allocating an
  unbounded glyph list; layout fitting is not treated as a memory-safety boundary.
- Shader/program selection is never a theme operation. All drawing uses the existing
  compiled OpenGL shader. Fonts, textures, clip stacks and audio owners remain host-owned.

Only referenced resources are read on load. Installation scans/bounds the entire supplied
folder; the existing launcher updater bounds the whole archive. A pack is presentation data,
not a trusted source of workflow authority. The native confirmation is the action boundary.

## Offline Themezer/NXTheme lite conversion

This section describes the separate Python author tool, not the console import workflow.

```sh
python3 -B tools/launcher/scene/themezer.py convert my-theme.nxtheme converted --id my-theme --lite
# A ZIP containing up to seven .nxtheme parts is also supported.
```

The parser reads actual `info.json` metadata (`Version`, `Target`, `ThemeName`, `Author`) from
legacy SARC (either byte order), Yaz0-compressed SARC, and modern ZIP containers. It validates
SARC offsets, extents, name hashes/duplicates; Yaz0 sizes/back-references; ZIP paths, modes,
compression, expansion and CRC. Version 1–17 and the seven real target IDs `home`, `lock`,
`apps`, `set`, `news`, `user`, `psl` are recognized. A pack rejects duplicate parts. Synthesized
screens retain these parts and have original ProsperoEden navigation, with a host-library
recovery screen even for a non-home single part. News/lock are wallpaper presentation, not
emulated Nintendo applets.

Supported wallpaper: `image.dds` DXT1/3/5, 1280×720; `image.jpg` through optional **Pillow** on
the authoring PC. PNG decoding is available through Pillow but icon-to-scene mapping is not
implemented. The dependency-free path and current tests use DDS. No Pillow is installed by
the tool; missing dependency/unsupported codecs fail explicitly. No Themezer network service,
account or download is required. Supply files you have permission to use.

`layout.json`/`common.json` are parsed as firmware-patch documents, not mistaken for standalone
scenes. Their pane/material/animation changes, and custom icon extras, require explicit
`--lite` consent to discard. Every omission is written to `import-report.json`; output is
always marked `mode:lite` and `firmware_used:false`, even for a wallpaper-only input. A
layout-only theme is refused. Raw BFLYT/BFLAN/BNTX, firmware archives, NCA/NSP/XCI/NSO, keys,
Lua and shaders are rejected even with `--lite`. Converted folders are never silently
overwritten. Imported artwork retains its own license; conversion does not grant distribution
rights. The repository contains only independently generated synthetic parser fixtures.

The Python tool remains deliberately lite; native functionality below does not make its
output faithful. Full Themezer fidelity and the complete Canopy reference remain incomplete.

## Console-native offline importer

From the host launcher picker, **Secondary** opens the native import workflow. No Python,
subprocess, network, keys, encrypted firmware extraction or theme code runs on the console.
Copy user-owned `.nxtheme` or ZIP-of-parts inputs to
`/data/prosperoeden/theme-import/inbox/`. Up to 64 source names are shown. Optional extracted
user-owned Yaz0/SARC firmware archives go in `theme-import/firmware/<target>.szs`; common
patches use `common.szs`. Originals are not copied into a distributed pack.

1. A worker reads and converts the bounded package without GPU/service access.
2. The host shows the original wallpaper preview, package SHA-256 and scrollable report.
   This preview is artwork, not an assertion of the original UI's appearance.
3. Ordinary Confirm cannot accept reported omissions. Secondary explicitly requests partial
   conversion; a **separate host consent screen** must then be confirmed. Back cancels.
4. A complete converted scene is staged, validated and synchronized. A new content-derived
   `nx-<sha256-prefix>` ID is reserved with exclusive `mkdirat`. Resources use exclusive
   temporary writes and atomic **no-clobber hardlinks**; the manifest is published last.
   A mount without hardlink support fails safely; overwrite-capable rename is never a fallback.
5. `import-pending.json` binds staged filenames to SHA-256 values. Startup can resume a
   partial resource copy or finish a published manifest, but will not overwrite conflicting
   bytes or follow resource links. An empty reservation before journal publication is inert
   and needs manual inspection. Staging is retained for diagnosis/recovery (maximum 64
   staged directories); there is no silent recursive cleanup of user data.
6. Installation preserves the selected launcher. Only a further explicit Confirm selects
   the imported scene. Built-in picker recovery remains host-owned. Converted data and
   reports stay in this console's data directory; no automatic upload/export is provided.

`nx_archive.hpp`, `nx_image.hpp`, `nx_layout.hpp`, `nx_animation.hpp`, `nx_import.hpp` and
`nx_install.hpp` are the same C++ backend used by the console launcher and focused host driver.
They read actual ZIP/SARC/Yaz0 metadata and DDS DXT1/3/5, PNG and JPEG from bounded memory
(vendored stb, no optional installation). Wallpaper dimensions must be 1280×720. Native
settings-icon DDS/PNG input is checked at 64×56 and mapped to a native settings focus control.
Explicit `apps.dds/png` and `user.dds/png` roles (16–256 pixels per dimension) map to the
native games/profiles screens, preserving aspect ratio. These are original native workflows,
not Switch applets; every remapping is reported and requires partial-import consent. Unknown
icon filenames are reported, never guessed from dimensions or silently dropped.

Firmware-backed supported logic, exercised with original synthetic fixtures:

- FLYT byte order, sections, parent hierarchy, pane names, flags/visibility, alpha, origin,
  translation, scale and size. Layout patch values **replace**, rather than add to, the
  corresponding base pane fields. Ancestor scale and origin are composed in layout space;
  host Y orientation and canvas scaling are applied only at projection.
- Versioned material headers, texture-name references, wrap values and texture SRT records;
  material color/reference/transform JSON edits resolve by exact material and texture name.
- BNTX little-endian `NX` texture tables, bounded 64-bit pointers, uniquely named single-layer
  2D base mips, pitch-linear and Tegra block-linear untile. RGBA8 UNORM and BC1/BC2/BC3 UNORM
  decode into ordinary RGBA. sRGB, other channel/component swizzles/codecs, arrays, volumes
  and unsupported tiling fail explicitly; no original BNTX is copied into the generated pack.
- A bounded single-sampler material subset bakes affine UVs, clamp/repeat/mirror, matching
  nearest/bilinear filter flags, translation/scale SRT, foreground/background colors and
  uniform vertex tint into ordinary TGA images. Rotated samplers, four-corner gradients,
  additional combiners/coordinate pipelines are reported, not approximated. Up to 32 images
  and 64 MiB decoded generated images; each baked pane is at most 2048×2048. This is static
  image reconstruction, not an original GPU material/TEV implementation.
- Binary FLAN section/entry/tag/key readers and the actual `AnimJson` replacement schema.
  Supported leaf-pane FLPA X/Y translation curves retain their **relative-to-layout** values,
  integer step keys or floating Hermite slopes. The scene Player executes them; Reduce motion
  settles them at the last key. Animated parents, rotations, competing tracks, other tags
  and qlaunch event/group scheduling are explicitly unsupported.
- Separate runnable **geometry-preview** screens retain patched pane bounds, supported
  material images and curves. Unsupported panes remain visibly diagnostic geometry, not
  substitutes for missing material, text or firmware art. Material records/patch results are
  retained in the report; full material blending, fonts, original applet bindings and firmware
  release compatibility fixes are not implemented. Firmware identity is its actual SHA-256
  and structural/version evidence; a firmware release is never guessed from a filename.

Missing firmware/layout/pane/material/texture/animation input fails precisely or appears in
the omission report. Raw firmware/assets inside the NXTheme input, executable code, shaders,
keys and encrypted NCA/NSP/XCI are rejected. Imported artwork retains its original license;
conversion grants no redistribution rights. No Nintendo asset or upstream source is bundled.

## Author preview and focused tests

The existing `tools/launcher/runtime-preview.sh` recognizes these packs via the same native
RuntimeLauncher integration: set `PE_LAUNCHER_DATA` to an isolated data root, `select <id>`,
`key <physical-button>`, `tick`, `capture` and `recreate` as documented for runtime launchers.
The continuation adds a complete linked preview and native-workflow regression driver.
The standalone `scene-check` validates resources without the UI kit or emulator.

Run inside the assigned Docker image/private volume, with a read-only source mount:

```sh
bash tools/launcher/scene/test.sh
bash tools/launcher/scene/render.sh
bash tools/launcher/scene/native.sh
bash tools/launcher/scene/frontend.sh
```

`test.sh` builds ASan/UBSan parser/controller/resource tests and `scene-check`, checks the two
integration translation units with `-Wall -Wextra -Werror`, then exercises synthetic imports
and safe installation. `render.sh` links only the scene projection and existing draw-list/
OpenGL batch sources, checking clipping, atlas UVs, nine-slice projection and reduced-motion
settling at 720p/1080p through Mesa. These are **host evidence**, not console qualification or
a visual approval of the full Canopy scene. `native.sh` builds the native backend with
ASan/UBSan and exercises real containers, firmware patch math, material edits, animation
projection, consent, staged publication and recovery. `frontend.sh` links the complete
RuntimeLauncher/builtin UI/GL/font/mixer and runs the real updater regressions. Dependencies
are pinned by `tools/deps.json` (HarfBuzz/ui-kit must already be fetched); no emulator or native
target is built. `build_frontend.sh` records compiler dependencies and refuses mixed scene
header layouts if a source edit occurs during compilation. Test evidence uses fresh folders.

Workstream replay (PowerShell; serialize runners sharing this private volume):

```powershell
docker run --rm --cpus 2 --memory 3g --network none `
  --mount 'type=bind,src=D:/leonardommello/PS5/ProsperoEden/.local/agent-worktrees/scene-themes,dst=/src,readonly' `
  --mount type=volume,src=prosperoeden-team-scene-themes-20261027,dst=/work `
  -e XDG_CACHE_HOME=/work/cache prosperoeden-host:features `
  bash /src/tools/launcher/scene/frontend.sh
```

Replace the final runner with `native.sh` or `test.sh` for those checks. `render.sh` uses
`--cpus 1 --memory 1g`. Linked captures, command/stderr logs and the binary/capture SHA-256
manifest are retained in `/work/scene-linked/evidence.*/report.json` and sibling directories.
All services that would launch/delete/transfer real games are simulated in the linked host
driver; GL, scene runtime, resource loader, updater and mixer are the actual implementations.

PS5 native compilation/linkage, hardware audio/context behavior, 4K five-minute timing (no
frame over 25 ms), memory/performance with large real libraries, full accessibility/localization
inspection and real user-firmware visual parity still need qualification.

## Provenance and localization (feature-local ledger)

Read-only format research on 2026-10-27: `exelix11/SwitchThemeInjector` at
`66ccc56c6370908cbc1b09059122004cbb6ee605`:

- `NxThemeTool/NxTheme.cs`: container members, image naming, wallpaper sizes, layout/icon roles.
- `SwitchThemesNX/source/SwitchThemesCommon/NXTheme.cpp`: legacy Yaz0/SARC vs ZIP and manifest fields.
- `SwitchThemesNX/source/SwitchThemesCommon/Common.cpp`: format version 17 and seven part IDs.
- `SwitchThemesCommon/Layouts/LayoutPatches.cs`: firmware pane/material/animation patch semantics.
- `Bflyt/{BflytFile,Pan1Pane,Pic1Pane,BflytMaterial}.cs` and
  `Bflan/{Bflan,BflanSerializer}.cs`: versioned field/offset and serialized key definitions.
- Themezer LayoutDocs, [PaiTags and targets](https://layoutdocs.themezer.net/guide/7-animations/paitags-and-targets):
  FLPA relative translation and tag/target vocabulary (consulted 2026-10-27).
- Root `LICENSE`: GPL version 2 text. No upstream implementation source or assets were copied
  or vendored; these files were consulted as format documentation. The independent reader,
  converter and generated original scene definitions are GPL-3.0-or-later with this project.

Existing ProsperoEden draw lists, WAV decoder, mixer, font renderer, Canopy colors and update
lifecycle remain under their existing licenses. Default font artwork is not duplicated into
the sample. Optional Pillow is external author tooling, not a shipped console dependency.

New host UI msgids for later aggregate catalog work (catalogs intentionally not edited):
`Confirm launcher action`, `This action may change or remove your data.`,
`Launcher action failed`, `Preparing game`, `Choose which save data to use`, `Use console save`,
`Use server save`, `Keep both unchanged`, `Save synchronization failed`, `Retry`,
`Play without synchronizing`, `Synchronizing saves`.
Example/imported static text is also passed through host translation, with fallback to the
literal pack text. Button-name hints follow the current physical mapping; translating every
composed native hint and all author text remains localization work. No POT/locale generated
files were changed.

Continuation-local UI text ledger (no aggregate catalogs edited):
`Import an offline NXTheme`, `Copy your .nxtheme or theme-pack ZIP to:`,
`No packages found. Back exits; reopen after copying your files.`,
`Reading bounded package and building import report…`,
`Installing validated staged package. Do not power off…`,
`Artwork preview — not faithful parity`, `Source SHA-256:`,
`No Nintendo assets or keys are supplied.`,
`Converted data stays in this console's launcher folder.`,
`Install PARTIAL conversion with the listed omissions?`,
`Install this converted artwork and original native workflows?`,
`Default`, `Unavailable`. Full literal/composed hints and errors live in
`scene/import_runtime.inc`, `scene/nx_*.hpp` and the example's screen JSON files;
paths/hashes/source metadata are data, not translated msgids. Translation coverage is not
claimed merely because literals pass through `tr()`.

## Initial increment evidence — 2026-10-27 (`b01f254`, `6a1bf96`)

Baseline: `953effe455387bc186b70a182a20a15354354f48`, branch `team/scene-themes-1006000`.
Docker image: `prosperoeden-host:features`, image ID
`sha256:e6137ec556fe8e63ebf25e7fabe36e573d089c469d0e9d391c3072f2ae04d551`.
Only private volume `prosperoeden-team-scene-themes-20261027` was used. Sources were mounted
read-only; parser/controller checks used 2 CPUs/3 GiB, GL checks 1 CPU/1 GiB; network disabled.

- `scene-check`: Canopy example, 3 screens, no bundled images/recordings; accepted.
- ASan/UBSan scene parser/controller/projection/resource checks: passed; 2,000 deterministic
  JSON/TGA/font mutations, no sanitizer finding. Checks include clip/transform/opacity
  stack balance, visible-item bounds, stable selection, removed-item handling, Reduce motion,
  strict JSON, no-follow paths, image/font/audio bounds and forbidden actions/fields.
- Both modified runtime/updater translation units: host `-fsyntax-only` with
  `-Wall -Wextra -Werror`; passed. This is not a native build or complete linked frontend run.
- Python import suite: four test groups passed, including 2,000 deterministic SARC/Yaz0/DDS
  mutations, both SARC byte orders, modern ZIP, seven-part packs, explicit lite omissions,
  unsupported/proprietary inputs and non-overwriting/link-rejecting installation. The generated
  seven-screen/seven-image package was also accepted by the native `scene-check` subprocess.
- Mesa/llvmpipe scene GL pixel tests: 1280×720 and 1920×1080 passed; each projected 14 instances
  in 3 draw batches, with checked mask containment, UV selection, single-application subtree
  opacity and settled motion. Synthetic
  geometry tests are not screenshots of the complete launcher, typography approval or a
  performance benchmark.

At the initial increment, visual review was **recapture**: complete 720p/1080p Canopy-scene captures and accessibility
variants have not been produced/inspected. No further agent was spawned under the workstream
restriction; no native-UI web detector was used. Source review preserved the incumbent's
`theme.cpp` base/panel/title/accent values and borrowed the existing host font. It did not
invent a new brand or canonize missing artwork/reference-screen parity as a design decision.
No PS5 qualification, full updater regression, firmware import, account/network service,
release/version modification, push or issue closure was performed.

## Remaining acceptance gaps (continuation)

| Requirement | Exact remaining gap |
| --- | --- |
| Full firmware-backed theme import | BNTX base-mip UNORM extraction/untile and a static single-sampler material subset are runnable. Complete formats/color management/material blending/TEV, proprietary fonts, windows/parts/text, animation parent composition and original event/group/app binding remain absent. Diagnostic screens are not faithful reconstruction. |
| Firmware compatibility | SHA-256/structural identity is supported; release compatibility profiles and automatic matching to the emulator's existing firmware store are not. The native workflow currently takes explicit user-extracted target SZS files. |
| Full lite import | Native wallpaper and explicit Settings/games/profiles artwork roles are usable; other applet mappings, full color extraction and complete original console-style reference parity remain incomplete. The separate PC tool remains lite. |
| Complete Canopy reference | Eleven usable workflow screens are not the entire builtin launcher; setup, library-source management, pairing/network/multiplayer, downloaded-cheat management and advanced system tools still use builtin recovery. No copied reference artwork is substituted. |
| Optional Lua event hooks | Scene packs remain script-free. No sandboxed optional scene event-hook contract is implemented. |
| Accessibility/localization | Host overrides are implemented and captured by the linked harness; full controller-remapping, all languages/long names and human accessibility approval still require review. |
| Console acceptance | Native PS5 linkage/device interaction and both reference themes' 4K five-minute ≤25 ms/frame qualification are unexecuted. Host GL/sanitizers do not qualify the console. |

Integration must preserve additive edits in `runtime_launcher.{cpp,hpp}` and
`host/runtime_preview.cpp`, plus the first increment's updater/capability files.
`headless/CMakeLists.txt` also adds `runtime_launcher.cpp` to the existing source-local
OpenSSL include list for the native importer's SHA-256; it does not change the emulator
build or introduce a new dependency. Cover Flow
renderer modes 9/10 and `Preferences.library_view` are untouched. No generated
`runtime_models.hpp` replacement, other worker cherry-pick, coordinator checkout edit or
aggregate documentation/catalog change is part of this workstream.

## Continuation evidence — 2026-10-27

- Native C++ importer: **25 checks passed**, including 2,000 deterministic mutations under
  ASan/UBSan. Mutation fixtures use a tiny complete DDS tile, then hit the normal 1280×720
  import-policy rejection; separate accepted full-size fixtures exercise actual conversion.
  This is bounded reader fuzzing, not a wall-clock throughput/performance claim.
- Synthetic FLYT/SARC little- and big-endian hierarchy/origin/scale patches, material
  color/reference/wrap/SRT edits, binary and `AnimJson` FLAN translation keys, actual Hermite
  Player projection and reduced-motion settling passed. These use no real firmware assets.
- New-ID native publication, duplicate/no-consent rejection, explicit partial consent, seven
  parts, Settings PNG mapping, and recovery at reservation/resource/manifest phases passed;
  differing resources and file links were refused without overwriting the destination.
- Launcher-updater regressions: **68 cases passed** in the linked-preview test environment,
  including scene install/rollback, staged cancellation and pre-replacement rejection of
  shader/missing-image/invalid-focus scenes. The script also passed its existing 34 bounded
  application-update compatibility checks and catalog regression checks.

The first linked-preview attempts exposed a mixed-header object cache (now guarded by a
scene-input fingerprint) and an incorrect assumption that the native mods/cheats query
returned a bare array (now unwrapped by name). A later deletion assertion assumed the
invented local sample had a downloadable source; the preview-only service now records a
successful deletion dispatch without touching real files. These failed attempts are not
counted as frontend passes. Native fuzzing initially timed out decoding thousands of full
wallpapers; the bounded tiny-tile mutation fixture above replaces that throughput workload.

- Linked frontend: **84 checks passed** against the final stable linked binary
  SHA-256 `efa245e3a566ca6f906af5ac39054545c9b90bcaf19edab6f35e02fe1b13174a`.
  The real RuntimeLauncher/builtin UI/GL/font/mixer ran with simulated services. Coverage
  includes saved scene startup, picker scene/builtin selection and failure recovery,
  force-builtin recovery, profile choose/rename/add/cancel-remove, host deletion consent,
  native preference edits, save conflicts/failure/cancellation, removed-game launch recheck,
  exactly-once `will_play`, actual EGL context destruction/recreation, menu-volume Off,
  queued PCM retirement and stop/drain before pack teardown. Every command had `GL=0` and
  the tested processes reported no ASan/UBSan finding.
- Native artwork, missing-firmware partial and synthetic firmware-backed workflows passed preview/context reload,
  omission-specific consent, cancelled installation, manifest-last install, unchanged
  selected-launcher config and separate explicit selection after installation.
- **81 complete-launcher PNGs** (54 at 1280×720, 27 at 1920×1080) and command/stderr logs are
  retained at `/work/scene-linked/evidence.dJXgkU/` in the private Docker volume and exported
  to `.local/scene-evidence-dJXgkU/` in this worktree. `report.json` binds every capture to
  its dimensions/SHA-256; the exported files' hashes were verified. Library/profiles/Settings
  include normal, larger-text, high-contrast, reduced-motion and combined variants; workflow,
  confirmation, picker/recovery and native import captures are also included.
- Batched visual inspection checked basic fit, readable focus/dialogs, the enlarged rail
  identity (no longer ellipsized), native value labels and explicit non-parity/partial-import
  messaging in representative 720p/1080p captures. Long game names are intentionally fitted
  with ellipses and the selected title remains available above the grid. This is **not**
  reference parity, all-locale/controller-remapping coverage or accessibility approval.
- The subsequent firmware-workflow expansion passed 83 structural checks but its geometry
  captures exposed an invisible same-filled child pane: retained `rect` nodes did not forward
  their declared `border`/`edge` to the existing native border primitive. That feature-local
  forwarding is corrected, with a new pixel regression and a required difference between
  animated and reduced-motion geometry captures. The earlier geometry images alone are
  **not** evidence of visible animation. No shared renderer/shader/mode was changed.
- Final `native-import-firmware/firmware-geometry-{motion,settled}.png` captures were inspected:
  the nested pane's contrasting native border is now visible and moves to its final X
  position under Reduce motion. The linked regression requires different captures; the
  backend separately checks the exact expected coordinates at mid-curve and settling.
  This demonstrates runnable synthetic FLYT/FLAN subset conversion, **not material fidelity**.
- Final parser/controller/resource rerun: **11 screens validated**, 2,000 scene mutations,
  both integration syntax checks and all four PC-lite Python test groups passed. Final Mesa
  pixel rerun, now including declared rectangle borders, passed at 1280×720 and 1920×1080
  (15 instances / 3 batches each).

## Composition handoff — callers, storage and msgids

### Callers and ownership boundaries

- Apply the scene workstream commits in order (`b01f254`, `6a1bf96`, `37ca7fc`, then the
  closing increment), without replacing other workers' versions of shared files wholesale.
  Integration itself is not performed by this workstream.
- `headless/prosperoeden/frontend.cpp` constructs `RuntimeLauncher` with eight arguments:
  Services, Textures, Fonts, GlBatch, first-start, data root, optional mixer, font configurator.
  This remains source-compatible. `host/runtime_preview.cpp` also retains its existing call.
  The added **ninth** argument is `pe::ui::ThemeImportStorage`; omitted means the local bounded
  filesystem reservation below. Rebuild callers when composing this C++ constructor change;
  this is not a binary ABI promise or a Lua launcher API/version change.
- Preserve the existing frontend loop: physical keys → `press`, `update`, `draw`, font atlas
  synchronization and batch draw; cues/menu-volume use the native UI bus. Output recreation
  releases pack textures before context loss, rebuilds fonts, calls `set_font_texture`, then
  `reload_pack_textures`. Stop/drain the frontend's audio thread before launcher/retired PCM
  owners are destroyed; keep the GL context valid while releasing pack textures. Do not
  replace the launch/save-sync/host-confirmation path with a direct game-start call.
- Keep `launcher-runtime.json`'s additive `themes.scene.v1`, updater pre-replacement validation
  in `launcher_updates.cpp`, the Lua guard in `runtime_update_view.inc` and the source-local
  OpenSSL include addition in `headless/CMakeLists.txt`. No new shader or renderer mode is
  required. Preserve Cover Flow modes 9/10 and other workers' `Preferences.library_view` edits.
- Parent coordination at `1c4bb61` / aggregate `1b63` and Settings fix `133f604` is not rebased
  into this exclusive tree. Preserve the parent's **17** builtin Settings categories and
  **16**-item grid pagination/previous positions, corrected network summary, USB/network/OAuth
  and native-hook integrations. This workstream changes its separate scene Settings JSON and
  runtime dispatch, not builtin `launcher.cpp` categories/grid; do not restore baseline totals
  while composing shared runtime/updater hunks. The parent owns minimal conflict resolution.
- `theme_import_status()` is read-only diagnostics, not a theme action or consent API.
  Import preview/consent/install/selection remain host-owned. No EdenServices, USB or shared
  admission implementation was edited for this adapter.

### Storage reservation adapter

The public type is in `headless/prosperoeden/pe/ui/theme_import_storage.hpp`:

```cpp
using ThemeImportStorage = std::function<std::shared_ptr<void>(
    const std::string& root, std::uint64_t bytes)>;
```

The coordinator supplies a function with this exact signature, wrapping its existing global
storage guard in an owning `shared_ptr`. Pass it as the ninth constructor argument:

```cpp
pe::ui::RuntimeLauncher launcher(services, textures, fonts, batch, first_start,
    Eden::kDataDir, audio_ready ? mixer.get() : nullptr, LoadLanguage,
    reserve_scene_storage); // coordinator-owned adapter, not an invented Services method
```

Contract for that adapter:

1. `root` is the host-selected data root, never a path from theme JSON. Validate/map it to the
   correct storage device/admission domain. A successful token owns a reservation for at
   least `bytes`; null or an exception denies installation. Do not return a non-owning/dummy
   token in production or fallback to the local ledger after a global denial.
2. The amount is `2 * sum(converted file bytes) + largest file bytes + 1 MiB`: retained stage,
   installed copy, publication temporary and metadata. Input/report bounds cap file count at
   128 and total converted bytes at 128 MiB. The complete install shares **one** token across
   staging and publication; destruction releases credit on success or exception. Published
   and staged disk bytes remain real usage after credit release; staging is not deleted.
3. Install invokes it on the async install worker. Startup recovery invokes it synchronously
   on the constructing thread, before writing recovered resources, using the same conservative
   bound calculated from staged files. It may run again after restart; it is not serialized
   into the recovery journal. The callback and token deleter must be thread-safe, perform no
   UI/GL calls, and keep referenced admission services alive until joined launcher destruction.
4. Standalone `nx::stage`, `nx::complete` and `nx::recover` also accept the optional callback.
   Callers using these directly must not split a production installation into unreserved
   phases. Prefer `nx::install(root, plan, consent, partial_consent, callback)`.
5. Without the callback, `nx_storage.hpp` checks `statvfs` capacity and tracks process-local
   credits for the exact root string with RAII. This supports existing callers and focused
   host use; it is **not** global admission across subsystems, root aliases or other processes.
   Native SDK availability of `statvfs` and console filesystem hardlinks remains unqualified.

### Exact importer literals for later msgid composition

Do not edit POT/catalogs during this handoff. In addition to the action/save-sync ledger above,
the host importer passes the following exact static strings through `tr()` (UTF-8 punctuation
is significant):

```text
Import an offline NXTheme
Copy your .nxtheme or theme-pack ZIP to:
Optional user-extracted firmware: theme-import/firmware/<target>.szs
No packages found. Back exits; reopen after copying your files.
Reading bounded package and building import report…
Installing validated staged package. Do not power off…
Artwork preview — not faithful parity
Source SHA-256:
No Nintendo assets or keys are supplied.
Converted data stays in this console's launcher folder.
Up / Down: report · Confirm: continue without omissions · Secondary: explicitly accept partial import
Install PARTIAL conversion with the listed omissions?
Install this converted artwork and original native workflows?
Confirm installs a NEW ID only. Existing launchers and your selection are never overwritten.
Installed. Confirm selects this launcher; Back keeps your current launcher.
Back: cancel / close · Options: built-in recovery picker
Finish the pending operation before importing a theme
Unsupported items remain. Review the report; secondary explicitly accepts a partial import.
```

`Unsupported / remapped items: <count>`, filename-prefixed diagnostic-screen titles and
`path: code`/detail lines are currently composed strings, **not** interpolated gettext msgids.
Do not invent `%s` catalog entries without also changing those call sites. Package names,
paths, hashes, firmware identities and author text are data. Static scene screen text/labels
live in `examples/canopy-scene/screens/*.json` and `scene/nx_import.hpp`; they fall back to
literal text through the same host translation path. Backend errors in `nx_*.hpp` also fall
back to their literal text. This ledger is an integration input, not localization completion.

### PS5 package blocker (reproduced, not a successful build)

In the exclusive tree, this bounded, network-disabled command exits **1** before compiling:

```powershell
docker run --rm --cpus 1 --memory 256m --network none `
  --mount 'type=bind,src=D:/leonardommello/PS5/ProsperoEden/.local/agent-worktrees/scene-themes,dst=/src,readonly' `
  prosperoeden-host:features bash /src/tools/build-package.sh release
```

Exact output: `cat: .local/headless-cache: No such file or directory`.
The worktree has the pinned host UI dependencies, but no prepared native dependency cache or
PS5 payload SDK/sysroot (`.deps/pacbrew-0.40.2/opt/ps5-payload-sdk/.../libcrypto.a` is also
absent). `build-package.sh` obtains `PS5_PAYLOAD_SDK` from that cache; a Linux host preview
cannot supply it. No shared/coordinator cache, native emulator build, dependency installation,
version/catalog modification or release was attempted to bypass this boundary. The native
owner must prepare the approved SDK/cache, compose the callers/global reservation adapter,
then perform native compile/link and device/filesystem qualification separately. Until then,
the runnable build/evidence from this workstream is **Linux host frontend only**, not a PS5
package or #173/#239 acceptance.

## Closing increment evidence — 2026-10-28

- Native C++ backend finished **50 checks**, including 2,000 BNTX mutations plus 2,000
  archive/metadata/image mutations under ASan/UBSan. Both FLYT byte orders produce checked
  baked pixels; eight pitch/block-linear RGBA8/BC1/BC2/BC3 combinations match exact expected
  RGBA hashes. Unsupported pointer/swizzle/sRGB/tile/size/codec/block-height and ambiguous
  names are rejected. Patched colors/vertex tint/repeat/SRT affect real generated image bytes;
  unsupported gradients/rotated samplers are reported. Storage denial precedes staged/live
  resource writes; a shared credit releases on success and failed staging, local overcapacity
  is refused, and recovery denial preserves its journal until a subsequent successful retry.
- Complete Linux linked frontend built and finished **110 checks**. Updater finished **68
  cases**, its **34** existing application-update compatibility/bounded-JSON checks and catalog
  regressions. No test still running is counted here. Known vendored stb/self-update compiler
  warnings remain; the focused integration syntax checks separately use `-Werror`.
- Final linked binary SHA-256:
  `81051d6b321ffcbfb27db2aa779fa521f8042020a89460dbafd038c7512fe783`.
  Source, real GL/font/mixer and simulated game services have the same scope as the earlier
  linked evidence. This is an ASan/UBSan Linux test frontend, not a PS5/release artifact.
- **99 PNGs** (63 at 1280×720, 36 at 1920×1080), `report.json`, command/stderr logs and the
  executable were exported to `.local/scene-close-20261028/`; all exported capture hashes and
  the executable hash were verified against the report. Canonical volume evidence is
  `/work/scene-linked/evidence.hDbAxq/`; host export is its `evidence.hDbAxq/` subdirectory.
- The added firmware-material workflows exercise actual BNTX decoding/static material baking,
  report/partial consent, install, explicit selection, leaf FLAN motion, Reduce motion and
  real EGL context recreation at both resolutions. Representative material/report/native
  icon-role images were inspected: synthetic colored pixels and diagnostic pane bounds are
  visible after reload. The source diagnostic border intersects its title; these diagnostic
  screens are **not** a reference-design/accessibility approval. Synthetic solid-color icon
  tiles establish role mapping, not authentic reference artwork or full icon coverage.
- Final parser/controller/resource run validated **11 screens**, passed **2,000 scene
  mutations**, both runtime/updater `-Werror` syntax checks and all **four** PC-lite test groups.
  Final GL pixel tests passed at **1280×720 and 1920×1080**, each with 15 instances / 3 batches.
  Reproducible runners are listed above; exported logs are `native.log`, `frontend.log`,
  `parser.log` and `render.log` under `.local/scene-close-20261028/`.
- No parent/coordinator HEAD was built or integrated here. No publication, native compile,
  PS5-live operation, credentials, IME review, aggregate catalog change or issue closure is
  part of this closing increment. The parent runs its final composed-HEAD gates; this workstream
  supplies the ordered commits, exact adapter contract and explicit acceptance gaps above.
