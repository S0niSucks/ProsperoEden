# Native UI adaptation plan — Encore × ps5-homebrew-ui

Date: 2026-10-31. **Read-only integration research, not an implementation, execution result, or formal egm-review.**

## Recommendation

Keep Encore's launcher, renderer, multilingual font engine, services, input paths and game handoff. Adapt the reference kit's **theme-as-data, count-backed grid, content-space highlight, visible-cell drawing and independent screen-design concepts** into that launcher. Do not replace it with Homebrew UI Lab.

Ship three separately selectable appearance controls:

1. **Interface:** Encore / Switch / Library Hub.
2. **Colours:** Interface default / Eden / Midnight / Paper.
3. **Library view:** Interface default / Showcase / Cover grid / Details list.

Encore and Switch remain available with their current default compositions. Library Hub is a genuinely different, library-first home with a collection and a selected-game detail pane, not another colourway of the current cinematic Home. Grid and list are real navigable collection implementations, not pictures or a differently coloured five-cover shelf. All combinations must be supported; choosing Paper must not select Switch, and choosing a grid must not change the controller profile.

**Fix the existing FSR/AA service-read omission first:** an unrelated appearance save can otherwise overwrite already saved global video choices.

## Scope and evidence conventions

- `E/` = `D:/leonardommello/PS5/ProsperoEden-Encore/`.
- `H/` = `D:/leonardommello/PS5/ps5-homebrew-ui/`.
- To keep repeated body citations readable, `E/.../ui/`, `E/.../gfx/` and `E/.../core/` expand respectively to `E/headless/prosperoeden/pe/ui/`, `E/headless/prosperoeden/pe/gfx/` and `E/headless/prosperoeden/pe/core/`. `E/.../host/` expands to `E/headless/prosperoeden/host/`; `E/.../eden_services.cpp` means `E/headless/prosperoeden/eden_services.cpp`. Bare filenames in a paragraph retain that paragraph's explicit path.
- Citations are **1-based source lines** from the files actually delivered by the read tools. Symbol names are included so later edits can locate bodies if lines move.
- Applicable instructions read before source: `D:/leonardommello/PS5/AGENTS.md` and `H/AGENTS.md` in full. No `E/AGENTS.md` exists. Ancestor and scoped nested AGENTS discovery found no additional instructions in the inspected launcher/UI/app/host/docs paths. In particular, `H/AGENTS.md:17–46,148–206` and `H/docs/ADOPTING.md`, `H/docs/CRAFT.md` governed reference interpretation.
- Serena's initial instructions were read and its tools discovered. `get_symbols_overview` returned **No active project; known projects []**. No activation, onboarding, project configuration generation, or memory access was attempted; the task-authorized read-only filesystem fallback was used.
- Local knowledge context lookup for `ProsperoEden` returned no established context or suggested sources. Conclusions below come from current permitted source, not an assumed historical project context.
- No vendor/third_party source, `.serena` memory, private configuration, dependency source, console asset extraction, build, test, container execution, package installation, hardware action, or network operation was performed. References to dependencies in first-party include/build files were read; their target source files were not opened.
- Only Docker **image inventory, selected non-secret image metadata, and image history** were inspected. No container was started. Only this report was written. Graph generation, skill launchers, browser inspectors and other workflows requiring extra writes/execution were not run under this narrower assignment.

## 1. What the two codebases actually provide

| Concern | Encore, observed source | Reference kit, observed source | Consequence |
|---|---|---|---|
| Application owner | `Launcher` owns one screen/modal state machine, async jobs, settings, selection and launch state (`E/headless/prosperoeden/pe/ui/launcher.hpp:22–36,237–377`). | A shell owns multiple independent `Concept` instances; a concept gets a demo catalogue and settings via `Context` (`H/src/app/concept.hpp:56–89,118–143`; `shell.hpp:102–123`). | Borrow the separation of design and style, not the app shell or demo catalogue. |
| Current interfaces | `prefs_.ui_theme == 1` selects Switch input/render branches, modal skins and background; otherwise Encore (`E/.../ui/launcher.cpp:435–451,593–609,1181–1238`). | `Theme` is data; a `Concept` is a complete screen, a separate contract (`H/src/ui/theme.hpp:51–114`; `src/app/concept.hpp:118–138`). | Encore currently calls an **interface** a theme. Introduce separate palette and collection-view preferences rather than expanding this ambiguity. |
| Encore Home | Already full-screen game hero, scrims, compact title/description/actions, with real title metadata (`E/.../ui/home.cpp:565–734`). | Reusable cards, grid/list, highlight, painter and motion primitives exist in `H/src/ui/`; they do not require adopting a particular demo screen. | Merely increasing glow or changing this existing hero's colour does not satisfy the new interface requirement. |
| Encore Library | Five portrait retail sleeves plus one large detail surface, rear-cover focus and full-screen zoom/pan (`E/.../ui/library.cpp:1369–1595`). | `GridView` supports count-only data, independent geometry/style, retained column across short rows, edge exits, scrolling and visible-cell iteration (`H/src/ui/components/grid.hpp:15–43,58–117,151–168`; `grid.cpp:146–210,378–427`). | Implement a collection view from these actual algorithms, retaining Encore's authoritative games and artwork. |
| Switch | A real alternate HOME rail/shortcuts, 18-position six-column library page, settings/sidebar, advanced modal family (`E/.../ui/switch_theme.cpp:255–521,782–893`). | Component styles are replaceable without replacing selection/state (`H/src/ui/components/component.hpp:5–20,87–103`). | Preserve Switch geometry and original drawn assets. Its interface-default library must still use the existing Switch grid; a new explicit grid/list choice can override the library only. |
| Text | Montserrat plus system fonts, BiDi, HarfBuzz, dynamic atlas updates, fitted/wrapped UTF-8 (`E/.../gfx/font.hpp:32–41`; `font.cpp:305–456,462–523,618–632,761–791`). | Six `FontRef`s, theme-selected font roles, ASCII uppercase helper (`H/src/ui/fonts.hpp:20–61`; `widgets.cpp:95–192`). | Keep Encore text completely. Do not swap it for kit fonts or case conversion. |
| Persistence | Native JSON via `EdenServices` and `settings_store.h`; fake preview service stores a whole struct in memory (`E/.../eden_services.cpp:1973–2025`; `E/headless/settings_store.h:294–401`; `E/.../host/fake_services.hpp:74–82`). | Shell persists its own encoded settings after a delay (`H/src/app/shell.cpp:81–106,157–168`). | Do not import the kit save format or delayed-success model. Keep native save failure/rollback semantics. |

The reference explicitly supports **read the ideas and write your own** as the lightest adoption mode (`H/docs/ADOPTING.md:8–12`). Its later adoption account explicitly warns that baked fonts failed on real multilingual names, and cites ProsperoEden's own engine as the replacement (`:117–129`). That is evidence against replacing Encore's text stack, not an invitation to copy the reference's entire `src/gfx/`.

## 2. Concrete existing preference defect — FSR sharpness and anti-aliasing

The missing round trip is specifically **native store → UI service DTO**, not missing JSON persistence:

- `E/headless/settings_store.h:105–106` defines authored defaults; `LoadPreferences():313–316` loads `/video/fsr_sharpness` and `/video/anti_aliasing`; `SavePreferences():363–364,380–381` validates/writes both.
- `E/.../ui/services.hpp:154–155` gives the UI DTO defaults `50` and `1`.
- **`EdenServices::preferences()` (`E/.../eden_services.cpp:1973–1998`) copies filter and then refresh, skipping both fields** at `:1981–1983`.
- `EdenServices::set_preferences():2009–2010` does copy both UI values back. Therefore reopening the launcher can show DTO defaults instead of saved custom values, and saving Appearance/Accessibility/Language can write those defaults over the actual saved video values.
- Per-game DTO conversion is already symmetric (`game_settings()/set_game_settings():1936–1970`); do not “repair” it by replacing overrides with global values.

Required initial change in `EdenServices::preferences()`:

```cpp
result.filter = saved.upscaling_filter;
result.fsr_sharpness = saved.fsr_sharpness;
result.anti_aliasing = saved.anti_aliasing;
result.refresh = saved.refresh;
```

Carry the two new appearance fields through **both** preference structs and **both** service conversion directions at the same time. A FakeServices screenshot cannot prove this fix: its setter/getter copy the struct directly and bypass the native conversion and JSON.

Future regression case, not run here: persist FSR=35 and AA=2 with a valid Custom video configuration; construct/read native UI preferences; change only the palette; save; reload native settings and verify FSR=35, AA=2, renderer/output/resolution/filter/refresh/mapping/language unchanged. Also exercise switching an authored tier and a manual override. Profile detection itself considers FSR and AA (`E/headless/settings_store.h:129–138`), so asserting only the visible preset label is insufficient.

## 3. Minimal preference contract and migration

### 3.1 Keep the legacy field; clarify its meaning

For the smallest compatible source change, retain **`int ui_theme` as the historical interface selector** in both `Preferences` structs; update its comment and labels. Do not also introduce a second competing interface field. Append:

```cpp
int ui_theme = 0;       // interface: 0 Encore, 1 Switch, 2 Library Hub (legacy name)
int ui_palette = 0;     // 0 interface default, 1 Eden, 2 Midnight, 3 Paper
int library_layout = 0; // 0 interface default, 1 Showcase, 2 Cover grid, 3 Details list
```

Existing definitions: `E/headless/settings_store.h:97–121`, `E/.../ui/services.hpp:145–171`. Appending fields preserves earlier positional aggregate initializers better than inserting them among video/control fields.

Use one small first-party shared header, proposed `E/headless/appearance_preferences.h`, for stable key arrays, counts and validation/lookup helpers. It should depend on standard headers only, not `pe/gfx`, the kit or native platform APIs. Both the store and UI DTO can include it, following the already shared `button_mapping.h` arrangement in `ui/services.hpp:7–8,38–49`. Do not store numeric gallery indices or translated strings.

Recommended stable JSON:

```json
"appearance": {
  "interface": "library-hub",
  "palette": "midnight",
  "library_view": "list",
  "theme": "encore"
}
```

Keys for interface: `encore`, `switch`, `library-hub`; palette: `default`, `eden`, `midnight`, `paper`; library: `default`, `showcase`, `grid`, `list`.

Migration rules:

1. Recognized `/appearance/interface` is authoritative.
2. If that key is **absent**, migrate legacy `/appearance/theme == "switch"` to Switch, otherwise Encore. If canonical interface is present but invalid, use Encore rather than a stale legacy value.
3. Absent/unknown palette and library view resolve to `default` independently.
4. On save, validate all three ranges; write canonical keys. Keep `/appearance/theme` as a downgrade alias: `switch` only for Switch, otherwise `encore`. It is never the palette.
5. Keep existing additive JSON/version handling; `SavePreferences()` loads the existing document before editing it (`E/headless/settings_store.h:374–401`). No database or new preference file is needed.

`default` is important: a legacy Switch setting must keep light paper and its existing grid; legacy Encore must keep its Eden palette and Showcase shelf. Proposed Library Hub defaults are Midnight + Cover grid. **Explicit** palette/view choices apply across interfaces without rewriting one another.

### 3.2 Appearance UI

Replace the two “Encore/Switch” selection rows with three **property rows**: Interface, Colours, Library view. Up/Down and modal L1/R1 move between rows; Left/Right adjusts the focused value; Cross may advance one value; Circle closes. Merely focusing a row must not change or save a setting.

Exact existing sites requiring coordinated changes:

- `press_settings()` Appearance opening currently sets `option_` to the selected theme (`E/.../ui/settings.cpp:182–185`): instead start on property row 0; do not treat a preference index as a row index.
- `draw_settings()` summary and Appearance detail (`:206–216,288–293`).
- `dialog_rows(appearance)` currently returns 2 (`:383–405`): return 3.
- `dialog_row_top(appearance)` (`:416–417`): fit three readable rows plus description/preview within the existing modal.
- `press_dialog()` branch currently `prefs_.ui_theme = option_` (`:568–571`): cycle the appropriate property, not the row index.
- `draw_dialog()` Appearance rows (`:797–802`).
- Switch settings summaries (`E/.../ui/switch_theme.cpp:499–503`) and `draw_switch_dialog()` Appearance labels (`:814–817`) must use exactly the same three-property model and values. There is one underlying handler, not a second persistence implementation.

Player-facing descriptions: Showcase — “Large jackets and game details”; Cover grid — “More games at a glance”; Details list — “Names and details side by side”. Translate through existing `TR`/`tr`, not reference-kit ASCII uppercase. A compact preview must be drawn with the native painter from a **local palette argument**, not by mutating a global active theme.

Maintain save failure rollback: current `press_dialog():466,602–614` snapshots `before`, restores preferences on failure, then reapplies look. Apply new appearance resolution **after a successful save**, or restore both preference and derived look on failure. Do not say “Saved” for preview-only changes.

An appearance edit does not change effective docked/video defaults. `Launcher::save_preferences():236–251` currently clears the home settings cache and schedules a full library scan after *every* save. A minimal compatible refinement is an optional `game_defaults_changed = true` parameter; Appearance calls it with false, existing callers retain the default. This avoids scanning mods/ROM metadata when cycling colours, without changing any actual video-setting save behavior. Keep the service setter as the owner of disk operations.

## 4. Native C++ implementation shape

### 4.1 Palette: adapt the data model, not thirty renderer-dependent themes

Introduce a compact `UiPalette` and `SurfaceTokens` in `E/.../ui/theme.hpp` plus proposed `palette.cpp`. Resolve from `(ui_theme, ui_palette, high_contrast)`; **do not mutate namespace globals** when the selection changes.

Minimum roles: page, panel, raised/row, selected row, page text, panel text, muted text, outline, focus, primary action, on-primary, warning, scrim, plus **on-image text/scrim**. Surface knobs can remain small: flat vs existing glass, radius, border/focus width, shadow amount. Start with the three palettes above, not every SurfaceStyle in the kit.

Reference source basis: `H/src/ui/theme.hpp:60–113` separates page, surface, text, action, outline/focus and shape tokens; `theme.cpp:53–101` demonstrates light/dark base recipes, `:369–391` a high-contrast construction. This is a useful native token model, not a requirement to take their colours, sounds or fonts unchanged.

Add a palette reference at the end of native `Canvas` (`E/.../ui/widgets.hpp:45–52`); `Launcher::draw()` constructs it (`launcher.cpp:1183`). Pass it explicitly to Backdrop drawing, which currently takes no style (`widgets.cpp:75–108`). Keep accessibility flags separate from palette choice. Never change fonts, controller layout, guest renderer or video profile in this resolver.

Tokenization is more than editing `theme.hpp` constants:

- Existing `theme::kBase`, `kPanel`, `kText`, `kFocusCore`, historic accent names etc. are fixed values (`E/.../ui/theme.hpp:21–53`). For screen drawing, resolve semantic roles from `c.palette`; retain immutable legacy values for untouched artwork/case paths.
- `kRowPlate`, `kListPlate`, `kButtonPlate`, `kTilePlate`, `kNavPlate` capture colours at static initialization (`widgets.cpp:307–341`). Convert screen use to palette-derived plate factories or a pre-resolved plate set. Rebinding only constants will leave stale colours in those objects.
- **Do not lose artwork-plate identity:** `plate_focus():368` currently detects `&style == &kTilePlate` to avoid painting over cover images. A by-value factory breaks that test. Add an explicit plate kind/artwork flag and preserve its edge-only treatment (`:369–391`).
- Hard-coded backdrop gradients and high-contrast panels (`widgets.cpp:87,104–107,276–304,343–361`), scrollbar colours (`:923–931`), Switch `kPaper/kInk/kSubtle/kFocus` (`switch_theme.cpp:15–21`) and focused-row tints (`:164,808`) need deliberate role mapping.
- Do **not** replace every white/black literal: white image tints are neutral sampling, warning colours carry meaning, controller/case/packaging ink is content. The fixed printed sleeves and plastic must not change when Paper is selected.
- Current `text_ink()` always lifts colour toward white in high contrast (`widgets.cpp:24–32`). That is unsuitable on light paper. Prefer a resolved high-contrast palette with opaque surfaces and role-appropriate text; do not apply a universal white lift to arbitrary ink. Keep on-image ink explicitly distinct from dark text on Paper.

For full-bleed Showcase/Home and the launch scene, retain dark photo scrims + light **on-image** text even when surrounding chrome uses Paper. Otherwise switching to Paper would turn dark text onto dark artwork. The gallery's separate page/panel text roles (`H/src/ui/theme.hpp:63–66`; `widgets.hpp:109–117`) are the right lesson here.

### 4.2 Collection geometry/state: a narrow native adaptation of GridView

Proposed new `E/.../ui/collection_view.hpp/.cpp`: `CollectionView` contains **only geometry, focus/scroll springs, preferred column and visible ranges**. It receives count and canonical selected index; it does not own a second `std::vector<Game>` or copied CardItems.

Suggested API:

```cpp
void set_bounds(Rect bounds, int columns, float cell_height, float gap_x, float gap_y);
void set_count(int count);
void set_focus(int index, bool snap);
int move(Key direction);        // candidate index, or unchanged at an edge
void update(float dt, bool reduce_motion);
Rect cell_rect(int index) const;
Rect focus_rect() const;
std::pair<int, int> visible_range() const; // half-open, bounded to count
```

Adapt the actual `H/src/ui/components/grid.cpp` bodies:

- `set_count():28–39`: count-only binding with no per-cell item/check allocations.
- `step():146–185`: remember the desired column when moving through a short final row; horizontal movement chooses a new column. Do not flatten all directions to `selected ± 1`.
- `retarget():188–210`: scroll by the least whole-row distance that reveals focus; store the highlight in **content space**, then subtract the same scroll when drawing.
- `draw():384–427`: iterate visible rows only; render focus halo below art and ring above it.
- `H/src/ui/motion.hpp:19–47` supplies a four-spring rectangle that maps directly to Encore's existing tween primitives. No new tween library is necessary.

Native `ListView` already offers appropriate selection and scroll behavior for equal-height lists (`E/.../ui/widgets.cpp:830–921`). Its held-navigation catch-up at `:881–895` is a prior performance improvement worth preserving. The reference's own ListView retains/copies items and scans them in drawing (`H/src/ui/components/list.cpp:201–239`); do not port it wholesale just to display Encore's already owned catalogue. A one-column instance of the adapted collection state can implement Details list with bounded drawing.

No per-item font measurement, settings read, metadata fetch or image decode in navigation. Use existing text wrappers and `Textures::cover()` for only visible cells and the selected detail pane.

### 4.3 Real compositions, not colour variants

**Existing Encore** remains the current cinematic Home and Showcase library when defaults are selected. Preserve its current hierarchy, quick per-game settings, recents and utilities.

**Existing Switch** remains its existing rail/shortcut HOME and current default library. Do not replace its native geometry with the new collection layout implicitly. The reference Switch body is independently implemented already, not something to delete during palette extraction.

**New Library Hub** (`library_hub.cpp`, with `draw_hub_home` / `press_hub_home`) is library-first:

- Calm top navigation with actual profile/time; a compact Continue action from `Home.last_*` and access to game files/settings, not another full-screen hero.
- Main content is the shared collection + a selected-game detail pane. On the Recent tab, show a real Recent selector and resolve its file into the canonical library when opening options; do not pretend alphabetical catalogue order is recency.
- Setup failure/loading/empty-library states retain navigable settings/storage recovery. `finish_home_scan()` deliberately sets `home_loaded_` even on failure for that purpose (`E/.../ui/library.cpp:933–950`). Do not block the entire Hub waiting for artwork or a successful library scan.
- It consumes the same `games_`, `home_`, services, async completion paths and modal actions. Do not call `enter_library()`/reset selection inside `draw_hub_home()`.

**New Cover grid** (`collection_screens.cpp`, with `draw_collection_library` / `press_collection_library`):

- Concrete 1920×1080 starting geometry: readable content inside x=96..1824; body y=216..912; collection width 1096, 24px gap, details width 608. Sum: **1096 + 24 + 608 = 1728**.
- Four columns, 20px horizontal gaps: `(1096 - 3×20)/4 = 259px` cells. Two 320px rows with a 24px gap occupy 664px. Square source icons can sit at 224px, with fitted two-line 26px titles; preserve aspect ratio. This is a proposed target, not a measured screenshot result.
- A selected-game pane shows a real banner/cover, title, short description, language/add-ons and visible action hints. No fabricated playtime, network state, compatibility rating or game-loading progress.
- If a real icon is unavailable, show a readable native placeholder; do not accidentally treat a front scan as a square crop of its printed text. Offer the existing jacket viewer as a separately identified action.

**New Details list** uses the same two-pane geometry, one-column collection, approximately eight 84px rows within 696px. Title 26px, secondary metadata 22–24px; a stable art column and text columns; selected detail remains visible. No 12px web dashboard typography. The ten-foot reference is explicit about safe area, focus, text and hints (`H/docs/CRAFT.md:12–30`).

**Showcase** reuses `draw_library()`/`press_library()` and its retail sleeves/detail/viewer. It is not replaced or reimplemented. Existing Switch default grid remains a distinct default; the new explicit Cover grid is the shared count-backed view.

Resolve once with a small switch, not virtual screen plugins:

```text
library_layout == default:
    Encore -> existing Showcase
    Switch -> existing Switch grid
    Library Hub -> new Cover grid
otherwise:
    showcase -> existing Showcase
    grid -> new Cover grid
    list -> new Details list
```

The same resolver must drive **both** rendering and key dispatch. A palette must never enter this switch.

## 5. Exact integration seams and invariant preservation

### 5.1 Dispatch, selection and focus

- `E/.../ui/launcher.cpp:372–419`: preserve startup/launch gating and exclusive confirmation/game/mods/mapping/modal handlers. Only route new interface/collection handlers **after** those gates.
- `:420–429`: L1/R1 already belong to global Home/Library/Recent/Settings tabs outside modals. Do not copy the reference shell's design-cycle behavior (`H/src/app/shell.cpp:125–155`) or advertise the old library-page branches as reachable shortcuts. Add paging only on an otherwise unclaimed, documented input; it is not necessary for the first grid/list implementation.
- `press_top_nav():612–687` sets `active_main_tab_`, prepares library/settings and maps Recent to Home focus. Extend its **Recent-to-Hub** behavior explicitly; otherwise the old home focus values 5..11 would have no visible meaning in the new Home.
- `press()/draw_screen()/draw()` have all interface branch sites (`launcher.cpp:435–451,593–609,1184–1238`). Replace scattered truthy/`==1` decisions with named helpers for interface family plus the shared library resolver. A third interface must not accidentally fall through ternaries that label any nonzero value as Switch.
- `enter_library():1123–1139` is currently a five-slot/shelf setup and resets selection to zero. Add a layout-aware setup helper; preserve a valid current selected filename when changing a layout rather than invoking this reset. Configure layout geometry on actual entry/change, never each draw.
- `apply_games():743–778` preserves filename focus across reorder/removal, but only resets `library_` when ordering changes. Notify the new collection state in **both** same-order and changed-order branches. Sync counts/focus after deferred option selection too (`open_game_settings_at_file():1161–1184`).
- `refresh_selected_game():1191–1209` is deliberately free of per-title JSON reads and updates cached mode/mods/media. Every new collection move must update `library_.selected`, call this once, and retarget its own geometry. New state must follow the canonical selected index after async results; it must never overwrite canonical state from a stale copy.
- Collection `update()` belongs alongside `library_.update(dt)` (`launcher.cpp:541–548`); a count/selected change is not an excuse to snap/reset every frame. Keep accepting input while springs are moving.
- Rear-cover zoom/pan remains exclusive and Cross must not launch from it (`library.cpp:1213–1244`). Route viewer input before a new collection's D-pad/confirm handler. The existing global shoulder interception occurs before `press_library`, so **do not infer** from the local viewer return that every shoulder is already trapped globally. Preserve existing tab ownership; any new viewer exclusivity change needs an explicit contract and matching checks, not an accidental side effect of grid work.
- A new-view jacket action can set `library_back_focused_` and reuse the existing Cross-open viewer path. Draw the same extracted viewer helper for new collections. Inside it retain L2/R2 zoom, D-pad pan, Circle-only close and Cross-ignore; do not reuse these keys for collection paging while the viewer is open.
- `mapping.cpp:67–80` contains a special Back route for the Switch HOME controller shortcut. Preserve it as an interface-route decision, not a palette decision. A new Hub shortcut needs an equally explicit return destination, not a synthetic Controls modal.

### 5.2 The case renderer is not theme chrome

Keep these bodies/semantics unchanged except extraction of a shared **viewer** wrapper if needed:

- `flat_retail_sleeve()` and `flat_composed_ink()` (`E/.../ui/library.cpp:282–406`): real scan precedence, contain/full UV, physical paper insets, exact shell ratio 454/730 and common 440×704 composition.
- `draw_composed_case_face()` / `composed_case_face_ready()` (`:434–472`): same composition in shelf and launch; check exact texture buckets rather than an unrelated prefetch size.
- `case_media::FrontPaper/BackScenes/PageInputReady` (`E/.../ui/case_media_selection.hpp:16–24,42–85`): distinct cached scenes; no cover/front incorrectly reused as a rear screenshot; elapsed time does not alone permit refreshing an existing unready provisional page.
- `Launcher::launch()` (`launcher.cpp:262–369`): existence validation, cancellation, selected path, one metadata snapshot, cached retail faces, media prefetch and launch timing. All interfaces call it; none implement their own handoff or Nlib request.
- `draw_launch()` (`launcher.cpp:836–1000` inspected): same physical pivot, perspective-rendered face/spine, back hold and composed-page readiness. Do not replace it with a kit card flip or recolour its paper/plastic.
- `Textures::render_case_page()` (`textures.cpp:47–115`): maximum two pages, refresh in place, atlas sync, framebuffer/viewport/clear/scissor restoration. A grid must not allocate one composed FBO page per game.

Physical case ink, logo/footer orientation and the cinematic handoff survive all palette/interface choices. Use existing sleeve rendering directly if an additional preview needs sleeves; its helper is currently private to `library.cpp`, so expose a narrow forwarding function rather than copying its printing code into a new file.

### 5.3 Async, input, text and lifetime boundaries

- `start_scan()` builds mod/docked metadata off-thread from one global preference snapshot (`library.cpp:476–518`); `finish_scan()` discards canceled snapshots and handles preference overlap (`:521–554`). `read_home()/finish_home_scan()` similarly apply complete snapshots (`:852–954`). Do not move these to components or perform them in draw.
- `finish_selected_media():697–740` merges onto owned games and invalidates only replaced textures. New collection art reads these live fields, not copied `CardItem.texture` handles that can outlive invalidation.
- `Textures::pump():264–393` uses one decode future, validates generations, handles at most one completed upload, starts a bounded queue lookup and shares bounded deletion reclamation. Keep its budgets/cache behavior. Layout work does not authorize replacing texture ownership.
- `Launcher::~Launcher():96–118` cancels and joins jobs while Services is alive. `Textures::release():174–198` joins decoding before deleting GL resources. The frontend tears down audio/input/textures/batch before closing display (`frontend.cpp:443–455`). Do not make a reference-kit shell/renderer survive into gameplay.
- `frontend.cpp:294–347` clamps frame-start dt and drains native radio events. `radio_input.c:121–164,270–296` accumulates sample edges, neutralizes intercepted/disconnected samples, and repeats held directions/shoulders. Keep that menu path.
- Gameplay has a **different** existing recovery path: `Pad::ReopenSlot()` (`E/headless/pad.cpp:213–236`) uses handle reuse/backoff; the poll body (`:303–352`) neutralizes error reads and resets recovery state after valid reads. UI changes need no edits there. Do not substitute the kit's `core/input` or platform controller implementation for either existing owner.
- Encore `Key`/radio events do **not** expose the reference kit's `InputFrame::nav_repeat` metadata (`E/.../ui/services.hpp:20–36`; `radio_input.c:110–118`). The kit's “silent refusal on held navigation” therefore cannot be copied faithfully by a key adapter that invents `nav_repeat=false` every time. For this minimal port, clamp silently at an edge rather than emit an error cue for every repeated Key. Extending native event metadata is separate input work.
- Keep `Font::shaped()` font fallback/BiDi/HarfBuzz (`font.cpp:305–456`; `system_fonts.cpp:328–353`), `Font::fit()` UTF-8/attached-mark boundaries (`:761–791`), existing shrink/wrap/semantic text wrappers (`widgets.cpp:112–215`) and current hero typography.
- Native render order is `launcher.draw` → `batch.sync_font_texture` → `batch.draw` (`frontend.cpp:379–384`); host Stage uses the same order (`host/preview_main.cpp:107–115`). New text laid out anywhere, including case pages, must retain this atlas sync contract. Font caches are single-owner; don't measure titles on metadata workers.
- `press_language()` persists first and requests a launcher restart (`browse.cpp:326–341`); `LoadLanguage()` rebuilds catalogue/system font selection (`frontend.cpp:84–147`), and `SelectProsperoEdenGame()` restarts in process (`:462–473`). Appearance changes need no restart, and must not bypass this language path.

## 6. Renderer compatibility: why direct kit import is unsafe

Both use native C++/OpenGL and a 1920×1080 virtual canvas, but they are **not binary/API-compatible draw lists**:

| Detail | Encore | Reference kit | Port rule |
|---|---|---|---|
| Shape 7 | Horizontal rounded rectangle | Arc | Never pass kit instances to Encore GL. |
| Shape 8 | Rounded image | Chamfer | Same-sized structs do not imply same shader semantics. |
| Additional shapes | Native case faces/spine/plastic/caps/corners, IDs 9–15 | Stars/arcs/chamfers and mesh runs | Preserve Encore shader and case IDs. |
| Font texture handle | Actual GL texture name in run | Encoded font handle base `0xf0000000`, six slots | Keep native atlas handling. |
| Run structure | Texture + clip | Texture + clip + `mesh` discriminator | Do not memcpy or alias run buffers. |
| Glass | Existing blurred-art image sampled with `Backdrop::uv` | Renderer captures/blurred scene then overlay, `DrawList::glass` | Do not claim live-scene blur by importing a Painter call. |

Sources: `E/.../gfx/draw_list.hpp:49–99,109–159`; `H/src/gfx/draw_list.hpp:69–120,134–158`; `H/src/ui/widgets.cpp:346–353`; `H/src/app/shell_render.cpp:18–58`; `E/.../ui/widgets.cpp:276–304`. Both instance types happen to assert 96 bytes; their tags/parameters still differ.

Reusable now, through Encore primitives:

- Theme token partitioning and selected/native surface construction. Adapt **flat/soft/outlined** surface recipes (`H/src/ui/widgets.cpp:267–298,355–358`) rather than port every shape style.
- Count-backed grid navigation/viewport iteration and content-space focus; native `SpringRect`-equivalent; selected detail slots rather than demo catalogue copies.
- Focus-ring/halo construction from bordered rectangles (`H/src/ui/widgets.cpp:441–452,455–518`), mapping simple rectangle expansion locally. Crucially, draw halos **under** art; native shadows fill their interior (`E/.../ui/widgets.cpp:374–376`).
- Label/value rows, toggles/slider **drawing ideas** via native wrappers, not duplicated settings state.
- Tour metadata and capture-before-input convention (`H/src/app/tour.cpp:93–141`) as inspiration for extending Encore's existing screenshot Stage.

Not part of this implementation: `hui::Renderer`, procedural backdrop/post stack, multi-font slots, mesh/chamfer/arc shaders, kit save files, sound bank/mixer, reference platform service functions, app Concept registry, invented catalogue, PS5 entry point or build system. `H/docs/ADOPTING.md:77–81` also excludes OpenGL ES as-is; this is not a reason to change Encore's renderer. Preserve GPL notices/attribution for actually adapted code; no new font/sound assets are required.

## 7. Host screenshot preview using existing Docker images

### 7.1 Read-only image evidence collected

Local image metadata at research time:

| Image | ID prefix | Recorded package-layer evidence | Assessment, not runtime verification |
|---|---|---|---|
| `prosperoeden-host:features` | `e6137ec556fe` | Ubuntu 24.04 base; build-essential, clang/lld/llvm 18, Python, Ninja; `libegl1-mesa-dev libgl1-mesa-dev libgl1-mesa-dri` | Best static-evidence candidate for a launcher-only Mesa preview. |
| `prosperoeden-host:network-features` | `b6fe3bf9422f` | Same host compiler/Mesa development packages recorded | Also suitable in principle; network features are not needed or authorized. |
| `prosperoeden-host:integration` | `ffafe2e1edac` | Host compiler/tools recorded, but no EGL/GL development packages in the recorded install line | Do not assume headers/link libraries are present. |
| `prosperoeden-release:recovery-5211718` | `f54e66776e38` | Base host install plus a separate copied CI setup script invoked by a later layer | That script's resulting package inventory was not inspected; Mesa/Pillow presence remains unverified. No need to use a release/emulator build for UI screenshots. |

All four inspect as Linux/amd64. Evidence came only from `docker image ls`, selected `docker image inspect` fields (ID/platform/workdir), and `docker image history --no-trunc --format '{{.CreatedBy}}'`. No image filesystem/package probe or container was run. In particular, a successful recorded apt layer does not prove the required runtime remains intact or that all transitive dependencies are available now.

### 7.2 Existing Encore preview is already the right target

`E/tools/launcher/preview.sh:28–57` compiles only `host/*.cpp`, `pe/core/*.cpp`, `pe/gfx/*.cpp`, `pe/ui/*.cpp`, links EGL/GL/math, and runs surfaceless llvmpipe. It does not build Eden, guest emulation, Vulkan/RADV or a PS5 executable. The native CMake has an explicit launcher source list (`E/headless/CMakeLists.txt:1705–1758`), so new `.cpp` files must be added there even though preview uses a glob.

`Stage::render()` calls the **real** Launcher and GlBatch and writes pixels (`E/.../host/preview_main.cpp:107–136`); `main():696–703` requests OpenGL 4.5 and reports version/renderer. Current knobs: `PE_LANG`, `PE_LOOK`, `PE_PREVIEW_SWITCH_THEME`, library/launch-only branches (`:227–339,707–790`). Add stable-key `PE_UI_INTERFACE`, `PE_UI_PALETTE`, `PE_LIBRARY_VIEW` switches for the new matrix, retaining the legacy Switch knob as a compatibility alias. These are proposed variables, not existing features.

**Do not invoke the current script unchanged in a read-only/offline integration harness:**

- `preview.sh:23` invokes `render-art.py`, which writes into repository `headless/prosperoeden/ui/art`.
- `preview.sh:24` explicitly runs `deps.py fetch harfbuzz` (possible download/mutation); line 25 resolves the path.
- The art script imports Pillow (`render-art.py:21`) and depends on canonical JPEG/icon plus fixed source templates (`:76–98,103–145`). None of the inspected image package layers proves Pillow is installed.
- The art script explicitly rejects trusting the checked-in legacy backdrop as canonical (`:103–125`). Bypassing generation is acceptable only with a separately staged **known-current** asset set, not by silently claiming legacy art matches the package.

### 7.3 Build plan for a later authorized execution task

No new image is necessary if `features` has the expected installed files. Use its immutable ID, override its command/entry point so the default host test runner is not executed, and run with `--pull=never --network=none`. Bind the source repo **read-only**; put objects, staged assets, fake-service images, logs and PNGs under the authorized external artifact directory. No console credentials, ROM root, ports, GPU devices or privileged mode are needed.

Recommended small adaptation to the first-party preview tooling:

1. Accept explicit `PE_PREVIEW_ASSETS` and existing pinned `PE_HARFBUZZ_SRC` paths; add an offline/no-fetch option. Validate required file presence only; no vendor source inspection or automatic install/fetch. The pinned single-file wrapper is first-party `E/.../gfx/harfbuzz.cpp:19–22`; native build expects HarfBuzz 12.3.2 (`E/headless/CMakeLists.txt:1750–1756`).
2. Compile the **same** current preview source manifest with host C++20, existing include paths, `GL_GLEXT_PROTOTYPES=1`, EGL/GL/math and explicit pthread support; retain the current two-worker default. Use ordinary host compiler, not the PS5 cross-compiler/sysroot or an old executable bundled in an image. Compilation can consume pre-provisioned pinned dependency files without the research agent opening them.
3. Stage assets outside the source tree. For regenerated art, copy only the necessary public assets/templates and the generator into a temporary directory with the same expected relative structure, or teach the generator an output-directory option. Use already provisioned Pillow only; if missing, report that prerequisite rather than downloading it under offline execution. Keep top-left TGA orientation/canonical wallpaper and fixed packaging conversions.
4. Directly run the resulting `pe_preview <staged-ui> <output> 1920 1080` with `EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe`. Host screen states remain synthetic/offline through FakeServices.
5. Capture legacy Encore, legacy Switch, Hub grid and Hub list, then the same compositions in all explicit palettes. Add appearance chooser/saved rollback, empty/missing media, long multilingual names, large text/high contrast/reduced motion, selected-title changes, jacket zoom/pan and shared case launch captures. Use separate output directories keyed by interface/palette/view/locale/size.

The reference kit can also be rendered **separately** in this host image using `H/tools/host-snapshots.sh:20–53` and its baked assets; it compiles the whole reference UI app (not Encore). Its own script needs clang commands and Ninja and writes into `H/build`, so use a staged writable copy or parameterize that build directory rather than mounting the reference checkout writable. Its stb include dependency is visible in first-party `H/host/snapshot_main.cpp:28–33`; no need to open dependency source. Rendering that gallery is reference evidence, not validation of the adapted Encore UI.

### 7.4 Do not mistake fast simulated frames for ready async media

Encore `Stage::frame()/wait()` only update on non-tour frames (`E/.../host/preview_main.cpp:84–105`), so a long simulated wait before the first draw does not itself request visible textures or guarantee workers completed. The existing library-only branch explicitly performs real renders plus bounded waits and checks actual textures/fades (`:275–318`). Reuse that readiness approach for the new collections; do not mark a placeholder screenshot as a loaded-art result. The fallback-only branch is more time-based (`:244–250`) and should not be copied as a proof of readiness.

The reference's `Shell::rehearse()` explicitly records non-rendered frames (`H/src/app/shell.cpp:195–198`; `shell.hpp:41–44`), which is a useful harness idea. Any adapted rehearsal must still use the real native draw path and respect its memoized case-page work; it must not create a parallel HTML renderer or emulate the artwork in Python.

**Unverified prerequisites:** current image filesystem contents; available HarfBuzz/stb headers; Pillow and ffmpeg; supplied system fonts; actual compiler/linker success; EGL context creation; shader results; captured pixels. No screenshot was produced or inspected in this assignment. Native feel, frame rate, audio, controller recovery and game handoff require separately authorized hardware checks. Neither repository's claim that host shaders match console shaders converts a host picture into PS5 qualification (`H/AGENTS.md:144–146,316–318`).

## 8. Nonoverlapping edit ownership for the implementation session

These are proposed work packages, **not agents started by this task**. Assign whole-file ownership; do not have a “palette worker” and “layout worker” simultaneously rewrite `launcher.cpp` or `library.cpp`.

| Owner/package | Exclusive editable files | Output contract |
|---|---|---|
| A — Preference contract/native bridge | `E/headless/appearance_preferences.h` (new), `E/headless/settings_store.h`, `E/.../ui/services.hpp`, `E/.../eden_services.cpp`, `E/headless/preferences_check.cpp` | FSR/AA fix, shared stable IDs, defaults, migration, validation, symmetric DTO bridge and persistence cases. No rendering/input edits. |
| B — Native tokens and collection primitives | `E/.../ui/theme.hpp`, `palette.cpp` (new), `widgets.hpp/.cpp`, `collection_view.hpp/.cpp` (new) | Explicit per-canvas palette, plate kind/edge-only art handling, native palette factories, pure geometry/count-backed collection state. No source in kit, renderer, font or native input. |
| C — Launcher/presentation integrator | `E/.../ui/launcher.hpp/.cpp`, `home.cpp`, `library.cpp`, `settings.cpp`, `switch_theme.cpp`, `browse.cpp`, `mapping.cpp`, `library_hub.cpp` and `collection_screens.cpp` (new) | All dispatch/focus/layout setup, both appearance modal skins, palette role wiring in actual screens, existing showcase/switch preservation, shared viewer extraction. Sole owner of these shared bodies. |
| D — Host preview/build/contract checks | `E/.../host/preview_main.cpp`, `fake_services.hpp/.cpp` as needed, `E/tools/launcher/preview.sh`, `render-art.py` only for output-path support, `E/headless/CMakeLists.txt`, new focused host checks and relevant existing UI contract scripts/catalog updates | Offline external-output preview route, canonical fixture readiness, independent selector matrix, fake-save failure and selection edge cases; native source-list registration; translations for new copy. No platform/build-system migration. |

Order: A publishes key/default contract → B publishes palette/collection APIs → C integrates screens → D captures/checks. D may prepare fixtures before C completes but must not interpret old screenshots as new implementation evidence. The central integrator resolves shared API changes rather than parallel edits to the same file. All `H/` files remain read-only references.

Existing source checks worth preserving/adapting intentionally, not deleting to obtain green output:

- `E/tools/check-launcher-shoulder-and-cover.py:27–38,71–85`: shoulders/tab routing and modal editing; `:39–70,93–115`: real-scan precedence, UV orientation, common case geometry and fallback content.
- `E/tools/check-switch-theme-layout.py:6–35,46–77`: Switch exact geometry, native option/mod/mapping routes, current hints/back behavior and no firmware asset import.
- Some assertions check exact source spellings. A helper extraction may require an assertion update while preserving behavior. A source-string PASS is not a visual/behavioral proof; no checks were executed here.

## 9. Acceptance checklist for the later implementation

- [ ] Legacy settings with no Appearance keys open the current Encore; legacy `theme=switch` opens current Switch with unchanged default geometry.
- [ ] Interface, palette and collection view independently round-trip through native JSON and UI service conversion; invalid values safely fall back. FSR/AA and all unrelated settings survive every appearance/language/accessibility edit.
- [ ] Selecting Colours never changes Interface or controller profile; selecting a library view never changes a game video preset.
- [ ] Hub is visibly a library-first two-pane composition; grid/list differ in geometry **and** navigation, not only fills. Existing Encore/Switch are still selectable.
- [ ] One obvious active focus; no focus on absent cells; correct short final row, zero/one/many titles, fast held navigation, async reorder/removal and return from game/options/viewer. Existing canonical filename focus retained.
- [ ] No image decode, settings read, full catalogue copy or full catalogue draw loop introduced into per-key/per-frame collection work.
- [ ] Multilingual title rendering, shaping, attached-mark-safe fitting, dynamic atlas updates, language restart and custom Montserrat remain intact. New copy fits large-text mode and has translations.
- [ ] Same service launch gates, safe launch, mods, per-game mapping/options/save transfer routes and setup recovery; no second application/core/platform ownership.
- [ ] Case scans/fallbacks, paper/plastic, common aspect/UV orientation, two-page cap, late-media refresh and original cinematic handoff unchanged across palettes.
- [ ] Native and host builds use the new source files; offline previews show actual current native UI at required resolutions, with readied media, relevant failure/empty states and GL error checked.
- [ ] No claim of native smoothness/recovery/sound/controller feel without a separately authorized console observation.

## 10. Sources actually read and limits of delivery

The source ledger distinguishes full files from body excerpts. Abbreviated paths use the explicit expansions in Scope and evidence conventions above. Discovery grep output alone was not treated as a body inspection.

### Instruction/context documents, fully read

- `D:/leonardommello/PS5/AGENTS.md`; `H/AGENTS.md`.
- `H/docs/ADOPTING.md` (1–210), `H/docs/CRAFT.md` (1–187), `H/docs/THEMES.md` (1–567). The last has an older “Honest limits” paragraph inconsistent with today's component catalogue; recommendations rely on actual component source rather than its outdated absence claims.
- `E/.../ui/EGM.md` (1–32), `E/headless/prosperoeden/host/EGM.md` (1–26), used as orientation, not as execution evidence.
- Loaded design/planning skills and read their applicable app-screen/KISS references. Console-specific brief, source geometry and repository CRAFT take precedence over generic web-dashboard typography/art-direction rules. No browser/visual implementation workflow was claimed complete.

### Encore headers fully read

- `E/.../ui/launcher.hpp` (1–379), `services.hpp` (1–382), `theme.hpp` (1–117), `widgets.hpp` (1–234), `textures.hpp` (1–136), `case_media_selection.hpp` (1–87).
- `E/.../gfx/draw_list.hpp` (1–215), `font.hpp` (1–149), `system_fonts.hpp` (1–89), `gl_batch.hpp` (1–61); `E/.../core/log.hpp` (1–26).
- `E/headless/prosperoeden/host/fake_services.hpp` excerpt 1–145 only.

### Encore code/build bodies read (partial unless stated)

- `E/.../ui/launcher.cpp`: 48–262, 262–371, 372–612, 612–689, 836–1000, 1181–1249. In particular **not** the remainder of `draw_launch`; renderer preservation recommendations intentionally avoid changing it.
- `E/.../ui/home.cpp`: 162–196 and 565–734.
- `E/.../ui/library.cpp`: 1–80, 107–211, 282–406, 434–475, 476–556, 697–779, 852–954, 1123–1598. Per-game/mod bodies later in the file were discovered but not fully inspected.
- `E/.../ui/settings.cpp`: 1–617 and 780–824; `switch_theme.cpp`: 1–525 and 782–895; `widgets.cpp`: 1–395 and 830–934; `mapping.cpp`: 38–92; `browse.cpp`: 291–403.
- `E/.../ui/textures.cpp`: 47–203 and 264–393.
- `E/.../gfx/font.cpp`: 305–456, 462–523, 618–642, 761–794; `system_fonts.cpp`: 328–357; first-party `harfbuzz.cpp` (1–22, full wrapper only). No HarfBuzz implementation source read.
- `E/headless/prosperoeden/eden_services.cpp`: 1933–2031; `frontend.cpp`: 60–473.
- `E/headless/prosperoeden/radio_input.c`: 74–301; `E/headless/pad.cpp`: 213–261 and 303–359.
- `E/headless/settings_store.h`: 97–143 and 294–405; `E/headless/CMakeLists.txt`: 1705–1776.
- `E/headless/prosperoeden/host/preview_main.cpp`: 43–217, 227–339, 682–791, 853–877; `fake_services.cpp`: 1–45.
- `E/Makefile` (1–81, full); `E/tools/build-headless-host.sh` (1–100, full); `E/tools/launcher/preview.sh` (1–72, full); `render-art.py`: 17–32 and 76–147.
- `E/tools/check-launcher-shoulder-and-cover.py` (1–116, full); `E/tools/check-switch-theme-layout.py` (1–77, full).

### Reference kit source read

- Full: `H/src/ui/theme.hpp` (1–119), `widgets.hpp` (1–128), `fonts.hpp` (1–146), `motion.hpp` (1–146); `src/ui/components/component.hpp` (1–179), `card.hpp` (1–184), `grid.hpp` (1–171).
- `H/src/ui/theme.cpp`: 53–132, 369–395, 903–942; `widgets.cpp`: 95–192, 267–402, 441–520.
- `H/src/ui/components/grid.cpp`: 17–81, 82–381, 378–447; `list.cpp`: 94–273. No claim that the entire component library was reviewed.
- Full: `H/src/app/concept.hpp` (1–145), `shell.hpp` (1–126), `shell_render.cpp` (1–68); `shell.cpp`: 81–200; `tour.cpp`: 79–177.
- `H/src/gfx/draw_list.hpp`: 1–160 only, for public tag/handle/run compatibility; `H/host/snapshot_main.cpp`: 1–140; `H/tools/host-snapshots.sh` (1–53, full).

No UI binaries, screenshot fixtures, reference concepts, source-repository git history, native game core internals or dependency implementations were used to infer visual quality/performance. Names such as Library Hub, new source filenames, APIs, palette choices, JSON keys and layout geometry above are **implementation recommendations**, not features already found in source.
