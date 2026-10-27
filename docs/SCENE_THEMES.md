# External scene themes — experimental implementation

This workstream adds a data-only scene renderer and an offline **lite** NXTheme converter.
It does **not** complete issue #239: firmware-layout reconstruction, console-native import,
complete reference-theme parity and PS5 qualification remain outstanding. Do not describe a
wallpaper conversion as a faithful Switch HOME-menu import.

## Players

Scene packs share `/data/prosperoeden/launchers/<id>/` and **Appearance > Launcher** with Lua
packs. They do not alter the compiled **Theme** selector. `manifest.json` retains the
`launcher-v1.0` contract/API 1.1, declares `themes.scene.v1`, and names `"scene":"theme.json"`
instead of `entry`. Old hosts refuse the required capability rather than trying to run the
pack as Lua. Existing Lua packs do not need to change.

`examples/canopy-scene` is an installable experimental library/profile scene using the existing
Canopy palette, rail and host typography. It is **not** a full replacement for every built-in
screen or Canopy's artwork. Its Settings page explicitly opens the built-in launcher. No
Nintendo artwork, game covers, keys or firmware are included.

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
`profiles`, `status`, `preferences`, `item` (template row), or `selected` (focused collection
row). Examples: `/library/games`, `/item/name`, `/item/handle`, `/status/clock`,
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
Mutating actions other than profile selection require a **host-drawn, host-input confirmation**;
the theme cannot supply `confirmed` or answer that dialog. Profile deletion/rename captures
and re-resolves its stable handle before confirmation. A selected game handle is revalidated
by the existing host action at execution. Themes cannot invoke `launch.commit` themselves.

Each animation track has `node`, `property` (`x/y/alpha/scale`), `trigger` (`enter`, `focus`, or
`state:<id>`), `duration`, optional `loop`, and ordered `keys:[{"at":0,"value":...},...]`
including 0 and 1 endpoints. Values interpolate linearly. Reduce motion always chooses the
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

**Not implemented:** firmware identity matching, reading the player's firmware layouts,
complete layout/material/animation conversion, custom-icon mapping, console-only retention
of converted data, console-native import UI, full Themezer fidelity and the complete Canopy
reference. The converter is currently PC/offline tooling, so it does not satisfy the
console-local firmware-based import acceptance criterion. The generic original lite scene
is not advertised as Nintendo's console UI.

## Author preview and focused tests

The existing `tools/launcher/runtime-preview.sh` recognizes these packs via the same native
RuntimeLauncher integration: set `PE_LAUNCHER_DATA` to an isolated data root, `select <id>`,
`key <physical-button>`, `tick`, `capture` and `recreate` as documented for runtime launchers.
That full interactive integration has **not** been executed in this isolated workstream.
The standalone `scene-check` validates resources without the UI kit or emulator.

Run inside the assigned Docker image/private volume, with a read-only source mount:

```sh
bash tools/launcher/scene/test.sh
bash tools/launcher/scene/render.sh
```

`test.sh` builds ASan/UBSan parser/controller/resource tests and `scene-check`, checks the two
integration translation units with `-Wall -Wextra -Werror`, then exercises synthetic imports
and safe installation. `render.sh` links only the scene projection and existing draw-list/
OpenGL batch sources, checking clipping, atlas UVs, nine-slice projection and reduced-motion
settling at 720p/1080p through Mesa. These are **host evidence**, not console qualification or
a visual approval of the full Canopy scene. The repository-wide launcher updater test needs
`wav.cpp` now because scene update validation decodes audio; its test script includes that
link dependency, but the full updater suite was not run here.

PS5 native compilation/linkage, audio/picker/confirmation interaction, context loss/teardown,
4K five-minute timing (no frame over 25 ms), memory/performance with large real libraries,
full accessibility/localization inspection and firmware imports need further qualification.

## Provenance and localization (feature-local ledger)

Read-only format research on 2026-10-27: `exelix11/SwitchThemeInjector` at
`66ccc56c6370908cbc1b09059122004cbb6ee605`:

- `NxThemeTool/NxTheme.cs`: container members, image naming, wallpaper sizes, layout/icon roles.
- `SwitchThemesNX/source/SwitchThemesCommon/NXTheme.cpp`: legacy Yaz0/SARC vs ZIP and manifest fields.
- `SwitchThemesNX/source/SwitchThemesCommon/Common.cpp`: format version 17 and seven part IDs.
- `SwitchThemesCommon/Layouts/LayoutPatches.cs`: firmware pane/material/animation patch semantics.
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

## Workstream evidence — 2026-10-27

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

Visual review remains **recapture**: complete 720p/1080p Canopy-scene captures and accessibility
variants have not been produced/inspected. No further agent was spawned under the workstream
restriction; no native-UI web detector was used. Source review preserved the incumbent's
`theme.cpp` base/panel/title/accent values and borrowed the existing host font. It did not
invent a new brand or canonize missing artwork/reference-screen parity as a design decision.
No PS5 qualification, full updater regression, firmware import, account/network service,
release/version modification, push or issue closure was performed.
