# Admission fixture composition (tests only)

Authoring base: `3d78049bd2fe810ee7219dd2d64aa6927d50e2e0`, private branch
`team/admission-fixtures-1006000`. No production guards, identity script, version, catalog,
CMake, host aggregate or network aggregate shell script is modified here.

## What the fixtures now compile

- Launcher admission uses the actual member block, including the real shared theme ledger
  and atomic network busy flag. `Game::network` is explicit metadata. The composed checker
  only supplies it for an older fixture that lacks it; it never injects a duplicate/shadow
  network flag. The legacy launcher-check entrypoint delegates to this same maintained matrix.
- Service definitions and USB/Nextendo includes are extracted verbatim, with lexical brace
  handling. New theme tests hold an actual `ThemeStorage::Credit`, including a last shared
  owner, and check refusal before reads/writes/dispatch/USB `exchange`. Nextendo additionally
  checks refusal while the network reservation flag is held. Existing runtime assertions,
  real worker lifecycles and the synthetic public OAuth/NEX contract remain intact.
- USB writer tests check the new guards of update/apply/download/pairing and void refresh/sync
  starts without changing existing settings/snapshot/drain tests. The feature service fixture
  also owns a real theme ledger, so the production include compiles with the new symbol.
  `usb_backup_service_syntax.cpp` already includes the real service header and needs no fake
  field. Its strict syntax test is retained. The local USB runner adds only the new guarded
  assertion switch and the missing real `third_party/miniz` include for scene headers.
- The theme storage test no longer unconditionally inserts an already-present USB guard.
  It extracts the actual prefix and tests reverse exclusion when that guard is present.
  Callback capacity/RAII/last-write/cold-recovery tests continue to execute actual unchanged
  production code. No fake storage or network token grants runtime acceptance.
- The Nextendo native UI seam links the real network prompt-helper translation unit needed
  by the current launcher. Its keyboard collaborator aborts if reached: no keyboard path is
  executed, reviewed, accepted or simulated as approved. Default network/theme service denial
  is asserted. Normal, ASan/UBSan and optional `--tsan` use the same real controller test.

## Source versus optional overlay

`tools/admission_fixture.py` records original and compiled body hashes, writes
`admission_fixture_scope.json`, and explicitly prints **VERBATIM PRODUCTION** or
**PRIVATE PREDICATE OVERLAY**. It never writes to the source checkout.

- By default, extracted methods are byte-for-byte unchanged. On the old `3d78049` source
  without theme guards, the report says that theme exclusion is absent and **not claimed**.
- If any required theme guard is present, all guards required by that fixture must be present;
  a partial composition fails instead of silently skipping the new contract tests.
- `ADMISSION_REQUIRE_THEME=1` makes absence a failure. Use this for the parent's combined gate.
- Optional `ADMISSION_THEME_OVERLAY=1` adds only missing minimal busy predicates to private
  generated fixtures. It is explicit and idempotent; never label those runs as production.
  The final validation described below does **not** need this predicate overlay.

For actual parent-guard validation while keeping the authoring checkout tests-only, use exact
files from parent commit `aa2baf4` as read-only file mounts over `/repo`:

```text
headless/prosperoeden/eden_services.cpp
headless/prosperoeden/usb_backup_services.inc
headless/prosperoeden/nextendo_services.inc
headless/network/services.cpp
```

The two inherited strict-warning fixes in `scene/nx_install.hpp` and `scene/nx_material.hpp`
may likewise be mounted from that snapshot; no library edit or warning waiver is delivered.
The parent identity/docs/runner changes are intentionally not imported. This validates real
parent method bodies but is **not** a full combined-HEAD/build/runtime claim. The parent owns
merge and reruns its full gate against its own frozen tree.

## Existing dependency/archive layouts

`headless/network/compose_check.py` accepts the aggregate host environment:

```text
EDEN_NETWORK_DEPS=<repo>/.deps
NETWORK_WORK=<existing private mktemp>
```

Headers are read from `EDEN_NETWORK_DEPS/{libsmb2-libsmb2-6.3,libnfs-libnfs-8.0.0}/include`.
When present, it reuses the archives produced by the preceding editor backend check at
`$NETWORK_WORK/editor/build/{smb2/lib/libsmb2.a,nfs/lib/libnfs.a}`. Otherwise it retains the
original read-only `$NETWORK_DEPS_VOLUME/deps` source and
`$NETWORK_DEPS_VOLUME/network-editor-build` archive route (`/dependencies` by default).
Missing dependencies fail with an offline diagnostic; no fetch, configure, dependency build,
library substitution or synthetic SDK is performed by this checker.

## Focused commands

Run inside `prosperoeden-host:features`, with `--cpus=2 --memory=3g --network=none`,
`--security-opt seccomp=unconfined`, immutable `/repo` and dependencies, private writable
`/work`/`/private`, and disposable independent tmpfs `/data` and `/mnt/usb0` (512 MiB each).
Use a fresh output path; the theme fixture requires a fresh `/data/prosperoeden`.

```text
ADMISSION_REQUIRE_THEME=1 python3 tools/check-launcher-admission.py
ADMISSION_REQUIRE_THEME=1 python3 tools/check-usb-writer-admission.py
ADMISSION_REQUIRE_THEME=1 python3 headless/network/compose_check.py --admission-only
ADMISSION_REQUIRE_THEME=1 python3 tools/check-theme-storage.py --out /private/NEW-theme
bash tools/docker/run-usb-backup-tests.sh
python3 tools/check-nextendo-consent.py --out /private/NEW-ui
python3 tools/check-nextendo-consent.py --out /private/NEW-ui-asan --sanitize
python3 tools/check-nextendo-consent.py --out /private/NEW-ui-tsan --tsan
```

The worker runs network composition with `--admission-only`: zero keyboard, catalog-reload or
controller subtests execute. Existing default controller tests remain available to the parent;
their existence is not independent IME review/acceptance. No blocked catalog import/rephrasing
or translation workaround is introduced. Service tests retain leak detection and sanitizer
halt-on-error; the existing Mesa-only UI leak exclusion is unchanged.
