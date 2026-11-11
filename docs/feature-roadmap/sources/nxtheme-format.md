# .nxtheme format — pinned public specification (2026-11-10)

Pinned by the coordinator from public documentation (LayoutDocs
themezer.net definitions/filetypes guides; exelix11 SwitchThemeInjector
readme/CustomLayouts.md; fileinfo.com). This unblocks the c-themes front
under the same rule as other fronts: only what is documented here may be
implemented; anything beyond is a declared gap.

## Purpose

The Switch homebrew community created .nxtheme to share custom themes
legally: SZS files extracted from firmware contain copyrighted data and
cannot be shared; .nxtheme contains ONLY differential/shareable parts.

## Container

An .nxtheme file is a SARC archive (same family as .szs; Yaz0 compression
is used in .szs — treat both plain SARC and Yaz0-wrapped SARC as valid
inputs). Members observed by public tooling when extracting a theme:

- `image.jpg` — background image, 1280x720 (720p) JPG for home menu
  targets (DDS/PNG accepted by builders as inputs).
- `layout.json` — OPTIONAL diffed layout patch (differential data
  produced by LayoutDiff against the ORIGINAL firmware SZS; applying it
  requires the console-side installer and original SZS).
- `config.json` — theme details: name, author, target menu.

## Targets (one .nxtheme per menu; a full theme = multiple files)

| Menu | SZS filename | Applet |
| --- | --- | --- |
| common | common.szs | qlaunch |
| Home Menu | ResidentMenu.szs | qlaunch |
| Lockscreen | Entrance.szs | qlaunch |
| All Apps | Flaunch.szs | qlaunch |
| News | Notification.szs | qlaunch |
| Settings | Set.szs | qlaunch |
| Player Select | Psl.szs | playerSelect |
| User Page | MyPage.szs | myPage |

Applet TitleIDs: qlaunch 0100000000001000, playerSelect
0100000000001007, myPage 0100000000001013.

## Explicit gaps (NOT covered by this pin — do not implement)

- layout.json schema semantics (pane properties/USD sections/PaiTags):
  documented at LayoutDocs for EDITORS, but console application requires
  original copyrighted SZS; our tooling reads/stores it opaquely.
- bflyt/bflan/bntx internals beyond what tools/ps-glyph-* inspectors
  already parse first-party.
- Applet icons and common.szs custom layouts (WebInjector lacks them;
  full builder feature set is out of scope).

## Implications for the Encore front (c)

1. Our existing .szs structural subset (Yaz0+SARC) already parses the
   CONTAINER layer of .nxtheme.
2. A valid increment: .nxtheme reader = SARC parse + member inventory +
   config.json/name-author-target validation + image.jpg presence +
   optional layout.json carried opaquely. Refusal on missing/invalid
   members. NO layout semantics.
3. Theme application on PS5 remains out of scope; this is format
   compatibility only.

## Sources (public, consulted 2026-11-10)

- https://layoutdocs.themezer.net/definitions (nxtheme definition)
- https://layoutdocs.themezer.net/guide/3-filetypes (SZS=SARC+Yaz0)
- https://layoutdocs.themezer.net/guide/2-firmware-files (menus/targets)
- https://github.com/exelix11/SwitchThemeInjector (readme, CustomLayouts)
- https://fileinfo.com/extension/nxtheme (container contents summary)
