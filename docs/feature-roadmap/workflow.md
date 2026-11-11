# Remaining feature ports — drafting flow

Migration note (2026-11-02): this file preserves the historical drafting flow
and its stage-specific authority/worker state. The maintained documentation now
lives in this repository; use the [index](README.md),
[current progress](implementation-20261031/progress.md) and
[decisions](decisions.md) for the later state. Temporary paths below identify
historical artifacts, not the location for future permanent documentation.

## Current execution authority — 2026-10-31

Owner now authorizes autonomous production implementation, all documented feature
groups, themes and distinct interfaces, four NX2 upstream comparisons and separate
PS5-host versus guest-firmware compatibility. Policy choices are delegated, not
pending interactive approval. Earlier workflow/approvals below are preserved as
history. No console/deploy, staging/commits/publication or protected-core inspection.
Builds strictly Docker. Primary baseline preserves 1,526 owned files externally
under implementation-20261031/primary-baseline/ (concurrent forwarder files are
explicitly worker-owned, not original byte-freeze assertions).

Round 01 reviewed the old 402-line target/hash, not current expanded design.
Do not rewrite original reviewer reports or mark convergence. Decisions.md and
the design alternatives record A02–A06 outcomes under delegated authority.
Current work: primary owns integration/settings/UI; forwarder worker owns only
its helper/test/doc; three researchers are read-only. No overlapping source edits.

## Working agreement

The owner selected all pending feature ports, then delegated the workflow to
the assistant. That authorizes choosing the drafting target and process; it
does not approve an unreviewed architecture or authorize implementation.

Working artifact directory:
`C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/encore-feature-ports-20261030`.
The target for drafting and later review is [design_draft.md](./design_draft.md).
No repository files need to change during the initial drafting phase.

## Sequence

1. Draft The Problem, Overview, Goals and Non-Goals from the approved scope
   and the existing inventory. Present the Overview and Goals approval gate.
2. After approval, compare the permitted application-owned source boundaries
   in ProsperoEden and the current Encore working tree. Check local project
   instructions before inspecting any new subtree. Do not inspect excluded
   vendor internals or inherit upstream execution claims.
3. Draft the Technical Plan. Separate evidence from proposals, expose missing
   interfaces, and present any substantive architecture choice to the owner.
   Request approval of that section before proceeding.
4. Record Alternatives Considered from actual owner decisions and their
   evidence. No alternatives were rejected in a predecessor brief; do not
   fabricate that history. Request approval of the section.
5. Draft one Detailed Implementation Plan with every proposed created,
   modified or removed file and the reason and tests for each. If required
   evidence or decisions remain missing, keep the relevant feature blocked
   rather than presenting the whole program as ready. Request approval.
6. When the draft is ready, run `egm-review` on this explicit target, using the
   interactive full pipeline. Use fresh-context reviewers, the actual persona
   files and standards, and the document's permitted reading list. Preserve
   the stage gates. Do not pass the parent conversation as reviewer context.
7. Use `egm-resolve` for actual findings. Save proposed fixes separately and
   obtain approval before applying them. Preserve contested and unresolved
   issues; do not replace a review with this author's own assertion.
8. Finalize the document after the required approvals and target-directory
   gate. Implementation is a separate authorization, not the last automatic
   step of this documentation workflow.

The completed contract bootstrap and its original drift report are not rerun.
Changes to contracts, if later justified by an approved design or implementation,
must be scoped separately from this initial draft.

The owner approved Overview and Goals through the section gate. The initial
draft is preserved as `approved-overview-01.md`. Three independent read-only
source comparisons are complete: game integration (P01/P02/P06/P07), storage
and network access (P03/P04/P05/P09), and input/presentation/launch (P08/P10/P11).
Their reports are under `sources/`, with a synthesis in `feature-evidence.md`.
These scouts are not the later formal design reviewers.

The owner also approved designing explicitly confirmed next-start USB restore;
the overview's startup-restore exclusion was clarified accordingly. This does
not authorize a runtime restore. `decisions.md` records that decision and the
Nextendo/offline-policy boundary. The owner then permitted designing a default-off,
session-bound Nextendo exception, without enabling it or resolving provider/core
authentication. Both decisions are recorded in `decisions.md`.

The owner approved the Technical Plan's boundaries through its section gate. The
gate version is preserved in `approved-technical-plan-01.md`. It covers all eleven
groups and distinguishes proposed integration boundaries from unresolved owner
choices and external evidence blockers. No Detailed Implementation file list or
readiness approval is implied. Alternatives intake is next; the only recorded
unselected gate paths so far are retaining the USB startup-restore block and
keeping guest-offline-only Nextendo.

## Scope evidence

- Scope was selected in chat: all pending feature ports.
- The existing inventory names the remaining groups in lines 31–35 and the
  preservation/core-inspection limits in lines 28–42. Splitting SMB and NFS
  into separate tracking rows preserves both named protocols; it does not
  choose separate implementations.
- No predecessor `concepts_brief.md` or `discussion_summary.md` was found in
  the workspace or the conversation scratch area. This draft uses the chat
  scope and the inventory instead of inventing a prior architecture decision.
- No feature-specific source comparison or new build/runtime check has been
  completed for this draft. Earlier EGM source reviews are architectural
  evidence, not a feature-parity audit.

## Recovered skill instructions

The local installation lacks the shared EGM runtime resources. The installed
drafting skill was compared with the official repository at commit
`403d8881d62022490637d740657b33f01e321ea8`; its text matches after normalizing
line endings and outer whitespace.

- Installed `egm-draft/SKILL.md` SHA-256:
  `1718eab3546b0888ef58dcca20c0143cd431a6645da8c6f633840d617b0fb621`.
- Upstream raw `skills/egm-draft/SKILL.md` SHA-256:
  `7908cb6d6b9ab82fef416211988ab9abd6681d4c783b2c31b1a8c40f97009ab0`.
- Read the [pinned Codex integration](https://raw.githubusercontent.com/google/elephant-goldfish-model/403d8881d62022490637d740657b33f01e321ea8/docs/codex.md).
- Read the [pinned execution instructions](https://raw.githubusercontent.com/google/elephant-goldfish-model/403d8881d62022490637d740657b33f01e321ea8/resources/execution_instructions.md).

These were read-only reference requests. No plugin was installed and no local
skill files were repaired or overwritten. Thirteen shared reference files,
including the review personas and standards, were then saved under
`egm-runtime/`; every copy's Git blob was checked against the pinned upstream
tree. `egm-runtime/provenance.json` records URLs and hashes. The personas and
standards still must be read before review dispatch. Loading or downloading them
alone is not a completed review or a complete plugin installation.

## Source comparison preservation

`source-comparison-baseline.json` records both working-tree states and HEADs.
`tracked-owned-hashes-before.json` records 760 tracked owned files from
ProsperoEden and 1,111 from Encore, excluding vendor/third-party subtrees and
symlinks. The earlier Encore preservation receipts also cover untracked prior
port files. Neither source tree is a scratch or output directory.

Serena reports no active project and no known projects. No project was activated
and no configuration was created. Source scouts were instructed to use read-only
file/search tools where the symbolic tools cannot operate without setup.
