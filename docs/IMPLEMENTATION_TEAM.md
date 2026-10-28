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

| Workstream | Current evidence / disposition |
|---|---|
| Cover Flow | Worker `903c233` reviewed and cherry-picked as `63b5c97`. Integrated focused controller/Lua/render regression passed at 720p/1080p (`sh_17fb458ed001cnUb8V89g1PhE5`), with 18 captures reporting no GL errors. Five new msgids translated in all 30 catalogs: 581 texts, zero problems and no additional length warnings. PS5 remains unqualified. |
| Physical Pro Controller | Wired-USB `b2073bd` and recovery follow-up `f3c02ec` reviewed/cherry-picked as `b714eaa` / `a41f29b`. Integrated ASan/UBSan synthetic protocol/launcher regression, SDK compile/link smoke and actual PS5-target `pad.cpp` compilation passed (`sh_17fbd112a001iOTZpFyGhrt15K`). Review gap in primary DualSense recovery/duplicate user slots was fixed before integration. Bluetooth and physical execution are not established. Worker reassigned to combined cheat cross-layer qualification. |
| Scene themes | `b01f254` / `6a1bf96` are an **unaccepted partial increment**, not integrated. Worker is continuing complete linked frontend/updater validation, reference screens and console-native import/actual layout conversion. Offline lite conversion is not full #239 acceptance. |
| Amiibo sync | `f596678` / `18fcf11` / `79fcf93` reviewed/cherry-picked as `a72e26b` / `f8febee` / `8e8bdd6`. Local-priority/fallback fixes and individual normal/ASan/UBSan/TSan/TLS tests passed. Combined real USB/Amiibo/cheat workers/services passed on frozen `878d97c` (`sh_17fd7b2c7001OEJMHG3xgCQKtV`) after fixing the USB-close resume defect. Combined UI remains running. No PS5 qualification. |
| Nextendo | Initial/OAuth `cdaa6af` / `ab5f70d`, security `d96e8cc`, storage gates `dd3865c` and SDK snapshot compatibility `82b4ba4` integrated. Coordinator security/HTTP repeat passed 29 OAuth cases plus existing Nextendo/cheat/Amiibo TLS. Composed normal/TSan/ASan/UBSan Nextendo/Amiibo/cheat/USB and full USB/catalog repeat passed on frozen `5bd9d21` (`sh_17fe9989c0019XcJlwUBhCeLlP`). Worker `de3db59`, imported `5c77ed8`, closes both genuine-target dependency gaps: six host/six PS5 objects plus resolver/CA/OpenSSL partial-link checks pass on `62e99ba`. Bundle/provenance hashes coordinator-verified; 343 core/platform imports remain in the relocatable subset, not a complete executable. Live registration, BAAS/signing, operator trust/routes and PS5 game sign-in remain external requirements. |
| SMB/NFS | Pinned sessions `b057a17` imported as `931522e` and export privacy `6d47104` as `ac4a0b1`; VFS/frontend `dbaf12d` and metadata/history/add-ons `20ea7c6` await shared-service composition. Worker real-loader, signed/encrypted authenticated SMB, Ganesha MEM, focused ASan/UBSan and SDK evidence is delivered, not full app/PS5 acceptance. Original #111 explicitly requires adding a share in Settings; the delivered JSON-only setup is insufficient. The worker continues a private native add/edit/remove flow without publishing credentials through generic Lua keyboard status. |
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
is intentionally preserved. **Package inclusion and corresponding-source/relink-material delivery
are not yet complete**; file presence and attribution are not a compliance certification.

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
