# Game files, app data and updates

Where ProsperoEden looks for your keys, firmware and games, where it keeps its own data, and how it is updated. The install steps themselves are in the [README](../README.md#install).

## Game files folder

Only these subfolders matter; the folder itself can have any name and location.

```text
<game files folder>/                    # e.g. /data/prosperoeden, /mnt/ext1/eden, /mnt/usb0/eden
├── keys/
│   ├── prod.keys
│   └── title.keys                      # optional
├── firmware/
│   └── *.nca                           # extracted firmware NCAs
├── amiibo/                             # optional: your own 532/540/572-byte .bin dumps
├── roms/
│   └── <Game>/                         # one folder per game
│       ├── <game>.nsp                  # the game (NSP or XCI, or NSZ or XCZ: see Compressed games)
│       ├── <game> update.nsp           # optional: its update and DLC files, beside it
│       ├── <game> DLC.nsp
│       ├── mods/                       # optional: its mods, one folder each (see Mods and cheats)
│       │   └── <mod name>/
│       │       ├── exefs/              # code patches: *.pchtxt, *.ips
│       │       ├── romfs/              # replacement game files
│       │       └── romfs_ext/          # files added to the game's own
│       ├── cheats/                     # optional: its cheats
│       │   └── <build ID>.txt          # one file per version of the game
│       └── reshade/                    # optional: its post-processing presets and effects
│           ├── <preset>.fxp            # offered in this game's settings
│           └── <effect>.fx             # and the .fxh files and Textures/ they use
├── save-import/                        # optional: saves to import, one folder per title ID
├── ryujinx/                            # optional: a Ryujinx data folder to import saves from
├── save-export/                        # written by "Export a copy"
└── .remote-downloads/                  # downloads that are not finished yet (see Download sources)
```

Each game sits in a folder of its own in `roms/`, with everything that belongs to it: its update and DLC files, its mods, its cheats and its post-processing presets. ProsperoEden tells the game from its update and DLC by what each file holds, not by its name, so the files can have any names. Folders inside a game folder are not searched for games, and folders whose names start with a dot are skipped. **Delete from console** also deletes the update and DLC files in the game's folder, and the folder once it is empty.

The folder browser shows how many keys, firmware files, and games each folder holds, so you can check a folder before selecting it. Moving your files later only needs a new selection in Settings; saved settings, covers, and recently played games carry over.

### Compressed games (NSZ and XCZ)

Play your compressed library as it is: no converting, no extra copies of games that do not need one.

`NSZ` · `XCZ` · updates and DLC too · compressed with [nsz](https://github.com/nicoboss/nsz)

NSZ and XCZ files are NSPs and XCIs whose larger contents nsz compressed. Put them in `roms/` like any other game. How a game plays depends on how nsz compressed it:

| | **Block** (`nsz -B`) | **Solid** (`nsz -S`) |
|---|---|---|
| nsz's default for | XCZ | NSZ |
| First start | at once | unpacked once, which takes a while for a large game |
| Later starts | at once | at once, from the unpacked copy |
| Space on the console | the file itself | the file, plus the last played game uncompressed |

**Block compression** can be read anywhere in the file. These games play straight from the compressed file.

**Solid compression** can only be read from its start. The first time such a game starts, ProsperoEden unpacks its compressed contents once into `/data/prosperoeden/user/cache/nsz`. That needs about as much free space as the game uncompressed; `stderr.log` shows how far it is. Only the game you play last keeps its unpacked copy: unpacking another one removes it. Touchpad + L1 while it unpacks stops it, and it is done again the next time the game starts.

**In the Library,** a solid-compressed game or update is read at the end of its data when it is first looked at, which takes a few seconds per GB. What that finds is kept beside the unpacked copies (up to 16 MiB per game or update, usually about 5), so it is quick from then on.

**Short on space?** When the console's storage is too full to unpack a game, the log says so, and the game is read without unpacking instead: far too slow to play, so free some space first. To play a solid-compressed game without ever unpacking it, compress it again with block compression on a computer:

```text
nsz -D <game>.nsz
nsz -C -B <game>.nsp
```

The folder `/data/prosperoeden/user/cache/nsz` can be deleted whenever no game is running; it is made again as needed.

### Separate games folder

**Games on a drive of their own.** Choose **Settings > ROMs and updates** to keep `roms/` and `updates/` in one folder (a USB drive, say) without moving keys, firmware or saves. Triangle saves the folder; Square removes the independent choice and follows **Game files** again. Both changes apply after closing and reopening the app. The same choice can be set as `"games_folder"` in `/data/prosperoeden/config/prosperoeden.json`, next to `"game_files"`:

```json
{ "game_files": "/data/prosperoeden", "games_folder": "/mnt/usb0/games" }
```

It applies from the next start, and the Library, the home page, downloads and forwarders then use its `roms/` and `updates/`; the GAMES and UPDATES lines in Settings > Game files and in About show where they are (the folder browser still counts each folder's own `roms/`). When the folder is not there at start (a drive not plugged in), the Library lists no games from it and downloads wait; the log says `games folder: Not found`. Without it, or when it does not name an absolute folder, the games are in the game files folder as before. Whatever the folders, the shader caches, saves and everything else written during play stay in `/data/prosperoeden` on the console (see App data).

### NRO homebrew

Put Switch homebrew `.nro` files in `roms/`, or one game folder below it, like
`roms/Tools/Demo.nro`. You can also copy a complete `switch/` folder into `roms/`,
for example `roms/switch/Tools/Demo/Demo.nro`: the tree is searched without flattening
or moving files. Up to eight directories below `switch/` and 16,384 entries are
examined; hidden entries and symbolic links are skipped. Commercial containers
still use the existing one-folder layout, not recursive scanning.

NROs appear in the Library, launch through Eden's NRO loader, and work with
recent-game paths and forwarders. This is not
support for PS5 ELF files or Switch NSO/NSP conversion.

The Library reads the optional ASET icon and NACP title, program ID and language
flags using bounded reads. An icon is limited to 1 MiB and 4096 pixels per side;
truncated, overlapping or out-of-file assets are ignored. A bare NRO falls back
to its file name and does not need an icon or NACP.

Unencrypted NRO homebrew can start without keys or firmware, provided console
storage is accessible. NSP/XCI/NSZ/XCZ still require the normal setup. This does
not supply missing services to a homebrew application: host loader/JIT fixtures
do not guarantee that its GPU/services work on PS5.

### In-game quick commands and Amiibo

**Left touchpad click + Square** opens a compact command strip during gameplay. The
existing left touchpad + L1 return shortcut and left touchpad + R1 performance-overlay
shortcut remain available. Circle closes the strip; Up/Down switches between **Amiibo**
and **Cheats**. The strip works with performance counters turned off. It takes controller
input while open; **the game keeps running**, so use its own pause menu when appropriate.

Put your own Amiibo dumps in `<game files folder>/amiibo/` (not the independent
ROMs folder). Left/Right selects a file, Cross presents it when the game is waiting for
an NFC scan, and Square removes the presented tag. Only regular `.bin` files of 532,
540 or 572 bytes are offered. Loose files and subfolders work, up to eight folder levels
and 4,096 entries across the whole tree; links and hidden files/folders are skipped.
No tag files or NFC keys are included or downloaded. Game writes go back to the
selected dump, so keep a backup of the original. File size is not proof of valid Amiibo
contents; the game's NFC validation still applies.

An optional local `amiibo/amiibo.json` catalog supplies names and series by the dump's
full public model ID. Both AmiiboAPI's compact `amiibos` database and its `amiibo` API
response format are supported, with a 2 MiB/4,096-item limit. There is **no online sync
yet**. Without a matching catalog entry, the strip shows the subfolder name, or the
filename for loose files; long labels are abbreviated. Renaming never changes the tag's
identity. Two dumps of the same model remain separate files and retain separate progress.

For example, keep `amiibo/Mario - Smash/tag.bin` and optionally `cover.png` in the same
folder. Local image association recognizes `<dump filename without .bin>.png` first,
then a folder's `cover.png` only when it contains one dump, then
`amiibo/images/icon_<8-digit head>-<8-digit tail>.png` by model ID. The selected photo
appears below the command strip in both OpenGL and Vulkan. PNG files are limited to
4 MiB and 1,024 pixels on either side, with bounded decoder memory; only the selected
photo is decoded, then aspect-fitted to a 256-pixel thumbnail. Missing, unreadable or
invalid pictures fall back to text without changing the tag or preventing a scan.
No artwork is bundled, and the in-game menu never downloads it. See the
[catalog study and test scope](AMIIBO_CATALOG.md).

To prepare names and optional photos **on a PC**, use the opt-in helper below, then copy
that folder to your `amiibo/` folder. It never reads, renames or downloads `.bin`/`.nfc`
files. No network access happens when the in-game menu opens:

```powershell
python tools/sync-amiibo.py .local/amiibo-setup
# Optional photo, selected explicitly by its public 16-digit model ID:
python tools/sync-amiibo.py .local/amiibo-setup --id 0000000000000002
```

The default metadata service is `amiiboapi.org`. `--catalog-url` and `--image-template`
can select an HTTPS mirror; the template accepts `{head}` and `{tail}` only. At most 64
explicit IDs are downloaded per run. Valid cached photos are reused unless
`--refresh-images` is given. Failed downloads/validation leave the previous cache in place;
individual file replacements are atomic, not a multi-file transaction. Use an output
folder you control, without symlinks/junctions, and do not move its parents while it runs.
Photos remain subject to their owners' rights; the helper does not grant redistribution
permission. Updating by network from the PS5 launcher is not yet implemented.

The **Cheats** page lists ordinary cheats actually loaded for the running game/build,
not master codes. Left/Right selects one and Cross switches it on/off without returning
to the launcher. Changes apply on the cheat engine's next timing tick and last for this
session only. Disabling a cheat stops further execution; it cannot undo memory already
changed by that cheat. Cheats in a mod disabled before launch are not loaded: enable
that mod in the launcher first. No database download is implied by this runtime menu.

The strip uses the existing baked HUD font: uppercase Latin text and numbers, with
unsupported filename characters shown as dashes. Real PS5 NFC/game compatibility and
console rendering still need console testing; host driver/VM and Mesa checks do not
establish those results.

## App data

ProsperoEden keeps its own data in `/data/prosperoeden`, separately from the game files folder:

```text
/data/prosperoeden/
├── config/prosperoeden.json            # every profile's settings, and the game files folder
├── config/remote/                      # download sources and save sync, when you use them:
│   ├── sources.json                    #   the sources and how to reach them (written by you)
│   ├── queue.json                      #   the download queue
│   ├── <source>/catalog.json           #   each source's game list, kept for the next start
│   ├── save-sync.json                  #   where each profile syncs its save data (filled in by you)
│   └── save-sync/                      #   what the save sync keeps: the console as a device of
│                                       #   each profile's server, the games still to be synced
├── backup/save-sync/                   # save data the save sync replaced, the last three per game
├── covers/                             # cached game covers
├── logs/                               # current and previous session logs, crash reports
├── previous-version/PPSA99008-backup/  # the version the last self-update replaced
└── user/                               # every profile's saves, and emulator user data
    ├── cache/nsz/                      # solid-compressed NSZ games: the last one played, unpacked
    └── lossless/Lossless.dll           # optional: your copy, for frame generation
```

The app itself stays in `/data/homebrew/PPSA99008`; see [Updating](#updating).

ProsperoEden does not include keys, firmware, games, or other copyrighted console data. Dump these files from hardware and software you own. Do not download or redistribute them.

This is the only place it keeps them. If ProsperoEden cannot get access to the console's storage when it starts, the home screen says so and no game can be started; your data is untouched, and closing ProsperoEden and opening it again is the first thing to try. Earlier versions kept a second set of settings and saves in the app's own sandbox in that case, which made saves and settings seem to disappear on consoles where access came and went.

Keys, firmware and games that an early version kept inside the app's folder (`/data/homebrew/PPSA99008/assets`) are moved to `/data/prosperoeden` the first time this version starts, so that replacing the app cannot remove them.

## Updating

Close ProsperoEden first. Your settings, saves, covers and logs are in `/data/prosperoeden`, outside the app, so an update keeps them. Afterwards the About screen shows the version that is running.

Once each time it opens, ProsperoEden asks [homebrew.page](https://homebrew.page/ps5) which release of it is listed there. The request carries the app's title ID and nothing else. If the listed release is newer than the one running, a dialog offers it, with its version and download size:

- **Update now** downloads the release ZIP from its GitHub release, checks it against the SHA-256 in the catalog's signed list, and unpacks it beside the app. A ring shows how far it is and the time left; Circle cancels, and nothing has changed until the end. Then ProsperoEden closes, the update helper (`self-updater.elf`, sent to the console's payload loader on port 9021) replaces the app's files, and the console shows a notification. Open ProsperoEden again to use the new version. The version it replaced is kept; see [Going back to the previous version](#going-back-to-the-previous-version).
- **What's new** (when the release has notes) shows what changes in it before you decide: the release notes as homebrew.page lists them. Up/Down scroll, L1/R1 jump a page, Cross updates and Circle goes back to the offer. Triangle opens them from the offer too.
- **Skip** keeps the version you have. The dialog shows again the next time ProsperoEden opens, not when you come back from a game.

To stay on a version without being asked at every start, turn off **Settings > Diagnostics > Update notices** (`"system": { "update_notices": false }` in the settings file). ProsperoEden still asks homebrew.page, but neither the dialog nor the notification below shows; the About screen names the newer version beside the one running. It applies the next time ProsperoEden opens.

If the download or the unpacking fails, the dialog says why and offers **Try again**; the app stays as it was. Without a network, or without an answer, nothing is shown. When ProsperoEden cannot install the release itself (no payload loader, or an install it cannot find), the menu says **Update available** at the top right for ten seconds instead, and the steps below still work:

- **Folder install.** Copy the `PPSA99008` folder from the new release ZIP over `/data/homebrew/PPSA99008`, replacing the files it has, then start ProsperoEden. Files you put there yourself, such as `language.txt`, stay.

## Going back to the previous version

When ProsperoEden updates itself, the files of the version it replaces are kept in `/data/prosperoeden/previous-version/PPSA99008-backup`, until the next update replaces them in turn. If a new version misbehaves, you can put the previous one back:

1. Close ProsperoEden.
2. Copy everything inside `/data/prosperoeden/previous-version/PPSA99008-backup` over the app's folder (`/data/homebrew/PPSA99008`), replacing the files it has, for example over FTP.
3. Start ProsperoEden. The About screen shows the version that is running.

The update will be offered again the next time ProsperoEden opens; choose **Skip** to stay on the version you put back. Settings, saves and game files are not part of this: they stay as they are. An update done by hand, by copying a release over the app's folder, keeps no previous version.

## Upgrading from an earlier alpha

Earlier versions read everything from `/data/homebrew/PPSA99008/assets/`. That folder keeps working until you choose a game files folder, and settings are migrated automatically on first launch. To move to the new layout, move `assets/keys`, `assets/firmware` and `assets/roms` into any folder, then select it in **Settings > Game files**. The release ZIP contains no user files, so copy its app files over your installation without deleting your own data.
