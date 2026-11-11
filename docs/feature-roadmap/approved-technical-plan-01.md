# Encore — Remaining Feature Ports

- **Author:** OpenCode drafting assistant, for the repository owner
- **Status:** Draft — Overview and Goals approved; Technical Plan awaiting review
- **Date:** 2026-10-30
- **Scope:** All pending feature ports in the existing partial-port inventory
- **Current phase:** Design only; no production implementation or build

## The Problem

Encore has received a limited set of improvements from ProsperoEden, but the
remaining user-facing features have not been established as working Encore
features. Existing source references, settings labels, partial adapters and
packaging safeguards do not establish complete behavior on the console.

The owner needs a checked design for the remaining ports, rather than another
architecture index or an unsupported statement that every feature is present.
The design must explain what each feature lets a user do, what is missing,
what changes are needed, and what evidence will be required before calling it
complete. It must also preserve Encore's current renderer, memory, input and
storage work instead of replacing those systems wholesale.

The main obstacle is incomplete evidence, not just unfinished UI. Some features
may depend on core interfaces that have not been verified. The selected Eden
vendor subtree remains excluded from inspection and modification. This draft
cannot assume those interfaces exist or turn source-origin claims into PS5
test results.

## Overview

This design covers every remaining feature group named in the
[partial-port inventory](../PROSPEROEDEN_PORTS_2026-10-30.md),
especially its lines 26–42. The inventory is the scope source, not proof that
the old or new application implements each behavior completely.

The source application snapshot is
`5211718fb77f687a027dc5f675209bc64af4b982`; the recorded Encore base is
`21fdbc1ca7f5a74e4f4a67c0b721349cf1353ffb`. Encore also contains local,
uncommitted port work and architectural contracts. Future comparisons must use
the current working files as well as the recorded base, so earlier changes are
not lost or misclassified as missing.

The scope below is complete for this inventory, but it is not yet a file-level
implementation plan. The next drafting phase must check permitted application
sources and record the actual integration boundaries before choosing an
architecture. Any inaccessible core dependency must remain an explicit blocker.

### Feature groups to cover

These are proposed user-outcome and investigation targets. They do not certify
current behavior or settle storage formats, libraries, protocols beyond those
already named in the inventory, or UI structure.

| ID | Pending feature group | What the design must establish |
| --- | --- | --- |
| P01 | Amiibo/NFC | A user can find and select their supplied Amiibo data through folders, names and PNG artwork, then use it in a supported game. Identify the permitted application-to-core boundary, cancellation behavior and failure reporting; do not infer support from an applet comment. |
| P02 | Per-code and live cheats, with explicit database installation | Define individual-code selection, how a live change reaches the running title, and a user-triggered database installation flow. Establish title/build matching and distinguish these behaviors from existing basic mod or cheat-file discovery. |
| P03 | SMB read-only game access, with a Settings editor | Specify configuring and editing a share through Settings, browsing and reading game data, and reporting authentication or connection failures. Treat a configuration file or partial adapter as insufficient evidence of this complete flow. |
| P04 | NFS read-only game access, with a Settings editor | Specify the corresponding NFS setup and read flow, including unavailable exports and interrupted access. Establish its actual requirements separately from SMB rather than assuming the protocols have identical behavior. |
| P05 | Independent games and wallpaper storage | Establish that a user can select games and wallpaper locations independently, including unavailable locations and restart behavior, without resetting or silently moving existing user data. Exact persistence and root-selection rules remain open. |
| P06 | Nextendo | Establish what the named upstream feature actually does from permitted sources before defining Encore behavior. Identify user actions, any external-service dependency and private state; do not invent an API or contact the service during design. |
| P07 | Native in-game keyboard | Define the text-entry request, successful entry, cancellation, input limits and return to the game. Verify the permitted native/core integration boundary before claiming that launcher text input satisfies it. |
| P08 | Experimental USB Pro Controller | Define the intended controller and connection mode, attachment/removal, player assignment and coexistence with current input. Keep its experimental qualification visible; partial driver code is not a completed controller feature. |
| P09 | USB backup and restore | Define the data included, user-selected backup/restore actions, overwrite handling and recoverable failure outcomes. Do not assume automatic startup restore or general transactional safety from existing save-copy helpers. |
| P10 | Missing theme and layout parity | Enumerate the actual missing screens, controls and behavior from the permitted source comparison. Define parity targets explicitly rather than treating translation strings or a host preview as console support. |
| P11 | Launch-argument forwarders | Establish which arguments the upstream feature forwards, who supplies them and how Encore admits and carries them to the intended launch. Define invalid-input and launch-failure outcomes without assuming arbitrary paths are safe. |

## Goals

1. Cover all eleven feature groups, without silently reducing the requested
   scope to Amiibo or to features that are easy to inspect.
2. For every group, identify a user journey, the permitted source evidence,
   the current Encore boundary, the missing behavior and any unresolved core
   or platform dependency. Unknowns must be marked unknown, not absent or done.
3. Produce a checked technical plan and one detailed implementation list with
   every proposed file change, its purpose and its verification requirements.
   Do not label a feature implementation-ready while an important interface or
   product decision remains unresolved.
4. Preserve the earlier controller, ExeFS history, loader, metadata/image,
   library-export and packaging-privacy adaptations. Keep their existing
   verification evidence separate from new feature evidence.
5. Define failure outcomes for cancellation, unavailable storage or network
   sources, failed writes and device removal wherever those conditions apply.
   User saves, settings and private credentials must not be treated as
   disposable fixtures.
6. Separate source inspection, focused host checks, native compilation and PS5
   runtime checks. Feature completion requires the applicable end-to-end
   evidence; a host pass or a build alone cannot qualify console behavior.
7. Keep each feature's progress and evidence distinguishable so that a missing
   dependency in one group cannot be hidden by successful checks in another.
   Delivery order and implementation boundaries will be settled during the
   technical plan, not implied by the row order above.

## Non-Goals

- No production code changes, builds, dependency installation, packaging,
  deployment, console operations, Git staging, commits or publication during
  this design workflow. Any later compilation remains Docker-only.
- No repeat of the completed EGM contract bootstrap or its thirteen resolved
  documentation findings. Contracts remain references for current boundaries,
  not substitutes for feature-specific verification.
- No inspection or modification of `vendor/sources/eden`, or other excluded
  vendor/third-party internals. If a required interface cannot be established
  from permitted evidence, record the blocker rather than bypassing the limit.
- No wholesale replacement of Encore's CMake/source tree, renderer, JIT,
  heap, firmware/elevation, title/profile, glyph or existing input work.
- No silent save/settings reset, automatic migration, unsolicited or
  startup-only restore, credential disclosure, new feature-service requests or
  hardware experiments. The owner permits designing an explicitly confirmed,
  validated USB restore that applies on the next startup; no restore operation
  is authorized during this workflow.
- No unsupported release date, version bump, compatibility promise or claim
  that the old 1.006 candidate's results qualify this Encore tree.
- No new service, dependency, shared framework, persistence format or UI
  structure chosen merely to make the document look complete. Substantive
  alternatives require an explicit owner decision.

## Open Questions

These are unresolved design inputs, not decisions made by this draft. They will
be investigated and, where a choice is needed, presented one at a time.

- Which existing public integration points can support the core-dependent
  features without entering the excluded Eden subtree?
- Which compared theme/layout/runtime variants and forwarder argument rules
  should Encore expose, and which provider contracts can support Nextendo?
- Which persistence, selection and conflict rules should apply to independent
  storage, network settings, cheat installation and USB restore?
- What supported hardware/game cases and observable outcomes will qualify
  each feature, including the explicitly experimental controller?

The owner approved Overview and Goals, confirmed next-start USB restore as a
design direction, and permitted designing a default-off, session-bound Nextendo
exception. [Owner decisions](./decisions.md) records the scope of those choices.
Three focused source comparisons are complete; their
[evidence matrix](./feature-evidence.md) records the actual application boundaries
and limits. The initial approved wording is preserved in
`approved-overview-01.md`.

## Technical Plan (Architecture)

### Readiness boundary

This is a high-level porting plan for all eleven groups, not a claim that the
whole program is implementation-ready. The source comparisons found substantial
application implementations, but several are tied to a different core, startup
policy, input mapping or service lifetime. Copying those files as a unit would
discard Encore behavior and import unverified assumptions.

Keep the existing Encore launcher, `Services`/`EdenServices` boundary and native
session path. Add feature-specific operations through those boundaries rather
than bypassing per-title setup, Safe Launch, input/glyph selection or teardown.
The source's named helpers identify candidate adapters; they do not establish
the selected core's API or a new permission to inspect it.

The exact public core/SDK/client contracts must be established through permitted
evidence before their adapters become implementation-ready. If evidence cannot
be obtained within the inspection restrictions, keep that feature blocked and
describe the missing contract. Do not add speculative implementations or alter
maintained core owners through configure-time text rewrites.

### Control and data boundaries

The following diagram describes the proposed integration boundaries, not new
APIs that already exist:

```text
User action in the existing launcher or in-game controls
    -> existing application service/session boundary
    -> validate selection, identity and operation admission
        -> synchronous validated setting change, where appropriate
        -> asynchronous owned operation, where appropriate
             -> generation-bound progress/result snapshot
             -> current UI/input/render consumer

Admitted local, network or forwarded game selection
    -> current Encore per-title setup and Safe Launch policy
    -> fixed input/glyph and optional Nextendo session state
    -> verified feature adapters and selected guest-core interfaces
    -> current stop/join/input-neutralize/JIT/session teardown
```

An asynchronous result must remain bound to the request's game copy, profile,
storage root and generation as applicable. A stale result must not modify a new
selection or running guest. A posted request is not a completion acknowledgment;
the responsible worker or consumer must establish completion and storage lifetime.

For all state-changing operations, define who admits the operation, who owns
its resources, which other operations conflict, its commit point, cancellation
after that point, and shutdown behavior. Whether this admission is represented
by a central owner or coordinated existing owners remains an architecture choice;
no new general job framework is selected by this draft.

### P01 and P02 — in-game actions and cheat management

P01 needs the file-backed NFC adapter, validated collection/catalog handling,
bounded optional PNG decoding, explicit scan/remove actions and current GL and
Vulkan display consumers. Preserve separate files even when model IDs match.
Browsing or highlighting must not present a tag. Missing artwork must not make
an otherwise supported dump unavailable. Driver reads and guest writes need the
same descriptor/path admission boundary; discovery checks alone are insufficient.

P02 separates three paths: persistent per-code choices, a session-only mailbox
consumed by the guest's verified timing owner, and explicit catalog/cache/install
work. Catalog installation must not enable codes or mutate a running VM.
Identity must come from the effective admitted game content, not a title label
or filename; unsupported identity cases must remain unsupported rather than
guessing a build. Preserve whole-mod switches, mixed-mod behavior, Safe Launch
and existing glyph protections. Replace the source's post-read cheat size check
with a pre-read admission requirement in the eventual design, not a claim that
the source already protects allocation.

Both groups need an in-game request/display surface without losing Encore's
HOME, Capture, custom mappings, rumble or held-input suppression. Its exact
chord, menu structure and worker ownership are still open. File discovery and
image decoding must not be placed in render/timing callbacks. The selected
parser, NFC, callback and destruction contracts remain evidence blockers.

Candidate existing surfaces are `headless/{pad.h,pad.cpp,main.cpp,graphics.h,
graphics.cpp,mods.h,settings_store.h,CMakeLists.txt}` and the current launcher
provider/library UI. Candidate new helpers are the source-shaped Amiibo,
quick-picture/overlay and cheat catalog/store/manager/mailbox files listed in
the game-integration report. These are not a final changed-file list.

### P03, P04 and P05 — network sources and independent storage

P03/P04 require the whole read-only network route: editable saved locations,
private input, session/discovery, bounded reads/cache/handles, VFS integration,
metadata/add-on identity, launch error propagation and disconnect/cancel/join.
No transport-only or configuration-file-only increment can be marked complete.
Failed sessions must remain unusable even when cached bytes exist; reconnect
must not turn an old open reader into a reader of different content.

Keep SMB and NFS acceptance distinct. Source evidence names SMB2 client calls
and an NFS v3 numeric-identity branch, not a generic secure filesystem mount.
Authentication/signing/encryption exposure and NFS identity policy remain open.
Remote read-only behavior still allows local config, history and cache writes.
Protect credentials at runtime and in exports/backups; the existing release
preflight is necessary but not a credential store.

Network game identity must be admitted separately from local relative filenames.
Retain complete local `library.json` publication before enrichment and preserve
Encore's local formats and narrow ExeFS/history rules. Supported remote formats,
startup connection behavior and whether exports include network entries require
explicit decisions; source exclusions cannot silently narrow local behavior.

P05 adds independent location resolution and user-visible status without moving
existing data. Map every library, metadata, add-on, launch and relevant writer
to the selected root before adding settings. Games and wallpaper locations must
not accidentally change keys, firmware, saves or mods. The source has a games
folder browser/reset and an image picker, but its explicit wallpaper folder is
configuration-file-only. Whether to add a wallpaper-folder Settings picker,
the exact fallback hierarchy and profile/shared scope remain open.

Candidate surfaces are `assets_dir.h`, `storage_paths.h`, `settings_store.h`,
`main.cpp`, `metadata_bridge.cpp`, the launcher provider/browse/settings/library
and texture paths, plus additive network and wallpaper-path helpers. New client
pins, licensing, selected-core VFS/error hooks and native linkage remain
unverified; no dependency adoption is approved merely by those candidate names.

### P06 and P07 — identity, network policy and native dialogs

For P06 the owner permits designing a default-off, session-bound Nextendo
exception. Retain enforced-offline behavior by default. The exception needs an
explicit guest-user authority, exact routes, trust ownership, consent, private
state, cancellation and launch-time snapshot. A running guest must not silently
acquire another user's account or route policy. Unmapped official-service names
and unresolved authentication must fail closed; no TLS bypass or fabricated
BAAS/id_token success is a fallback.

The source's current OAuth-to-NEX bodies repair the older documented response
shape; do not plan that old defect as if it were still present. A repaired app
flow is still not provider acceptance, a usable registration, verified routes
or working guest authentication. Local linkage and full online gameplay must
have separate evidence states. All service contact stays outside this design run.

P07 needs a native text backend and the guest keyboard adapter, not just a
Settings input field. Serialize native dialog ownership, preserve buffers while
the system may retain them, reject stale callbacks after close/replacement and
never expose text/passwords in diagnostics. The source's detached workers,
fallback text, automatic text-check confirmation and final-submit inline model
are compatibility/lifetime decisions to resolve, not assumed exact parity.
Network private prompts and in-game requests need a shared serialization contract;
the implementation owner and worker model remain open.

Candidate surfaces are current applet/session composition, launcher services,
startup/build/package import owners, source-shaped Nextendo and system-keyboard
helpers and the keyboard applet adapter. Do not assume source module IDs, SDK
imports, guest frontend callback types, resolver/trust producers or core rewrite
recipes are compatible. Preserve elevation, existing controller/error applets
and current teardown while those contracts are established.

### P08, P10 and P11 — controller, presentation and launch selection

P08 remains experimental and opt-in. It adds a physical wired controller route,
not a new guest controller type. Preserve Encore foreground-user selection,
failed-read recovery, uninitialized scratch, reserved slots, motion/rumble cleanup
and captured per-session mappings. Resolve Nintendo-label versus position
normalization, native/USB player-1 priority, glyph eligibility and opt-in UI before
wiring the source decoder. USB FS access, descriptors and native permissions
need independent qualification; no Bluetooth, clone-device expansion or privilege
fallback follows from the source code.

P10 must add only explicitly selected parity targets while preserving both
Encore and Switch presentations. The compared targets are role-based built-in
themes/accents, named navigation/settings/Home variants, Grid/Cover Flow and
theme-driven resources. An external runtime-pack host is a separate boundary;
scene/Themezer completion is not established even in the source. The target set,
persistence coexistence and optional UI-kit/runtime dependencies remain open.
Preserve case browsing/launch, accessibility, focus/selection, texture budgets
and language/display reconstruction rather than replacing the frontend.

P11 parses and admits a one-shot native request, then feeds the ordinary Encore
selection/session path. It must not bypass per-title settings, crash feedback,
Safe Launch, ExeFS checks or teardown. Define argument bounds, duplicates,
unknown/flag-like values, allowed roots and link handling before implementation.
The source accepts forms that do not fit Encore history; do not widen persisted
history as a shortcut. Moved-file fallback and non-persistable accepted paths
need an explicit policy. A host parser check cannot prove home-screen argument
delivery or already-running-process behavior.

Candidate surfaces are current pad/radio input and mapping boundaries; launcher
theme/state/navigation/drawing/texture and preference adapters; and native
startup plus a source-shaped forwarder helper. Any graphics-instance/shader
change or runtime-pack dependency needs its own checked owner boundary. The
source reports enumerate proposed counterpart files; no wholesale replacement
or complete file list is approved here.

### P09 — explicit USB backup and confirmed next-start restore

The owner permits designing this complete user-driven path:

```text
Choose an offered USB backup
    -> inspect and bind confirmation to the inspected content
    -> explicit user confirmation and operation admission
    -> stage locally and independently validate
    -> publish the pending-restore record
    -> require close/reopen; do not resume ordinary writers or launch
    -> next-start restore gate before ordinary migration/data writers
    -> verify application outcome or stop with retained recovery evidence
```

Backup likewise needs explicit selection, safe traversal, space estimation,
copy validation and completion publication. Incomplete output must not be
presented as a completed backup. Restore publication is a commit boundary;
later cancellation cannot promise that no pending operation exists.

Do not use per-game save import/export as this transaction. The source backs
up a fixed app-data root and performs a cold-start merge, retaining previous
content and recovery state; it does not supply an encrypted exact snapshot or
whole-tree atomic rollback. The final backup scope, merge/conflict rules,
credential/key inclusion, format compatibility and recovery retention still
need owner decisions. External roots selected by P05 are not automatically
backup payloads merely because settings contain their paths.

A complete current-writer census is required before planning the admission and
drain barriers. Include present media/metadata, library/history/preferences,
cache cleanup and startup migration as well as newly added writers. App gates
cannot promise control over external FTP writers. The exact startup gate must
account for Encore's writable-root probe, which itself may create the root;
copying the source's line position would not establish the intended ordering.
Safe-stat, no-follow/descriptor-relative operations, available-space reporting,
sync/rename and interruption recovery require separately authorized native
filesystem evidence. Host fixtures cannot qualify power loss or USB removal.

Candidate surfaces are native startup/storage/build, current service lifecycle,
launcher modal/action gates, a USB backend/service/UI and recovery documentation.
Do not add source-only collaborator calls for services Encore does not have.

### Evidence gates and remaining decisions

For each group, carry forward separate records for permitted source evidence,
chosen contracts, focused host checks, selected native compilation and attended
PS5 outcomes. No new build or feature test has run. Later compilation remains
Docker-only. Current source and earlier partial-port evidence are preserved,
not relabeled as acceptance of these new features.

Before Detailed Implementation can be complete, resolve the open choices in
[owner decisions](./decisions.md): theme/runtime target set; storage/picker and
profile scope; in-game strip/keyboard ownership; controller normalization and
source priority; forwarder admission; backup scope/recovery; and operation
admission representation. Protected-core, native and provider evidence may
remain external blockers even after the owner makes those choices.

Approval of this Technical Plan accepts the integration boundaries and explicit
blockers. It does not resolve the listed open choices, authorize production
changes, select new dependencies or certify implementation readiness.

Alternatives Considered and Detailed Implementation Plan have not been drafted
or approved yet.

## Additional Resources

- [Partial-port inventory and recorded validation limits](../PROSPEROEDEN_PORTS_2026-10-30.md).
- [Encore architecture boundaries](../../EGM.md).
- [Drafting flow and evidence provenance](./workflow.md).
- [Feature-source evidence matrix](./feature-evidence.md).
- [Owner decisions and open policy boundary](./decisions.md).

*Drafted with EGM*
