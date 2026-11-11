# ProsperoEden integration: established defects, remaining gates, and causal hypotheses

* **Author:** Leonardo Medeiros de Mello Jr (repository Git identity; analysis prepared with Codex)
* **Date:** 2026-11-02; reviewed and revised 2026-11-03
* **Status:** Six fresh-context Goldfish reviews completed after the user approved review. The user subsequently clarified the expected outcome as the complete requested delivery. This clarification and the delivery matrix below were added after review; they have not received a separate Goldfish pass. No recovery implementation strategy has been approved.
* **Inspected checkout:** `5211718fb77f687a027dc5f675209bc64af4b982`

## Overview

The established observation is that documented component progress coexisted with unresolved target/provider prerequisites, shared-state composition work, and final qualification of the combined application. Component completion did not establish usable target workflows or a qualified combined release. The records explicitly distinguished those milestones; they do not show that component owners falsely claimed product completion.

Repository records describe a complete-scope v1.006.000 request and say that using a team changed execution, not scope ([UPSTREAM_INTEGRATION.md](docs/UPSTREAM_INTEGRATION.md), lines 5–6; [IMPLEMENTATION_TEAM.md](docs/IMPLEMENTATION_TEAM.md), lines 3–4). After the review, the user answered “Tudo!” when asked what expected result was missing. For this analysis, the expected outcome is therefore the complete requested application, working together on PS5; isolated component work does not satisfy that outcome. A smaller release has not been selected as a substitute. Relative priorities, timeframe and the historical sequence of decisions remain unknown. These distinctions limit causal blame, but the desired delivery scope is no longer wholly unknown.

The preliminary investigation used selected documents, Git history and a targeted theme-storage source check. This review used the frozen brief and bounded local readings; it retrieved no conversation history or memory and accessed no private evidence archives. Reported tests were not replayed, private hashes were not independently verified, and no current provider or console behavior was established. Historical results retain their snapshot boundaries. See [goldfish_review_report.md](goldfish_review_report.md) for reviewer coverage, limitations and dispositions, and the unchanged [frozen input](.local/egm-review-20261103/concepts_brief.input.md) for original line references.

## Analysis

### 1. There was an earlier build-verified checkpoint with a reported console smoke test

[UPSTREAM_INTEGRATION.md](docs/UPSTREAM_INTEGRATION.md), lines 254–315, records a passing host suite, a successful native package build through `9ef8012`, package verification, and a subsequent user report that the package worked normally on PS5. The report did not include a game/firmware/scenario matrix.

Later work targeted a release combining 17 upstream PR heads with additional features. [IMPLEMENTATION_TEAM.md](docs/IMPLEMENTATION_TEAM.md), lines 13–30, names seven parallel workstreams and assigns shared integration, review, cheats, and release qualification to the coordinator. [CHANGESET_1006000.md](docs/CHANGESET_1006000.md), lines 8–32, lists the combined scope.

**Inference:** multiple streams shared release-critical state and operation lifetimes. Component delivery could precede composition and qualification. Coordinator responsibility concentrated dependencies, but coordinator capacity, excessive scope and avoidable delay have not been demonstrated as causes. Their relative contribution requires a chronology of known prerequisites and acceptance decisions, not a count of workers or features.

**Counterpoint:** the team record says parallel implementation was requested by the user. This does not establish unauthorized scope expansion, nor does it show that parallel work itself was the mistake. The issue to examine is how work was divided and accepted.

### 2. Shared lifecycle rules needed substantive repairs during integration

[USB_WRITER_ADMISSION_AUDIT.md](docs/features/USB_WRITER_ADMISSION_AUDIT.md), lines 9–40, records concrete defects at `62e99ba`: a settings writer could pass the USB check before locking and then write during backup; a new worker could start after the old worker had been joined; remote stop could return while callbacks still wrote; apparent read operations could migrate settings or create files.

These are behavioral defects, not simply missing paperwork. The repair established a shared order for admitting work, acquiring the bridge lock, and draining writers. [IMPLEMENTATION_TEAM.md](docs/IMPLEMENTATION_TEAM.md), lines 40–76 and 440–469, records later integration and reciprocal USB/network/theme/Nextendo guards.

**Inference:** the tested caller set and shared contract had to expand as later operations were composed. The team did document admission, draining, final callbacks, lock order and teardown requirements (lines 440–469); ownership alone did not demonstrate that every caller obeyed them. The useful distinction is documented invariant, callers audited against it, and composed behavior demonstrated on a particular snapshot. These records do not establish when each rule first became known or that all follow-up work was avoidable.

The coupling is concrete: operations retain roots, publish files or finish callbacks after cancellation, so exclusion must last through their final side effect. Conservative shared exclusion may be simpler than allowing concurrent transactions. This evidence does not establish a need for a new scheduler or architectural rewrite.

**Current limit:** the audit describes an older snapshot and its corrections. It is not evidence that those exact races remain in the inspected checkout.

### 3. Some tests validated an assumption that had not been checked against its dependency

[NEXTENDO_PUBLIC_OAUTH_CONTRACT_REVIEW.md](docs/features/NEXTENDO_PUBLIC_OAUTH_CONTRACT_REVIEW.md), under “Conclusions,” “Contract comparison and source locations,” and “Offline reproduction and limits,” reports that the initial conversion expected nested `account.pid` and `nex_token` directly from OAuth. Its synthetic success fixture supplied that shape. The captured provider documentation instead described a web access token followed by a separate game-token request. An offline fragment test rejected the documented shapes.

**Finding:** the earlier passing tests did not establish compatibility with the documented provider contract. Implementation and fixture shared the same assumption.

[IMPLEMENTATION_TEAM.md](docs/IMPLEMENTATION_TEAM.md), lines 52–58, records subsequent OAuth/web/identity/NEX repairs. [features/NEXTENDO.md](docs/features/NEXTENDO.md), lines 33–64, still identifies independent prerequisites: registration, browser/callback operation on the PS5, a suitable game-authentication contract, routes, trust, and actual game/server evidence.

**Inference:** earlier contract and target-feasibility checks might have clarified the deliverable boundary. Whether that information was available before the relevant acceptance decision, or would have changed it, is unestablished. The historical response mismatch is not an unfixed current bug or an observed live-server failure.

### 4. Aggregate validation repeatedly discovered fixture and environment mismatches

[IMPLEMENTATION_TEAM.md](docs/IMPLEMENTATION_TEAM.md), lines 155–226, records native compilation/link attempts followed by failing checks, host fixtures missing newly composed members, missing container server tools, and later link/cleanup fixture repairs. At `2bdedef`, native linking succeeded but the aggregate failed at translations; the host aggregate stopped at a Nextendo fixture missing theme/network members. No package was produced by that failed aggregate.

[CHANGESET_1006000.md](docs/CHANGESET_1006000.md), lines 41–46, subsequently records a passing 764-text/30-catalog checker after translation corrections. Lines 31–32 also record replacing directory reopening that relied on a native-libc-unsupported direct `dup` syscall.

**Finding:** aggregate attempts encountered preparation, fixture and environment gaps that required additional work. A passing host fragment could not answer all the questions needed by the native application. Earlier representative focused checks might have avoided some reruns, but their cost and avoidability are unmeasured. The existing team constraints already preferred focused checks and batched whole builds (lines 475–476).

**Counterpoint:** failing checks caught real problems and were retained rather than waived. The proposed lesson is earlier, more representative checking—not removing checks or counting several partial passes as a complete run. This investigation has not measured build time, token use, or the cost of each rerun.

### 5. “Current status” is spread across documents that disagree

There are concrete stale-status examples:

* [UPSTREAM_INTEGRATION.md](docs/UPSTREAM_INTEGRATION.md), lines 566–590, calls itself the current composition checkpoint but still describes network extraction/composition as pending. It delegates to the newer team ledger, which records those integrations.
* [IMPLEMENTATION_TEAM.md](docs/IMPLEMENTATION_TEAM.md), lines 229–240, still treats the unchanged translation failure as a release blocker. Its last commit is `a26ea5d`, earlier than the changeset's recorded correction.
* [CHANGESET_1006000.md](docs/CHANGESET_1006000.md), lines 41–46, reports the corrected checker passing, while lines 73–75 retain wording about catalog/IME-review blocks. Lines 56–60 already record a separate review and repair of the new keyboard change.

**Finding:** an investigator must reconstruct chronology to distinguish a repaired historical failure from a still-required final-snapshot qualification. That increases the chance of duplicating work or reporting an obsolete blocker. A fresh final run remains required; correcting the stale description would not establish that run.

The theme handoff has the same maintenance problem: lines 54–60 record coordinator composition, while lines 80–83 retain the earlier branch-local warning that the hunks are not production changes. Both can describe their own snapshots. This is fragmented status reconciliation, not absence of acceptance controls: the records retain hashes, commands, exit status and explicit refusal to claim aggregate success. No duplicated work or delay caused by the drift was demonstrated.

| Example | Recorded failure or boundary | Later disposition in the read records | Qualification still absent |
| --- | --- | --- | --- |
| Catalogs on `2bdedef` | Aggregate failed at 764 texts/30 catalogs | Changeset lines 41–46 record corrected focused checker success | Complete final frozen host/native run and console font checks |
| Composed Nextendo fixture | Missing theme/network members stopped host aggregate | Team lines 194–200 record real-ledger fixture repair and focused passes | Complete aggregate on final snapshot |
| Reciprocal theme guards | Original branch required parent additions | Theme handoff lines 54–60 record composition; changeset lines 36–38 record focused proof | Whole application and console qualification |

These are documentary dispositions. A short snapshot-scoped current disposition above retained history would reduce ambiguity; this brief does not edit the source ledgers or independently certify their results.

### 6. The feature records expose a gap between a completed component and a usable workflow

The strongest present example is theme import. [THEME_IMPORT_STORAGE.md](docs/features/THEME_IMPORT_STORAGE.md), lines 41–50, explicitly says that PS5 capacity is unknown and import/recovery writes must remain closed. Its focused PASS includes verifying that denial. A targeted read of [headless/theme_storage.h](headless/theme_storage.h), lines 23–27, confirms that `AvailableBytes` returns false under `__PROSPERO__`. The actual service in [theme_storage_services.inc](headless/prosperoeden/theme_storage_services.inc), line 27, refuses the reservation when that call fails.

**Finding:** this PS5 import/recovery path is deliberately unavailable in the inspected source. More successful host parser, rendering, or reservation tests cannot make it usable. This does not disable every built-in theme or every scene operation. The safety refusal should remain until its prerequisite is resolved; removing the check would conceal the missing capability.

The component preserves existing packs in its tested failure paths, but the documents do not demonstrate the user-facing PS5 behavior when an interrupted import requires recovery. Source-enforced refusal, reported host proof of refusal, and an observed console incident are different evidence. No console attempt, actual pending journal, startup failure or data loss is established. The callback returns an empty token for several conditions; reason visibility elsewhere and the exact message users see were not inspected, so no application-wide diagnostic defect is asserted. Qualifying capacity alone would not establish every remaining filesystem requirement.

Nextendo has a similar delivery boundary. [NEXTENDO.md](docs/features/NEXTENDO.md), lines 33–64 and 287–305, records requirements for PS5 browser operation, game authentication, routes and trust. [NEXTENDO_OAUTH_NEX_CONTRACT.md](docs/features/NEXTENDO_OAUTH_NEX_CONTRACT.md), lines 239–286, closes an offline component delivery while explicitly leaving full application integration and live gameplay outside that closure. Its 111-case proof has value, but it does not establish that the expected online workflow can complete on PS5.

Nextendo has several separate acceptance boundaries: **offline component contract → composed consent/account workflow → registered PS5 linking → game/server behavior**. NEX here is the game-session credential path, distinct from web identity and the game's BAAS authentication token. The contract closes its offline proof and focused SDK component; the changeset records request-bound consent in the combined code. An older handoff saying its frontend is unchanged cannot prove the current frontend is unwired. Registration, same-device browser operation, a suitable BAAS contract, verified routes/trust and live game evidence remain independent prerequisites in the feature record. Existing local rooms are a different workflow, not evidence of Nextendo online play.

**Inference:** research/component acceptance can be legitimate while its product workflow remains unavailable or unqualified. Missing external capability or authority cannot be supplied by faster coordination alone. The retrospective question is when those workflows became required release dependencies and whether the bounded component milestones matched that requirement; this review does not settle that chronology.

### 7. Privacy and account-preservation errors were real, with bounded reproductions

[USB_NETWORK_PRIVACY.md](docs/features/USB_NETWORK_PRIVACY.md), lines 44–83, records a completed pre-fix backup containing six staged files with synthetic password bodies. The fix excluded staged siblings and aliases from inventory, copying and restore. Lines 126–130 record an independent integrated repeat. This is a demonstrated defect in an older build, not evidence that real user credentials were exported.

The same document, lines 38–40, states that affected older completed backups are intentionally refused on restore. The fix does not scrub those backups or revoke previously exported credentials. Preserving live data is distinct from accepting every historical backup. No real-user affected archive or user-facing remediation workflow is established here.

[EXPORT_PRIVACY.md](docs/features/EXPORT_PRIVACY.md), lines 107–130, independently records baseline synthetic leaks in actual release and launcher ZIP fixtures. Its replacement tests exercised the collector CLIs and checked archive membership and attempted private-node access, extending earlier source-filter checks. The USB and package collectors had different scopes and required their own integration.

[NEXTENDO_OAUTH_SECURITY.md](docs/features/NEXTENDO_OAUTH_SECURITY.md), lines 28–66, records two further repaired defects: header allocation failure could discard authorization headers and lead a profile-check response to delete a valid saved account; duplicate JSON fields could ambiguously select identity or consent data. The report reproduces both before repair. It does not establish account takeover.

**Inference:** protecting secrets and preserving prior user state needed behavioral acceptance across every data-export and failure path. Checking a filename rule or having a private file mode was insufficient. These repairs are substantive safeguards, so their existence does not justify describing the review effort as pointless overhead.

### 8. Host language support concealed a native incompatibility

[NEXTENDO_CORE_QUALIFICATION.md](docs/features/NEXTENDO_CORE_QUALIFICATION.md), lines 201–224, records that the SDK rejected `std::atomic<std::shared_ptr<const Session>>`. The repair used atomic shared-pointer load/store functions while retaining immutable ownership. This was an actual target compilation defect; it had not been answered by host behavior tests.

The later genuine-header/partial-link success in that same document, lines 3–77, supersedes earlier missing-header blocks. Its remaining 343 imports describe an intentionally incomplete relocatable subset, not 343 newly proven production defects. The lesson is to check the real target contract early, without misreporting the limits of a partial link.

### 9. Final artifact qualification is a separate boundary

[RELEASE_NATIVE_SOURCE_DELTAS.md](docs/features/RELEASE_NATIVE_SOURCE_DELTAS.md), lines 6–17, 35–42 and 293–308, distinguishes retained inputs and static mappings from actual used-source closure, final binary correspondence and modified-library relink. A source-only restoration can pass while those final distribution qualifications remain pending. Generated and transient inputs must be captured with their actual producer/consumer identities; an older input bundle cannot prove what a later binary used. Missing evidence in the delivered set does not prove source files are absent from another machine. This boundary is independent of provider access and does not invalidate the completed source inventory or bounded native results.

### Feature-folder evidence map

The preliminary investigation inventoried all 15 Markdown files and the historical patch. This review reread selected sources, as recorded in its report; it did not repeat that complete investigation. The map below retains the original source coverage rather than claiming independent replay. Read each entry with these documentary status categories: **historical and repaired**, **component complete**, **integration recorded/final aggregate pending**, or **external/platform prerequisite**. More than one category may apply to a document. Original statuses belong to their snapshots; later composition may supersede a handoff.

| Document | What its reported work establishes | Boundary relevant to this investigation |
| --- | --- | --- |
| [ADMISSION_FIXTURES](docs/features/ADMISSION_FIXTURES.md) | Composed fixture members, real guards/credits and explicit optional overlays | A private predicate overlay is not production; final acceptance needs actual combined guards and callers. |
| [EXPORT_PRIVACY](docs/features/EXPORT_PRIVACY.md) | Actual synthetic CLI/ZIP leak controls and corrected private-path pruning | Separate from USB policy and optional ELF/symbol packaging. |
| [NATIVE_LICENSE_PACKAGING](docs/features/NATIVE_LICENSE_PACKAGING.md) | Notice staging and verification in a synthetic app-folder workflow | Does not build or execute the native app, or close final source/relink delivery. |
| [NEXTENDO](docs/features/NEXTENDO.md) | Account/routing implementation and explicit dependency inventory | Browser, game-authentication and deployment/trust prerequisites remain independent. |
| [NEXTENDO_CORE_QUALIFICATION](docs/features/NEXTENDO_CORE_QUALIFICATION.md) | Genuine-header object compilation and bounded partial linking; native shared-pointer fix | No complete executable or console operation follows from these checks. |
| [NEXTENDO_OAUTH_NEX_CONTRACT](docs/features/NEXTENDO_OAUTH_NEX_CONTRACT.md) | Repaired web/identity/game-token flow with offline proof and caller contract | Offline delivery closure is distinct from UI integration and live PS5 gameplay. |
| [NEXTENDO_OAUTH_SECURITY](docs/features/NEXTENDO_OAUTH_SECURITY.md) | Reproduced/repaired session-preservation and duplicate-field defects | Security of the tested old response shape did not establish the later public response contract. |
| [NEXTENDO_PUBLIC_OAUTH_CONTRACT_REVIEW](docs/features/NEXTENDO_PUBLIC_OAUTH_CONTRACT_REVIEW.md) | Historical mismatch with captured documentation and a reproducible fragment failure | Later repaired; not a current live-provider failure observation. |
| [NEXTENDO_STORAGE_GATES](docs/features/NEXTENDO_STORAGE_GATES.md) | Profile/root snapshots, busy lifetime and USB/launch coordination at its base | Predates network/theme composition; cancellation alone cannot establish quiescence. |
| [RELEASE_LICENSE_NOTICES](docs/features/RELEASE_LICENSE_NOTICES.md) | Retained notice/source attribution inputs | Repository files alone did not put notices into all distribution outputs. |
| [RELEASE_LICENSE_PACKAGING](docs/features/RELEASE_LICENSE_PACKAGING.md) | Actual ZIP collector behavior with synthetic application bytes | The ZIP-root fix left an app-folder placement gap, addressed by the later native-staging work. |
| [RELEASE_NATIVE_SOURCE_DELTAS](docs/features/RELEASE_NATIVE_SOURCE_DELTAS.md) | Static input/transformation/output inventory and missing-evidence ledger | Final used-source mapping, binary match and modified-library relink remain unestablished by this inventory. |
| [THEME_IMPORT_STORAGE](docs/features/THEME_IMPORT_STORAGE.md) | App-owned capacity/credit callback and failure-safe behavior | Current PS5 unknown-capacity branch denies import/recovery. |
| [USB_NETWORK_PRIVACY](docs/features/USB_NETWORK_PRIVACY.md) | Actual synthetic completed-backup leak, then repaired exclusion/restore behavior | Historical repaired defect; independently scoped from public package collectors. |
| [USB_WRITER_ADMISSION_AUDIT](docs/features/USB_WRITER_ADMISSION_AUDIT.md) | Reproduced/repaired admission, worker-publication and remote-drain races | Later network/theme APIs require reciprocal composition beyond that audit's base. |
| [THEME_STORAGE_PARENT_GUARDS.patch](docs/features/THEME_STORAGE_PARENT_GUARDS.patch) | Historical exact integration proposal for shared entrypoints | A patch handoff is not an applied change; the later ledger records composition. |

### Refined hypothesis and competing explanations

**Primary documentary evidence:** a complete-scope release was recorded; bounded component work and substantive repairs were delivered; shared-state composition expanded; some PS5/provider capabilities remained unavailable or unqualified; and no final qualified combined release was recorded. **Inference:** the release depended on three interacting boundaries—target/provider capability, shared operation lifetimes, and final artifact qualification. Component closure could not close all three. The evidence does not rank their causal contribution or establish that development outpaced integration capacity.

Three competing explanations still need challenge:

* **Missing access/prerequisites dominated.** If the expected outcome was usable online play or native theme import, obtaining target/provider feasibility evidence may have mattered more than team size or testing order.
* **Research milestones were legitimate within a broader release.** Offline component closure can be valid under its stated scope. The user's subsequent clarification establishes that those milestones do not substitute for the complete application expected here; it does not invalidate their technical results.
* **Tests primarily prevented a bad release.** The absence of publication may reflect successful refusal gates. That protection can coexist with the clarified full delivery remaining unfulfilled; the count of failed checks alone does not identify an avoidable cause.

### What the evidence does not establish

The documents do not support saying that all work failed, that agents fabricated passing results, or that the latest build caused the supplied Galaxy crashes. [CRASH_VALIDATION.md](docs/CRASH_VALIDATION.md), lines 8–16, attributes those logs to an older 17 October diagnostic executable whose identity differs from the subsequent integration.

The documents explicitly distinguish host tests, native builds and console qualification. The established gap is the unqualified combined result alongside accepted increments. Against the now-clarified full-application outcome, those increments do not establish the requested delivery. This does not establish that every feature fails at runtime, that any missed deadline existed, or which historical decision was wrong. Successful refusal gates and necessary defect discovery remain competing explanations for non-publication. No elapsed work, resource cost, queue duration or token use was measured; repeated work does not establish economic waste.

### Full-scope delivery matrix — added after user clarification

This matrix translates the user's complete-delivery expectation into the existing documented acceptance targets. It is a checklist for the missing result, not a new test run or a claim that every row currently fails. The checkout and source records are unchanged. Focused historical passes remain scoped to their recorded inputs.

| Scope | Required user-visible result or delivery proof | What remains unestablished in the read records | Basis |
| --- | --- | --- | --- |
| USB backup/restore | Settings workflow, progress/cancel, complete protected backup, confirmed restore and cold recovery preserving existing data | Final combined build and PS5 filesystem/removal/recovery behavior; compatibility handling for affected older backups | [Team targets](docs/IMPLEMENTATION_TEAM.md); [USB workflow](docs/USB_BACKUP.md); [privacy correction](docs/features/USB_NETWORK_PRIVACY.md) |
| SMB/NFS | Configure read-only network game access and play through real random reads, with clear disconnect/reconnect behavior | Final native application, actual PS5 game access and real storage/server interoperability; local fixture results do not replace these | [Network shares](docs/NETWORK_SHARES.md) |
| Nextendo | Complete the agreed opt-in account/service and guest-network workflow through its real target dependencies | Registration/browser operation, game-authentication contract, routes/trust and title-specific live evidence; default-off offline plumbing alone does not demonstrate the complete workflow | [Nextendo scope and blockers](docs/features/NEXTENDO.md) |
| Scene themes/Themezer | External scene packages, navigation/motion/sound, safe import and supported conversion behavior | PS5 import/recovery is source-denied while capacity is unknown; full NXTheme parity and issues #173/#239 remain open in the changeset | [Theme storage](docs/features/THEME_IMPORT_STORAGE.md); [scene implementation](docs/SCENE_THEMES.md); [changeset](docs/CHANGESET_1006000.md) |
| Cover Flow | Selectable, responsive library view with bounded resources and existing input/data behavior | Final combined PS5 rendering/runtime validation; host captures do not establish target behavior | [Cover Flow](docs/COVER_FLOW.md) |
| Amiibo/NFC | Explicit catalog/photo sync with local preservation/offline fallback, and requested NFC operation in the application | Composed native/PS5 network, storage, NFC and lifecycle evidence | [Launcher sync](docs/AMIIBO_LAUNCHER_SYNC.md); [changeset](docs/CHANGESET_1006000.md) |
| Physical Pro Controller | Actual supported wired controller input, connection/recovery and appropriate feedback | Physical device, native sandbox/USB access, rest-mode and packaged-app execution; Bluetooth is not implemented | [Controller scope](docs/PRO_CONTROLLER.md) |
| Cheats | Requested session/build-bound live behavior and explicit default-off database installation | Final combined package and console gameplay/lifetime qualification; focused cross-layer proof is not full application proof | [Cross-layer qualification](docs/CHEAT_CROSS_LAYER_QUALIFICATION.md); [changeset](docs/CHANGESET_1006000.md) |
| Integrated upstream changes and base behavior | Preserve launch/return, saves, input, audio/video and qualify the specific claimed fixes | Exact-build Galaxy sequence, #284 comparison and other feature-specific physical retests; old smoke reports do not qualify new edits | [Crash retests](docs/CRASH_VALIDATION.md); [integration record](docs/UPSTREAM_INTEGRATION.md) |
| Shared application | All requested features coexist with correct cancellation, final writes, profile/root changes and launch/teardown behavior | Complete host, all-feature UI and native qualification on one final frozen snapshot | [Shared gates](docs/IMPLEMENTATION_TEAM.md); [changeset acceptance](docs/CHANGESET_1006000.md) |
| Final artifacts | Matching package, executable, symbols, source inputs and reproducible evidence of the actual binary inputs/relink | Final binary-bound source correspondence and actual modified-library relink, alongside final package/identity checks | [Native evidence ledger](docs/features/RELEASE_NATIVE_SOURCE_DELTAS.md); [source/relink process](docs/RELEASE_SOURCE_RELINK.md) |

The strongest supported delivery diagnosis is now specific: **the complete application requested by the user remains unestablished, while a substantial part of the accepted evidence concerns components or earlier snapshots.** That is a delivery gap against the clarified outcome. The underlying causes still have to be assessed separately: demonstrated implementation defects, unresolved target/provider capabilities, fixture/environment integration failures, and final qualification work. Their existence does not prove that a single scheduling or architecture decision caused all of them.

A candidate sequence for closing this gap keeps the full scope:

1. Reconcile the current disposition of each row against the actual checkout; preserve old results as historical evidence instead of rerunning known stale failures.
2. Resolve target/provider capability questions that can prevent a workflow from succeeding at all, particularly theme storage and Nextendo. Each needs a concrete supported path or a precisely identified missing input; additional component tests cannot manufacture the capability.
3. Complete the shared caller/lifetime integration and its representative fixtures while preserving the accepted privacy, cancellation and data-preservation repairs.
4. Freeze a candidate and run the required complete host/UI/native checks on it, collecting actual source/tool/object identities during production. Any repair requires an updated snapshot and affected qualification.
5. Bind the final package, symbols and source/relink evidence, then perform the recorded physical PS5 workflow and regression checks against that exact candidate. Early targeted console checks can inform step 2; they do not replace final-candidate qualification.

This is a proposed dependency order, not an authorization to use accounts, deploy to a console, downgrade data, publish, or remove features. It has no estimated duration because none is supported by the inspected evidence.

## Goals vs. Non-Goals

* **In scope for this clarification:** reconstruct documented defects, repairs and evidence boundaries; challenge causal explanations; distinguish remaining prerequisites from stale summaries; map the complete-delivery expectation to existing acceptance targets.
* **Not established by this brief:** a complete code audit, current PS5 behavior, a definitive root cause for every failure, a budget estimate, or a final recovery plan.
* **Not part of this step:** implementation changes, release builds, deployment, publication, or removing existing work.

## Alternatives Weighed

The user's post-review clarification keeps the complete requested delivery as the target. The alternatives below retain the review's comparison, not three equally selected scope choices:

| Alternative | What it might resolve | What it retains or leaves open | Strongest counterargument and recovery limit |
| --- | --- | --- | --- |
| Complete the existing candidate together | Preserves the recorded complete scope | All three boundaries: capability prerequisites, shared lifetimes and final artifact qualification | Scope may keep independent prerequisites on one path; no evidence establishes that this path costs more than extraction. |
| Compare a narrower candidate, possibly using the earlier build-verified checkpoint | May defer selected workflow prerequisites | Applicable safety fixes, shared guards, package/source identity and console checks remain | Requires revisiting the recorded complete-scope commitment. An earlier source baseline is not a proven operational rollback. Persistent-state compatibility, pending restore/theme recovery and owner quiescence are unassessed; no later candidate's actual use of user data was established. |
| First investigate feasibility of blocked workflows | May distinguish unavailable platform/provider inputs from implementable integration work | Does not close backup privacy/recovery acceptance, final artifact qualification or unrelated features | Investigation itself delivers no application workflow. The outcome depends on access and contracts; no shortcut through guessed tokens, substitute local rooms or removed guards is established. |

The comparison must identify which prerequisites disappear, which shared obligations survive and which delivered capabilities are deferred. Removing a visible feature need not remove persisted state or late side effects. The Nextendo storage-gate handoff reports unchanged account format and signatures at its base; this review establishes neither a whole-candidate schema break nor universal downgrade compatibility. A narrower candidate must preserve applicable safeguards and account for existing transactions. Costs and feedback-cycle savings cannot yet be ranked.

The complete requested result is the user-stated target. No narrower substitute, operational rollback, particular implementation sequence or feature deferral has been approved.

## Key Decisions & Staging for Next Steps

The user authorized the retrospective review, and all six requested Goldfish reviewers completed. They ran in three independent parallel pairs because two reviewer slots were available. Fresh conversation context was requested for each; host-injected instruction isolation and reviewer tool traces were not independently verified. Read coverage is reviewer-reported. No source implementation, build, deployment or publication was performed by this review.

The surviving hypothesis is bounded: **documented component deliveries did not establish the combined release because target/provider capability, shared operation lifetimes and final artifact qualification had separate unresolved requirements.** The user's post-review clarification establishes that the complete delivery, rather than component progress, is the expected result. This does not establish false completion claims, a dominant planning failure or universal runtime failure.

The outcome is now anchored: the complete requested application working together on PS5. Relative priorities, timeframe and any future deferral remain undecided. Remaining factual unknowns include the timing of prerequisite discovery, the critical dependency that prevented the next deliverable, final-snapshot qualification, target recovery symptoms and persistent-state compatibility. The matrix and proposed dependency order above were added after the six reviews and must not be presented as independently reviewed or as authorization to reduce scope, revert binaries or publish.
