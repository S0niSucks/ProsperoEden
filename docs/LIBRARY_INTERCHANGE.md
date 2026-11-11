# Local library interchange

Encore can write `/data/prosperoeden/library.json` after a complete local library
scan with filesystem access. Publication itself does not launch or validate an
external forwarder; the separate [forwarded launch boundary](FORWARDER.md) owns
argument handling. Existing PR #292 schema-1 fields remain compatible.

```json
{
  "version": 1,
  "app": "PPSA99008",
  "game_files": "/data/prosperoeden",
  "games_folder": "/data/prosperoeden/roms",
  "games": [
    {
      "file": "Example/main",
      "path": "/data/prosperoeden/roms/Example/main",
      "title": "Example",
      "title_id": "0100000000000001",
      "cover": "/data/prosperoeden/covers/example.tga"
    }
  ]
}
```

- `version` is the schema version; compatible additions retain these fields.
- `game_files` retains Encore's selected game-files root. The additive optional
  `games_folder` identifies the actual ROM directory when independent locations
  are available. `file` is relative to that directory (otherwise `game_files/roms`);
  `path` is the full local path. Selecting a folder is not data migration.
- `title_id` is 16 uppercase hexadecimal digits, or `null` when unavailable.
  `cover` is a local TGA path, or `null` when unavailable.
- Only the local scan is exported, not downloadable catalogs, credentials, key
  contents or saves. The file **reveals installed game names and local paths**:
  treat it as user data, not a release asset.
- Failed/canceled scans preserve the previous snapshot; a complete empty scan
  replaces it with `games: []`. Missing filesystem access leaves it untouched,
  so a previous snapshot may be stale.
- Identical text is not rewritten. The writer caps output at 16 MiB and uses
  exclusive staging and rename, avoiding partial writes seen by live readers.
  Existing links, hard links and special destination files are rejected. This
  is **not** a crash/power-loss-safe save transaction or a sandbox against
  concurrently replaced ancestor directories.

Implementation: `headless/library_file.h`, invoked by the local scan worker in
`headless/prosperoeden/eden_services.cpp`. Host witness:
`python3 -B tools/check-library-file.py`. Native/PS5 validation is pending.
