# Encore — Remaining Feature Ports

- **Author:** OpenCode drafting assistant, for the repository owner
- **Status:** Implementation authorized by owner; evolving design, fresh review pending
- **Date:** 2026-10-30
- **Updated:** 2026-10-31
- **Scope:** Twelve feature groups plus separate host/guest firmware compatibility
- **Current phase:** Autonomous application implementation and evidence gathering

Implementation state is tracked in [the twelve-group matrix](./implementation-20261031/progress.md)
and [the bounded application increment](./implementation-20261031/increment.md).
P10/P11 and application-side P05 are implemented locally, with explicit native
gates; the other groups are not represented as complete. Research compares all
four named upstreams. Five bounded EGM rounds returned findings; cap5 was reached
without convergence and the increment returned to drafting. The final Major
(exceptional directory-stream lifetime) has an author repair passing current host checks;
there is no sixth round or accepted-risk waiver. Author repairs
are not convergence. The original parent review is still historical/non-clean.
Saved policy decisions below are delegated, not blocked on the sleeping owner.

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

The owner also requires alternatives to the current Encore presentation, not
just recolouring that interface, and has named `ps5-homebrew-ui` as the base
reference for building them. Switch 2 support is now an additional requested
capability, with Eglantine proposed as a candidate to study. Neither a UI library
nor shared emulator ancestry proves that these additions already work in Encore.

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

On 2026-10-31 the owner expanded P10 to selectable themes and alternative
interfaces based on `D:/leonardommello/PS5/ps5-homebrew-ui`, and added P12 for
Switch 2 support, evaluating [Eglantine](https://github.com/Djihads80/Eglantine)
as a candidate. These are new design requirements beyond the original inventory;
they do not change its historical port or validation results.

The source application snapshot is
`5211718fb77f687a027dc5f675209bc64af4b982`; the recorded Encore base is
`21fdbc1ca7f5a74e4f4a67c0b721349cf1353ffb`. Encore also contains local,
uncommitted port work and architectural contracts. Future comparisons must use
the current working files as well as the recorded base, so earlier changes are
not lost or misclassified as missing.

The scope below contains twelve tracked groups, including that extension, but
is not yet a file-level implementation plan. The original eleven have focused
application-source comparisons. The new UI reference and Switch 2 candidate have
documentation-level intake only, not source/API or native compatibility audits.
Any inaccessible core dependency remains an explicit blocker.

### Feature groups to cover

These are proposed user-outcome and investigation targets. They do not certify
current behavior or settle storage formats, libraries, protocols beyond those
already named in the inventory, or UI structure.

| ID | Pending feature group | What the design must establish |
| --- | --- | --- |
| P01 | Amiibo via local .bin files only | A user can select a supported local `.bin` and explicitly load/remove the virtual Amiibo in a supported game. Folder browsing, names and optional PNG artwork aid selection. Physical NFC hardware/readers and native reader drivers are excluded; the file-to-core session, validation, cancellation and failure boundary still require integration evidence. |
| P02 | Per-code and live cheats, with explicit database installation | Define individual-code selection, how a live change reaches the running title, and a user-triggered database installation flow. Establish title/build matching and distinguish these behaviors from existing basic mod or cheat-file discovery. |
| P03 | SMB read-only game access, with a Settings editor | Specify configuring and editing a share through Settings, browsing and reading game data, and reporting authentication or connection failures. Treat a configuration file or partial adapter as insufficient evidence of this complete flow. |
| P04 | NFS read-only game access, with a Settings editor | Specify the corresponding NFS setup and read flow, including unavailable exports and interrupted access. Establish its actual requirements separately from SMB rather than assuming the protocols have identical behavior. |
| P05 | Independent games and wallpaper storage | Establish that a user can select games and wallpaper locations independently, including unavailable locations and restart behavior, without resetting or silently moving existing user data. Exact persistence and root-selection rules remain open. |
| P06 | Nextendo | Establish what the named upstream feature actually does from permitted sources before defining Encore behavior. Identify user actions, any external-service dependency and private state; do not invent an API or contact the service during design. |
| P07 | Native in-game keyboard | Define the text-entry request, successful entry, cancellation, input limits and return to the game. Verify the permitted native/core integration boundary before claiming that launcher text input satisfies it. |
| P08 | Experimental USB Pro Controller | Define the intended controller and connection mode, attachment/removal, player assignment and coexistence with current input. Keep its experimental qualification visible; partial driver code is not a completed controller feature. |
| P09 | USB backup and restore | Define the data included, user-selected backup/restore actions, overwrite handling and recoverable failure outcomes. Do not assume automatic startup restore or general transactional safety from existing save-copy helpers. |
| P10 | Selectable themes and alternative interfaces | Provide distinct visual themes and interface layouts/navigation, using ps5-homebrew-ui as an explicit construction reference. The current Encore presentation must not be the only available interface. Define shared feature behavior, selection/persistence, compatibility and recovery; no wholesale kit copy or default interface is selected yet. |
| P11 | Launch-argument forwarders | Establish which arguments the upstream feature forwards, who supplies them and how Encore admits and carries them to the intended launch. Define invalid-input and launch-failure outcomes without assuming arbitrary paths are safe. |
| P12 | Switch 2 support | Plan the capability and its feasibility gates, with Eglantine as a candidate. Establish engine integration, CPU/GPU/memory/native requirements, content and state separation, licensing and per-stage runtime evidence. A platform label or desktop upstream result is not completed PS5 support. |

## Goals

1. Cover all twelve feature groups, without silently reducing the requested
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
   Delivery order remains open and is not implied by the row order above.
8. Let the user choose alternative interface layouts as well as visual themes.
   The existing Encore presentation must not be the sole experience; preserve
   game-service behavior across the selected alternatives.
9. Keep Switch 2 support as an explicit capability goal with feasibility,
   integration and native-runtime gates. Do not report P12 complete based on
   discovery, a settings toggle, Eglantine's README or a desktop-only test.

## Non-Goals

- The owner's latest instruction authorizes production implementation and
  assistant-resolved product decisions while unavailable. Compilation remains
  Docker-only. Deployment, console operations, Git staging, commits and
  publication remain excluded. Authorization does not establish compatibility.
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
  hardware experiments. Implementing explicitly confirmed, validated next-start
  USB restore is permitted; applying a restore to real user data is not part of
  development verification.
- No unsupported release date, version bump, compatibility promise or claim
  that the old 1.006 candidate's results qualify this Encore tree.
- No new service, dependency, shared framework, persistence format or UI
  structure chosen merely to make the document look complete. Consequential
  choices are now delegated to the assistant and recorded with alternatives.
- No automatic adoption of Eglantine, import of all UI-kit designs/assets, or
  replacement of the current Switch engine. Dependency/core adoption still
  requires compatibility/provenance evidence, not just a repository name.

## Open Questions

These are remaining evidence inputs. Product policy choices are delegated and
recorded in decisions.md; no interruption is required while the owner sleeps.

- Which existing public integration points can support the core-dependent
  features without entering the excluded Eden subtree?
- Which compared theme/layout/runtime variants and forwarder argument rules
  should Encore expose, and which provider contracts can support Nextendo?
- Which persistence, selection and conflict rules should apply to independent
  storage, network settings, cheat installation and USB restore?
- What supported hardware/game cases and observable outcomes will qualify
  each feature, including the explicitly experimental controller?
- Which interface/theme combinations and recovery/default-selection rules
  should P10 expose, and how can the reference kit fit Encore's current owners?
- Can a Switch 2 candidate meet Encore's permitted core boundaries and actual
  PS5 memory/GPU/CPU constraints, and which integration model should be chosen?

The owner approved Overview and Goals, confirmed next-start USB restore as a
design direction, and permitted designing a default-off, session-bound Nextendo
exception. [Owner decisions](./decisions.md) records the scope of those choices.
Three focused source comparisons are complete; their
[evidence matrix](./feature-evidence.md) records the actual application boundaries
and limits for the original scope. The later P10/P12 requirements and their
[reference intake](./sources/themes-switch2.md) were not covered by those scouts
or round 01 of the design review. The initial approved wording is preserved in
`approved-overview-01.md`.

## Technical Plan (Architecture)

### Readiness boundary

This is a high-level design for twelve groups, not a claim that the whole
program is implementation-ready. The original source comparisons found substantial
application implementations, but several are tied to a different core, startup
policy, input mapping or service lifetime. Copying those files as a unit would
discard Encore behavior and import unverified assumptions.

The new `ps5-homebrew-ui` and Eglantine references have documentation intake,
not an implementation/API audit. P12 is an additional platform-support workstream,
not a port claimed to exist in the original ProsperoEden snapshot.

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

Chosen admission semantics (A02): reserve before dispatch; reject conflicting
new work with busy/cancel feedback. Root/profile change and launch explicitly
cancel and fully drain prior bound jobs before revalidation/admission. Native
dialogs serialize and retain buffers until system release; teardown waits for
actual owner acknowledgment, not just cancellation. USB excludes new application
writers after the existing owners drain. Published pending restore refuses
launch/writers through close/reopen/application. No implicit queue is selected.

### P01 and P02 — in-game actions and cheat management

P01 is **Amiibo via local `.bin` files only**, as explicitly clarified by the
owner in decisions.md AMIIBO-01. No physical NFC reader, antenna, USB/Bluetooth
tag device or native reader driver is required or planned. Internal virtual
Amiibo/NFP terminology in the selected core does not introduce physical hardware
into this scope.

The feature needs the file-backed virtual Amiibo adapter, validated `.bin`
selection/collection handling, bounded optional PNG decoding, explicit load/remove
actions and current GL and Vulkan display consumers. Preserve separate files even when model IDs match.
Browsing or highlighting must not present a tag. Missing artwork must not make
an otherwise supported dump unavailable. Dump reads and any supported guest writes need the
same descriptor/path admission boundary; discovery checks alone are insufficient.
Validate supported dump content/layout through the verified core parser, not
only the filename extension. Do not silently overwrite the user's original file.

P02 separates three paths: persistent per-code choices, a session-only mailbox
consumed by the guest's verified timing owner, and explicit catalog/cache/install
work. Catalog installation must not enable codes or mutate a running VM.
Identity must come from the effective admitted game content, not a title label
or filename; unsupported identity cases must remain unsupported rather than
guessing a build. Preserve whole-mod switches, mixed-mod behavior, Safe Launch
and existing glyph protections. Replace the source's post-read cheat size check
with a pre-read admission requirement in the eventual design, not a claim that
the source already protects allocation.

Disabling a live code stops future execution; it does not undo prior memory
writes or saved effects. Relaunch may be needed, and saved changes can persist.
Matching builds do not prove code safety or compatibility with other patches.

Both groups need an in-game request/display surface without losing Encore's
HOME, Capture, custom mappings, rumble or held-input suppression. Its exact
chord, menu structure and worker ownership are still open. File discovery and
image decoding must not be placed in render/timing callbacks. The selected
dump parser/virtual Amiibo, callback and destruction contracts remain evidence
blockers; physical NFC hardware/SDK availability is not a P01 blocker.

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

Read-only does not mean immutable. Chosen consistency contract (A04): trusted
user-controlled stable share; game/add-on content must not change during use.
Detected mutation poisons the reader and requires a fresh launch, never mixed
cached/new bytes after detection. Open-time metadata cannot detect every change
or malicious response. A qualified immutable snapshot may be added later; a
share path or metadata tuple alone is not one.

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

Chosen egress scope (A03): only verified explicitly admitted service destinations.
Unapproved guest direct-IP, alternate-resolver and peer traffic is denied; no
arbitrary nonofficial-host fallthrough. DNS redirection is not enforcement.
Until permitted socket/network owners enforce that scope and provider/trust/
authentication are established, the opt-in remains unavailable. Future wider
peer gameplay needs a separately named scope and consent, not silent broadening.

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

### P08 and P11 — controller and launch selection

P08 remains experimental and opt-in. It adds a physical wired controller route,
not a new guest controller type. Preserve Encore foreground-user selection,
failed-read recovery, uninitialized scratch, reserved slots, motion/rumble cleanup
and captured per-session mappings. Resolve Nintendo-label versus position
normalization, native/USB player-1 priority, glyph eligibility and opt-in UI before
wiring the source decoder. USB FS access, descriptors and native permissions
need independent qualification; no Bluetooth, clone-device expansion or privilege
fallback follows from the source code.

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

### P10 — selectable themes and alternative interfaces

The owner explicitly requests both themes and interfaces built with
[ps5-homebrew-ui](../../../ps5-homebrew-ui/README.md) as a base
reference. A theme changes visual roles, surfaces, type, motion and sound. An
interface changes screen composition, navigation and presentation of the same
library/actions. Changing only colours does not meet the requirement.

The reference documentation names 21 example designs, 30 themes and 99
components at local HEAD `bbe19054ee2e7ec069254775273aeb8846c4bd3e`.
Those counts are not a promise to import every example, nor evidence that any
combination is already compatible with Encore. Its themes, components and
adoption guides are input to a scoped integration comparison, not instructions
to replace Encore's renderer/platform/build tree.

The design must establish:

- A user-visible choice of interface and theme, with documented compatibility,
  persistence and save-failure behavior. The current Encore appearance must
  not be the only choice. The exact initial layouts, default and selection
  controls are not chosen by this request.
- Equivalent admitted actions and truthful state across interfaces, using the
  same Encore service/session boundaries. Do not create separate emulation,
  library, save, account or networking implementations per interface, or ship
  the kit's invented demonstration data as the real game library.
- A recoverable presentation when an interface/theme is missing or fails to
  load, without resetting unrelated settings or user data. Retaining existing
  Encore/Switch routes for compatibility is not a mandate that Encore remain
  the default. Exact recovery and default-selection policy remains open.
- Controller-first focus/navigation, readable TV-scale text, input-edge and
  glyph consistency, reduced-motion/high-contrast behavior and multilingual
  shaping. Do not replace current fonts with ASCII-only assumptions from a
  kit example.
- Bounded texture/font/image ownership and nonblocking resource work, preserving
  case browsing/launch, language/display reconstruction and current guest
  rendering. No new renderer or wholesale shader/batch copy is implied solely
  by selecting a different UI.
- A checked file/asset/license list for any reused code, fonts, sounds or art.
  Source screenshots, component counts and source-reported PS5 measurements
  are not Encore validation. Future host captures and attended PS5 navigation,
  frame-time, resource-lifetime and audio checks remain separate evidence.

The ProsperoEden comparison also identified Canopy/Slate, accents, layout
variants and Grid/Cover Flow. An external runtime-pack host and partial
scene/Themezer work are separate boundaries. Compare building registered
interfaces with adapted existing owners against a separately reviewed pack
host; neither a scripting runtime nor all source examples is selected yet.

Candidate Encore surfaces remain the launcher theme/state/provider/navigation,
widgets, textures, preferences and narrowly additive build/assets wiring.
Before any copying, compare the reference kit's actual drawing, input, font,
audio and lifecycle contracts with those current owners. The detailed
[reference intake](./sources/themes-switch2.md) records what was and was not read.

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
up a fixed app-data root, creates unencrypted backups and applies a cold-start
merge with previous content/recovery retained. It does not provide exact
snapshot restoration or atomic whole-tree rollback. The final backup scope, merge/conflict rules,
credential/key inclusion, format compatibility and recovery retention still
need owner decisions. External roots selected by P05 are not automatically
backup payloads merely because settings contain their paths.

Chosen trust model (A05): trusted user-owned backups only, with explicit warning
that hashes detect corruption, not authorship or benign restored payloads.
Confirmation binds inspected bytes but is not a signature. Independently validate
paths/scope. Exclude named account/network credentials before child access and
reject reinjected manifest/payload copies; that is not a general secret scanner.

A complete current-writer census is required before planning the admission and
drain barriers. Include present media/metadata, library/history/preferences,
cache cleanup and startup migration as well as newly added writers. App gates
cannot promise control over external FTP writers. The exact startup gate must
account for Encore's writable-root probe, which itself may create the root;
copying the source's line position would not establish the intended ordering.
Concurrent external modification is unsupported (A06). Require FTP/file-manager
and other external writers stopped before enumeration/inspection, through the
whole operation and staged restore close/reopen/apply. Detected changes fail with
retained recovery evidence, not promised whole-tree rollback. The app cannot
detect/exclude every privileged writer; this is an operating precondition.

Native metadata queries, link/type checks, no-follow descriptor-relative traversal,
available-space reporting (source uses fstatvfs), synchronization and rename need
native evidence. Host fixtures cannot qualify power loss or USB removal.

Candidate surfaces are native startup/storage/build, current service lifecycle,
launcher modal/action gates, a USB backend/service/UI and recovery documentation.
Do not add source-only collaborator calls for services Encore does not have.

### P12 — Switch 2 support: candidate and feasibility gates

Switch 2 support is now a requested capability. Evaluate
[Eglantine](https://github.com/Djihads80/Eglantine) as a candidate; it is not an
already selected or compatible Encore core. The inspected `nx2` snapshot is
`b40ad217ab829226841cb71f6e4c5cb05977a225`. Its repository description says
“VERY experimental”; its README claims game execution and names desktop builds.
Those claims must remain attributed to that project and cannot qualify PS5.

The pinned development notes record host experiments, automatic stubs, partial
graphics and title-specific progress, fixes and open issues. They also record
substantial memory/GPU pressure and desktop-specific assumptions. The notes
contain different stages: neither an early limitation nor a later successful
run is a universal current compatibility result. No source body, build or
runtime behavior was audited in this intake.

The design must answer these gates before implementation readiness:

1. **Provenance and lawful inputs:** establish the intended snapshot, file and
   dependency provenance, license obligations and supported user-owned content.
   Do not bundle proprietary games, keys, firmware or SDK material.
2. **Engine integration:** identify permitted API/ownership boundaries and compare
   a distinct candidate backend with selective reviewed adaptations under
   allowed maintained owners. Keep the existing Switch path working. Shared
   Eden ancestry does not establish interchangeable classes, ABI, shaders or
   source patches; the existing excluded subtree remains excluded.
3. **CPU/kernel/memory:** verify required instruction translation, guest services,
   scheduling, address spaces, executable-memory policy and total guest/host
   commitments against the actual native title budget. The upstream note of a
   particular desktop configuration requiring about 20 GiB of free commit is a
   risk to investigate, not proof that a console configuration can fit or cannot
   ever fit.
4. **GPU and presentation:** establish native capability and correctness for the
   required command/shader, texture, task/mesh, synchronization and presentation
   paths. Do not assume an NVIDIA desktop path, skipped shader, automatic stub
   or lower-fidelity stand-in becomes correct PS5 support. Preserve Encore's
   current renderer ownership and qualify each required capability separately.
5. **Library, state and services:** distinguish Switch and Switch 2 identity,
   supported content and selected backend without accidental routing to the old
   core. Define firmware/settings/cache/save boundaries and feature availability
   per platform. Do not overwrite Switch saves or advertise every P01–P11 feature
   as supported by the new backend without separate evidence.
6. **Evidence:** track discovery, admission/load, boot, graphics, audio/input,
   save persistence, repeated lifecycle and sustained gameplay separately.
   Qualify exact titles/versions/configurations on PS5 when separately authorized.
   A tile, file extension, desktop pass or README claim does not close P12.

Integration mechanism, dependency adoption, supported title set and resource
budgets remain decisions/evidence gates, not generated APIs or promises. If no
candidate meets the constraints, keep P12 planned and explicitly blocked rather
than deleting it from scope or calling metadata-only support complete. No
universal multi-emulator framework is selected by this addition.

See the [pinned-reference intake](./sources/themes-switch2.md) and its provenance
for the exact documents and limits. No Eglantine code or dependency was imported.

### Evidence gates and remaining decisions

For each group, carry forward separate records for permitted source evidence,
chosen contracts, focused host checks, selected native compilation and attended
PS5 outcomes. Docker application-fixture compilation has begun; exact outcomes
are recorded separately under implementation-20261031/, not generalized here.
Current source and earlier partial-port evidence are preserved,
not relabeled as acceptance of these new features.

Before Detailed Implementation can be complete, resolve the open choices in
[owner decisions](./decisions.md): theme/runtime target set; storage/picker and
profile scope; in-game strip/keyboard ownership; controller normalization and
source priority; forwarder admission; backup scope/recovery; and operation
admission representation. Protected-core, native and provider evidence may
remain external blockers even after the owner makes those choices.

P10's additional kit integration and P12's platform/engine choices are also open.
They need their own review; the first review round covered the earlier
eleven-group draft, not these additions. Author repairs/choices for A01–A10
are applied under delegated authorization, not independently verified closure.

The owner subsequently authorized implementation of the complete documented
scope, delegated decisions and requested all four NX2 upstreams and broader
firmware support. This supersedes the earlier documentation-only and interactive
approval gates, not preservation, inspection or verification limits. Earlier
approval snapshots and review-round bytes remain historical records.

## Alternatives Considered

| Decision | Selected mechanism and rationale | Alternative and trade-off |
| --- | --- | --- |
| Operation conflicts (A02) | Reject conflicting new actions with named busy/cancel feedback; the admitted owner continues. Before launch or root/profile mutation, explicitly cancel and fully drain relevant old jobs, then revalidate and reserve the new action. Published restore refuses launch/writers until restart. | Implicit queues retain stale player intent; unconditional cancel-and-wait can stall the UI on I/O. Generation checks alone do not drain resources. |
| Nextendo scope (A03) | Default-off, per-session, selected-user opt-in restricted to verified exact service destinations. Deny unapproved guest direct-IP, resolver and peer routes. Remain unavailable until allowed owners enforce this at socket/destination level and provider authentication is verified. | Wider peer gameplay can help games, but needs a separate named scope and informed consent; DNS-only redirection cannot enforce it. No unbounded nonofficial fallthrough. |
| Remote consistency (A04) | Trusted user-controlled stable share: game/add-on bytes must stay unchanged during a session. Detected change poisons the reader, stops loading/running safely and requires fresh launch. Explain that checks cannot detect every malicious or concurrent response. | Qualified immutable snapshots are stronger, but neither a path nor metadata tuple proves one. Support later when a real snapshot/version contract is available. |
| Backup trust (A05) | Explicitly trusted user-owned, unencrypted backups; warn that hashes detect corruption, not authorship or benign content. Never include named account/network credentials. Restore revalidates scope and content independently. | Signatures add key lifecycle and compatibility requirements and still do not prove benign content. No fabricated app signing identity. |
| External writers (A06) | Require FTP/file-manager/other external writes stopped before enumeration/inspection, throughout staging, close/reopen and apply. Detected changes fail with recovery evidence. | App-only locks cannot exclude other privileged processes; do not present them as coherent snapshots. |
| Appearance | Independent built-in palette and interface choices, reusing current rendering/input/font/services. Preserve Encore/Switch; add a distinct collection view, with dense list/grid behavior. Keep existing default for saved-config compatibility, not as the only choice. | Wholesale UI-kit/platform replacement risks existing languages, case rendering and input. Runtime packs add a loader/dependency/trust surface before built-in parity. |
| Forwarders | One bounded request under current local library/history admission, no arbitrary outside-root paths, same Safe Launch and teardown. Invalid/failed request returns to launcher; no guest-fault retry. | Broad absolute-path launch bypasses current storage/history contracts and changes threat assumptions. |
| Storage | Independent games and wallpapers selectors; changing roots is not migration. Apply games-root changes on restart so existing handles remain owned. | Immediate root replacement while metadata/game work lives risks mixed identities. Automatically copying saves is not implied by choosing games. |
| Switch 2 | Compare all four named upstreams and preserve existing Switch core; no selected replacement until feasibility is established. | A label-only platform toggle would misrepresent support; bulk core replacement would discard Encore adaptations. |

Live-cheat disablement stops future code execution, not earlier memory writes or
saved effects (A07). Exact-build matching does not prove safe code/patch interplay.

## Detailed Implementation Plan — active increments

The complete scope remains tracked, including blocked interfaces. The following
owned increments are ready without inventing protected-core APIs:

1. Preserve an external byte baseline of the current owned tree, including prior
   uncommitted ports. Do not rewrite source READMEs or remove earlier work.
2. P10: add shared bounded appearance identifiers; extend both preference models
   and settings load/save without replacing legacy appearance/theme; update the
   existing provider round trip (including FSR/AA); add native palette application,
   collection drawing/navigation and controls reachable from both old interfaces.
   Host-preview and focused persistence/navigation witnesses are required.
3. P11: new headless/forwarded_launch.h and tools/check-forwarded-rom.py with
   fail-closed path/argument tests; primary integrates main.cpp through ordinary
   selection/per-title/history flow. docs/FORWARDER.md states native argv limits.
4. Continue independent storage, feature adapters and native capability checks
   only against established allowed interfaces. Record exact files/tests in each
   increment before edits; do not add unavailable UI promises to hide blockers.

Execution records, research and review receipts live under
implementation-20261031/ outside the repository. Source contracts are updated
only after the corresponding behavior exists, never to present goals as code.

## Firmware compatibility scope

Track **PS5 host firmware** separately from **Switch/Switch 2 guest firmware**.
Host elevation/SDK/import/JIT/GPU capability checks must retain existing safety
guards; no guessed offsets or permissive unknown-version fallback. Guest firmware
parsing/services depend on the selected core and user-supplied lawful content.
Unknown versions remain unknown, not supported because a version string parses.
No universal firmware claim or console test is authorized by this requirement.

## Additional Resources

- [Partial-port inventory and recorded validation limits](../PROSPEROEDEN_PORTS_2026-10-30.md).
- [Encore architecture boundaries](../../EGM.md).
- [Drafting flow and evidence provenance](./workflow.md).
- [Feature-source evidence matrix](./feature-evidence.md).
- [Owner decisions and open policy boundary](./decisions.md).
- [Themes/interfaces and Switch 2 reference intake](./sources/themes-switch2.md).
- [PS5 UI construction reference](../../../ps5-homebrew-ui/README.md).
- [Eglantine pinned upstream README](https://github.com/Djihads80/Eglantine/blob/b40ad217ab829226841cb71f6e4c5cb05977a225/README.md).

*Drafted with EGM*
