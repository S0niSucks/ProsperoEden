# P05 — independent storage increment

Authority: decisions.md AUTH-02. No data migration/restore or console work.

## Chosen contract

Games and wallpapers each get a separate absolute user-data directory under the
existing /data or /mnt path grammar. Empty setting uses assets/roms or
assets/wallpapers. No symlink ancestors at selection; final files are regular,
bounded and no-follow. Selection persists and applies on restart, so live readers
cannot switch roots. Settings/saves/keys/firmware/mods remain under current owners.
Wallpaper selection is explicit filename under the selected folder; unavailable/
corrupt images use the packaged backdrop. No auto-select-first-file or download.

## Files and action

- Modify headless/settings_store.h: additive storage/games_folder,
  wallpapers_folder and wallpaper filename, validate without touching other JSON.
- Modify headless/assets_dir.h: process-frozen GamesDir/GamePath/WallpapersDir.
- Create headless/user_content.h: bounded regular-file admission/path helpers for
  wallpaper enumeration/read; no protocol client or core dependency.
- Modify headless/main.cpp and prosperoeden/eden_services.cpp: every actual ROM
  listing, metadata, presence, last/recent, forwarder and launch path uses GamesDir.
  Library export reports selected ROM root, not incidental assets root.
- Modify services.hpp, eden_services.h/.cpp, launcher.hpp, settings.cpp,
  browse.cpp, switch_theme.cpp: one Storage property modal chooses Files/Games/
  Wallpapers; existing folder browser remains. Wallpaper browser also offers
  bounded image files; explicit Cross persists selection. Triangle saves folder,
  Square resets that one setting. Warn restart/no-copy, report save refusal.
- Modify textures.hpp, widgets.hpp/.cpp, launcher.cpp: saved wallpaper is an
  existing async texture-cache request; missing file uses packaged backdrop.
  Palette/accessibility overlays remain; no synchronous image decode/render I/O.
- Modify host fixture: deterministic independent selections and test refusal;
  add actual UI navigation specimens. No external host/user data is read.
- Create tools/check-content-roots.py and focused user-content witness; modify
  tools/check-recent-games.py only to follow intentional GamesDir rename.
- Add docs/CONTENT_LOCATIONS.md; update narrow EGM contracts after actual code.

## Verification

Docker only, repo read-only. Actual settings round-trip preserves game_files,
history, unrelated keys and all appearance/video values. Reject traversal,
control bytes, wrong types, symlinks, special nodes, hardlinks and oversized data;
reopen uses chosen directory, reset returns only that location to default.
Exercise GamesDir across discovery/path/history/export/forwarder source seams.
Native C++ host preview verifies Storage modal and wallpaper picker recovery.
No host/guest firmware, native syscall, PS5 codec or USB qualification inferred.
