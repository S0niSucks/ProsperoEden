# Network / USB admission composition (939e447 follow-up)

Private branch/worktree: `team/network-usb-compose-1006000` / `network-usb-compose`, based on
`939e447`. This work does not modify the earlier network worktree/commits or coordinator checkout.

## Contract

- `reserve_network_job()` claims under the real `job_admission_mutex_`, before dispatch or IME.
  It uses `try_lock` so UI/bridge callers never queue behind an incumbent writer/HTTP/drain.
  USB, launch commitment, Nextendo or another network lease refuse admission without side effects.
- The move-only `NetworkReservation` is opaque and credential-free. The backend binds its
  identity and private configuration path. Reserved operations consume it once; foreign handles
  cannot enter. RAII releases the atomic busy publication on normal/error/cancel and failed
  `std::async` creation. Workers never reacquire admission or hold `bridge_`.
- A successful `Configure()` is not the completion boundary: discovery is asynchronous. The
  reserved worker waits/cancels and calls `StopDiscovery()` to join the last callback before
  releasing. Disconnect, partial-Configure failure cleanup and saved-file reconfiguration have
  the same lifetime. A cancelled post-publication reconnect does not pretend the file rolled back.
  Connection failure is separate from a successfully persisted save and remains disconnected.
- Startup/legacy connect/disconnect also reserve before async dispatch. Background future
  bookkeeping is short; ready-only collection never waits for I/O on the caller. Teardown moves
  the future out, cancels and joins with neither admission nor bridge held. The launcher retains
  its cancel/join-before-services-destruction rule for its own enum-only futures.
- Busy is exposed by `network_job_busy()` and `network_shares().busy`. USB **refuses** while it
  is set. Existing USB/writer/launch guards remain; `can_launch_game`/`will_play` and explicit
  profile/path changes cannot overtake an admitted network operation.
- Native UI builds `PrepareKeyboardPrompts` synchronously using copied `tr(string)` results,
  then captures the immutable UTF-16 snapshot by value. No translator/catalog is retained on a
  worker. Default names are translated only for creation, never for an existing/user-entered
name. Password masking/empty initial/private answers and all b207941 encoding limits remain.

## Coordinator integration point (50c3a2d+)

The parent must add `theme_storage_->busy` to the rejection predicate in
`EdenServices::reserve_network_job()` (`headless/network/services.cpp`), under the acquired real
`job_admission_mutex_` and **before** constructing/publishing `NetworkAdmission`. This is the
single acquisition point for native UI IME/save/remove/connect/disconnect, synchronous editor
operations and startup/legacy dispatch. The returned lease already remains held through final
save I/O, cancellation callbacks, Configure/Disconnect and the last discovery join; USB refuses
for that entire lifetime. This branch does not edit the parent's theme token/caller or import any
parent commits. The final combined host/UI/native build belongs to the coordinator.

## Focused checks / reproduction

Freeze the tracked source and new fixture files into an ignored, **new** snapshot directory;
never replace a snapshot while it is being tested. Bind it read-only at `/src`. The test container
uses the existing `prosperoeden-host:features` image, 2 CPUs / 3 GiB, no network, dropped caps,
exec tmpfs `/tmp`, and independent disposable tmpfs `/data` and `/mnt/usb0`. `/work` is the new
private volume `prosperoeden-team-network-usb-compose-20261028`; the old network dependency volume
is mounted **read-only** at `/dependencies`, and coordinator archives **read-only** at `/coordinator`.
`seccomp=unconfined` is needed only for this host's TSan `personality(ADDR_NO_RANDOMIZE)` startup;
the default-seccomp runtime attempt (`compose-03.log`) passed normal/ASan/UBSan, then was denied
before TSan test execution.
That runtime-startup denial is not a race report or a passing TSan run.

Minimal container creation (PowerShell; `$snapshot` is the frozen absolute source directory):

```powershell
docker run -d --name prosperoeden-team-network-usb-compose-05 `
  --network none --cpus 2 --memory 3g --pids-limit 512 --cap-drop ALL `
  --security-opt no-new-privileges --security-opt seccomp=unconfined `
  --tmpfs /tmp:exec,size=1200m,mode=1777 --tmpfs /data:size=384m,mode=1777 `
  --tmpfs /mnt/usb0:size=192m,mode=1777 `
  --mount "type=bind,source=$snapshot,target=/src,readonly" `
  --mount type=volume,source=prosperoeden-team-network-usb-compose-20261028,target=/work `
  --mount type=volume,source=prosperoeden-team-network-vfs-20261027,target=/dependencies,readonly `
  --mount type=volume,source=prosperoeden-integration-20261027,target=/coordinator,readonly `
  -e TMPDIR=/tmp -e PYTHONDONTWRITEBYTECODE=1 prosperoeden-host:features sleep infinity
```

Minimal test commands inside that container (run the network/USB suites **sequentially**: their
real-filesystem fixtures share the disposable `/data` and `/mnt/usb0` roots):

```sh
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export TSAN_OPTIONS=halt_on_error=1
python3 -B /src/headless/network/compose_check.py
python3 -B /src/tools/check-usb-writer-admission.py
python3 -B /src/tools/check-network-frontend.py
python3 -B /src/tools/check-network-privacy.py
```

For the standalone USB lifecycle regression, compile/run below, repeating with `SANITIZERS`
empty, `-fsanitize=address,undefined -fno-omit-frame-pointer`, and
`-fsanitize=thread -fno-omit-frame-pointer` (also sequential):

```sh
clang++-18 -std=c++20 -O1 -g -UNDEBUG -Wall -Wextra -Werror $SANITIZERS -I/src/headless \
  /src/headless/usb_backup_service_check.cpp /src/headless/usb_backup.cpp \
  -lcrypto -lz -ldl -pthread -o /tmp/usb-service-check
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  TSAN_OPTIONS=halt_on_error=1 /tmp/usb-service-check
```

The unchanged prompt/Editor runner additionally uses `/work/deps` and
`/work/network-editor-build`; in this new private volume those are symlinks to the read-only
`/dependencies/deps` and `/dependencies/network-editor-build`, respectively. Its command is
`python3 -B /src/tools/check-network-keyboard-prompts.py`.

The first fixture compiles verbatim service/controller bodies, real Editor/private writer/native
keyboard adapter/USB admission and real catalog functions. Only the surrounding launcher shell,
console metadata and NAS/IME transport are synthetic. It covers the pre-dispatch gap, dispatch
failure, storage callbacks after public discovery busy clears, joins/Disconnect/cancel/error,
foreign-token refusal, unchanged stored passwords, UI-thread catalog snapshots across language
reload, creation-only names, explicit path/profile/launch exclusion and simultaneous USB claims.
It runs normal, ASan/UBSan (including leak checks) and TSan with halt-on-error. It does not modify
the independently owned Nextendo fixtures, settings pagination, build scripts or CMake.

The USB-writer runner retains its actual writer/drain/file assertions; only fixture
support needed by composed symbols is updated: `IsPath` is extracted verbatim from the actual
transport, and the USB-service fixture declares/tests the new busy refusal. The frontend runner
syntax-checks real native-gated service/Library/caller paths with saved host flags/headers; this
is a focused build check, not an emulator rebuild or PS5 cross/runtime qualification.

The existing full controller/Mesa fixture now implements the reserved service interface and keeps
its real Settings-entry assertion, updated to index 16 after USB/Amiibo. On this unchanged
`939e447` Settings baseline that entry is unreachable. Its green end-to-end run depends on the
coordinator's separately owned overflow-pagination fix; this branch does not bypass that assertion
or edit `settings.cpp`/the coordinator's pagination fixture. No changes to the coordinator's
`NETWORK_SAVED_CACHE` include-path correction are included here.

## Evidence provenance

Frozen sources: `.local/compose-snapshots/attempt-04` (read-only `/src`); external manifest
`attempt-04.sha256`, SHA-256 `65704561a1dd0e6c722bfe0fd3d6d5bd0ca8b5ba879f04a5df65248c6646655b`.
All changed production/fixture bytes match that snapshot; this evidence document alone is finalized
after the run. Image: `sha256:e6137ec556fe8e63ebf25e7fabe36e573d089c469d0e9d391c3072f2ae04d551`.
Pinned read-only SMB/NFS archives: `bdd38b766b162cca4bfabe313cef0cc9734e7e3a73f755643e07305d6d2ec538`
and `43f48445c523ef5ed9c587e3cc15f03d94d09c55aa8909889a2d57d911063041`, respectively.
Logs remain in the private `/work` volume, with final copies and container metadata under the
ignored `.local/compose-evidence/acceptance-05`. Initial compile-only/host-runtime setup attempts are
retained separately; only the final isolated-root runs are acceptance evidence.
The overlapping shared-root suites in attempt 04 were stopped, not accepted; attempt 05 runs
network composition and USB writer/lifecycle suites sequentially.

Final host results (2026-10-28, container `prosperoeden-team-network-usb-compose-05`):

| Check | Result / log in `/work` |
|---|---|
| New service/controller/USB composition | normal + ASan/UBSan/leaks + TSan PASS; `compose-05.log` |
| Existing USB writers + remote snapshot workers (all assertions retained) | normal + ASan/UBSan/leaks + TSan PASS; `usb-writers-05.log` |
| Existing standalone USB lifecycle + new network-busy refusal | normal + ASan/UBSan/leaks + TSan PASS; `usb-service-05.log` |
| Existing prompt + Editor/private config regressions | normal + ASan/UBSan/leaks PASS; `keyboard-prompts-05.log` |
| Real native-gated frontend syntax | PASS; `frontend-05.log` |
| Packaging/privacy/native-only boundary | PASS, 87 actual synthetic CLI cases; `privacy-05.log` |

The existing real Launcher/Mesa fixture also passes reserved-interface syntax with `-Werror`
(third-party stb uses a system include); `editor-ui-syntax-04-system-stb.log`. This is **not** a
passing full UI runtime run: the index-16 Settings-entry assertion is intentionally retained for
the coordinator's pagination fix. The older prompt runner's final “composition PENDING” line
describes its old isolated seam; actual new composition evidence is the separate `compose-05.log`.

**Independent IME review remains NOT ACCEPTED.** No retry/rephrasing/alternate reviewer or
tool route was requested or used. Functional composition and synthetic host results do not
substitute for that review, actual PS5 IME execution, console filesystem behavior or real NAS/game
qualification. Catalog publication and the parent's separate Settings overflow fix are not part
of this branch.
