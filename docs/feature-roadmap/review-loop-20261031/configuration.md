# Feature-design review loop — configuration

Date: 2026-10-31. Proposed configuration, awaiting owner confirmation.

## Target and scope

- Target: `C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/encore-feature-ports-20261030/design_draft.md`.
- Semantic target: **full file**. Current bounds: lines 1–402. Before each round,
  reread the actual target and recalculate the line bounds and content hash.
- Review the current feature design against the feature-specification standard.
  Do not run an implementation-readiness review: there is no Detailed
  Implementation Plan yet.
- Drafting is paused for this manual review-loop invocation. No alternatives,
  implementation choices or reviewer findings are silently filled in first.

## Fresh reviewers and standards

All paths below are under
`C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/encore-feature-ports-20261030/egm-runtime`.
These are verified reference copies from official EGM commit
`403d8881d62022490637d740657b33f01e321ea8`, not similarly named project personas.

| Reviewer | Persona | Standard |
| --- | --- | --- |
| `egm-design` | `agents/design_critic.md` | `resources/document_standards/feature-spec-standard.md` |
| `egm-structural` | `agents/structural.md` | `resources/document_standards/feature-spec-standard.md` |
| `egm-style` | `agents/style.md` | Persona's own style rules; no additional style standard |

Use direct parallel dispatch through native general-purpose subagents. The tool
starts each new child with fresh context; do not reuse source-scout sessions or
pass the parent transcript, memory or private reasoning. Reviewers are read-only:
they return their complete reports to the parent, which saves them as artifacts.
Each must read its persona, target and assigned standard before producing findings.

## Goldfish context

Required reading list, using full paths resolved before dispatch:

1. `../feature-evidence.md`.
2. `../decisions.md`.
3. `../workflow.md`.
4. `../sources/game-integration.md`.
5. `../sources/storage-network.md`.
6. `../sources/input-presentation.md`.
7. `D:/leonardommello/PS5/ProsperoEden-Encore/docs/PROSPEROEDEN_PORTS_2026-10-30.md`.
8. `D:/leonardommello/PS5/ProsperoEden-Encore/EGM.md`.

Read the target's existing linked/referenced files as required by the EGM workflow,
within the permitted inspection boundary. Proposed future file names are not
claims that those files already exist. Record actual reading limits and inaccessible
required inputs; do not report an incomplete review as a clean result.

Exclude vendor/third-party and selected Eden internals, credentials/private
configuration, unrelated user files, conversation history and `.serena` memories.
No builds, tests, installs, feature-service calls, hardware operations, repository
writes, Git staging or commits. Do not activate/configure Serena merely to read
sources; use a read-only fallback if no project is active.

## Round and resolution rules

- Assign stable finding IDs: `<reviewer>-<round>-<number>`. Preserve each
  reviewer's severity, affected target lines, discrepancy and rationale.
- Record every requested reviewer as completed or not run. Retry a failed or
  missing report once; a second failure is not run, never zero findings.
- Preserve raw reports. Consolidate findings without hiding their source IDs.
  The primary context then follows `egm-resolve` and writes
  `goldfish_review_report.md` with proposed edits, contests, escalations or
  declined nits.
- No draft edit occurs until the owner approves the proposed repairs. Do not
  use repair approval as permission to choose unresolved architecture.
- Judge round cleanliness from returned findings **before repairs**. A clean
  round has no unsettled Blocker/Major and no reviewer marked not run.
- Convergence requires two consecutive clean rounds. Cap: five rounds. Track
  settled findings, failed fixes and churn by issue and scope.
- After each resolution, wait across the owner turn. Start another round only
  when the owner explicitly requests it. On cap or churn, expose open findings
  and request the prescribed continuation decision.

No reviewer has been launched and no convergence result exists at configuration
time. Reports will remain outside both source repositories.
