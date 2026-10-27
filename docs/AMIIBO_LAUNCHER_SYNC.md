# Launcher Amiibo catalog/photo synchronization

Feature workstream: `team/amiibo-sync-1006000`. This adds the console C++ worker and
Settings > Amiibo flow, without Python/subprocesses on the console. It does not qualify
the feature on PS5 or change release/version metadata.

## User flow and privacy

- Open **Settings > Amiibo** to read the offline catalog only. There is no startup sync
  and no network request when browsing. Cross on **Download / refresh public catalog...**
  opens an explicit confirmation naming both public hosts and the artwork rights boundary.
- Once names are cached, navigate the model list (D-pad; L1/R1 page six rows). Selecting a
  model opens a separate photo confirmation with its exact full 16-digit model ID. The
  built-in UI fetches one selected photo per run; the backend can accept at most 64 explicit IDs.
- Circle declines a confirmation or cancels an active worker. The dialog stays open during
  cancellation. Transfer bytes, phase, completion, failure and offline-retry paths are shown.
- The sync worker never discovers/opens dumps or keys, reads tag bytes, calls NFC, uploads
  files or generates tag data. An API response's `image` URL is never a request or filename.
  IDs identify; folders organize; PNG illustrates. The existing quick menu reads the cache
  on its next opening and retains paired/cover/custom-image priority.
- Public metadata: `https://amiiboapi.org/api/amiibo/`. Photos: the existing upstream
  `raw.githubusercontent.com/N3evin/AmiiboAPI/master/images/icon_{head}-{tail}.png` template.
  The built-in and Lua surfaces do not accept arbitrary source URLs. C++ request fields
  support explicitly configured HTTPS mirrors without changing the transport safeguards.
- No artwork is bundled. The API software's MIT license does not grant blanket rights to
  third-party artwork. Only an explicitly confirmed user request fetches photos.

## Preservation and storage

- `amiibo/amiibo.json` is user-owned and **never written** by console sync. Remote names go
  into the separate, reserved `amiibo/catalog-sync.json` cache. The real catalog reader merges
  these with local names/series taking priority by full model ID. Broken/missing downloaded
  metadata does not suppress a valid local catalog.
- Only `images/icon_<head>-<tail>.png` can be created. All existing photos, including invalid
  or empty custom files, are preserved. Invalid cached art fails the requested run rather
  than silently replacing it. Paired PNGs and `cover.png` are never touched. A regular photo
  appearing during HTTP also wins atomic create-only publication.
- Collection ancestors are opened descriptor-relative without symlinks or `..`; the worker
  retains collection/images descriptors throughout staging and publication. Symlinks/FIFOs/
  nonregular cache targets are rejected. Windows junctions are not relevant to the console
  POSIX backend; the existing PC helper retains its Windows reparse-point checks.
- `.amiibo-sync.lock` uses a separate advisory descriptor lock to reject another cache
  publisher. It is not an NFC lock and never shares live NFC descriptors. An external tool
  editing the reserved downloaded cache should honor the lock; do not edit/rename this
  cache during sync. Descriptor anchoring prevents an ancestor replacement redirecting writes.
- Each staging file is exclusive/no-follow, flushed and atomically published. Photos use
  `linkat` create-only publication; metadata uses `renameat`. All requested responses are
  validated before any publication. Failure/cancellation before publication leaves existing
  cache bytes unchanged. Publication is **per file**, not a multi-file transaction: disk failure
  or cancellation between commits can leave already-published new photos, which remain valid
  offline. Original dumps, keys, local metadata and custom art are never rolled back/deleted.
- Temporary files owned by a live run are removed by RAII on failure/cancellation. Abrupt
  process/power loss can leave bounded hidden staging files; automatic deletion of unknown
  pre-existing files is intentionally not performed.

## Bounds and async lifetime

`amiibo_sync_format.cpp` ports strict compact/API catalog validation: 2 MiB, 4,096 entries,
65,536 parser events, depth 16, unique keys/normalized IDs, UTF-8 JSON and 128-byte labels.
Downloaded metadata is normalized to names/series/full IDs, stripping unrelated API URLs/data.
PNG validation covers signature, chunk names/order/CRC, palette/transparency, dimensions
(1,024 per axis), supported bit depth/color, exact bounded zlib scanlines (including Adam7),
filter bytes and trailing/truncated streams. Input is at most 4 MiB. The existing private
PNG thumbnail decoder independently applies its own allocation/pixel limits before rendering.
No worker/GPU coupling or loading of every photo is added.

HTTP uses the existing console libcurl/CA setup with identity encoding and 10-second connect
limit. The new optional `no_redirect` transport field defaults off for existing callers; sync
follows at most five manually inspected HTTPS, same-origin redirects, under one 60-second
deadline per response. Credentials/fragments/control characters and malformed ports are
rejected before requests. HTTP 200 and complete bounded bodies are required. Batch work is
budgeted to ten minutes checked between phases and in body callbacks; an in-flight transfer
still has its own 60-second deadline. Cancellation is atomic and checked by libcurl progress,
body callbacks, staging and between file commits. Filesystem flush/OS resolver latency remains
platform-dependent; no hard PS5 latency claim is made.

`AmiiboSync` owns its request strings, result and worker. Start/Stop are serialized, busy starts
are refused, and Stop closes admission, cancels and joins before destruction. The service's
separate path mutex serializes request admission, file-folder changes and launch commitment.
Folder changes are refused while busy. `can_launch_game` refuses a busy worker; `will_play`
also stops/joins it defensively. Workers never read mutable global paths or retain launcher,
metadata-bridge, Lua-VM or NFC references. A new service owns a fresh worker after gameplay.

## Optional Lua contract

Capability: `amiibo.catalog.v1` (no API/release version change).

```lua
local s = host.query("amiibo_sync", {offset=0, limit=128})
-- s: phase, error, folder, busy, offline, generation, done, total,
--    photos, reused, entry_count, offset, entries[{id,name,series}], confirmation_pending
-- Pages default to 128, maximum 256; offset 0..4096.

host.action("amiibo_sync.offline", {})  -- no network; refresh local catalog
-- From a real input handler, after describing the intended selection:
host.action("amiibo_sync.start", {ids={"0000000000000001"}, confirmed=true})
-- Omit ids for catalog only. An accepted action opens HOST-OWNED confirmation; it is not
-- authorization to fetch. A separate physical Cross, consumed by the native host view,
-- starts the request. Circle declines. Init/update/draw cannot start a network operation,
-- even if a pack supplies confirmed=true. The pack is suspended while confirmation is open.
host.action("amiibo_sync.cancel", {})
```

The Lua host cannot request dump filenames, change cache paths or supply image URLs. During
an active worker the normal launch-commit boundary refuses game launch. The builtin and Lua
paths use the same real service/worker on console; preview service data is explicitly synthetic.

## Focused verification

Run only in the existing Docker image, using this workstream's private volume:

```powershell
docker run --rm --name prosperoeden-team-amiibo-sync-suite --cpus=2 --memory=3g --pids-limit=256 `
  --cap-drop=ALL --security-opt seccomp=unconfined --entrypoint setarch `
  --mount type=bind,source=D:/leonardommello/PS5/ProsperoEden/.local/agent-worktrees/amiibo-sync,target=/checkout,readonly `
  --mount type=volume,source=prosperoeden-team-amiibo-sync-20261027,target=/tmp --workdir /checkout `
  prosperoeden-host:features x86_64 -R bash /checkout/tools/docker/run-amiibo-sync-tests.sh
```

The TSan startup workaround follows `docs/BUILDING.md`: use the relaxed syscall filter only
with this trusted checkout, no extra capabilities, and its private volume. Docker's ordinary
seccomp/ASLR combination can intermittently prevent TSan initialization; that is not a passing
test. The standalone backend check needs only loopback networking and can use `--network=none`.

- `tools/check-amiibo-sync.py`: standalone actual C++ worker/transport/format code, synthetic
  PNG/catalog fixtures, confirmation gate, offline/cancel/failure/no-clobber, staging cleanup,
  path/FIFO/link/lock rejection, unknown/oversized ID requests, value-owned stale configuration
  and shutdown. Normal, ASan/UBSan and TSan variants are selected by the script.
- Actual `remote/http.c` libcurl over private loopback TLS exercises catalog + photo + offline,
  same-origin redirects, cross-origin/downgrade/loop refusal, certificate trust/hostname failures,
  chunked/truncated/oversized/error responses and cancellation. No public service/art is fetched.
- The real catalog Parse/Read/Find source section is compiled separately for downloaded-cache
  fallback/local-name precedence and invalid/symlink downloaded-cache cases. NFC-dependent
  functions are excluded from this focused check, not replaced with fake layouts. The existing
  core/NFC catalog suite remains the coordinator's integration regression gate.
- `tools/launcher/test-amiibo-sync-ui.py`: actual builtin/Lua controllers + Mesa renderer at
  720p/1080p; simulated service only. Tests startup/offline/confirmation/progress/photo-ID/cancel/
  failure paths plus Lua startup-refusal and host-owned confirmation. JSON report/screenshots
  stay in the private volume under `/tmp/amiibo-ui-results`.

Observed on 27 October 2026 with `prosperoeden-host:features` image
`sha256:e6137ec556fe8e63ebf25e7fabe36e573d089c469d0e9d391c3072f2ae04d551`:

- Final actual backend/reader checks passed normally, under TSan, and under ASan/UBSan; the
  actual loopback TLS worker/catalog/photo/offline scenarios also passed with ASan/UBSan.
  An earlier final attempt under default Docker seccomp failed during TSan **startup**, before
  test execution; the capability-dropped `setarch x86_64 -R` rerun passed all three variants.
- Real builtin/Lua controller + Mesa UI checks passed **25 assertions**, at 720p and 1080p
  (simulated service). The UI C++ preview was built with ASan/UBSan; driver leak accounting
  was disabled as in the existing launcher preview workflow. The test initially assumed Lua
  init could save state; after respecting the existing init-action prohibition, the complete
  controller suite passed. No production authorization gate was relaxed to satisfy the test.
- A batched visual inspection of offline list, photo confirmation, progress, failure and the
  native Lua confirmation found readable text/focus/hints without clipping at those sizes.
  The Impeccable mechanical detector returned no findings for the two feature UI includes.
- The JSON report and synthetic screenshots are additionally copied under this worktree's
  ignored `.local/amiibo-sync-evidence/`. They are host evidence, not bundled application art.
- The existing `tools/check-cheat-download.py` normal/ASan/UBSan and real-TLS manager checks
  also passed against the changed shared HTTP transport (default redirect behavior retained).

No full emulator/native build is run by this workstream. PS5 CA/HTTP behavior, POSIX imports,
FAT/exFAT support for descriptor operations/hard links/advisory locks/directory fsync, disk-full
and interrupted-write recovery, actual DNS/connect cancellation latency, rest mode and launch/
NFC/render lifetime must be qualified on the console. Unsupported filesystems fail closed;
they must not be worked around with unsafe overwrite or shell commands.

## Localization handoff (no POT/locale edits here)

New msgids used by this feature (existing generic Select/Back/Cancel/Page are reused):

```text
Amiibo
Optional catalog and photos
Cancelling Amiibo request...
Amiibo sync could not start. Check storage access and try again.
Public names and optional photos. No tags or keys are read, sent or downloaded.
Download the public catalog?
Download this model's photo?
Connects to amiiboapi.org; selected photos come from raw.githubusercontent.com. Artwork may have third-party rights. Existing local names, dumps and pictures are kept.
Confirm download
Downloading public catalog...
Downloading selected photo...
Saving validated cache...
Checking local catalog...
Cancellation keeps local data. A file already saved stays available offline.
Download / refresh public catalog...
Use offline catalog
Select to fetch photo; existing art is kept.
{0} public models available offline
Request failed. Use offline catalog or retry.
Request cancelled. Local data was kept.
Catalog ready. Photos are fetched only when selected.
No network activity until you confirm a download.
Finish or cancel the Amiibo request before starting a game.
Amiibo download confirmation
This launcher requests public names and optional photos. No tags or keys are read, sent or downloaded.
Public catalog only
{0} explicitly selected model photos
```

Backend/HTTP errors remain technical English, following existing service patterns. Shared
integration touchpoints: `CMakeLists.txt`, `remote/http.{c,h}`, services, launcher/settings,
runtime capability/query/action routing, preview hooks, and the local catalog reader. The
coordinator should merge these small hunks with other workstreams rather than replace files.
