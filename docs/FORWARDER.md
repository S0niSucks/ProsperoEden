# Forwarded launch helper (Encore)

`headless/forwarded_launch.h` provides bounded argument parsing and local-ROM
admission. It does not launch a process, load a game, persist settings or implement
a forwarder maker. Native argument delivery and already-running-process behavior
remain **unverified**; host tests do not establish either.

## Arguments

- `--rom X` or `--rom=X`: request one selection.
- `--exit-after-game`: modifier for that forwarded session only. Alone it must
  not exit the app. The caller decides normal-completion behavior after teardown.
- Unknown tokens are ignored. Scanning starts at index zero, accepting both an
  ordinary argv with executable name and a raw vector beginning with a flag.
- At most 64 arguments, each at most 1024 bytes, excluding its NUL terminator.
  Empty ROM values, flag-like ROM values beginning `--`, missing values, duplicate
  ROM/exit flags (even identical), `--exit-after-game=...`, control bytes/DEL,
  negative argc and null entries within argc invalidate the **whole** request.
  Zero argc is an empty request, including with null argv; positive argc with
  null argv is invalid. Invalid results clear both ROM and exit modifier.
- Strings are not trimmed, URL-decoded, case-folded or normalized. Backslashes,
  colons, embedded NUL and unsafe path components fail admission. Whitespace in
  a real filename is preserved; trailing CR/LF never repairs an invalid argument.

## Admission

Pass the already selected `Eden::GamesDir()` as `roms_dir` to
`Eden::ResolveForwardedRom(rom, roms_dir)`. It returns an existing loader-input
path, or an empty string on failure. The root must be absolute, non-root-only,
bounded, and have no trailing slash, empty/dot/traversal component or unsafe byte.

Admitted identities are a strict subset of current `ValidRomFilename`:

- A top-level file recognized by `IsGameFile`: NSP, XCI, NSZ, XCZ, NRO, NSO, NCA,
  NAX or KIP, including mixed-case extensions. Direct-loader files may still
  require additional content; admission is not a successful-load promise.
- Exactly `Folder/main`, with a real folder containing real `main` and
  `main.npdm` files, using `IsDeconstructedGameDirectory`.
- An absolute spelling of either selection **under that exact selected ROM
  root**, at a component boundary. Outside paths and alternate root aliases are
  rejected, even if a basename would otherwise be valid.

Names remain 5–255 bytes. Ordinary nested files, arbitrary nested ExeFS paths,
schemes, wrong-type nodes, filesystem errors, symlinks in any ancestor/member,
and multi-link regular files are rejected. No persisted-name rule is widened.
Missing `Game.nsp` does **not** fall back to `Game/Game.nsp`, even if it is the
only match: that identity is inadmissible in Encore. There is no compatible
one-level moved-file fallback under the current contract, and another folder's
`main` is not proof of the missing title's identity. An existing admitted file
is used directly, without scanning or choosing another title.

Admission inspects metadata without opening ROM contents or enumerating the
library. It is bounded by argument/path length, not by library size. It is **not
race-free**: the standard-library API cannot hold no-follow directory/file handles
through the later native loader open. The tree must remain quiescent through
loading. Permission to inspect metadata is not proof that ROM bytes are readable.

## Native integration contract

1. Parse once with `ParseForwardedArgs(argc, argv)`; check `valid()` before using
   either field. A nonempty valid `rom` is the only forwarded request. Errors
   should produce bounded generic feedback, not log arbitrary raw argv bytes.
2. After filesystem/storage selection is established, resolve against that ROM
   root. Recovery/crash notices and explicit development boot intent must keep
   precedence. On failed admission, show launcher feedback; do not try the raw
   path or a different root.
3. Consume the request once **before** feeding the existing title-aware native
   selection/session route. Retain renderer/input/profile/glyph setup and normal
   successful-load history writes (`RememberedRomName`, `SaveLastGame`,
   `SaveRecentGame`). Do not call a core loader directly.
4. Only a normally completed forwarded session with its exit modifier may exit,
   after existing teardown. Loader/backend/fault failures return via existing
   diagnostics; no retry, replay or guest-fault restart is introduced here.

`main.cpp` now parses once after storage initialization, suppresses unattended
forwarding after a previous crash, preserves explicit developer-boot precedence,
and consumes the request through existing selection/per-title setup. Invalid
requests return to the launcher; the exit modifier is considered only after
normal session teardown. The host helper fixture does not compile that complete
selected-core/native lifecycle. Real argv delivery and lifecycle still require
a separately observed native forwarder launch. Library interchange is in
[LIBRARY_INTERCHANGE.md](LIBRARY_INTERCHANGE.md).

## Focused host fixture

`tools/check-forwarded-rom.py` compiles the **actual helper** with only the C++
standard library, plus the current persisted-name validator extracted from its
source to check accepted-identity compatibility. Synthetic local files exercise
parsing bounds, all recognized extensions, ExeFS membership, absolute containment,
traversal/control/long inputs, permissions, special nodes, symlinks/hardlinks,
existing-file preference, and refusal of otherwise unique/ambiguous nested moved
matches. It runs only in Docker; no SDK, core or vendor source is inspected.

From an approved Docker environment (do not run a host compiler):

```sh
docker run --rm --pull never --network none --read-only --cap-drop ALL \
  --security-opt no-new-privileges --user 65534:65534 \
  --mount type=bind,source="$PWD",target=/repo,readonly \
  --tmpfs /tmp:rw,exec,nosuid,nodev,size=256m \
  --workdir /repo --env PYTHONDONTWRITEBYTECODE=1 \
  prosperoeden-host:network-features python3 -B tools/check-forwarded-rom.py
```

The fixture is host evidence only, not native argv, loader compatibility or PS5
hardware qualification.
