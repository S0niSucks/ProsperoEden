# Remaining feature ports — evidence matrix

Current scope update (2026-10-31): twelve groups; P10 now requires selectable
themes AND distinct interfaces referencing ps5-homebrew-ui. P12 compares
Takoroka/eden-nx2, Djihads80/Eglantine, chrisjenkynw/notx2 and Nikilite/oboromi.
The owner now authorizes implementation; this table's source comparisons remain
historical evidence, not new test results. Separate host/guest firmware research
and implementation evidence live under implementation-20261031/.

This is a synthesis of three completed, focused application-source comparisons,
not a formal design review, full source audit, implementation or PS5 test.
Source bodies and current Encore integration points were examined in the
reported ranges. External implementations, selected Eden internals and native
runtime behavior remain unverified.

## Source records

- [Game integration: P01/P02/P06/P07](./sources/game-integration.md).
- [Storage and network: P03/P04/P05/P09](./sources/storage-network.md).
- [Input, presentation and forwarders: P08/P10/P11](./sources/input-presentation.md).
- [P10/P12 reference intake](./sources/themes-switch2.md).

Each report defines its source/destination abbreviations and gives exact
file/symbol/line citations and read limits. References below point into those
reports; their source conclusions are not independently reproduced test results.

## Compared boundaries

| ID | Source application evidence | Current Encore integration | Constraint on the port design |
| --- | --- | --- | --- |
| P01 | File-backed NFC wrapper, bounded collection/catalog/PNG handling and explicit scan/remove strip; report lines 28–67 | Gamepad factory, current shortcut behavior and existing HUD/loading snapshots, without that NFC/strip composition | Driver I/O adaptation and selected-core compatibility are unknown. Preserve Encore input and both renderer owners; do not equate PNG browsing with guest NFC. |
| P02 | Persistent per-code choices, session mailbox and exact-build explicit download/cache/install flow; report lines 69–110 | Whole-mod switches, next-launch application, Safe Launch and glyph guards | Maintained parser/VM/timing/shutdown ownership differs. Source rewrites are not a drop-in plan; managed downloads stay off until explicitly selected. |
| P03 | SMB session/cache/discovery, read-only VFS, Settings editor and private text entry; storage report lines 28–67 | Local ROM admission, current local snapshot export and media enrichment, without the complete share service | Remote read-only does not mean no local writes. Preserve local format/history/export rules; client/core/native behavior needs separate evidence. |
| P04 | Shared network pipeline with explicit NFS v3, numeric UID/GID and protocol-specific fields; storage report lines 69–89 | No corresponding shared share pipeline or NFS editor/transport composition | Numeric AUTH_SYS identity is not secure authentication. No NFSv4, confidentiality or client availability is inferred. |
| P05 | Games-only root picker/reset, wallpaper-folder resolver and wallpaper image picker; storage report lines 91–120 | One files root, no equivalent independent games/wallpaper providers | The source's explicit wallpaper folder is configuration-file-only. A folder picker is extra adaptation, not an already implemented source UI. Folder selection is not migration. |
| P06 | Private profile store, confirmed OAuth-to-NEX flow, frozen launch routing and app-side core recipes; game report lines 112–152 | Packaging privacy protection already present; native guest policy is intentionally offline | Planning guest-online behavior requires an owner decision. BAAS/provider/trust/core/native acceptance remains unverified; current source OAuth repairs must not be mislabeled as still missing. |
| P07 | Native IME backend, guest keyboard adapter and source/link/import recipe; game report lines 154–191 | Controller/error applets installed without supplying that native keyboard at the composition point | Core callback/SDK ABI remains unknown. Source uses detached workers and fallback text/inline approximations; copying it does not establish exact guest semantics or safe shutdown. |
| P08 | Opt-in wired Nintendo 057e:2009 protocol/USB route and player-1 arbitration; input report lines 25–60 | Foreground-user native pad route with earlier recovery and per-session mappings | Source physical-button normalization conflicts with Encore defaults. Preserve foreground-user policy, no-zero scratch, recovery and glyph eligibility; USB access is hardware-unqualified. |
| P09 | Explicit USB backup/inspect/confirm UI, validated staging and cold-start merge; storage report lines 122–161 | Per-game save transfer and startup migrations, not that bulk transaction | Restore is a merge, not whole-tree rollback. Full writer coordination, native filesystem primitives and external-root scope are unverified. Confirmed next-start restore was approved as a design direction. |
| P10 | Canopy/Slate role themes, accents, named layout variants and selectable Grid/Cover Flow; a separate runtime-pack host also exists; input report lines 62–100 | Existing Encore/Switch presentations, navigation, shelf/case art and launch behavior | Exact added targets and persistence coexistence need decisions. Runtime packs/scene import are separate dependency boundaries; upstream scene/Themezer work itself is partial. |
| P11 | Native --rom/--exit-after-game parsing and one-shot startup selection; input report lines 102–133 | Native startup discards argv; selection/history/export/session systems already exist | Source path acceptance is not a safe arbitrary-path policy. Define admission without weakening Encore's narrow ExeFS/history rules; native argv delivery is unverified. |
| P12 | Eglantine documentation intake pinned at b40ad217; four-source research in progress | No native Switch 2 backend established | CPU/GPU/memory/core/native and guest firmware compatibility remain unverified; no platform tile counts as emulation. |

## Shared design risks

### Protected core boundaries

NFC, cheats, guest keyboard and network VFS/error routing have app-side caller
or generation recipes in the source. Those recipes do not prove compatibility
with Encore's selected core, nor authorize reading or replacing its excluded
implementation. A file-level plan cannot be called ready until the required
permitted interfaces are established.

### Admission and lifetime

The source uses collaborators and busy flags that Encore does not have. A
design must explicitly own launch, account/profile/path changes, native dialogs,
network editor work, cheat installation, UI replacement and USB operations.
Copying method names from the source does not create the required stop/join
barriers. This does not yet choose a shared coordinator or a data model.

P01 and P02 share an input/render strip; P03/P04 and P07 share a native keyboard
need. P09 must coordinate present and later writers, including current media,
library/history/settings/cache work. Retained native dialog buffers cannot be
freed merely because an application request was canceled.

### Persistence and identity

Game-copy identity, title-wide preferences, guest-user identity, console user
identity and a source profile UUID are not interchangeable. New keys cannot
silently replace existing Encore keys. Remote URIs and arbitrary forwarded
paths cannot simply be admitted into current local history validation.

P05's external roots also change what a fixed-root USB backup contains. A
stored path does not make the data behind that path part of the backup.

### Safety claims that the source does not establish

- Network reads can copy a prefix then return failure; poisoned sessions must
  remain unusable, including their cached bytes.
- Network settings have a rename commit point; cancellation after that point
  cannot mean nothing was saved.
- The source's local cheat-file path checks a size limit after reading; it is
  not pre-allocation protection.
- USB backup can leave incomplete new directories, and failed cold-start merge
  can leave mixed-but-recoverable state. Hashes are not trusted authorship or
  encryption. Writer gates cannot stop unrelated external FTP writers.
- USB uses `fstatvfs`; Encore diagnostics avoids `statfs`/`statvfs`. The different
  API is not proven prohibited or safe by that comment. Native evidence is needed.
- Source controller defaults and argument parsing contain compatibility choices
  that cannot be imported as if Encore had the same mappings or path policy.
- Real SDK symbols, service registration, native filesystem support, working
  transports, authentication and PS5 gameplay were not established here.

## Planning boundary

No production file changes or dependencies have been selected. The reports'
candidate files identify where a later plan needs exact ownership, not an
approved complete file list. The Technical Plan must reflect owner decisions
and explicit blockers before Detailed Implementation is attempted.
