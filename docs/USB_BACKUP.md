# USB app-data backup and restore (#157)

## Workflow and scope

The built-in launcher's **Settings > USB backup** offers backup and restore. It pauses app
storage jobs, discovers mounted `/mnt/usb0` through `/mnt/usb7`, and lets the player choose
a drive. A directory on the console's own filesystem is not accepted as a USB drive.

Backup preflight counts files and bytes and shows required/available space **before** a separate
confirmation (Cancel is the default). Required space includes 32 MiB reserve, 16 MiB for
bounded metadata and allocation overhead per entry (at least 64 KiB, or a larger filesystem
allocation unit; doubled for restore). No drive, insufficient space,
unreadable data, unsafe paths and unsupported primitives produce errors, not a partial success.
Copying and independent validation show byte progress and accept cancellation. Cancellation
is checked during enumeration and between payload entries, reads and short-write retries
(256 KiB maximum transfer). Bounded metadata parsing/publication is not interrupted mid-call.
A blocking kernel I/O call or a background network job's shutdown cannot be forcibly interrupted.

The scope is exactly `/data/prosperoeden`, not the install folder or externally configured
Game files/ROMs/wallpaper folders. Settings preserve their saved absolute paths; the files those
paths point to outside the data root are **not** silently included or relocated. All profiles,
account/device/system saves and their indexes, keys/firmware when inside the root, custom art,
mods, settings and existing save backup folders are included. Backups can contain private
service tokens and user-provided sensitive files: use a private drive. They are not encrypted.

Excluded game content, regenerable data and explicitly named credentials:

- `roms/`, `updates/`, `switch/` and `.remote-downloads/`;
- `logs/`, `cache/`, `radv-shader-cache/`;
- `user/cache/`, `user/log/`, `user/shader/`;
- the regenerable published `library.json` catalog;
- the entire secret `config/nextendo/` subtree, including imports, account/session files,
  routes/OAuth configuration and all unknown, hidden or staging names; it is excluded before
  opening/enumerating the directory, not inspected for tokens or selectively filtered;
- the exact `config/network-shares.json` credential node, which can hold plaintext SMB/NFS
  passwords; if supplied as a directory, none of its children are traversed either;
- files ending in `.nsp`, `.xci`, `.nsz`, `.xcz` or `.nro` (case insensitive), anywhere in the tree.

Custom covers and wallpapers **inside the data root are kept**, not treated as disposable caches.
Both explicit credential policies conservatively fold ASCII case and trailing-dot/space aliases
on their first two components, covering case-insensitive USB namespaces without changing nonsecret paths.
Dot segments, backslashes, absolute paths and malformed components are rejected, not resolved.
Restore manifests cannot reintroduce either credential node; progress/diagnostics never scan or
hash their contents. Harmless siblings such as `config/network-shares.json.bak`,
`config/network-shares-public.json` and `config/nextendo-public.json` remain included. There is
no heuristic scanning for passwords/tokens and no exclusion of unknown config files generally.

## Backup format and safety boundary

Each backup exclusively creates a new `ProsperoEden-YYYY-MM-DD_HH-MM-SS-N` directory (UTC,
collision suffix), with `data/`, `manifest.json` and a last-published `COMPLETE` marker.
Existing folders/files are never opened for truncation. An interrupted backup keeps its dated
partial directory but is never offered as completed; it can be removed manually when the app
is closed. The last marker contains the SHA-256 of the versioned manifest. The manifest
contains relative paths, file/directory types, byte sizes and a SHA-256 for every file. Backup
completion independently rereads the output and compares its full inventory and content.
Restore revalidates inventory/content before its estimate, again before staging and after copying.
Confirmation is bound to that inspected manifest digest, not just its folder name.

No archive extraction or path-following filesystem recursion is used. Absolute paths are walked
from directory descriptors; every child uses `openat` + `O_NOFOLLOW` and regular-file/directory
checks. Symlinks, hardlinked files, FIFOs/devices, nested mounts, unsafe components, excessive
depth and duplicate/unordered manifest paths are rejected. File I/O is bounded to 256 KiB;
manifests are bounded to 16 MiB, JSON depth <16, total entries 100,000, relative depth 32 and
relative path length 1,024 bytes. Native enumeration uses bounded 64 KiB kernel directory
reads, without the existing iterator's followed-stat fallback. There is no fallback that follows
links or pretends that unsupported primitives succeeded.

This is an app-private local-filesystem protocol, **not protection against a privileged process
actively modifying app-owned directory descriptors/contents during a transaction**. The workflow
explicitly asks the user not to write app data over FTP. External writers cannot be locked by
this app. Source changes detected during copying cause failure; app writers are coordinated below.
SHA-256 detects corruption, not the authorship/trustworthiness of a user-selected backup.

## Restore: stage now, apply before next startup

Restore is a **merge**, not a destructive snapshot rollback. Files absent from the backup are
untouched, so newer/other saves and ROMs remain. The confirmation explains this, restart and
recovery preservation. Console space preflight reserves **two copies** of restored bytes:
immutable staging and installed copies while existing bytes remain preserved. Copying the USB
backup only creates a new transaction under `/data/.prosperoeden-restores/restore-<UTC>-N/`.
No active settings, profile or save is replaced.

After validated staging, `plan.json` records the expected inventory and which target files
existed. An exclusive, synced `pending.json` is published in `.prosperoeden-restores/` only
after the stage is durable. The launcher then remains locked with a close/reopen requirement,
shows the recovery path and keeps jobs paused. USB removal is safe after this state: startup
uses local staging, not USB. Cancellation is no longer offered after this publication boundary.

`main.cpp` invokes `ApplyPending` immediately after filesystem access and before migration,
directory creation, profile/settings resolution, metadata, workers or the emulator. It validates
the pending plan and staged content again, moves existing files into the transaction's
`previous/<original relative path>` tree, then durably installs verified files. It never deletes
unmentioned data. Staging remains immutable so interrupted per-file renames/copies can resume.
Already installed files must match their expected hashes; an unexpected target stops startup.
Only after all files are installed does it write `APPLIED` and withdraw `pending.json`.
Failures block startup rather than letting a half-restored app run. With no pending marker,
ordinary startup does not require `fstatat`/`lstat` support.

The transaction directory (including `previous/`, `data/`, `plan.json` and `APPLIED`) is retained
for manual recovery and inspection; there is no automatic expiry or silent cleanup of prior
restores. While the app is closed, originals can be copied back from `previous/` to their exact
paths. New files that had no previous counterpart are identifiable by `had: false` in the plan.
Do not delete a pending transaction. On an I/O failure, reconnect/fix console storage and reopen
to resume. If a pending marker/plan is damaged, preserve the entire transaction and existing data
before manual recovery; do not bypass the startup gate on an unexamined mixed state. A crash
during marker creation can leave a malformed marker: fail-closed/manual recovery is intentional.
If live publication fails and its pending marker cannot be durably withdrawn, validated staging
is retained and the service keeps the same close/reopen lock, with an explicit recovery diagnostic.
It must not resume writers behind a possibly published transaction.

## Lifetimes and coordination

`EdenServices` owns and joins the USB worker. Entering the workflow sets a launch/write gate,
stops/joins the cheat job, download workers, save sync and pairing, cancels/finishes an updater
job and waits for the initial update checker's catalog-sequence write (without holding the metadata
bridge), then drains the bridge. Slow inspection/copy/validation runs on that worker, never on
the UI thread. The UI suspends library-result application and periodic job launch/poll actions
while its protected modal is open; progress/status copies use a separate short-held mutex.
The outer runtime defers launcher-pack update polling (which persists checked-at settings),
blocks picker/launcher switching and refuses launch completion throughout the USB session.
Settings/profile/save/mod/cheat/download/room/update mutators reject requests during the session.
The service validates action names, offered paths and the last inspected path before starting.
Integration of other workstreams must add any newly introduced app-data writer to the same
stop/join and write gate before enabling it alongside USB backup. Cold-start restore itself runs
before launcher services exist; no running worker is permitted to hot-apply the staged data.

Closing a completed/cancelled/failed backup joins its worker and resumes downloads and pending
save-sync work. A staged restore cannot close back into the launcher. Destruction cancels/joins
the USB worker before disposing of the bridge or other jobs, without restarting jobs.

Coordinator review also guards thread-construction failure. If the initial worker cannot start,
no jobs have been paused: admission is released and the caller receives failure. If a later
inspection/copy/restore worker cannot start, the protected session remains active but is marked
failed/not busy, so it can be closed; it never claims publication or leaves an inescapable busy
modal. The remote-resume flag resets for each new session, rather than leaking across a failed
start. No restore bytes are applied on the launcher thread.

Amiibo/cheat/USB/launch admissions now share one mutex. USB preparation stops and joins both
real cache workers before draining the metadata bridge; no bridge is held over either join.
The first composed test passed the individual Amiibo/TLS suite but exposed that closing USB
left Amiibo permanently closed. Ordinary USB closure now calls `AllowStart()` under admission;
a staged restore never closes and a terminal launch cannot be reopened. Real workers with
held synthetic transports test busy/cancellation, new-job refusal and races against launch/USB.
The corrected clean/frozen `878d97c` snapshot **passed** the individual Amiibo/reader/TLS,
actual composed service/worker, cheat-manager and full USB backend/lifecycle/update/syntax
runners (`sh_17fd7b2c7001OEJMHG3xgCQKtV`), including normal/ASan/UBSan/TSan admission races,
both credential exclusion/reinjection controls and unchanged restart-lock assertions.
The raw logs/snapshot/status are in `.local/docker-release/amiibo-usb-integrated-frozen-20261027/`.
Unrelated platform jobs and admission-test transports/probes are still simulated; dedicated
worker TLS tests use actual libcurl/loopback TLS. This predates Nextendo's shared-service changes
and is not a full frontend/core or PS5 execution. Initial failed evidence is retained under
`.local/docker-release/amiibo-usb-integrated-20261027/`, not counted as a pass.

### Cross-delivery integration handoff

The initial worker recorded the requirements below before the other APIs arrived. Amiibo/cheat
coordination in items 1–2 is now implemented/tested above. Nextendo and later scene/network
writers remain under integration review. Credential exclusion does **not** replace coordination:

1. In this branch's `EdenServices::begin_usb_backup()` (the USB-enter hook), keep the existing
   `usb_active_` gate set **before** scheduling preparation. On the USB preparation worker,
   cancel/stop **and join/drain AmiiboSync** before drive discovery or any snapshot/restore
   inspection. Do this before taking `bridge_`; never hold `usb_lock_`, a profile lock or a lock
   needed by the job while joining. A cancellation request alone is not a drain barrier.
2. Reject every `start_amiibo_sync` request while `usb_active_`, including automatic/background
   restart paths. Preserve the existing early rejection in `set_files_folder`; the composed
   implementation must not switch roots or start/restart AmiiboSync behind that guard.
3. Gate all Nextendo async starts and profile/session/account mutators while `usb_active_`,
   before they resolve paths or write state: profile selection/create/rename/delete, imports,
   authentication/link/unlink/logout and any completion-triggered persistence. Use the delivery's
   real entry points, not guessed method names. Either refuse USB admission atomically while
   an account job owns storage or cancel and join/drain it (including queued writeback callbacks)
   before snapshotting. Detached jobs need a completion barrier, not just cancellation.
4. No drained completion may publish afterward or restart a job during the session. Preserve
   generation/root checks for stale captured paths. Ordinary backup success/failure/cancellation
   may resume eligible jobs only after the USB worker is joined and the gate is released.
   Pending restore (including `RestorePending`) keeps **all** those jobs/starts/mutators locked
   until exit. Destruction joins the USB worker before destroying collaborators and never resumes.
5. Add integrated tests with the **real composed owners/APIs**: block each Amiibo/Nextendo writer
   just before publication, enter USB, prove inspection cannot begin until writes/callbacks drain,
   reject new sync/profile/root-change requests, cancel without deadlock, verify safe resumption
   without stale roots, and exercise destruction plus pending-restore restart lock. The focused
   synthetic service tests here do not claim this cross-delivery lifetime coverage.

## Focused validation and limits

Run from this worktree with the existing image, a read-only source mount and an isolated volume:

```powershell
docker run --rm --name prosperoeden-team-usb-backup-20261027 --cpus=2 --memory=3g --pids-limit=128 --mount 'type=bind,source=D:/leonardommello/PS5/ProsperoEden/.local/agent-worktrees/usb-backup,target=/checkout,readonly' --mount 'type=volume,source=prosperoeden-team-usb-backup-20261027,target=/work' --tmpfs /usb:rw,size=128m --tmpfs /mnt/usb0:rw,size=128m prosperoeden-host:features bash /checkout/tools/docker/run-usb-backup-tests.sh
```

`run-usb-backup-tests.sh`: ASan/UBSan synthetic real-filesystem tests, short writes/EINTR,
no drive/fake mount/low space, failures at read/write/fsync/completion marker, cancellation,
hostile ancestor/file/directory links, hardlinks/FIFO, inventory/content corruption, traversal
and JSON-depth attacks, unsupported safe-stat primitives, pending exclusivity, cold-start merge,
preservation and retries at every preservation/install rename boundary and each post-rename
durability barrier. It syntax-checks the native directory
branch against host headers, the changed UI translation units and the feature service against
the real `EdenServices` declaration (not a complete core/native translation-unit build).
The exact feature service implementation is exercised with synthetic job collaborators and
real filesystem operations: stop/join/resume, path/action/confirmation gates and pending-restore
restart lock. These collaborators do not qualify the real network/core job shutdown on PS5.
The actual update-check lifecycle is separately exercised with a blocked synthetic catalog:
the USB drain cannot finish before the checker completes its sequence-write step, repeated
starts remain idempotent, and the completed offer is still delivered once. No network is used.
Focused credential tests use synthetic bearer/password values with unknown/hidden/staging names,
case and trailing-dot/space aliases, and hostile children that would fail any accidental traversal.
They verify nonsecret siblings are backed up, progress/payload omit secrets, restore leaves live
sessions untouched, and self-consistent manifests are rejected by the exclusion policy before
payload validation; dot-segment/backslash/absolute aliases are rejected as unsafe paths.
They cover exact network-share config files, hostile directory/symlink aliases, absent credential
hashes in manifests, untouched live credentials and file/directory restore-injection attempts.
The final 2026-10-27 Docker run passed the filesystem/Nextendo/network-share, service lifecycle
and actual update-check drain suites under ASan/UBSan with leak detection, plus all focused
syntax checks (complete runner exit 0; a preceding 120-second invocation exhausted its wall-time
budget after the runtime suites, so the complete runner was repeated with a 300-second budget).

Coordinator focused run `sh_17fcaef8c0011cEcLmxb76dgSB` passed the exact service include with
ASan/UBSan, real filesystem operations and real `pthread_create` failure injection (`EAGAIN`),
plus syntax checking against the actual `EdenServices` declaration. It asserts that failed
initial creation neither pauses/resumes jobs nor leaves admission closed; failed later creation
keeps the protected session closable and permits a fresh inspection. Existing cancellation,
confirmation, restore-staging and restart-lock assertions remain intact. Snapshot/raw log/status
are preserved under `.local/docker-release/usb-service-failure-sanitizer-aware-20261027/`.

The first injection attempt failed inside ASan's thread bookkeeping: calling libc directly
with `RTLD_NEXT` bypassed Clang's executable-linked interceptor. A minimal program without any
USB/service code reproduced that failure, while the sanitizer-preserving injector passed.
Normal workers in the maintained test now go through the active sanitizer interceptor; only
the deliberately failed creation returns `EAGAIN`. The failed attempt and diagnostic control
are retained under `.local/docker-release/usb-service-failure-integrated-20261027/`; they are
not counted as a passing backup test. Job collaborators remain synthetic, not a full composed
service/console qualification.

`run-usb-backup-ui-tests.sh`: a standalone **launcher-only** renderer build, ASan/UBSan and
720p/1080p controller tests against a clearly synthetic USB service. Results and images go to
the isolated volume under `/work/usb-ui-results`. This is not an emulator/native build or a test
of real PS5 USB/storage/service scheduling. Leak detection is on for the filesystem tests and
off for the renderer's Mesa process-global allocations.

```powershell
docker run --rm --name prosperoeden-team-usb-backup-20261027 --cpus=2 --memory=3g --pids-limit=192 --mount 'type=bind,source=D:/leonardommello/PS5/ProsperoEden/.local/agent-worktrees/usb-backup,target=/checkout,readonly' --mount 'type=volume,source=prosperoeden-team-usb-backup-20261027,target=/work' prosperoeden-host:features bash /checkout/tools/docker/run-usb-backup-ui-tests.sh
```

The clean launcher-only ASan/UBSan run on 2026-10-27 passed **20 controller checks** across
720p/1080p with no GL errors: preflight/Cancel default, launch blocking, cancellation, backup
completion, low-space refusal, corrupt-backup refusal, restore warning/staging and restart lock.
The final renderer run also exercised direct outer picker/launcher-selection attempts during
restart lock. Its report and screenshots are retained at `/work/usb-ui-results` in the isolated
volume; the later network-share change is backend/docs/tests only and did not launch another renderer.
Both resolutions' confirmation and restart screenshots were inspected. An earlier incremental
preview run failed with mixed-object layout evidence; the runner now always builds in a fresh
isolated launcher directory so Windows source timestamps cannot reuse stale ABI objects.

**Not PS5 qualified.** Actual USB filesystem durability, permissions and capacities, rest-mode
and sudden removal/power-loss behavior, `fstatvfs`, descriptor-relative operations, safe-stat,
`fsync` and device identifiers require console evidence. Some native filesystems deny safe-stat:
restore must fail closed there. The UI and backend are implemented; host success is not evidence
that these filesystem primitives work on a particular console/USB filesystem. No release/version
metadata is changed by this feature.

## New localization msgids (coordinator-owned catalogs)

New user-facing `tr`/`TR` strings introduced by this feature are listed here rather than editing
`launcher.pot` or any translation catalog. Existing shared msgids (`Select`, `Cancel`, `{0} of {1}`)
are reused. Backend diagnostic exceptions are displayed verbatim in English, like other storage
job errors; their source of truth is `headless/usb_backup.cpp` / `usb_backup_services.inc`.

- `USB backup`
- `Back up or restore app data`
- `Settings, profiles and all saves on a USB drive. ROMs and caches are left out.`
- `USB backup needs storage access and no game starting.`
- `Settings, profiles and all saves. ROMs, logs and caches are excluded.`
- `Restore staged and verified. Close ProsperoEden, then reopen it.`
- `Do not change app data over FTP. Restore applies before the next startup. Existing files will be kept here:`
- `Validating copied data...`
- `Copying app data...`
- `Preparing and checking data...`
- `Cancel stops this job. Keep the USB drive connected until this finishes.`
- `Back up app data`
- `Restore from a backup`
- `Storage jobs pause here. Do not change app data over FTP.`
- `Choose a USB drive`
- `Choose a completed backup`
- `No completed backups or accessible drives found. Cancel goes back.`
- `Data: {0} in {1} files`
- `Required (with reserve): {0} / Available: {1}`
- `Restore merges backup files at the next startup. Existing files are preserved in a recovery folder. Files absent from the backup stay untouched.`
- `A new dated folder is created. Existing backups are never overwritten. Keep this drive private: backups may contain personal data and service tokens.`
- `Not enough space`
- `Stage restore`
- `Create backup`
- `Backup complete and verified.`
- `Operation stopped. Existing data was kept.`
