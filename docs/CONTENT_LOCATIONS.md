# Independent content locations

The application-side implementation provides **Settings → Storage** properties
for the existing game-files root, a games-only folder, and a wallpaper folder.
Selecting a location never copies/moves saves, games, keys, firmware or mods.

**Native availability:** the new descriptor-based content I/O is deliberately
unavailable by default on PS5 until its native libc/filesystem imports and
behavior are qualified. The UI says **Unavailable** and preserves existing
storage. `EDEN_NATIVE_USER_CONTENT_IO_QUALIFIED` is a qualification gate, not a
firmware workaround or a switch users should enable to obtain support. Host
fixtures do not authorize enabling it. Existing game-files selection is unchanged.

Where the content provider is available:

- Games chooses the directory containing existing supported top-level files
  and single-folder ExeFS selections. Keys/firmware/mods/saves keep their owners.
- Wallpapers offers folders plus PNG/JPEG/TGA files. Cross on a file selects it;
  Triangle saves a folder without automatically choosing an image. Square resets
  only that property. Missing or corrupt artwork falls back to packaged art.
- All location/image changes apply after fully reopening the app; a running
  reader cannot silently follow a replacement root. A missing games folder is
  unavailable, not a reason to launch a different same-name title elsewhere.

Saved JSON uses `storage.games_folder`, `storage.wallpapers_folder` and the
relative `storage.wallpaper` filename. Empty locations follow `<game-files>/roms`
and `<game-files>/wallpapers`. Paths use the existing absolute `/data`/`/mnt`
user-data grammar. No symlink ancestry, special files, hardlink aliases or
arbitrary nested wallpaper path is admitted by the descriptor reader. Directory
selection is not a transaction or a migration.

Independent discovery uses one cancellation-aware descriptor walk for folders
and images, not the legacy Files enumerator: at most 4,096 entries are examined,
with at most 256 sorted folders and 128 sorted images offered from those entries.
The directory stream is scope-owned. Discovery collects private results and
publishes them only after success; allocation/other exceptions close the stream
and leave caller lists empty, like cancellation or a filesystem error.
Encoded files are at most 16 MiB. Codec metadata is checked
before full decoding: at most 4,096 pixels per side and 8,388,608 pixels total.
Decode/upload uses the existing asynchronous texture owner. Custom backgrounds
are aspect-preserving and do not sample stale packaged blur underneath panels.
High contrast may suppress background artwork. Game hero art can cover wallpaper.

New-location discovery and selection admission run on owned background jobs.
Discovery failure, cancellation and unavailability clear both output lists.
Circle requests cancellation; the UI keeps rendering. Reopening Games/Wallpapers
before the old query drains defers only the current navigation request until
drainage; it neither overlaps readers nor leaves an inert empty browser. Leaving
again discards that intent. Cancelled old failures cannot replace newer feedback.
A pending write rejects
other mutations until its owner acknowledges completion. Cancellation is
cooperative between filesystem operations, not a hard timeout for a stalled
syscall. Protected preference/content writes exclusively create an owned
same-directory staging file (mode0600; up to8 O_EXCL/no-follow candidates),
never truncating a pre-existing staging symlink or hardlink. The cancellation
cutoff is the final token check after descriptor writes/close, before rename. Cancellation
observed there prevents publication and removes the temporary file. After that
decision rename is non-cancellable; its success is reported as saved, not undone.
Teardown cancels and joins the owners. This is not power-loss durability.

Appearance/content edits refuse an existing malformed or unreadable top-level
settings document without replacing its bytes. Reads may still show defaults.
A missing document may import legacy values; it is not treated as corruption.
This refusal policy applies to these writers, not a retrofit of every legacy
settings writer in the application.

Stop external FTP/file-manager writers while selecting/loading content. File
descriptors pin ancestry, but timestamp checks cannot detect every concurrent
in-place change or provide authenticated content. No native syscall, USB-removal,
power-loss or PS5 codec qualification is implied by a host fixture.

`library.json` retains its existing `game_files` meaning and adds `games_folder`
for the actual ROM directory. Existing version-1 fields and absolute game paths
remain compatible; no credential/firmware/key material is exported.
