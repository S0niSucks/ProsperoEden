# USB writer admission audit

Exact base: `62e99ba13563ac3d7f12947857c8e072c4afce74`.
Branch: `team/usb-writer-admission-audit-1006000`.
Scope: EdenServices and its existing worker lifecycle, not arbitrary processes writing the console.

## Findings and changes

1. **Check-before-bridge is not a drain.** The base `set_docked`, `set_game_settings`,
   `set_preferences`, mod/cheat setters and `delete_game` read `usb_active_` before acquiring
   `bridge_`. A caller can retain the false observation, pause before acquiring bridge, and resume
   after the USB preparer has closed admission and drained bridge. The base write then occurs in
   the protected session. Settings/room writes, mkdir/save transfer and remote queue changes also
   lacked a shared admission lock. These synchronous operations now take admission before the USB
   check, and bridge for local settings/metadata/file mutation. A pre-admitted caller finishes
   before USB closes admission; a queued caller observes the closed session before I/O.
2. **A worker starter must be published before it can be joined.** Base `StartSync` and
   `start_pairing` could retain a false USB observation and create a thread after the preparer
   stopped/joined the old thread. Public starters now retain admission through metadata capture and
   thread publication. `StartSync` remains an internal, caller-admitted helper for startup and USB
   resumption; workers never take admission. Metadata capture uses bridge, but joins and network
   callbacks do not. Existing workers are stopped and joined before drive discovery/snapshot.
3. **Update Begin could restart after preparer Finish.** `start_update` now retains admission
   around `UpdateNotice::Begin`. `apply_update` uses the same admission. There is **no independent
   demonstrated late-Apply finding**: `UpdateNotice::Finish` clears `begun` under its own mutex, so
   Apply alone cannot resurrect the job after Finish. A late Begin was the actual restart hole.
4. **Remote Stop was a courtesy wait, not confirmed quiescence.** Base `Remote::Stop` returns after
   at most three seconds even if a cover/list/download callback is still completing. Its download
   thread also drains discard files while halted, outside the remote mutex, without an activity
   flag. USB now calls a separate `StopForSnapshot(stop)` API: halt, wait for list/transfer plus
   queued/in-progress discard cleanup, or refuse on cancellation. A new `discarding` flag spans
   the actual unlocked cleanup and wakes drain waiters. Ordinary launch/destructor `Stop()` retains
   its existing bounded behavior. No timeout is mistaken for a safe snapshot.
5. **Some apparent readers write.** `home()` extracts covers/seeds recent history. Settings getters
   can perform legacy migration (`Settings::Load` -> `Migrate` -> `WriteWhole`). Profile resolution
   can initialize/settle selection. Preset catalog reload can install defaults. These entry points
   now obey admission too; during USB they return an inert/default view or existing preset labels,
   without calling those dependencies. Normal post-session reads initialize as before. The base
   `games()` already checked USB *inside* bridge and did not have the same late-write bug; it now
   shares admission and refuses before waiting for bridge, consistent with the other writers.

The same lock order is used throughout: **admission -> bridge**. Local filesystem work can hold
bridge; no worker join, HTTP operation or condition-variable wait is performed under bridge.
`connect_public_room()` does not call the newly admitted `remember_room()` recursively. Likewise,
`will_play()` uses the internal `SaveSyncWanted()` helper while holding admission/bridge instead
of recursively calling public `save_sync_wanted()`. Public signatures remain unchanged.

## Reviewed API inventory at this base

| Group / entry points | USB treatment and transitive behavior |
| --- | --- |
| `set_docked`, `set_game_settings`, `set_preferences` | Admission -> bridge; actual settings writes and performance settings are one admitted operation. |
| `set_mod_enabled`, `set_mods_enabled`, `set_cheat_enabled`, `set_cheats_enabled`, `make_mods_folder` | Admission -> bridge, including all cheat switch branches and mkdir. |
| `save_import`, `save_export` | Enabled-build bodies admitted and bridge-protected; disabled-build stubs remain inert. Source discovery only lists files. |
| `delete_game` | Admission -> bridge covers metadata lookup, removals, and remote generation notification. |
| `connect_room`, `connect_public_room`, `remember_room` | Admitted settings save; connect occurs after bridge release. Room client connection/leave/snapshot only changes memory/network state, not config. |
| `download`, `cancel_download`, `refresh_sources` | Admission spans queue mutation/notification; remote mutex serializes its own queue/files. No bridge held across remote calls. |
| `start_update`, `apply_update` | Admitted; no bridge across kit calls. Take/Poll only read job state; Cancel/Finish cannot restart it and serialize with preparer Finish using UpdateNotice's mutex. |
| `start_save_sync`, internal `StartSync` | Admitted public starts, bridge-bounded metadata capture; preparer signals/stops/joins. Constructor and USB resume use the internal helper without recursively acquiring admission. |
| `start_pairing`, `unlink_profile` | Start retains admission through reconciliation/thread publication; previous thread joined outside bridge. Unlink is admitted -> bridge. Pair worker is joined through final backend/config writes. |
| `will_play`, `save_sync_wanted`, `save_sync_setup` | Pending write remains admitted; profile-resolving query paths now admitted/bridge-protected. Launch still refuses USB/Nextendo before commitment. |
| `choose_save_data`, `end_save_sync`, `stop_save_sync`, `pairing`, `cancel_pairing` | State/signal operations only; never publish a new worker. Existing worker lifetime is drained, not inferred from cancellation. |
| Profile list/key/choose/add/rename/remove; both root setters; Nextendo action/status | Existing composed admission retained. Credential exclusions do **not** waive writer quiescence. |
| Amiibo/Cheat starts/install | Existing admission retained; Stop finishes their callbacks/cache publications. Cancel/status only signal/read memory. |
| `home`, `games` | Admitted cover/history/catalog/metadata writers, including implicit settings migration. |
| Preferences/game settings/docked; mods/cheats and their enabled queries; room connection; saved folder queries | Admitted -> bridge because settings reads can migrate. |
| Preset labels/shared count, wallpaper list/folder | Admitted -> bridge for lazy catalog/path initialization; private `ReadPostPresets` is called only from admitted entry points. |
| `sources`, `downloads`, title cache helpers | Read existing remote state/catalog and update only in-memory service caches; they do not enqueue/start/write settings. |
| Game path/existence; file/folder info/listing; resolved root paths; setup/crash/image paths; framegen availability | Filesystem reads only. `AssetsDir`/`GamesDir` are resolved in the constructor before services are published. Wallpaper lazy settings resolution is separately gated above. |
| Clock/controller/vibration/version/labels; room text/seed keyboard | No protected-tree mutation; unchanged. Keyboard calls are never under bridge. |
| USB begin/actions/end/cancel/status/resume | Admission closes atomically; existing workers drain before drive discovery. Action path/confirmation/restart guards retained. End joins before reopening, never reopens staged restore. |

### Startup and lifetime boundaries

The real constructor creates config/wallpaper directories, resolves profiles, reconciles save-sync,
starts remote sources and optionally pending SaveSync **before publication**. It is not concurrent
with a USB service call. Frontend `UpdateNotice::Start()` is called once, before the input loop can
open USB; the preparer already waits for that catalog checker. No late Start caller was found in
this base, so no invented checker race or frontend change is included. The destructor does not
start writers and runs after UI-owned jobs are finished. New network/theme branch APIs are outside
this frozen base; the coordinator must apply the same rule when integrating them.
Both built-in and runtime launch paths start/retry the before-game sync before `commit_launch()`
calls `will_play()`. Refusing a new public sync/pair start after terminal commitment does not remove
their normal pre-launch sync/retry path. Constructor/USB-resume pending work remains grandfathered.
`self_update_poll` updates progress only. The separate update helper stages/cleans under the install
drive's `self-update/`, not the launcher data root; its `previous-version` data-root publication
occurs only after it confirms the app has closed. `Finish` joins the app's kit thread, not that
external helper process. This audit does not turn that join into a claimed helper-process barrier.

## Reproducible evidence

`tools/check-usb-writer-admission.py` compiles verbatim production method/helper bodies and real
class/member declarations (inheritance/vtable removed only to avoid linking unrelated
console methods). Two test-only observed types pause at mutex acquisition or **after** an atomic
USB load; they do not change production guards. The latter models a schedulable retained-false
observation. The settings, mod listing/switching, profiles, reconciliation, pending JSON and
service-owned SaveSync/pair threads are real. Console metadata, preset renderer, room connection,
kit/backend transport are synthetic and use local fixture files; they do not make network calls.
The new executable does not link `home()`/`games()`' console metadata pipeline; their entry guards
and transitive helpers were source-reviewed. It is not a full-class/native link or a claim that
every menu branch/console dependency ran. Enabled save-transfer guards run against synthetic native
import/export callbacks; the actual console save-transfer engine is not replaced or qualified.

The remote worker test includes the exact remote source, runs its real list/download threads,
holds a synthetic source callback, and intercepts the actual OS removal during discard cleanup.
It is not a replacement worker-state model. Both cancellation refusal and successful drain through
the final file publication/deletion are asserted. The ordinary idle drain is asserted as well.

The optional baseline proof runs the same method bodies from saved exact-base sources, asserting
actual changed fixture files **after** preparation, not simply a fake busy flag:

- Nine reproduced service entry points: `set_docked`, `set_game_settings`, `set_preferences`,
  `set_mod_enabled`, `set_mods_enabled`, `set_cheats_enabled`, `start_update`, `start_pairing`,
  `start_save_sync`. Late Apply alone was tested and did not restart after Finish.
- These schedules pause after the preparer drain, not inside a physical USB copy. There is no claim
  that a bridge-protected setter can write through an already-held copy bridge. The demonstrated
  gap is mutation during the supposedly closed/drained session. The service fixture uses the
  production USB include with a synthetic remote drain; the exact-base real remote Stop/worker
  failures are reproduced separately, not inferred from that stub.

```sh
git show 62e99ba:headless/prosperoeden/eden_services.cpp > /work/base-services.cpp
git show 62e99ba:headless/remote/remote.cpp > /work/base-remote.cpp
TMPDIR=/work python3 /repo/tools/check-usb-writer-admission.py \
  --baseline /work/base-services.cpp --remote-baseline /work/base-remote.cpp
TMPDIR=/work python3 /repo/tools/check-usb-writer-admission.py
TMPDIR=/work python3 /repo/tools/check-launcher-admission.py
```

Run only in `prosperoeden-host:features`, read-only sources, private volume
`prosperoeden-team-nextendo-20261027`, **2 CPU / 3 GiB**, `--network none`, disposable
`--tmpfs /data:rw,size=128m --tmpfs /mnt/usb0:rw,size=128m`. The writer fixture refuses to run unless
both constant-path mounts are tmpfs. TSan uses the existing test-only
`--security-opt seccomp=unconfined` workaround for LLVM's ASLR personality call, not disabled checks.
No extra capabilities, toolchain installation, credentials, external network or physical USB.

### Final validation — passed

All commands above completed successfully, followed by these regressions in the same isolated,
capped container. No sanitizer findings were reported.

| Check | Result |
| --- | --- |
| Exact-base service + remote worker proof | Nine late service writes and both remote drain failures reproduced; independent late-Apply no-finding confirmed. Saved sources match exact-base Git blobs. |
| `check-usb-writer-admission.py` | Normal, ASan+UBSan, TSan PASS for service and actual remote worker suites. Nine pre-admitted callers drain; 22 queued mutator/start entry points refuse before I/O; held SaveSync/pairing callbacks finish before preparation; tested idle operations and migrating readers retained. |
| `check-launcher-admission.py` | Normal, ASan+UBSan, TSan PASS; existing composed Nextendo/Store/PKCE/Amiibo/Cheat/USB assertions retained, not replaced. |
| `tools/docker/run-usb-backup-tests.sh` | ASan+UBSan filesystem/service/update-check tests PASS, plus native USB/header/UI syntax checks; synthetic exclusion/alias/restore-injection tests retained. |
| `check-remote.py` | PASS against local synthetic RomM/FTP servers; expected error cases included. |
| `check-nextendo.py --source /pinned` | PASS: pinned derivatives, account/storage/routing/cancellation, TLS, actual adapter and curl/native contracts. |
| `check-nextendo-oauth.py` | All listed cases PASS with actual loopback/HTTPS, S256, strict TLS, parser/storage/cancellation and duplicate-field regressions. |

Private complete log: `.local/usb-writer-evidence/final-regressions.log` in this worktree.

## Limits / integration

This is not a full native/core build, PS5 filesystem/browser/network qualification, or proof against
an external FTP client/process or a future unadmitted writer. A backend that never returns can keep
USB preparation pending; cancellation refuses the snapshot rather than claiming an idle backend.
Logs/runtime buffers remain outside the snapshot's content contract. Excluded credentials are still
quiesced as described above. No online-play claim is made.

Production conflict surfaces: `headless/prosperoeden/eden_services.{h,cpp}`,
`headless/prosperoeden/usb_backup_services.inc`, and `headless/remote/remote.{h,cpp}`.
OAuth/store/HTTP/core overlays, renderer, Lua/UI, locales, versions and aggregate docs are untouched.
