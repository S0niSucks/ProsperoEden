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
| Physical Pro Controller | Initial wired-USB commit `b2073bd` and recovery follow-up `f3c02ec` await integration. Review found primary DualSense recovery/user-slot duplication after USB-only startup; follow-up addresses this with actual launcher-loop regression. Bluetooth and physical execution are not established. |
| Scene themes | `b01f254` / `6a1bf96` are an **unaccepted partial increment**, not integrated. Worker is continuing complete linked frontend/updater validation, reference screens and console-native import/actual layout conversion. Offline lite conversion is not full #239 acceptance. |
| Amiibo sync | Worker `f596678` delivered real C++ worker/HTTPS, safe publication and builtin/Lua confirmation. Worker reports normal/ASan/UBSan/TSan plus TLS and 25 UI assertions; coordinator review/integration pending. |
| Nextendo | Worker `9faebc0` delivered experimental profile/session, guest routing and launcher controls. Review pending; OAuth registration, reviewed BAAS contract/operator routes and real networking remain blocked, not qualified online play. |
| USB backup/restore, SMB/NFS | Await worker delivery; no acceptance or new compatibility claim. |

Coordinator cheat probes passed independent encrypted NSP/CNMT/base/update and wrong-key
refusal with actual key reload, per-copy native-VFS update/mod selection and real NPDM bounds.
Synthetic BKTR also passed in a separate coordinator-owned fixture worktree; integration pending.
The saved fresh-object host linked successfully but stopped at a guest-idle timing assertion.
Isolated idle checks and all remaining saved-snapshot tests passed separately, including actual
NFC/live-cheat factories, memory/JIT/device checks and three NRO launch/exit cycles. This is not
a single uninterrupted full-suite pass. See `CHEAT_DATABASE.md`.

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
