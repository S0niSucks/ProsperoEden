# EGM design review

- **reviewer:** egm-design
- **round:** 01
- **status:** completed
- **Mode:** REVIEW
- **Outcome:** The current draft does **not yet meet** the Feature Specification Standard.
- **Target:** `C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/encore-feature-ports-20261030/design_draft.md`
- **Reviewed range:** 1–402, complete file
- **Independently computed SHA-256:** `7cfca41de726b99b4925cba4da9ae119d60b7f3a7ab2486141048b92ce6d0f21`
- **Identity check:** Matches the supplied hash; unchanged on final recheck.

This evaluates feature design, not implementation readiness. Missing Detailed Implementation, unverified native/core contracts and proposed filenames are not independently reported as defects.

## Required-read receipt

Path prefixes:

- **B:** `C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/encore-feature-ports-20261030`
- **E:** `D:/leonardommello/PS5/ProsperoEden-Encore`
- **S:** `D:/leonardommello/PS5/ProsperoEden`

All ranges below were fully delivered.

| Required input | Delivered lines |
|---|---:|
| B/egm-runtime/agents/design_critic.md | 1–63 |
| B/egm-runtime/resources/document_standards/feature-spec-standard.md | 1–36 |
| B/design_draft.md | 1–402 |
| B/feature-evidence.md | 1–91 |
| B/decisions.md | 1–55 |
| B/workflow.md | 1–115 |
| B/sources/game-integration.md | 1–197 |
| B/sources/storage-network.md | 1–169 |
| B/sources/input-presentation.md | 1–141 |
| E/docs/PROSPEROEDEN_PORTS_2026-10-30.md | 1–93 |
| E/EGM.md | 1–124 |

**Inaccessible required inputs:** None.

### Additional inspection and limits

Applicable `D:/leonardommello/PS5/AGENTS.md` was read completely, lines 1–9, before application-source inspection. No additional AGENTS files were found in the inspected ancestor paths.

Referenced material independently followed:

| File | Delivered lines |
|---|---:|
| B/approved-overview-01.md | 1–138 |
| B/approved-technical-plan-01.md | 1–401 |
| E/README.md | 1–289 |
| E/CONTRIBUTING.md | 1–85 |
| E/SECURITY.md | 1–168 |
| E/docs/LIBRARY_INTERCHANGE.md | 1–43 |
| E/headless/EGM.md | 1–40 |
| E/headless/prosperoeden/EGM.md | 1–32 |
| E/headless/prosperoeden/pe/EGM.md | 1–29 |
| E/headless/prosperoeden/pe/ui/EGM.md | 1–32 |
| E/headless/CMakeLists.txt | 694–709; 1411–1478 only |
| S/README.md | 1–232 |
| S/docs/NETWORK_SHARES.md | 1–313 |
| S/docs/CHEAT_DATABASE.md | 1–287 |
| S/docs/MODS.md | 1–34 |
| S/docs/features/NEXTENDO.md | 1–373 |
| S/docs/FORWARDER.md | 1–69 |
| S/docs/USB_BACKUP.md | 1–315 |
| S/headless/network/transport.cpp | 1–381 |
| S/headless/live_cheats.h | 1–61 |
| S/headless/nextendo.cmake | 1–98 |
| S/headless/nextendo.cpp | 231–255; 400–438 only |
| S/headless/usb_backup.cpp | 102–129; 467–559 only |

Follow-up inspection was focused, not an exhaustive source/security audit. Excluded dependencies and core implementations were not followed. Historical test results remain attributed to their source trees.

Serena’s initial instructions were read; no project was activated or configured. No memory, prior reviewer reports, credentials, private configuration, excluded internals or feature services were consulted. No files were changed and no builds/tests were executed.

## Findings

### egm-design-01-1 — Mandatory alternatives evaluation is absent

- **Original severity:** Major
- **Exact target lines:** 391–392; related constraint at 114–116.
- **Discrepancy:** The draft explicitly defers Alternatives Considered. `decisions.md` provides a Nextendo policy comparison, but the target lacks an evaluation of alternatives for its consequential architectural choices.
- **Rationale:** Standard lines 13–18 make this mandatory. Approval of integration boundaries and acknowledgment of unresolved choices do not establish why competing viable designs should be rejected.
- **Actionable recommendation:** Add the alternatives evaluation using actual decisions and evidence. Compare meaningful options, document their costs and failure conditions, and distinguish rejected alternatives from choices still awaiting the owner. Do not invent rejection history.

### egm-design-01-2 — Cross-feature admission semantics remain instructions to a future designer

- **Original severity:** Major
- **Exact target lines:** 188–192; 357–362; 379–384.
- **Discrepancy:** The draft says to define admission, conflicts, ownership and shutdown, but does not establish their feature-level interaction rules. The missing decision is broader than choosing a central coordinator versus existing owners.
- **Rationale:** Generation checks reject stale results but do not prevent two conflicting operations from both being admitted. The design cannot yet determine whether launch, root/profile changes, account work, network reconnection, dialogs and USB operations reject, wait for or cancel one another.
- **Actionable recommendation:** Define a behavior-level conflict/admission matrix, including reservation before dispatch, cancellation versus completed draining, and pending-restore restrictions. This can remain independent of class names, APIs and implementation files.

### egm-design-01-3 — Nextendo’s effective networking exception is undefined

- **Original severity:** Major
- **Exact target lines:** 263–269; 286–291.
- **Discrepancy:** Exact routes and rejection of unmapped official-service names are required, but the draft does not state what happens to other hostnames, hard-coded IP destinations, alternate resolvers or peer-to-peer traffic during the exception.
- **Rationale:** Hostname redirection is not an egress boundary. The inspected source returns unmapped nonofficial names unchanged (`S/headless/nextendo.cpp:248–254`); its documentation explicitly excludes firewall protection (`S/docs/features/NEXTENDO.md:150–159`). Importing that behavior could broaden Encore’s offline-policy exception beyond what users understand as Nextendo routing.
- **Actionable recommendation:** Explicitly choose and document the permitted guest-network scope, including direct-IP and peer-to-peer behavior. Explain any broader exposure in consent. Keep enforcement blocked until permitted owner/interface evidence establishes that exact policy; DNS matching alone must not qualify it.

### egm-design-01-4 — Healthy-session remote content mutation has no stated mitigation

- **Original severity:** Major
- **Exact target lines:** 227–245.
- **Discrepancy:** The network design addresses failed sessions and reconnect isolation, but not game/add-on files changing while the original session remains healthy.
- **Rationale:** Read-only client access does not make server content immutable. The source checks identity when opening/reopening handles, not continuously for already-held handles (`S/headless/network/transport.cpp:185–223,324–376`). Cached old blocks and newly read modified blocks can therefore belong to different content versions without a transport failure.
- **Actionable recommendation:** Define the consistency assumption: for example, trusted, stable server folders that must remain unchanged throughout play, with detected changes terminating the session. Alternatively evaluate a versioned/snapshot-backed approach. State the residual malicious-server limitation and the user-visible failure/restart outcome.

### egm-design-01-5 — Restore validation does not establish a backup trust model

- **Original severity:** Major
- **Exact target lines:** 334–338; 349–355.
- **Discrepancy:** Inspection, content-bound confirmation and independent validation are proposed without distinguishing corruption detection from trustworthy provenance.
- **Rationale:** An altered backup can contain internally consistent hashes. The supplied evidence explicitly warns that hashes do not establish authorship (`B/feature-evidence.md:76–78`; `S/docs/USB_BACKUP.md:73–77`). Confirmation of inspected bytes does not establish that settings, mods or other restored payloads are trustworthy.
- **Actionable recommendation:** State whether restore supports only trusted, user-owned backups or another explicitly evaluated provenance model. Separate integrity, path/scope validation and trust. Define mandatory excluded/reinjected-secret handling and explain that successful validation is not certification of benign contents. This does not require adding a signing dependency.

### egm-design-01-6 — External-writer risk is acknowledged without an operating policy

- **Original severity:** Major
- **Exact target lines:** 344–365.
- **Discrepancy:** The draft correctly says app gates cannot control FTP writers, but does not define the operational precondition or user guidance that mitigates concurrent external modification during backup, staging and next-start application.
- **Rationale:** Draining application writers cannot establish a coherent source or stable restore targets while another privileged process modifies them. Independent copy validation does not resolve that consistency problem. The source explicitly requires users not to write app data over FTP (`S/docs/USB_BACKUP.md:73–77`).
- **Actionable recommendation:** Declare concurrent external mutation unsupported and require an explicit quiescence warning/precondition through the relevant transaction interval, including close/reopen. Define failure on detected source/target changes and preserve recovery evidence. Do not imply that the app can detect or exclude every external writer.

### egm-design-01-7 — Live-cheat disablement lacks an explicit no-rollback limit

- **Original severity:** Nit
- **Exact target lines:** 203–211.
- **Discrepancy:** Persistent selection, session-only changes and installation are separated, but the feature contract does not explain what turning a live code off can and cannot undo.
- **Rationale:** Stopping future cheat execution need not reverse previous memory changes or saved effects. Effective-build matching also does not prove compatibility with arbitrary code patches. These limits are already identified in `B/sources/game-integration.md:92,102` and `S/docs/CHEAT_DATABASE.md:59–64,123–128`.
- **Actionable recommendation:** Explicitly state that disabling a live code is not rollback, identify relaunch as a possible recovery requirement, and avoid presenting a matching catalog/build as proof of cheat safety. Preserve this limitation in the eventual user-facing toggle/confirmation language.
