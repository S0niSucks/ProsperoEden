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
