# Launcher appearance

**Settings → Appearance** has three independent properties:

- **Interface:** Encore (existing cinematic Home), Switch (existing console-style
  Home), or Collection (library-first home and game-detail pane).
- **Theme:** Original, Midnight, Canopy, Slate or Ember. Original preserves each
  existing interface's palette; the other four use native role-based colours.
- **Collection layout:** a four-column grid or a details list. This property is
  used by Collection; it does not replace the existing Encore/Switch libraries.

Changes save and apply immediately. A failed save restores the previous look.
These controls do not change game rendering, button mappings, language or saves.
Switch's filled Settings, Files and Language selections preserve dark text on
the new palettes' bright focus surface, including high contrast. The selected
language marker follows the same pairing. Other text keeps its existing contrast
behavior; the Original/non-high-contrast legacy pairing is retained.
L1/R1 retain their existing main-tab shortcuts. In Collection, D-pad browses,
Cross launches, Triangle opens the existing game settings, Options requests
Safe Launch, and R2 opens the existing rear-jacket zoom/pan viewer. Recent keeps
the actual recent-game view, not an alphabetical list labelled as recent.

## Compatibility and implementation

The reference is `ps5-homebrew-ui` at local reference commit
`bbe19054ee2e7ec069254775273aeb8846c4bd3e`: theme data is separate from screen
design, collections draw visible items, focus is controller-first, and typography
stays readable on a TV. This increment adapts those ideas using Encore's
renderer, fonts, texture cache and services; it does not import the reference
app/platform, demo games, music, fonts or shaders.

The legacy `appearance.theme` JSON key remains a downgrade alias. New builds
use `appearance.interface` when present (`encore`, `switch`, `collection`), plus
`palette` (`original`, `midnight`, `canopy`, `slate`, `ember`) and
`library_layout` (`grid`, `list`). Unknown values recover independently to defaults.
An invalid present interface does not revive a stale legacy alias. Unrelated
JSON fields are preserved. Missing new fields do not migrate/reset user data.

Built-in choices are not external theme packs or Themezer support. Host fixtures
exercise actual C++ drawing/navigation, save refusal, all interface/palette
combinations, settings round trips and preservation of FSR/anti-aliasing values.
They do not qualify native SDK linkage, fonts unavailable on the host, audio,
performance or PS5 gameplay. No current-artifact console qualification is claimed.
