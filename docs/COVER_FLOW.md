# Cover Flow library view (#133)

## Use and compatibility

Choose **Settings → Appearance → Library view → Cover Flow**. **Grid** is still the default.
The preference is stored as `launcher.library_view` in the existing JSON settings store;
accepted values are `grid` and `cover-flow`. Missing, unknown, and incorrectly typed saved
values read as Grid. A failed settings write rolls the visible choice back.

Left/Right moves one game at a time, without wrapping. Left at the first game opens the section
rail (or leaves the library panel in Slate); Up/Down does not skip games. Cross, Triangle,
Square, Circle, section switching, remapped face-button hints, game settings, mods, download
sources, homebrew launch, and the existing save-sync/launch checks keep their established paths.
Returning from Home or game settings retains the selection while the launcher is alive; rescans
follow the selected file when other files move/disappear. A newly constructed launcher reads
the saved **view**; this does not add a second persistent per-game selection/history store.

Lua launchers receive the additive `library_view` string in `host.query("preferences")` and may
set it with `preferences.set`. Invalid values are rejected. Legacy patches to unrelated fields
preserve it. This selects the builtin presentation; it does not override a Lua pack's own layout
or introduce a new launcher API version, drawing command, or game-launch contract.

## Rendering and resource bounds

- `pe/ui/cover_flow.hpp` owns camera/layout math, depth order, the visible window and motion.
- `pe/ui/cover_flow.inc` draws that stage from the existing library games and selected index.
  It is included by `library.cpp`, so the native source list needs no additional target.
- The existing instanced OpenGL batch gains two shape modes: **9** (perspective image) and
  **10** (perspective fallback fill). No separate renderer/context, web layer, geometry upload
  path, or depth-buffer dependency is introduced; `Instance` remains **96 bytes**.
- Covers have real Y-axis rotation and camera distance. Each edge is divided by its camera
  distance, and that distance is retained in clip-space `w` for perspective-correct UVs.
  Far-to-near painter order supplies occlusion. These are not scaled horizontal rectangles.
- At most **nine** covers are visited/drawn, independent of library size. The selected cover
  is requested first. Covers use the incumbent bounded lazy cache (160 entries, existing
  two-cover pump budget); no additional texture ownership or asynchronous lifetime is added.
  Selection changes discard only obsolete **unloaded** requests before the next window queues;
  repeated rapid bursts cannot accumulate a stale image-loading FIFO or starve the current cover.
- Motion uses a monotonic frame-rate-independent response, a maximum two-item catch-up gap,
  no queued navigation jobs, and a float-precision-aware settle threshold for very large lists.
  Reduced motion snaps selection position immediately; static perspective remains visible.
- Missing or unreadable art uses the same projected surface, the existing library icon, and
  readable fallback text. Names, metadata, position, and actions remain front-facing. The
  selected surface has an explicit outline, strengthened with high contrast; large-text
  support and palette/font roles come from the current theme.

## Direction contract and review scope

**Mode:** Operate/Experience, a controller-operated game library viewed from a couch.
**Thesis:** let owned cover art lead through a focused, dimensional browsing stage, without
replacing ProsperoEden's navigation or information hierarchy.
**Own world:** inherit Canopy/Slate inks, Montserrat, the rail/tabs, footer and game details.
No rebranding, new palette, borrowed web components, or new illustrative shipping assets.
**Story:** the centered cover, outlined choice and readable title identify the selected game;
the existing controller actions open its options or start it.
**First viewport:** preserve the Library header/footer and selected-game details; replace only
the covers region with a central face-on cover and yawed, smaller, depth-ordered neighbours.
**Form:** the user-requested Cover Flow structure, an extension of the established native UI;
no alternative-world/concept tournament was appropriate.
**Finish:** inspect actual renderer frames at 1280×720 and 1920×1080 with synthetic sample
art, alongside Grid, missing art, empty state, Slate and accessibility captures. The user
forbids extra agents, so finish review is in-thread rather than independently delegated.
The HTML/CSS detector is inapplicable to this native C++/GL surface.

## Focused verification

`tools/launcher/test-cover-flow.sh <output>` builds **only the launcher preview and tests**.
It exercises the real `Launcher::press/update/draw`, services contract, draw-list batch and
GLSL shaders with synthetic metadata/art, plus the actual JSON store and Lua runtime host.

Reproduction environment: Docker image `prosperoeden-host:features`, private volume
`prosperoeden-team-cover-flow-20261027`, 2 CPUs, 3 GiB memory, 256 PIDs. Source is copied from
this worker's read-only worktree mount into the private volume. No coordinator/release/launcher
shared volume, WSL installation, full emulator/native build, game payload or console is used.

### Final result (2026-10-27)

The focused Docker suite passed at **1280×720 and 1920×1080**, including:

- Camera projection/depth order, finite bounded motion, nine-cover window and 100,000-index math.
- Real builtin controller dispatch: navigation bounds, rapid bursts, returning to Library,
  selected-game options, mods, remote queue/cancel/download-before-launch and homebrew launch.
- Settings controller toggle, write-failure rollback, launcher reconstruction, JSON defaults,
  invalid saved values and unrelated preference preservation.
- Cancellation of 600 obsolete queued covers before any stale image read.
- Empty, single/missing-cover and 5,000-game libraries, changed-list selection identity,
  Slate's panel, reduced motion, high contrast and large text.
- Existing preferences-store/game-name regressions, theme-role check, read-only discovery of
  all five msgids, and the real Lua host's query/set/type/value/legacy-patch/builtin-return checks.

All **18 captured frames** reported `GL=0`. Counts below are the same at both resolutions;
they establish bounded rendering work, **not** a console frame-rate or memory benchmark.

| Captured state | Total instances | Draw calls | Projected instances |
| --- | ---: | ---: | ---: |
| Grid (preserved) | 796 | 31 | 0 |
| Cover Flow, populated | 530 | 16 | 17 |
| Cover Flow, 5,000 games/reduced motion | 469 | 9 | 18 |
| Cover Flow, Slate | 403 | 13 | 17 |
| Single missing cover/accessibility | 514 | 6 | 1 |
| Empty | 311 | 6 | 0 |

A textured cover uses two projected instances (backing and image), hence the 18-instance
ceiling for nine covers. The log retains incumbent stb initializer and runtime-preview
indentation warnings; there were no feature compile errors.

### Evidence and finish review

Committed, unmodified captures from the final run:

- [Cover Flow, 720p](evidence/cover-flow/cover-flow-720p.png)
- [Cover Flow, 1080p](evidence/cover-flow/cover-flow-1080p.png)
- [Missing cover, reduced motion, high contrast and large text](evidence/cover-flow/fallback-accessibility-1080p.png)
- [Slate, 720p](evidence/cover-flow/slate-720p.png)
- [Appearance setting, 720p](evidence/cover-flow/appearance-720p.png)
- [Preserved Grid, 720p](evidence/cover-flow/grid-720p.png)
- [Complete focused test output](evidence/cover-flow/test-output.txt)

The committed log removes only a trailing space on `READY builtin` for Git whitespace
hygiene. The raw `build/cover-flow/test.log` SHA-256 is
`eed9db38b5e8be8e40d683b3e18b5e61a72eda636df5842419a150299777b743`.

The complete two-resolution capture sets also remain in the ignored `build/cover-flow/`
folder and the assigned private volume's `/work/evidence`. These are actual native GL
renderer frames, not mockups. Art and game metadata come solely from the existing synthetic
`FakeServices` samples; no game payload, keys, or external cover assets are included.

Bounded in-thread finish review: an initial batched inspection and one final confirmation
across both resolutions, Grid, setting, Slate and accessibility states. Perspective edges,
occlusion, selected outline, text hierarchy and controller hints are legible; no material
feature visual defect remains. No independent reviewer was spawned, per the user's constraint.

Host GL success does **not** establish PS5 GPU-driver compatibility, physical-controller
transport, console performance, or rest-mode texture lifetime; those remain console
qualification checks. No full emulator/native build was run. The inherited single-game
header pluralization is unchanged and outside this presentation feature.

## New translation msgids (catalog integration owned by coordinator)

- `Library view`
- `Cover Flow`
- `Grid`
- `Browse covers in a grid or in perspective with Cover Flow.`
- `Cover unavailable`

No POT or language catalogs were edited in this workstream.

## Shared-file integration notes

Shared files requiring additive conflict resolution if another worker edits them:

- `headless/settings_store.h`
- `headless/prosperoeden/eden_services.cpp`
- `headless/prosperoeden/pe/ui/services.hpp`
- `headless/prosperoeden/pe/ui/runtime_models.hpp`
- `headless/prosperoeden/pe/ui/runtime_launcher.cpp`
- `headless/prosperoeden/pe/ui/settings.cpp`
- `headless/prosperoeden/pe/ui/launcher.hpp`
- `headless/prosperoeden/pe/ui/launcher.cpp`
- `headless/prosperoeden/pe/ui/library.cpp`
- `headless/prosperoeden/pe/ui/home.cpp`
- `headless/prosperoeden/pe/ui/textures.hpp`
- `headless/prosperoeden/pe/ui/textures.cpp`
- `headless/prosperoeden/pe/gfx/draw_list.hpp`
- `headless/prosperoeden/pe/gfx/draw_list.cpp`
- `headless/prosperoeden/pe/gfx/gl_batch.cpp`

The texture hunks add only pending-cover cancellation; loaded texture/LRU ownership is unchanged.
Scene-theme integration should preserve its own additions while retaining the new preference,
the two reserved shape modes (9/10), and the Slate home-panel delegation to library controls.
Do not regenerate `runtime_models.hpp` wholesale during cherry-pick: unrelated omissions in
the incumbent generator output predate this feature; the new field is an additive adapter entry.
