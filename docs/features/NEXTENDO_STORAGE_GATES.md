# Nextendo / launcher storage admission handoff

Base: exact `ab5f70d64787002c073816b546a57a2fddd4a582` (including the coordinator's USB/Amiibo/cheat
admission integration). Branch: `team/nextendo-storage-gates-1006000`.

## Policy and lock order

- The UI still owns its Nextendo future and stop flag. No service-side join or cancellation owner
  was added. A protected USB session or committed launch refuses a queued Nextendo action before
  profile resolution, root lookup, account I/O, listener creation or HTTP.
- `nextendo_action()` takes `job_admission_mutex_`, checks USB/launch/busy/cancellation, then takes
  `bridge_` to validate the selected profile and copy **both** profile key and `ConfigDir()` path.
  It publishes busy before releasing admission. Every store/check/link/failure-status operation
  uses these immutable snapshots; no worker-side root lookup remains.
- HTTP, OAuth loopback waits and account operations run without admission or bridge. The short URL
  callback takes only bridge. Busy is atomic and is released by the outermost action guard **after**
  LinkBrowser's final URL-clear callback and all account reads/writes, including failure recovery.
- USB start, `will_play()`, both storage-root setters and profile choose/add/rename/remove refuse
  while Nextendo is busy under the same admission mutex. No launch commitment, unlink or root
  mutation can pass the check and race a newly admitted action.
- Profile list/key methods are gated too: their ostensibly read-only `Profiles::Resolve()` can
  initialize/settle persisted profile choice. They return empty while storage/launch/Nextendo
  admission is closed. After an ordinary USB close or completed action, normal calls work again;
  launch commitment remains terminal.
- Nextendo status refuses during active work/USB/terminal launch. Otherwise it snapshots under
  admission -> bridge and releases bridge before reading the store, retaining admission for that
  bounded local read. It never blocks the metadata bridge behind an HTTP-held store mutex.

The actual Nextendo busy/URL declarations now live beside the other admission members, before
`CheatRequest`, so the admission test compiles the complete production collaborator/member slice.
Public method signatures, UI ownership, account format and transport policy are unchanged.

## Focused checks

Run in `prosperoeden-host:features` with a read-only source mount, private volume
`prosperoeden-team-nextendo-20261027`, CPU/memory caps and `TMPDIR=/work`:

```sh
python3 /repo/tools/check-launcher-admission.py
CXX=clang++-18 CC=clang-18 python3 /repo/tools/check-nextendo.py --source /pinned
CXX=clang++-18 CC=clang-18 python3 /repo/tools/check-nextendo-oauth.py
CXX=clang++-18 python3 /repo/tools/check-cheat-download.py
python3 /repo/tools/check-amiibo-sync.py
```

On a host with high-ASLR layouts, LLVM 18 TSan may need `personality(ADDR_NO_RANDOMIZE)`; Docker's
default seccomp filter can reject that call before the test starts. The test-only fallback is an
ephemeral `--network none --security-opt seccomp=unconfined` container with the same private volume,
read-only sources and CPU/memory caps (no privileged mode or extra capabilities). This changes the
container's seccomp filter, not sanitizer flags, production behavior or HTTP/TLS verification.

`check-launcher-admission.py` extracts the **verbatim actual methods and guard/worker member block**
and compiles normal, TSan and ASan/UBSan variants with assertions and `-Wall -Wextra -Werror`.
It preserves all existing real USB/Amiibo/cheat lifecycle/race assertions. Added composed coverage
uses the real Store, account checker and LinkBrowser loopback/PKCE implementation. Only provider
transport and unrelated console profile metadata/probes/jobs are simulated, not admission methods
or fixed-success identity APIs. Synthetic token exchange validates the actual verifier against the
challenge obtained from the actual loopback entry redirect before returning a synthetic session.

New cases hold account HTTP/token completion after cancellation, block the final URL callback on
bridge, verify USB/launch/root/profile refusal and free admission, then permit completion. They also
cover UI work queued before USB/launch commitment, delayed stale-profile refusal, exactly one root
snapshot per action, selected-root success with another root unchanged, existing account/consent
preservation on refusal/cancel, real profile unlink after completion, and concurrent
Nextendo/USB/launch admission races. The standalone Nextendo service test retains profile/busy/room
checks and adds USB/terminal refusal.

Final results (2026-10-27): composed normal/TSan/ASan+UBSan runs all passed, with no final sanitizer
findings. Existing Nextendo/pinned-source and actual-loopback HTTPS OAuth suites passed. Cheat
download unit/ASan+UBSan and actual HTTPS/worker/cache/install regression passed. Amiibo normal,
TSan, ASan+UBSan, merged-reader and real-loopback TLS regressions passed using the test-only seccomp
fallback above. The initial composed TSan run exposed a fixture-only bridge -> admission inversion;
the final fixture blocks bridge on a separate thread instead, preserving all assertions and the
production lock order. No sanitizer flags or assertions were disabled.

## Integration and limits

Shared production conflict surfaces are only `headless/prosperoeden/eden_services.{h,cpp}`,
`nextendo_services.inc` and the one-line admission predicate in `usb_backup_services.inc`.
Tests touch `headless/{launcher_admission_check,nextendo_services_check}.cpp` and
`tools/check-launcher-admission.py`. No OAuth/store/HTTP/core overlays, locales, versions, aggregate
documentation or coordinator files were edited. The independent USB-session security audit and
controller/core object qualification remain separate workstreams.

These checks establish host admission composition, not a full native/core build or PS5 storage,
browser/network/server interoperability. Existing OAuth registration, BAAS and endpoint/trust
requirements remain as documented in `NEXTENDO.md`; this change does not claim online play.
