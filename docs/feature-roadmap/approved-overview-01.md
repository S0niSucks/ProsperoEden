# Encore — Remaining Feature Ports

- **Author:** OpenCode drafting assistant, for the repository owner
- **Status:** Draft — Overview and Goals awaiting owner review
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
- No silent save/settings reset, automatic migration, startup restore,
  credential disclosure, new feature-service requests or hardware experiments.
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
- What are the exact upstream behaviors of Nextendo, the missing theme/layout
  features and launch-argument forwarders?
- Which persistence, selection and conflict rules should apply to independent
  storage, network settings, cheat installation and USB restore?
- What supported hardware/game cases and observable outcomes will qualify
  each feature, including the explicitly experimental controller?

The Technical Plan, Alternatives Considered and Detailed Implementation Plan
have not been drafted or approved yet.

## Additional Resources

- [Partial-port inventory and recorded validation limits](../PROSPEROEDEN_PORTS_2026-10-30.md).
- [Encore architecture boundaries](../../EGM.md).
- [Drafting flow and evidence provenance](./workflow.md).

*Drafted with EGM*
