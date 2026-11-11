# EGM Style Review

- **reviewer:** egm-style
- **round:** 01
- **status:** completed
- **Mode:** REVIEW; persona rules only, no additional style standard
- **Target:** `C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/encore-feature-ports-20261030/design_draft.md`
- **Range:** 1–402, full file
- **Independently computed SHA-256:** `7cfca41de726b99b4925cba4da9ae119d60b7f3a7ab2486141048b92ce6d0f21`
- **Hash comparison:** matches expected

Review concerns clarity, precision, brevity and framing—not implementation readiness.

## Required-read receipt

Path aliases:

- **B:** `C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/encore-feature-ports-20261030`
- **E:** `D:/leonardommello/PS5/ProsperoEden-Encore`

| Input | Delivered lines |
|---|---:|
| `B/egm-runtime/agents/style.md` — read first | 1–66, full |
| `B/feature-evidence.md` | 1–91, full |
| `B/decisions.md` | 1–55, full |
| `B/workflow.md` | 1–115, full |
| `B/sources/game-integration.md` | 1–197, full |
| `B/sources/storage-network.md` | 1–169, full |
| `B/sources/input-presentation.md` | 1–141, full |
| `E/docs/PROSPEROEDEN_PORTS_2026-10-30.md` | 1–93, full |
| `E/EGM.md` | 1–124, full |
| `B/design_draft.md` | 1–402, full |

Applicable instructions were read before repository sources: `D:/leonardommello/PS5/AGENTS.md` 1–9 and its referenced `.agents/skills/ps5-knowledge/SKILL.md` 1–52. No more-specific applicable `AGENTS.md` was found along the inspected paths.

Referenced supporting reads:

- `B/approved-overview-01.md`: 1–138, full.
- `B/approved-technical-plan-01.md`: **partial**, 1–155 and 329–401.
- `E/README.md`: 1–289; `CONTRIBUTING.md`: 1–85; `SECURITY.md`: 1–168; `docs/LIBRARY_INTERCHANGE.md`: 1–43—all full.
- Linked EGM contracts, all full: `headless/EGM.md` 1–40; `src/EGM.md` 1–171; `tools/EGM.md` 1–38; `headless/elevation/EGM.md` 1–33; `headless/prosperoeden/EGM.md` 1–32; `headless/prosperoeden/host/EGM.md` 1–26; `tools/ci/EGM.md` 1–30; `tools/launcher/EGM.md` 1–28; `tools/stubs/EGM.md` 1–24.

**Inaccessible required inputs:** none. Transitive implementation citations were not exhaustively re-audited; source-level conclusions remain attributed to the supplied comparisons. Proposed filenames were not treated as existing implementations. No writes, execution checks, project activation or excluded-file inspection occurred.

## Findings

### egm-style-01-1 — Stale phase wording obscures current status

- **Original severity:** Nit
- **Exact target lines:** 43–46 and 91–92
- **Discrepancy:** The Overview presents application-source comparison as the “next drafting phase,” and Goal 7 refers to boundaries being settled “during the technical plan.” Lines 135–138 already report completed comparisons, while lines 386–389 report approval of the Technical Plan’s boundaries.
- **Rationale:** These future-tense instructions make the current draft read partly like its earlier approval snapshot. Readers must reconcile outdated process wording with the current status.
- **Recommendation:** State that comparisons are complete and the Technical Plan records the integration boundaries. Identify delivery ordering and remaining choices as unresolved, rather than directing readers toward an apparently unwritten plan. Leave preserved approval snapshots unchanged.

### egm-style-01-2 — Compound negation hides separate backup limitations

- **Original severity:** Nit
- **Exact target lines:** 349–352
- **Discrepancy:** “Does not supply an encrypted exact snapshot or whole-tree atomic rollback” combines encryption and restoration semantics within one negated description.
- **Rationale:** The source evidence establishes two separate facts: backups are unencrypted, and restoration is a merge rather than exact snapshot restoration. The compound wording makes the encryption limitation unnecessarily indirect.
- **Recommendation:** Separate the claims, for example: “The source creates unencrypted backups and applies a cold-start merge with retained recovery data. It does not provide exact snapshot restoration or atomic whole-tree rollback.” Keep these explicitly attributed to the source, not the eventual Encore policy.

### egm-style-01-3 — “Safe-stat” is undefined safety shorthand

- **Original severity:** Nit
- **Exact target lines:** 363–365
- **Discrepancy:** “Safe-stat” appears without a definition, named helper or explanation of the required safety property.
- **Rationale:** Unlike the adjacent operation names, the term does not identify a clear contract. It could mean a particular metadata-query wrapper, link/type validation, or firmware-compatible syscall behavior. Its “safe” qualifier also sounds stronger than the immediately stated lack of native evidence.
- **Recommendation:** Identify the intended helper/API or briefly describe the metadata-query behavior requiring verification. Retain the explicit native-evidence uncertainty; no implementation choice needs to be settled merely to clarify this sentence.

**No Blocker or Major findings.**
