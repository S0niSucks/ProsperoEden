# Parallel implementation — 1.006.000

The user explicitly requested a team of agents on 2026-10-27. This changes how the remaining
work is executed, not the full release scope or its evidence requirements.

## Ownership

Each worker implements in its own Git worktree/branch. Workers must not edit the coordinator's
checkout, merge into it, reuse its Docker build volumes, change release version metadata or
publish anything. Commit feature work on the assigned branch and return the commit IDs,
focused-test results, conflicts and genuine blockers to the coordinator.

| Workstream | Acceptance target |
|---|---|
| USB backup/restore (#157) | Settings workflow, preflight space estimate, cancellable progress, dated complete backup excluding ROMs/regenerable caches, validated restore with confirmation and recovery of existing data |
| SMB/NFS (#111) | Configured network-backed read-only game access, random reads, bounded caching, credentials protected, explicit disconnect/reconnect/error behavior; not a local-download-only substitute |
| Nextendo (#89) | Opt-in account/service integration and guest-network plumbing adapted to the pinned core; secure account linkage, no official-service credentials or blanket TLS bypass |
| Scene themes/Themezer (#173/#239) | Validated external scene packages, host-owned drawing/navigation/motion/sound, reduced motion, bounded assets and safe import; compiled palette switches alone are not completion |
| Cover Flow (#133) | Selectable library cover-flow layout using the existing controller/data model, bounded resources, smooth navigation, 720p/1080p preview checks |
| Amiibo launcher sync | Explicit opt-in catalog/photo synchronization with cancellation/progress and offline fallback, preserving local dumps/names/images, never downloading or reading tag dumps/keys for sync |
| Physical Switch Pro Controller (#202) | Real input transport/driver path where exposed by the PS5 platform, report parsing/mapping and connection handling; an emulated guest Pro Controller type alone is not completion |

The coordinator owns cheat-manager completion, cross-feature conflict resolution, integration
review, the overall changelog/changeset and release qualification. A worker's completion report
is not an automatic feature acceptance or proof of console compatibility.

All seven workers were dispatched from checkpoint **`953effe`**, each on
`team/<workstream>-1006000` under `.local/agent-worktrees/` (workstream directories:
`usb-backup`, `network-vfs`, `nextendo`, `scene-themes`, `cover-flow`, `amiibo-sync`,
`pro-controller`). The coordinator remains on `integrate/upstream-prs-2026-10-27`.
Private session identifiers and dispatch records are in `.local/agent-team-20261027.json`.

## Review/integration ledger

### Closing composition — 2026-10-28

This checkpoint supersedes older pending-integration descriptions below, without promoting
their test results to the current combined HEAD:

* USB writer correction `a9833f9` is integrated as `012f2e1`; the owner released shared files.
  It covers old synchronous writers, SaveSync/pairing starts and confirmed remote drain.
  The owner's normal/ASan/UBSan/TSan result is complete; the separate frozen-merge repetition
  was dispatched and is not yet a reported PASS.
* Network VFS, metadata/add-ons, Settings editor, extraction and prepared keyboard seam are
  integrated as `294d1d1`, `ec560bb`, `bb95ebe`, `e3b0392`, `ef2027b`, `939e447`.
  Conflict resolution preserves USB/Amiibo Settings categories, admission guards, `.inc`
  extraction and actual packaging-privacy checks. The network owner is finishing shared
  admission and UI-thread prompt/value capture in a separate worktree.
* OAuth→web→identity→NEX `0699208` / `6943832` / `2ab6a4d` is integrated as `fafa43c` /
  `0378b8d` / `3dc04ed`. Its owner reported 111 offline cases and nine regression runners
  passed, plus SDK objects/partial link; that is not a combined application or live login.
  One-shot explicit consent and game-only check callers/UI `df5dbaf` / `5bd2e1e` are
  integrated as `ee96d24` / `a8835ac`. The owner completed 111 offline TLS cases,
  admission normal/TSan/ASan+UBSan and actual consent UI normal/ASan+UBSan at 720p/1080p;
  the six new texts are extracted but not imported. Final combined callers/gates remain pending.
* Native-delta inventory `2007e8d` is integrated as `cc4489f`. Independent read-only review
  confirmed five indexed files, 175 hashes, 446 source contexts and 1,890 embedded OpenGL
  source files (content/executability). The unsafe partial example path filter is replaced
  with the common `source_bundle.safe_name` gate before child metadata access. Production
  opt-in capture hooks remain assigned separately; closure and relink are still pending.
* The whole-host path now includes the USB writer suite and serial local-fixture SMB/NFS
  transport/security/VFS/loader/frontend/editor/NFS-server checks. Its Docker entrypoint also
  requires private `/data` tmpfs. Shell syntax, two documentary Python AST blocks and inert
  aggregate runner ordering/refusal checks passed; these are **not** execution of those suites.
* Scene themes' existing implementation is delivered and integrated as `d983547`, `1e27eda`,
  `63777aa`, `022cb0d`; ownership is released. Conflict resolution preserves USB/Amiibo
  dispatch, all capabilities, Cover Flow and builtin Settings pagination. The owner completed
  110 frontend, 50 importer, 68 updater and 34 compatibility checks plus parser/mutation/pixel
  proofs on its snapshot. The native storage callback/caller composition is now assigned;
  full NXTheme material/font/animation parity and automatic compatibility are not implemented,
  and #173/#239 remain open. This subset is not accepted as completion of those issues.
* Settings integration now has 17 categories. `133f604` keeps the existing first 16 tile
  positions, pages overflow using the D-pad and supplies the missing network summary.
  Exact production navigation/geometry tests passed normal and ASan/UBSan. The frozen
  renderer compile then caught an enum/int deduction error in the new draw-loop bound;
  the explicit `std::min<int>` correction and exact production draw-loop regression
  passed focused normal/ASan/UBSan. The next real-renderer attempt exposed missing
  `EditorMessage` linkage in the general preview scripts; `fdc4a2d` adds the real editor/config
  sources to both. The corrected renderer repeat is running, not PASS.
* Native producer hooks `e3c62de` are integrated as `1c4bb61`. Their owner completed 44
  synthetic tests, syntax and README checks in offline Docker; independent frozen-merge
  review completed 44 tests and seven shell checks, and found split credential arguments
  (`--token value`) were not refused. `788fc57` fixes that lexical check; all 46 tests passed,
  including actual CLI refusal without execution, event, canary capture or echo.
  `EDEN_NATIVE_EVIDENCE_DIR` remains opt-in and requires a new explicit
  root declaration. No actual SDK/configure/build/source-object correspondence or relink
  result follows from those synthetic tests.
* The additive network-catalog import assignment was interrupted by a tool `cyber_policy`
  error. It has not been retried, rephrased or routed around. This is not a translation-test
  failure or a completed import; original handoffs remain preserved, and final coverage is pending.

New private host/native workspace preparation uses read-only old caches only as seeds.
Host seed SHA-256: `2187466c0b5589b49fe985a0a3c1b02407aa24a1262ec8c4becab861f79e5988`.
Native seed SHA-256: `6846dfa7a2777b540f6ede995f2fcbd812c90a34a7f0c1796e98c3a50dee2cc7`.
The two pinned network source archives were restored offline into each new private workspace
with their original SHA-256 checks. Four pinned native sibling inputs were also seeded RO→RW;
their old derivative outputs are caches only, not final evidence. The first native seed attempt
failed to preserve upstream Unix ownership under dropped container capabilities and is retained;
the new successful workspace preserves bytes/modes/times, not original UID/GID. No source
transport or evidence original was changed.
No seeded old object/package is accepted as the final build. All remaining composition,
catalogs, final version/changeset, one frozen HEAD's host/UI/native gates, binary-bound
materials/relink and hardware limits must still be resolved before closing 1.006.000.

### Earlier evidence (snapshot-specific)

| Workstream | Current evidence / disposition |
|---|---|
| Cover Flow | Worker `903c233` reviewed and cherry-picked as `63b5c97`. Integrated focused controller/Lua/render regression passed at 720p/1080p (`sh_17fb458ed001cnUb8V89g1PhE5`), with 18 captures reporting no GL errors. Five new msgids translated in all 30 catalogs: 581 texts, zero problems and no additional length warnings. PS5 remains unqualified. |
| Physical Pro Controller | Wired-USB `b2073bd` and recovery follow-up `f3c02ec` reviewed/cherry-picked as `b714eaa` / `a41f29b`. Integrated ASan/UBSan synthetic protocol/launcher regression, SDK compile/link smoke and actual PS5-target `pad.cpp` compilation passed (`sh_17fbd112a001iOTZpFyGhrt15K`). Review gap in primary DualSense recovery/duplicate user slots was fixed before integration. Bluetooth and physical execution are not established. Worker reassigned to combined cheat cross-layer qualification. |
| Scene themes | `b01f254` / `6a1bf96` are an **unaccepted partial increment**, not integrated. Worker is continuing complete linked frontend/updater validation, reference screens and console-native import/actual layout conversion. Offline lite conversion is not full #239 acceptance. |
| Amiibo sync | `f596678` / `18fcf11` / `79fcf93` reviewed/cherry-picked as `a72e26b` / `f8febee` / `8e8bdd6`. Local-priority/fallback fixes and individual normal/ASan/UBSan/TSan/TLS tests passed. Combined real USB/Amiibo/cheat workers/services passed on frozen `878d97c` (`sh_17fd7b2c7001OEJMHG3xgCQKtV`) after fixing the USB-close resume defect. Combined UI remains running. No PS5 qualification. |
| Nextendo | Initial/OAuth `cdaa6af` / `ab5f70d`, security `d96e8cc`, storage gates `dd3865c` and SDK snapshot compatibility `82b4ba4` integrated. Coordinator security/HTTP repeat passed 29 OAuth cases plus existing Nextendo/cheat/Amiibo TLS. Composed normal/TSan/ASan/UBSan Nextendo/Amiibo/cheat/USB and full USB/catalog repeat passed on frozen `5bd9d21` (`sh_17fe9989c0019XcJlwUBhCeLlP`). Worker `de3db59`, imported `5c77ed8`, closes both genuine-target dependency gaps: six host/six PS5 objects plus resolver/CA/OpenSSL partial-link checks pass on `62e99ba`. Bundle/provenance hashes coordinator-verified; 343 core/platform imports remain in the relocatable subset, not a complete executable. Live registration, BAAS/signing, operator trust/routes and PS5 game sign-in remain external requirements. |
| SMB/NFS | Pinned sessions `b057a17` imported as `931522e` and export privacy `6d47104` as `ac4a0b1`; VFS/frontend `dbaf12d`, metadata/history/add-ons `20ea7c6` and native editor `ad79973` / `251c732` await shared-service composition. The editor now implements Settings Add/Edit/Remove, private masked IME, keep/replace/clear, atomic `0600` persistence and explicit reconnect. Coordinator independently repeated the delivered branch's editor/Mesa ASan/UBSan, native-gated frontend and privacy runners on a frozen `251c732` archive: all exit 0 (`sh_1801274fa001UiBIdAQ7lzUiFU`). Two synthetic captures were opened. Parent USB/launch reservation remains required; this is neither acceptance of JSON-only setup nor a full app/PS5 result. |
| USB backup/restore | `884a8a4` / `ba1bb06` reviewed/cherry-picked as `afe7d80` / `01c6912`. Credential exclusion/injection controls, backend runner and initial 20 UI assertions passed; restore/restart captures coordinator-inspected. Worker-creation recovery passed real OS `EAGAIN` tests. Combined actual Amiibo/cheat/USB guards/workers plus full backend/update/syntax passed `sh_17fd7b2c7001OEJMHG3xgCQKtV` on frozen `878d97c`. Other new-feature writer coordination and final regression remain mandatory. |

USB/Amiibo (54 msgids) and Nextendo (24 msgids) translations are now additively incorporated
in all 30 catalogs, with existing entries untouched. Source/include extraction and import tests
pass; the whole checker reports **659 source texts, 30 catalogs, zero problems**. Thirteen new
heuristic length advisories (five USB/Amiibo, eight Nextendo) and console-font/layout coverage
remain distinct from the passing static checker. The Nextendo security audit found and fixed
header-allocation omission and ambiguous duplicate JSON (`6ece3f1`, imported `d96e8cc`);
the integrated shared-HTTP regression passed all four runners on clean frozen `d96e8cc`
(`sh_17fe4bfea001szCLUFwW9EwKgG`). These tests precede the storage-gate/SDK compatibility merges.

Combined UI-only evidence is integrated as `88c4c06` / `62e99ba`: USB 20, Amiibo 26 and
ownership 32 assertions plus Cover Flow at 720p/1080p and Lua all passed using a single
archived Mesa/ASan/UBSan build. The worker inspected 22 of 63 captures; the coordinator also
opened Settings 1080p and host-owned Amiibo consent 1080p. Source/hash/scope details are in
`AMIIBO_USB_UI_VALIDATION.md`. This does not qualify actual workers or the later Nextendo UI.

The uninterrupted host path now invokes the integrated Amiibo/Nextendo/admission/cheat-flow/
Pro Controller/localization regressions and then the focused USB runner. Shell syntax and
unsafe-fixture refusal checks pass. The copied-snapshot USB runner also passed on clean frozen
`ede91f5`, 838 files (`.local/docker-release/usb-runner-snapshot-check-20261027/`, exit 0).
Its foreground caller timed out; the container completed and saved exit 0. A later `docker wait`
found it already auto-removed; that observation is not a failed test. No final uninterrupted
all-feature host/native build has run yet.

USB credential staging `bc95255` was reviewed/imported as `99f257c`. The coordinator retained
the actual pre-fix COMPLETE-backup leak (six synthetic credential files), and the clean frozen
integrated 841-file repeat passed the full focused runner (`sh_17ffacb8e001wDaLmpbECNeXO6`):
seven harmless files / 236 bytes, unchanged estimates/manifests and zero forbidden open/stat/read
probes, hostile aliases and restore reinjection rejection. Separate Python collector privacy
and older writer-admission reviews remain active.

Verified dependency/license notices are imported as `f6c3f45`: all 11 retained file sizes and
hashes match `third_party/licenses/SOURCES.json` (manifest SHA-256
`fd79de67ad2fd83a6cd781b31d672f408a6c35dd15d060af823f48d1ab0588dc`). Upstream whitespace
is intentionally preserved. This notices import did not include package placement or
corresponding-source/relink materials; file presence and attribution are not a compliance certification.

Python collectors `df7cfdf` were reviewed/imported as `f1a92c6`. Both now deny private aliases,
staged network configs and the complete Nextendo subtree before metadata/descent. Worker
actual CLI qualification passed 121 Linux / 35 Windows invocations including baseline controls;
the coordinator independently verified 635 indexed artifacts and repeated 19 fixed-source
Windows CLI cases successfully. `EXPORT_PRIVACY.md` preserves the demonstrated synthetic ZIP
leaks and zero private-node accesses after the fix. The full host path includes these checks;
the follow-up release-ZIP license integration is recorded below. Corresponding-source/relink
delivery and the final combined build remain outstanding.

Release-ZIP notice integration `62068cb` is reviewed/imported as `00b1fdc`. It includes all
12 license-tree files at their existing root-relative paths and refuses absent, altered,
undeclared, linked or special-node inputs. The coordinator independently verified all 783
exported artifact hashes, checked the scoped tested source hashes against the integrated
files, retained the verified evidence and repeated 26 actual Windows license CLI cases
successfully (report SHA-256 `51ee9d4a8d7e8f224a4d3f72d54d62b2696e9476faf9ba30fe673920c9037b57`).
Worker qualification also covers 37 Linux license and 121 Linux / 35 Windows privacy CLI
invocations. This is a synthetic app with real maintained notices, not a final release binary
or a corresponding-source offer. The uninterrupted host runner now includes the license CLI
test; that full runner has not yet passed. Native app-folder notice placement and the source/relink
mechanism are now integrated separately below; final material collection remains active.
See `features/RELEASE_LICENSE_PACKAGING.md`.

The delivered network editor's independent coordinator repeat is retained under
`.local/docker-release/network-editor-review-20261027/`: immutable Git archive SHA-256
`5a37fcbb57134b6e713891e4c9e20c0268755a45a17381845313c9394d430c91`, exact source/dependency
inventories and per-run logs/status. Both reference volumes were read-only, all new objects
were built in a private volume and source/dependency inventories remained unchanged. This
repeat precedes parent composition and does not repeat the SDK or protocol-server runs.
The handoff has 83 distinct msgids, not 89 (the latter counted file lines). Its additive
translation delivery supplies **81 new msgids × 30 locales = 2,430 translations**, plus five
existing reused keys from the expanded source inventory. The coordinator verified the 40
reference hashes, real-importer schema/private idempotence and unchanged catalog bytes, and
retained input SHA-256 `304bc230c8291da0912832a32a07a4f139911f264d8b7e61bf63fa4dcbdc5905`.
The current-source importer correctly refuses it before composition. Nineteen backend texts
are marked by delivered follow-up `1ec3910`, still awaiting network composition. The coordinator
independently repeated exact extraction/provenance preservation, real 81×30 importer fixtures,
parent-tools patch check, strict C++ syntax and fresh normal backend configure/build/run: all
seven steps passed on frozen `1ec3910` / `ec43c56` archives. The original `noexec` tmpfs blocked
binary execution; an isolated 0755 control reproduced it, and explicit `exec` resolved the
environmental refusal without changing code/assertions. Both attempts remain under
`.local/docker-release/network-text-review-20261027/` (passing status SHA-256
`6637791eb9867cdebab177cbff1c7cd2d3ae131319e7d1c8c344b7e7f341d969`).
The audit also found 16 native keyboard prompts and a persisted default name outside the batch.
The separate 17-key translation delivery is now verified: **510 additional cells**, input SHA-256
`f81047a2d1bcd86864005aa901201c78f8ecc2544c2c353cfae550d595d8fbf4`. The coordinator checked
69 protected hashes, immutable source blobs/literal sites and both batches together: **98 new
keys × 30 locales = 2,940 cells**, passing actual-importer future-map validation, original-byte
preservation and private idempotence. No root catalog changed; current-source refusal remains.
The new batch adds no length warnings. Seven locales have unqualified font fallback; the
largest translated title/hint measures 50/76 UTF-16 units, not proof of native IME compatibility.
Evidence is under `.local/docker-release/network-keyboard17-review-20261027/`.
The immutable, UI-thread-prepared native prompt seam remains active; shared UI/service callers
remain reserved to parent composition. Catalog/POT import, eleven earlier length advisories
and actual localized font/layout checks remain pending.

Public Nextendo OAuth contract review `e3c4791` is imported as `9cfebff`. It finds no proven
authorization-path defect (official examples use both paths), and `profil` is a documented
literal scope. More importantly, the existing direct nested-account conversion rejects the
documented web-token / separate-NEX response forms in an offline ASan/UBSan fragment proof.
The older 29-case synthetic OAuth pass used the legacy response shape and does not qualify
this public-provider contract. The OAuth/core owner is implementing and testing the separate
web-access/NEX acquisition and credential-type handling; no fix is integrated yet. The supplied
public identifier does not establish registration approval, and no exposed secret was used.
See `features/NEXTENDO_PUBLIC_OAUTH_CONTRACT_REVIEW.md`; the reviewed evidence archive SHA-256
is `c43e572defff0bdc24f33f3f463e55edccd93d8552cae68292bd464ae7d98517`.

Native app-folder notices `ac09f52` are reviewed/imported as `b8c3345`. The coordinator verified
and retained all **1,128 indexed worker artifact hashes**, matched the tested inputs to the
integration and independently repeated **122 native-notice CLI cases** (115 adverse) plus
**87 fixed-source privacy cases** on a clean frozen `b8c3345` archive as unprivileged UID 65534.
All passed, with no forbidden accesses. The snapshot archive SHA-256 is
`5e4eecd68b1b041a2cdd3e704c79f5cb872e0c028766512614e7fa418cf2acd0`; the notice report SHA-256 is
`04a23c25acfb5ec3f692e04fecc53a3148cc7ffbc91a86ee7936c0453cbc1934`. Evidence stays private under
`.local/docker-release/native-license-review-20261027/`, including deliberate synthetic leak
controls. Bytes, internal notice links and Lapy preservation are checked; actual native build,
install, PKG and corresponding-source/relink delivery are not. See
`features/NATIVE_LICENSE_PACKAGING.md`. This unprivileged notice test is a separate final gate,
not silently skipped or represented as run by the root-owned whole-host runner.

Offline source/relink mechanism `b037e54` is reviewed/imported as `3e0fb73`. The coordinator
independently repeated all **39 CLI tests** in offline Docker (log SHA-256
`5f45a1f40a07a44c280d1903251d67d0f92c3e751c9e84eb1332739f03a8ba84`). The whole-host path now
invokes that test too; its updated shell syntax passes, not the complete runner. The real
baseline plan discovers 81 input requirements and refuses missing materials without producing
a partial bundle. A worker is collecting the actual hash-pinned inputs and recording concrete
source-closure gaps; this is not a completed final source bundle or a modified-library relink
trial. `RELEASE_SOURCE_RELINK.md` keeps relink `PENDING` / `COLLECTED_UNVERIFIED_RELINK` until
the final binary-bound materials are supplied and independently exercised.

Real source preparation `8ee544e` is reviewed/imported as `0bc00f9`: 59 actual initial inputs,
22 provisional CPM non-use decisions and 15 selected source/recipe supplements. The material
index SHA-256 `35f79c0398074667598bab13bb5b7ce163521ffb7cc53bb52b1f5dcc0c2c2277` is independently
confirmed. The coordinator's frozen offline repeat **passed** all 17 preparation and 39 original
collector CLI tests, re-audited the 59 initial / 15 supplemental inputs, retained the 12 indexed
evidence hashes and restored the limited six-archive / 17-file / five-patch subset independently.
Its audit/restore JSONs match the original worker bytes exactly. Actual bundle creation still
refuses without leaving an output, as expected. Status SHA-256 is
`62cc49e05379d66343795e707ae392e30f786c0227f60aacc92a9775799a9463`; evidence stays under
`.local/docker-release/source-preparation-review-20261027/`. The input volume was read-only;
new work used a private volume. The whole-host runner now invokes the preparation CLI tests,
and its shell syntax passes; that is not a whole-host PASS. The initial policy refusals and
supplemental Git delivery are resolved by the reviewed follow-up below; transitive source
closure and final relink remain outstanding. Private-path exclusions remain mandatory.
See `RELEASE_SOURCE_MATERIALS_1006000.md` for the preserved historical preparation record.

Source-policy follow-up `3b2ffdf` / `ee0bfc2` is reviewed/imported as `970b884` / `b6909bb`.
The coordinator repeated **39 + 17 + 23 = 79 tests** offline from an immutable delivery archive,
confirmed all **23 indexed evidence hashes**, independently verified and recreated the real
integration TAR, and repeated restoration in a new private volume. Its TAR is byte-identical
by SHA-256 `540dcdae30b5045f7b4fc691960e9015b029e208e3fa755563bef0a4b2939ac5`;
all three restore inventory/result JSONs match the worker bytes exactly. The restored inputs
cover 13 Git components plus app, 17 direct/supplemental archives, 22 CPM caches, five Eden
patches and the source-header Xbyak overlay. Original inputs and worker evidence were read-only.
Status SHA-256: `bc88e4c4957615232b03ce4b9e7495cda2e14b171f7e50d2731004b4bf38147e`;
evidence stays under `.local/docker-release/source-policy-review-20261028/` and private Docker
volume `prosperoeden-coordinator-source-policy-review-20261028`. A synthetic diagnostic also
confirmed that a modified link containing CRLF is refused, without modifying received bytes.
The original 39/17 tests and license files are unchanged; the new policy suite is now included
in the whole-host path. This is **integration snapshot `8ee544e`**, not the final coordinator
source/binary identity, top-level native configure, complete source closure or relink trial.
Relink remains **PENDING**. The source owner is separately mapping remaining applied SDK/driver
and generated-source deltas, without changing delivered evidence. See
`RELEASE_SOURCE_POLICY_FOLLOWUP.md`.

Native keyboard seam `b207941` is delivered above `1ec3910`, not integrated. It reports an
immutable, credential-free prompt snapshot, synchronous-only translation, bounded Unicode,
masked private input and creation-only localized names; its shared UI/service callers remain
documentary proposals. The independent backend/extraction review was interrupted by a tool-policy
error before a final technical result was delivered. This is neither a test failure nor a PASS;
the review remains unaccepted and must not be retried through a different route to bypass the
guard. The earlier extraction/import checks retain only their documented narrower scope.
Parent USB/launch composition and actual PS5 IME execution are also not established.

Coordinator cheat probes passed independent encrypted NSP/CNMT/base/update and wrong-key
refusal with actual key reload, per-copy native-VFS update/mod selection and real NPDM bounds.
Synthetic BKTR also passed in a separate coordinator-owned fixture worktree and was integrated
as `2d6b596`. Worker cross-layer qualification was reviewed/imported as `8a126f4`, with verified
exported evidence: actual encrypted/per-copy probe → TLS → manager/cache/default-off Mods install;
29 worker probes, 11 HTTPS requests, 34 read-only archive hashes unchanged.
The saved fresh-object host linked successfully but stopped at a guest-idle timing assertion.
Isolated idle checks and all remaining saved-snapshot tests passed separately, including actual
NFC/live-cheat factories, memory/JIT/device checks and three NRO launch/exit cycles. This is not
a single uninterrupted full-suite pass. See `CHEAT_DATABASE.md`.

The guest-idle worker subsequently demonstrated a late-entry harness race and committed a
test-only handshake fix (`3eea768`, integrated as `baefe49`). The integrated default invocation
passed ASan/UBSan, TSan and the kernel-lock check (`sh_17fc6ec0d001Uze9q247hdC5K2`) without
changing production policy or thresholds. This does not reconstruct the original failure's
schedule. See `GUEST_IDLE_TEST_INVESTIGATION.md` and its retained hashes/logs.

## Cross-feature storage/launch gate (integration acceptance, not completed evidence)

The initial workers started from one baseline; clean cherry-picks alone do not compose their
lifetimes. Before the combined release tests, the coordinator must verify these interactions:

- USB backup/restore closes admission before it drains app-data writers. Join workers **without
  holding `bridge_`**, then drain the bridge; a queued metadata scan must recheck the USB gate
  after acquiring it. A staged restore keeps the gate closed until cold-start application.
- Amiibo sync must refuse admission during the USB session and be cancelled/joined before a
  snapshot. Reopening its admission after a cancelled/completed backup must not reopen a
  committed game launch. The catalog worker's held paths cannot be changed under a run.
- Nextendo/OAuth account writers, profile changes, SMB credential editing and scene import must
  either be drained before backup admission or block that admission until their action finishes.
  Read-only network sessions are not themselves storage writers; library metadata/cover/history
  publication still uses the shared gate. Do not hold an owner mutex while waiting for a worker
  that needs that same mutex.
- Launcher-pack checked-at writes, installation/rollback and scene-pack publication must remain
  excluded throughout a USB snapshot, including late completion of an earlier asynchronous job.
- `config/nextendo/` and `config/network-shares.json` (including supported filesystem aliases)
  must not enter backup inventory, progress, hashes or restored payloads. Nextendo's PID/token
  and SMB passwords are credentials. Unknown hidden/staging names in the Nextendo subtree stay
  excluded without opening them; no secret-scanning heuristic substitutes for this path policy.
- Launch must refuse any pending storage/import/account transaction; teardown joins all owners
  before metadata, Lua, renderer or emulated-session objects disappear. Cancellation is not
  completion and cannot prematurely clear a launch barrier.

Focused integrated lifecycle tests must exercise the actual composed service methods, not only
independent fake collaborators. The final whole-host/preview/native snapshot is built **after**
all accepted feature commits and these guards are integrated. These requirements do not turn
host/SDK results into PS5 hardware, filesystem, server or gameplay qualification.

## Shared constraints

- Preserve **ProsperoEden** identity and the functioning base. No TGS or Nightshade rebranding.
- Docker only for Linux toolchains; do not install tools into Ubuntu/WSL on the host.
- Prefer focused tests per workstream. Batch complete host/native builds after integration,
  rather than rebuilding the whole emulator for every local edit. Existing builds may finish.
- No arbitrary deadlines or percentages. Report implemented, tested, untested and blocked
  separately. No fake hardware success, placeholder backend presented as a feature, or network
  compatibility claim derived from a successful compile.
- No public issue closure, pushes, release publishing, account creation, contact messages or
  credential use on behalf of the user. Read-only public-source research is permitted.
- No copyrighted games, keys, tag dumps or unlicensed assets in fixtures or release artifacts.
- Preserve existing launch/teardown/storage boundaries and document new asynchronous lifetimes.
- Keep changes scoped; feature-local docs/tests are welcome. The coordinator updates aggregate
  integration/release documents and resolves shared launcher/service/CMake changes.

## Completion gate

The version becomes **1.006.000** only when the requested implementation scope has been
integrated and qualified to its stated limits, with matching package/symbol/source hashes.
Console-only checks (Galaxy crash, NFC, GPU lifetime, USB filesystem behavior, rest mode,
physical controllers, network interoperability and performance A/B) require actual PS5 evidence.

### Final qualification sequence (planned, not passing evidence)

The remaining batch must use one frozen, clean commit/tree after feature composition. A worker
report, staged changes, partial link or older package cannot satisfy a gate for that snapshot.

| Gate | Required evidence before closing it |
|---|---|
| Feature composition | Accepted committed writer audit, SMB/NFS Settings/VFS, OAuth/NEX contract repair and complete scene-theme delivery; real shared admission through final callbacks/writes, cancellation, teardown and launch. Do not integrate into reserved service files before ownership is released. |
| Texts and capabilities | Extract the composed sources before additive catalog import; preserve existing translations and `.inc` provenance. Check placeholders, declared capabilities and every caller against the actual implementation. Keep font/layout advisories separate from static translation success. |
| Whole host | One uninterrupted `tools/docker/run-host-tests.sh` invocation, including all final feature regressions. Retain full logs and final exit status. `/mnt/usb0` must be disposable tmpfs; tmpfs that executes built fixtures must explicitly allow `exec`. Separate passing fragments do not replace this run. |
| Combined UI | Actual all-feature host-owned and Lua flows, input/consent/cancel/error states, reduced motion and 720p/1080p captures. Record which captures were inspected. Mesa previews do not qualify console rendering or native IME. |
| Unprivileged packaging/privacy | Repeat native notice and export privacy CLI tests on the frozen source as unprivileged Linux UID/GID 65534, using private writable outputs and read-only inputs. A root-owned host run cannot replace permission-denial controls. Preserve upstream notice bytes and negative-test evidence privately. |
| Complete native build | Full Docker native compile, link, identity stamping and packaging from that snapshot, with no stale port-dependent objects. Bind ZIP/app contents, `eboot.bin`, ELF, symbols, source identity and logs by hashes. SDK objects or a relocatable subset are insufficient. |
| Source/relink delivery | Recreate and verify the actual source bundle for the final snapshot; test offline restoration, review transitive source closure and supply the final binary-bound objects/commands/toolchain receipt. Exercise a modified-library relink before changing relink from `PENDING`/`COLLECTED_UNVERIFIED_RELINK`. See `RELEASE_SOURCE_RELINK.md`. |
| Console and publication | Retain the specific Galaxy/#284 and feature hardware retests with exact build/settings, and state unresolved limits explicitly. Confirm the destination remote with the user before any push/tag/release; the requested Swordpdf project and configured origin are not interchangeable. |

Record the full commit/tree, source archive and dependency inventory hashes, Docker image IDs,
commands, logs, exit codes and artifact hashes beside each final result. If composition changes
the snapshot, do not relabel an old result as current: rerun affected gates and the final combined
host/UI/native batch. Recreate binary-bound source/relink receipts after any final binary change.
Version metadata and the final changelog belong to that qualified snapshot, not to an earlier
intermediate build. These requirements are a checklist, not a claim that any final gate has run.
