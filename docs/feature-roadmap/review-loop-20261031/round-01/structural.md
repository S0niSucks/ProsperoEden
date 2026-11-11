# Structural review — round 01

- **reviewer:** `egm-structural`
- **round:** `01`
- **status:** `completed`
- **Mode:** `REVIEW`
- **Target:** `design_draft.md`, full file, lines **1–402**
- **Independently computed SHA-256:** `7cfca41de726b99b4925cba4da9ae119d60b7f3a7ab2486141048b92ce6d0f21`
- **Hash verification:** Matches the supplied expectation.
- **Findings:** **1 Major**; no Blocker or Nit findings.

The heading hierarchy is coherent. The current specification does **not yet satisfy the assigned standard’s mandatory alternatives evaluation**. This is a feature-specification completeness finding, not an implementation-readiness assessment.

## Required-read receipt

Path prefixes:

- **B:** `C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/encore-feature-ports-20261030`
- **R:** `D:/leonardommello/PS5/ProsperoEden-Encore`

All ranges below were delivered and read completely.

| Input | Path | Delivered lines |
|---|---|---:|
| Actual reviewer persona, read first | `B/egm-runtime/agents/structural.md` | 1–65 |
| Complete assigned standard, read second | `B/egm-runtime/resources/document_standards/feature-spec-standard.md` | 1–36 |
| Target | `B/design_draft.md` | 1–402 |
| Required context | `B/feature-evidence.md` | 1–91 |
| Required context | `B/decisions.md` | 1–55 |
| Required context | `B/workflow.md` | 1–115 |
| Required context | `B/sources/game-integration.md` | 1–197 |
| Required context | `B/sources/storage-network.md` | 1–169 |
| Required context | `B/sources/input-presentation.md` | 1–141 |
| Required context | `R/docs/PROSPEROEDEN_PORTS_2026-10-30.md` | 1–93 |
| Required context | `R/EGM.md` | 1–124 |

### Applicable instructions and followed references

| Path | Delivered lines |
|---|---:|
| `D:/leonardommello/PS5/AGENTS.md` | 1–9 |
| `D:/leonardommello/PS5/.agents/skills/ps5-knowledge/SKILL.md` | 1–52 |
| `B/approved-overview-01.md` | 1–138 |
| `B/approved-technical-plan-01.md` | 1–401 |
| `R/README.md` | 1–289 |
| `R/CONTRIBUTING.md` | 1–85 |
| `R/SECURITY.md` | 1–168 |
| `R/docs/LIBRARY_INTERCHANGE.md` | 1–43 |

**Inaccessible required inputs:** None.

**Reading limits:** Direct draft documentation links and the referenced approval snapshots were followed. The source-comparison reports were read fully; their transitive implementation citations and reported execution evidence were not independently re-audited or reproduced. Proposed future filenames were not treated as existing-file claims.

No files were written, agents spawned, builds/tests executed, services contacted, or excluded internals/private state inspected. No parent history, memories or prior reviewer reports were used.

## Extracted heading map

```text
L1   H1 Encore — Remaining Feature Ports
L9     H2 The Problem
L29    H2 Overview
L48      H3 Feature groups to cover
L68    H2 Goals
L94    H2 Non-Goals
L118   H2 Open Questions
L140   H2 Technical Plan (Architecture)
L142     H3 Readiness boundary
L162     H3 Control and data boundaries
L194     H3 P01 and P02 — in-game actions and cheat management
L225     H3 P03, P04 and P05 — network sources and independent storage
L261     H3 P06 and P07 — identity, network policy and native dialogs
L293     H3 P08, P10 and P11 — controller, presentation and launch selection
L329     H3 P09 — explicit USB backup and confirmed next-start restore
L371     H3 Evidence gates and remaining decisions
L394   H2 Additional Resources
```

No skipped heading levels, malformed table structure or unclosed fenced code blocks were found.

The assigned standard specifies required **content**, not an exact target heading template. Consequently, neither the absence of a separately titled Risks section nor the absence of Detailed Implementation is reported as a missing mandatory header.

## Findings

### egm-structural-01-1 — Mandatory alternatives evaluation remains deferred

- **Original severity:** Major
- **Exact target lines:** **188–192, 379–392**
- **Assigned-standard requirement:** `feature-spec-standard.md`, **15–18**, requires evaluation of viable alternatives and specific documented reasons for rejecting them; **28–34** identifies alternatives and rejection rationale as applicable specification content.

**Discrepancy:** Lines 188–192 identify central ownership versus coordinated existing owners as an architecture choice without evaluating their trade-offs. Lines 379–384 defer other consequential choices, and lines 391–392 explicitly state that Alternatives Considered has not been drafted.

The linked `decisions.md:21–40` provides a useful Nextendo two-option comparison. That is partial alternatives evidence, not a complete absence of alternatives. However, the current specification still lacks the mandatory evaluation supporting its broader proposed design.

**Rationale:** A fresh reader can identify proposed boundaries and unresolved choices, but cannot yet assess why materially different viable designs were rejected. Announcing that this evaluation will be drafted later accurately describes the document’s stage; it does not satisfy the current assigned standard.

**Actionable recommendation:** Add an alternatives evaluation using actual decisions and evidence. Record each material option’s mechanism, trade-offs, disposition and decision-specific rationale. Incorporate or explicitly summarize the existing Nextendo comparison, and evaluate consequential architecture choices such as operation admission ownership. Keep undecided options explicitly pending; do not fabricate rejection history or silently select them.

This recommendation does **not** require a Detailed Implementation section, code changes, builds or resolution of every external evidence blocker.
