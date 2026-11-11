# Goldfish Annotated Review Report

**Target:** `design_draft.md` (full file, lines 1–402)

- Date: 2026-10-31; round: 01.
- Status: awaiting owner resolution; no draft repairs applied.
- Target SHA-256: `7cfca41de726b99b4925cba4da9ae119d60b7f3a7ab2486141048b92ce6d0f21`.
- All three fresh reviewers completed and returned required-read receipts.
- Eleven original findings: seven Major and four Nit; no Blocker.
- Ten consolidated actions: six Major and four Nit. Only the duplicate
  alternatives finding is merged; all original IDs and severities are retained.
- All actions are **PROPOSE FIX**. There are no contests, escalations or
  declined findings. Four actions additionally need owner policy choices.
- Round 01 is **not clean**, judged before repairs. Consecutive clean rounds:
  zero. No convergence is claimed; the loop cap is five rounds.

## Reading and traceability

Original returned reports, including their complete reading receipts and limits:

- [Design](./design.md).
- [Structure](./structural.md).
- [Style](./style.md).
- [Review configuration](../configuration.md).

Each original finding is mapped to an action in the round's
[consolidated report](./consolidated.md).
Quoted discrepancies and rationales below are from those reviewer reports.

## Approval boundary

Approving the proposed repairs authorizes only the stated document edits and
recording of unresolved choices. It does **not** select an admission policy,
guest egress policy, remote consistency model or backup trust model. The
choices in A02–A05 must be decided explicitly, one at a time. Their Major
findings stay open until the selected contracts are written and reviewed.

No automatic second round follows repair approval. After resolution, the owner
must explicitly request another review round. A decision to leave text unchanged
does not silently accept every risk or settle unresolved architecture.

## Prioritized Findings & AI Suggestions

### A01 — Alternatives evaluation [egm-design, egm-structural] — Severity: Major

- **Original IDs:** `egm-design-01-1`, `egm-structural-01-1`.
- **Affected scope:** target lines 391–392; 188–192 and 379–392 for the structural duplicate.
- **Discrepancy description:** “The draft explicitly defers Alternatives Considered. `decisions.md` provides a Nextendo policy comparison, but the target lacks an evaluation of alternatives for its consequential architectural choices.”
- **Rationale:** “Standard lines 13–18 make this mandatory. Approval of integration boundaries and acknowledgment of unresolved choices do not establish why competing viable designs should be rejected.”
- **AI suggestion:** **PROPOSE FIX**.
- **Justification:** This is an actual missing standard requirement, not an unjustified demand for a Detailed Implementation section. Draft-stage disclosure does not satisfy the alternatives rule. The existing decisions are useful evidence, but do not authorize invented rejection history or new architecture selections.
- **Proposed edit:** Replace the deferred-alternatives statement at lines 391–392 with an `## Alternatives Considered` section. Retain a separate statement that Detailed Implementation is not written. Use this content:

  | Question | Options evaluated | Recorded disposition and reason |
  | --- | --- | --- |
  | USB restore direction | Keep startup restore excluded; or permit explicit confirmation, validated staging and next-start application | The owner selected the second direction. The unselected path would keep the requested restore capability blocked. This does not choose backup scope, conflict semantics or transaction primitives. |
  | Nextendo policy direction | Preserve guest-offline-only behavior; or design a default-off, session-bound exception | The owner selected exception design. Offline-only behavior would not permit designing the requested guest-online route. Offline remains the default; provider/core authentication remains unverified. |
  | Cross-feature admission | Reject overlapping requests; cancel and drain before admission; or queue a bounded request | Pending owner choice. Compare the mechanisms, trade-offs and failure conditions in A02. A stale-result check alone does not choose or implement admission. |
  | Guest networking, remote consistency and backup trust | The explicit alternatives in A03–A05 | Pending owner choices. No path is represented as selected, safe or rejected merely to complete this section. |

  Add: “Wholesale source-tree/frontend/core replacement is excluded by the
  preservation constraints, not a newly weighed alternative. Other product
  choices remain open; this table records only actual decisions and evaluated
  pending options. Recorded consequences are not claims about private owner
  reasoning.”
- **Closure limit:** The new section repairs the absence and provides an evaluation structure. It does not by itself close the standard's broader requirement while consequential choices remain unresolved.

### A02 — Operation conflict and admission contract [egm-design] — Severity: Major

- **Original ID:** `egm-design-01-2`.
- **Affected scope:** target lines 188–192, 357–362, 379–384.
- **Discrepancy description:** “The draft says to define admission, conflicts, ownership and shutdown, but does not establish their feature-level interaction rules. The missing decision is broader than choosing a central coordinator versus existing owners.”
- **Rationale:** “Generation checks reject stale results but do not prevent two conflicting operations from both being admitted. The design cannot yet determine whether launch, root/profile changes, account work, network reconnection, dialogs and USB operations reject, wait for or cancel one another.”
- **AI suggestion:** **PROPOSE FIX**; **owner choice required**.
- **Justification:** The accepted plan requires ownership but leaves observable conflicting-action outcomes undecided. Merely adding a coordinator class or another generation number would not resolve this behavior gap.
- **Proposed edit:** After Control and data boundaries, add an `Operation admission decisions` subsection containing the following proposed contract record:

  | Interaction | Established constraint | Missing outcome to choose |
  | --- | --- | --- |
  | Launch versus active work bound to game/profile/root | Reserve admission before dispatch; no stale result may affect the admitted session | Reject the new action, cancel and drain the old action, or queue under an explicit bounded contract |
  | Root/profile change versus network/account/cheat work | Keep the old request identity and resource lifetime valid until its owner completes | Same policy choice; generation invalidation is not resource draining |
  | Network reconfiguration versus retained readers | An old reader must never silently follow a replacement session | Define admission/failure outcome and when the old owner is fully drained |
  | Native dialog versus another dialog or game teardown | One dialog owns native buffers; cancellation does not establish system release | Define who waits/rejects/cancels, without freeing retained native buffers |
  | USB work versus all relevant application writers | Obtain admission and drain the actual writer owners before protected work | Define behavior for new requests during work; do not invent a complete writer census |
  | Published pending restore versus launch or ordinary writers | Preserve the approved close/reopen requirement and do not resume ordinary writers or launch | No queue or cancellation may bypass the pending-restore restriction |

  Evaluate these options without selecting one:

  | Option | Mechanism | Trade-off and failure condition |
  | --- | --- | --- |
  | Reject a conflicting request | The admitted owner continues; UI reports the conflict and the user may retry | Requires clear busy/cancel feedback; repeated user actions do not queue automatically |
  | Cancel and drain, then admit | Request cancellation, wait for actual completion/resource release, revalidate identity and then admit | Noninterruptible I/O or native dialog termination can delay progress; cancellation is not a bounded-time completion promise |
  | Bounded queue | Retain a limited request and revalidate selection, identity and policy when it reaches admission | Adds expiry, fairness and stale-intent handling; pending restore still refuses incompatible work |

  Add: “Policy is not selected. Each interaction needs an explicit outcome
  before implementation readiness. The representation of admission state is a
  separate decision from these user-visible semantics.”
- **Closure limit:** Approval may add this decision record, but the Major stays open until the owner chooses the required behavior and the matrix is completed.

### A03 — Effective Nextendo guest-network scope [egm-design] — Severity: Major

- **Original ID:** `egm-design-01-3`.
- **Affected scope:** target lines 263–269 and 286–291.
- **Discrepancy description:** “Exact routes and rejection of unmapped official-service names are required, but the draft does not state what happens to other hostnames, hard-coded IP destinations, alternate resolvers or peer-to-peer traffic during the exception.”
- **Rationale:** “Hostname redirection is not an egress boundary. The inspected source returns unmapped nonofficial names unchanged (`S/headless/nextendo.cpp:248–254`); its documentation explicitly excludes firewall protection (`S/docs/features/NEXTENDO.md:150–159`). Importing that behavior could broaden Encore’s offline-policy exception beyond what users understand as Nextendo routing.”
- **AI suggestion:** **PROPOSE FIX**; **owner choice required**.
- **Justification:** Permission to design an opt-in exception did not select its packet/destination policy. It cannot be interpreted as permission for all nonofficial destinations, direct IPs or peer traffic.
- **Proposed edit:** Add after the P06 policy paragraph:

  > DNS redirection is not an egress boundary. The source may leave nonofficial
  > hostnames unchanged, and the current plan has not selected behavior for
  > direct IPs, alternate resolvers or peer-to-peer traffic. The exception remains
  > disabled and implementation-blocked until the owner chooses that scope and
  > permitted enforcement-owner evidence establishes it. Consent must describe
  > the selected exposure; matching hostnames alone cannot qualify enforcement.

  Record these pending alternatives:

  | Option | Mechanism | Trade-off and failure condition |
  | --- | --- | --- |
  | Restrict to explicitly admitted service destinations | Define admitted destinations and deny other guest traffic, including unapproved direct-IP/resolver/peer routes | May prevent games needing additional or peer traffic; cannot be claimed enforceable without the actual allowed network-owner contract |
  | Permit an explicitly bounded wider gameplay-network scope | Define which additional game/peer destinations or classes are allowed and explain that exposure in consent | Broadens security and qualification scope; an unbounded “anything nonofficial” rule is not a substitute for the missing contract |

- **Closure limit:** The limitation text can be approved now. Neither alternative is selected by automatic repair; this Major remains open pending policy and evidence.

### A04 — Remote content consistency [egm-design] — Severity: Major

- **Original ID:** `egm-design-01-4`.
- **Affected scope:** target lines 227–245.
- **Discrepancy description:** “The network design addresses failed sessions and reconnect isolation, but not game/add-on files changing while the original session remains healthy.”
- **Rationale:** “Read-only client access does not make server content immutable. The source checks identity when opening/reopening handles, not continuously for already-held handles (`S/headless/network/transport.cpp:185–223,324–376`). Cached old blocks and newly read modified blocks can therefore belong to different content versions without a transport failure.”
- **AI suggestion:** **PROPOSE FIX**; **owner choice required**.
- **Justification:** A healthy transport and an unchanged open handle do not prove a stable game image. No source observation establishes immutable remote snapshots.
- **Proposed edit:** Add after P03/P04's session-failure paragraph:

  > Read-only client access does not make remote content immutable. A healthy
  > session can observe mixed cached and changed server bytes. Remote consistency
  > therefore needs an explicit operating contract, detected-change failure and
  > restart behavior, and a stated malicious-server limitation. The current
  > transport's open-time checks do not establish continuous consistency.

  Evaluate, without selection:

  | Option | Mechanism | Trade-off and failure condition |
  | --- | --- | --- |
  | Trusted stable share | Require server game/add-on content to remain unchanged for the session; detected changes stop the session and require a fresh launch | Depends on operator discipline; checks cannot guarantee detection of every changing or malicious-server response |
  | Versioned/snapshot-backed source | Require a qualified immutable version/snapshot identity for the admitted content and its add-ons | Adds server/version support and qualification requirements; a share path or metadata tuple alone is not an immutable version |

- **Closure limit:** Adding the risk and options is not selection of a consistency contract. This Major remains open until that contract and user-visible outcomes are decided.

### A05 — Backup integrity versus trust [egm-design] — Severity: Major

- **Original ID:** `egm-design-01-5`.
- **Affected scope:** target lines 334–338 and 349–355.
- **Discrepancy description:** “Inspection, content-bound confirmation and independent validation are proposed without distinguishing corruption detection from trustworthy provenance.”
- **Rationale:** “An altered backup can contain internally consistent hashes. The supplied evidence explicitly warns that hashes do not establish authorship (`B/feature-evidence.md:76–78`; `S/docs/USB_BACKUP.md:73–77`). Confirmation of inspected bytes does not establish that settings, mods or other restored payloads are trustworthy.”
- **AI suggestion:** **PROPOSE FIX**; **owner choice required**.
- **Justification:** Integrity, path/scope admission, provenance and benign content are different claims. Confirmation is not a signature or a safety proof.
- **Proposed edit:** Add to P09:

  > Hash validation detects corruption; it does not authenticate the backup
  > author or certify restored content as benign. Path/scope validation remains
  > mandatory independently of trust. Named private account and network-credential
  > state must be excluded before child access and rejected if reinjected by a
  > restore manifest or payload. This named-path policy is not a general secret
  > scanner. Exact backup scope, aliases and key/firmware inclusion remain open.

  Add a pending trust decision:

  | Option | Mechanism | Trade-off and failure condition |
  | --- | --- | --- |
  | Trusted user-owned backups | Accept only backups the user trusts, with explicit warning that integrity checks do not establish authorship | An altered backup may be internally consistent; the app cannot verify the user's trust assertion |
  | Authenticated provenance model | Define a separately reviewed identity/key/signature and verification policy | Introduces key management and compatibility requirements; authenticated authorship still does not certify benign content |

- **Closure limit:** No signing dependency or trusted-only policy is chosen by repair approval. The Major stays open until the supported trust model and admission/consent outcomes are decided.

### A06 — External-writer operating precondition [egm-design] — Severity: Major

- **Original ID:** `egm-design-01-6`.
- **Affected scope:** target lines 344–365.
- **Discrepancy description:** “The draft correctly says app gates cannot control FTP writers, but does not define the operational precondition or user guidance that mitigates concurrent external modification during backup, staging and next-start application.”
- **Rationale:** “Draining application writers cannot establish a coherent source or stable restore targets while another privileged process modifies them. Independent copy validation does not resolve that consistency problem. The source explicitly requires users not to write app data over FTP (`S/docs/USB_BACKUP.md:73–77`).”
- **AI suggestion:** **PROPOSE FIX**.
- **Justification:** This adds the operating constraint implied by the existing inability to exclude external writers. It does not claim the app can acquire system-wide isolation or detect every violation.
- **Proposed edit:** After the writer-census paragraph, insert:

  > Concurrent external modification is unsupported. Before backup enumeration
  > or restore inspection, instruct the user to stop FTP/file-manager and other
  > external writers to the affected data and keep it unchanged until the
  > operation completes. For a staged restore, that requirement spans close/reopen
  > and next-start application. Detected source or target changes fail the
  > operation and retain incomplete/recovery evidence; they do not promise
  > whole-tree rollback. The application cannot detect or exclude every active
  > privileged writer, so this is an operating precondition, not enforced isolation.

- **Closure limit:** Applying this text addresses the missing documented operating policy. Future execution evidence is still required; current round cleanliness does not change retroactively.

### A07 — Live-cheat disablement [egm-design] — Severity: Nit

- **Original ID:** `egm-design-01-7`.
- **Affected scope:** target lines 203–211.
- **Discrepancy description:** “Persistent selection, session-only changes and installation are separated, but the feature contract does not explain what turning a live code off can and cannot undo.”
- **Rationale:** “Stopping future cheat execution need not reverse previous memory changes or saved effects. Effective-build matching also does not prove compatibility with arbitrary code patches. These limits are already identified in `B/sources/game-integration.md:92,102` and `S/docs/CHEAT_DATABASE.md:59–64,123–128`.”
- **AI suggestion:** **PROPOSE FIX**.
- **Justification:** This Nit is worth fixing because it affects the meaning of a user-facing control and prevents a false recovery promise.
- **Proposed edit:** After the P02 paragraph, add:

  > Disabling a live code stops future execution; it does not roll back prior
  > memory writes or saved effects. Relaunch may be needed, and saved changes
  > can persist. A matching game/build identity does not prove code safety or
  > compatibility with other patches. The eventual toggle/help text must state
  > this limitation.

### A08 — Stale drafting phase language [egm-style] — Severity: Nit

- **Original ID:** `egm-style-01-1`.
- **Affected scope:** target lines 43–46 and 91–92.
- **Discrepancy description:** “The Overview presents application-source comparison as the ‘next drafting phase,’ and Goal 7 refers to boundaries being settled ‘during the technical plan.’ Lines 135–138 already report completed comparisons, while lines 386–389 report approval of the Technical Plan’s boundaries.”
- **Rationale:** “These future-tense instructions make the current draft read partly like its earlier approval snapshot. Readers must reconcile outdated process wording with the current status.”
- **AI suggestion:** **PROPOSE FIX**.
- **Justification:** Updating current phase wording is mechanical. Preserved approval snapshots must remain unchanged.
- **Proposed edit:** Replace lines 43–46 with:

  > The scope covers the complete pending inventory, but is not yet a file-level
  > implementation plan. Focused application-source comparisons are complete,
  > and the Technical Plan records the approved integration boundaries. Open
  > design choices and inaccessible core dependencies remain explicit blockers.

  Replace lines 91–92 with:

  > Delivery order and unresolved choices remain open; the Technical Plan records
  > the integration boundaries. Row order does not define delivery order.

### A09 — Separate backup limitations [egm-style] — Severity: Nit

- **Original ID:** `egm-style-01-2`.
- **Affected scope:** target lines 349–352.
- **Discrepancy description:** “‘Does not supply an encrypted exact snapshot or whole-tree atomic rollback’ combines encryption and restoration semantics within one negated description.”
- **Rationale:** “The source evidence establishes two separate facts: backups are unencrypted, and restoration is a merge rather than exact snapshot restoration. The compound wording makes the encryption limitation unnecessarily indirect.”
- **AI suggestion:** **PROPOSE FIX**.
- **Justification:** Both facts are established in the linked source comparison. Separating them does not select Encore's future backup format.
- **Proposed edit:** Replace the source-limit sentence at lines 349–352 with:

  > Do not use per-game save import/export as this transaction. The source backs
  > up a fixed app-data root, creates unencrypted backups and applies a cold-start
  > merge with retained previous content and recovery data. It does not provide
  > exact snapshot restoration or atomic whole-tree rollback.

  Keep the following owner-decision and external-root limitations.

### A10 — Replace undefined metadata shorthand [egm-style] — Severity: Nit

- **Original ID:** `egm-style-01-3`.
- **Affected scope:** target lines 363–365.
- **Discrepancy description:** “‘Safe-stat’ appears without a definition, named helper or explanation of the required safety property.”
- **Rationale:** “Unlike the adjacent operation names, the term does not identify a clear contract. It could mean a particular metadata-query wrapper, link/type validation, or firmware-compatible syscall behavior. Its ‘safe’ qualifier also sounds stronger than the immediately stated lack of native evidence.”
- **AI suggestion:** **PROPOSE FIX**.
- **Justification:** The draft can name the required verification properties without assuming a safe native helper or choosing a new API.
- **Proposed edit:** Replace lines 363–365 with:

  > Native file-metadata queries, link/type checks, no-follow descriptor-relative
  > traversal, available-space reporting (`fstatvfs` in the source), synchronization
  > and rename behavior require separately authorized native evidence. Host
  > fixtures cannot qualify power loss or USB removal.

## Next action

Review this report and choose how to resolve it. `auto accept` applies the
specified wording/evaluation records, but leaves A02–A05's owner choices and
any consequential unresolved alternatives open. `interactive breakdown` walks
through one item at a time. `continue`/`no changes` leaves the draft unchanged.
File comments or targeted custom instructions are also supported. No second
round starts until the owner explicitly requests it after resolution.
