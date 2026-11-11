# Encore roadmap — execution state

This is an execution record, not a release or full-completion claim. The owner
delegated decisions and implementation; no console/deploy/commit was performed.

Current DE-05-01 disposition: A17 independently closed the exact Entries
stream-lifetime/output mechanism in **source + host**, bound to current bytes
and eight fault witnesses per completed run. Earlier cap/nonconvergence records
below remain historical; no round06, native qualification or general lifetime
closure follows. See [A17](../campaign-20261107/agents/A17.md).

Documentation location (2026-11-02): this matrix is now maintained in the
repository. See the [roadmap index](../README.md), [migration](../MIGRATION.md)
and [raw-evidence locations](../EVIDENCE.md). Run/receipt names below still refer
to the external evidence root; build artifacts were not copied into `docs/`.

| Group | Current increment | Remaining boundary |
| --- | --- | --- |
| P01 Amiibo via .bin only | Owner confirmed local `.bin` only, no physical readers/drivers. Maintained controller source and native source selection now confirm a virtual-Amiibo Player1/Handheld path; partial source evidence, not runtime. | File picker/admission and core/session load/remove are not integrated. Backend parser/validation/writeback/threading contracts remain unverified; originals must be preserved. No physical NFC SDK is required. |
| P02 per-code/live/database cheats | r2 mod/master intent integrated; ReadFile ownership/error repair now integrated8db under A17 source+host authorization, new live validation pending. A14 accepted fresh30+5 on exact8db/Library5a/Texturese083 with TEST adapter/API persistence | No per-code/live VM/catalog or full EdenServices/native/natural-IO qualification. Broader JSON-type/parser/timing/teardown/effective-content contracts remain open; limited mod-intent/ReadFile evidence is not P02 completion. |
| P03 SMB | Read-only/session/consistency/credential policy chosen | No share editor/transport/VFS implementation. Pinned native client/core VFS/keyboard contracts missing. |
| P04 NFS | Separate v3/UID-GID risk and stable-share policy | No editor/transport/VFS implementation; no NFSv4/encryption claim. |
| P05 independent games/wallpapers | App settings/providers/root unification, picker and bounded host descriptor/codec path; A05 final ReadImage cancellation check reviewed/differentially tested and integrated | Native descriptor imports/semantics unqualified; default native gate stays OFF, explicit0 remains off. Cancellation is cooperative, not atomic; not complete on PS5. |
| P06 Nextendo | Default-off selected-user verified-destination-only policy; no official-service fallthrough | No account/route/guest-online implementation. Real registration/trust/authentication and allowed socket enforcement remain missing. No service contact. |
| P07 keyboard | Source/callback/native lifetime design; SDK v0.42 archive and `libSceImeDialog.so` located | No guest/native IME implementation. Actual selected import/callback ABI and owned cancel/drain behavior unverified; stub presence is not working IME. |
| P08 wired Pro Controller | Source decoder/mapping/ownership comparison | No new physical USB implementation. Native USB device API/access/player arbitration unqualified. Existing input recovery preserved. |
| P09 USB backup/restore | Explicit trusted-user backup, privacy, writer/external-writer and next-start policy | No backup/stage/apply implementation. Current writer census, native filesystem durability/capacity and recovery integration missing. Never run restore on real user data as a fixture. |
| P10 appearance | Three interfaces, five palettes, Collection grid/list and actual C++ host captures; persistence/provider fix. Nine UI+12 supporting/frontend units compiled with SDK v0.42; A49 additionally compiled SystemFonts/HarfBuzz and reproduced font, with a partial relocatable link. | Full core/provider/native executable link and native fonts/audio/resource/performance/PS5 navigation qualification absent. Runtime packs are not implemented. |
| P11 forwarder | Bounded one-shot parser/admission and main wiring; A11 guards fixed, four candidate host modes/eight SDK extracted expectations; QA-P11-01 guard-selection supplement8+8 executed and accepted by A23 | Full selected-core/main/native compilation, native argv delivery/handoff/normal exit and SDK-dependent development-input ABI remain unverified. Guard projection does not qualify runtime. |
| P12 Switch 2 | Four upstreams pinned/compared; research-only arithmetic CLI in tools/research/nx2;29 original and29 relocated A12+A36 methods passed, including16,384 capability combinations and mandatory documentary-input refusal | No backend imported or implemented; explicitly unsupported. Research tool always returns switch2_supported=false. Arithmetic/provenance-test success is not a platform-feasibility or compatibility result. |
| Firmware cross-cutting | Separate PS5-host and guest axes, truthful guest setup labels and unknown host diagnostics | No all-version support. Historical 13.60 artifact results do not qualify current changes; no new offsets, SDK claim or guest-version range. |

## Evidence

- `research/ui-adaptation.md`: actual UI-kit/native source comparison and limits.
- `research/nx2-comparison.md`: all four upstreams, exact pins and reading limits.
- `research/firmware-compatibility.md`: allowed app/source/corpus evidence and gates.
- `research/native-inputs.md`: verified local SDK/core/build layout, preserved
  failed native audits01/02, passing audit03 (21/21 objects) and v0.42
  filesystem-wrapper disassembly.
- `increment.md`: exact implementation contract/file list, not full-roadmap readiness.
- `reviews/increment-01/`: completed fresh round, 12 original findings (3 Blocker,
  7 Major,2 Nit), consolidated10 actions; author repairs applied.
- `reviews/increment-02-final/`: completed fresh round against279-line/hash
  838e6aa4… target,16 findings (5 Blocker,11 Major), consolidated8 actions.
  No severity downgrades or accepted-risk settlements. Their author repairs passed
  later gates; neither round was clean before repairs. They are historical, not
  evidence of current-byte closure.
- `focused-checks/`: first combined regression run, including the original
  library source-seam failure after intentional GamesDir unification. That fixture
  has been updated to test the new seam while retaining old schema assertions.
  `focused-checks-final/` passed all ten plus Python syntax751 files. After the
  second-round fixes, `focused-checks-round2-repairs/` also passed, including real
  provider enumeration and settings staging/flush/cancellation-cutoff witnesses.
  These are application host checks, not native qualification.
- `screenshots/`, `preview.log`: real C++/OpenGL synthetic host previews, not
  generated illustrations or PS5 measurements. Uses distribution HarfBuzz/stb,
  checked-in assets, and no real system fonts; optional print art is absent.
- `preview-final/`: completed1920×1080 matrix; `preview-round2-repairs/`: pt-BR
  large-text/high-contrast/reduced-motion matrix with visible Collection recovery
  states. `preview-round2-complete/` subsequently passed its then-current mock/
  Diagnostics matrix; none of those historical passes is promoted to current bytes.
- `preservation-audit.json`: baseline-relative census,1496 baseline files unchanged,
  30 explicitly changed,7 added; no deletion or unexpected path, READMEs unchanged,
  116 local Markdown links valid and git diff --check clean at latest audit. Rerun
  after any later source/doc edit; protected external sources are excluded.

## Active continuation

### Latest campaign evidence — 2026-11-07

Final CORE ran once on actual8db/034/27c7; all admitted internal product phases
returned0, including69+5/API68+5/11neg/five couplings/writers40/promoted36+16/
two probes/four witnesses+two gates/ten regressions/cancel-drain/syntax757 errors0.
External metadata collection failed twice, so core_pass remainsfalse. Original
scripts/logs/failed verification stay immutable; no test replay or UI followed.
Real closure reports removal/cleanup0/drainempty/0jobs. Closed CORE1274 and sibling147
indices preserved; coordinator checked1271+147regular files and1267production/248
prepared artifacts, without claiming the deferred synthetic reparse audit passed.

Original A29/A17 supplemental-audit/review dispatches both failed with expired
authentication tokens and delivered no result. This is a genuine continuation
blocker: refresh subagent provider authentication, then reuse the same sessions.
No replacement/retry/alternate route or UI authorization. Resume bindings in
final-combined-admission-01/core-closed-readback-and-auth-block-01.json46209a5e… .

Preparation19da/248 artifacts was read/hash-verified, including196 source bindings,
124 copies and41 textual replacements. A29 confirmed preserved functional logic
and identified G1: prepared launcher does not bind a new terminal/authorization.
Coordinator preserved prepared bytes and executed an explicit metadata-only
prelaunch gate, receipt2186, validating132 bindings/all44 required roles+ancillary,
new terminal299a, authorization16b96 and exact19da. One bounded CORE execution is
now admitted to original A50 on actual8db/034/27c7; result pending. No automatic
UI admission: actual CORE closure/removal/post-removal seal must be reviewed and
bound before separate UI permission. No combined/global/native result inferred.

Final joint UI contract e149 received both A14/A26 favorable reviews; CORE contract
and narrow A29 amendment were read/hash-verified. Coordinator admitted PREPARE_ONLY
to original A50 with124 explicit role/path/hash/size bindings and unique copy
destinations.25 closed indices plus the ancestral map form an exact6083-entry
union without conflicts;6080 regular files checked,3 synthetic aliases require the
existing metadata adapter. No Docker/compiler/checker/test is admitted during
preparation. Generated derivation manifest is an output, then reviewed/sealed
before a separate execution authorization. Actual8db/034/27c7 stays frozen; final
CORE/UI NOT_RUN and all earlier failures remain unchanged.

Integration06 texture is now source-applied under A17's source+hostGL permission:
only3b29 hunks,e083→27c7, exact tested bytes;1266 other paths and16 other texture
inputs unchanged. Current final source is ReadFile8db/Library034/Textures27c7.
No combined live PASS follows. Canonical context2772b5a9… binds the full04→05→06
chain and1267 final source paths explicitly, without basename/hash assumptions.

A29's static CORE contract/checklist/bindings and32 references were verified;
joint UI e149 from A02 is under A14/A26 review. An exact unchanged README outside
the code census remains an explicitly pinned ancillary reference, not a broad
missing-path fallback. New preparation/execution admissions are still pending;
old LIVE01/02 CORE/UI_NOT_RUN failures remain immutable, with no product failure
or global/native/render/glyph/EGM qualification inferred.

Integration05 presence is now applied under A40's exact authorization: only two
2ffc hunks,Library5a→034,1266 other production paths unchanged;store8db/Texturese083
remain. Readback verifies23 presence inputs (target updated, other22 unchanged)
and all17 texture inputs. Combined live validation is pending; A46-05 stays partial.

The new texture run finally completed REAL-GL host differential PASS:88 artifacts,
11 commands/7depfiles, exact baseline10/0/16/16/0 versus candidate0/0/0/16/16.
A17 post-QA is pending,27c7 not integrated. ReadFile live02 failed bootstrap before
any CORE/UI execution because of a HANDOFF/queue-handoff metadata association;
26 artifacts/0containers, not a product failure. A29 reviews only CORE bindings,
A02 prepares the future joint8db+034+27c7 TEST contract; no new run is admitted yet.

The first post-integration live attempt failed A50's metadata preflight: ed9
identified A17 review-final.json, not the selected postreview.json.29 artifacts
verified,0 containers/0 jobs,CORE+UI NOT_RUN, all1267 production hashes preserved.
A new explicit live02 admission corrects only artifact path/copy label and fresh
namespace; hash/decision/assertions/scopes/budgets remain unchanged. No product
failure or inherited PASS follows. Source remains frozen8db/Library5a/Texturese083.

Integration04 is now source-applied after terminal handoffs and A17 authorization:
exactly the three fe802 include/ReadFile/Load hunks, pre7f/post8db, no whole-file
replacement;1266 other production paths unchanged. New A50 core+UI LIVE validation
uses `/repo` actual8db, NOT_RUN at dispatch. Library5a/Textures e083 stay unchanged;
presence034 and texture27c7 remain external. No inherited global/render/native PASS.

The preceding closed queue yielded fresh external UI8db30+5 PASS (361 artifacts),
guard projection8+8 PASS (83), presence function-real differential PASS (61) and
Textures02 orchestration failure before its runner (62,11 NOT_RUN). Indices checked;
A14 accepted the fresh exact8db source/API/host-event scope; A23 satisfied only
QA-P11-01 guard projection. A40 authorized presence's two exact5a→034 hunks,
application deferred during A50's live ReadFile freeze; no UI034/combined acceptance.
A02 prepares only the future pin contract. A29 prepares a separate wrapper fix;
no third texture run is admitted. Old failures and refused attempts remain immutable.

ReadFile composed r4 completed its independent limited HOST gates: typed69+5,
API68+5/11 checker negatives, five read/close/allocation couplings, owned gates
and ten regressions/757 syntax;938 indexed artifacts checked. A17 authorized only
three fe802 hunks with pre7f/post8db and STOP on drift. Application awaits frozen
jobs ending; production remains7f. Nine actual consumers select8db; three identical
TEST binding copies were unused, with no coverage attributed. A14/A26 approved
the new A02 exact8db UI contract and A50 received fresh30+5 after Textures02;
execution is NOT_RUN, not inherited from UI7f. A35's8+8 guard supplement was also
admitted separately after A23 review, without repeating the previous syntax suite.

The new A10 oracle reevaluation matched both20/140 locale inventories and seven
exact checker negatives, verify0/controls0;46 indexed artifacts checked. A22
accepted the supplement and resolved the textual-oracle gap only for historical
integration03 source7f/e083. No global PASS/coverage transfer to8db or27c7 follows.
There was no new build/render; old FAILED_OR_BLOCKED receipts remain immutable.

Textures candidate compiled as one native SDK0.42 object (869 deps/ELF REL/79
undefined imports), not native runtime/GPU/ABI qualification. Its first host
attempt failed only A50's directory-census preflight before any container or
authorized command. New attempt02 is admitted with only that census corrected
for two exact historical metadata files; fixture/source/assertions unchanged.
No production texture or ReadFile hunk has been integrated at this cut.

- Round03 completed against immutable03-final:12 findings,2 Blocker/7 Major/
  3 Nit, consolidated8 actions; author repairs applied, not fresh closure.
- New WriteOwned protects the three mutation paths using exclusive same-directory
  staging, rejects aliases and preserves cancellation at the final decision.
  Metadata-error/decode-entry/provider assertions and runner timeout/start/syntax
  receipts are now exercised. `focused-checks-round3-repairs/` passed all ten plus
  Python syntax751; supplemental runner and --preferences-only witnesses passed.
- Corrected category/Files foregrounds, Switch refusal explanations and all
  P10 action assertions are included in `preview-round3-repaired-recent-flow/`:
  normal1080p complete PASS. Earlier `preview-round3-repairs/` failed because its
  Recent fixture omitted the existing card→Hero confirmation step; retain that
  receipt. The correction preserves the actual two-Cross behavior.
- Round04 completed against04-final (full1–392/hash862e4b10…,180 sources+4 harness):
  1 Blocker/4 Major/2 Nit, all7 PROPOSE FIX author-applied with original severities.
  Findings concern actual high-contrast text, Language selection, reentry before
  drainage, stale exception feedback, unavailable outputs and runner evidence/
  descendant containment. No accepted-risk closure or severity downgrades.
- `preview-round4-ptbr-accessible/` completed71 captures against04-final with no
  source mismatches, binary4d6db2cb… and exit0. Actual selected Files/refusal/
  Diagnostics/partial-grid captures were inspected. It does NOT validate later
  gutter or round04 source repairs, and the raw-ink test missed the transform bug.
- `focused-checks-round4-repairs/` passed all ten plus Python syntax751 after the
  new default/native/mock clearing and deferred-reentry changes. Supplemental
  runner checks passed direct and stdout-holding-descendant timeouts with terminal
  logs/reaping, startup/syntax failures and preservation/refusal of old evidence.
- `preview-round4-repairs-normal/` passed104 captures with actual glyph-colour
  assertions, all Language palette/contrast states and delayed-query same/different
  kind reentry. Inspected Slate Language/OK and Files, current parent navigation and
  retained refusal feedback. Binary93001cc7…; post-render source/harness hashes
  match frozen05 (no automated compilation-start hash receipt is asserted).
- All three round05 reviewers completed against `reviews/increment-05-final/`,
  FULL1–427/hash4dcc8cf9… with180 sources+4 harness: readiness0 findings, fidelity0,
  design1 Major (raw directory stream leaked on allocation exception and partial
  caller outputs could survive). That static finding is valid; no severity waiver.
  `preview-round5-ptbr-accessible/`
  completed104 captures on the same binary, exit0 and no post-render source
  mismatches (before-render check also passed). Inspected translated Diagnostics
  gutter, Slate Language/OK/category/Files, Switch unavailable explanation and
  same-kind browser reentry/current feedback. Host accessibility gate passed;
  this is neither a complete locale audit nor native qualification.
- Five completed rounds were unclean before repairs: clean streak0/converged=false.
  Cap5 reached; delegated outcome is returned to drafting/non-converged, not a
  sixth round or accepted-risk waiver. No parent/full-roadmap convergence exists.
- The final Major is author-repaired post-cap: immediate scoped stream ownership,
  private result vectors and exception-to-failure with empty outputs. The actual
  provider/future fixture injects bad_alloc after acquisition, repeats8 times and
  requires real closed-stream/EBADF checks. `focused-checks-post-cap-stream-repair/`
  passed all ten plus Python syntax751, including all8 fault-injection iterations.
  `preview-post-cap-stream-repair-normal/` passed104 captures; source/harness/target
  post-render checks match the validation package. UI binary is unchanged because
  the real helper resource repair is exercised by focused checks, not mock UI I/O.
  `preview-post-cap-stream-repair-ptbr-accessible/` also passed104 captures on that
  binary, exit0, no current-source mismatches before/after. Selected translated
  Language/OK, browser reentry and Diagnostics gutter PNGs were opened. Current validation-only
  snapshot is `reviews/increment-post-cap-stream-repair-validation/` (FULL1–436,
  hash228c4431…,180 sources+4 harness); it is NOT round06 and has no reviewers.
  Host validation passes do not close the fresh-review requirement.
  Frozen05 remains immutable and its old receipts are not relabelled as post-fix.

## Final bounded-increment state (before native-input continuation)

- At bounded-increment completion no jobs/reviewers remained. All current host
  gates above passed. The following native continuation has its own job state.
- Five-round EGM loop is capped/non-converged; DE-05-01 is author-repaired and
  host-tested but still requires fresh independent closure outside this loop.
  No risk waiver, silent sixth round, native acceptance or release is implied.
- The twelve-group table remains authoritative: other providers/core/native
  integrations are still missing. No native package, console operation, service
  contact, staging, commit, publication or deployment was performed.
- Preservation census:1496 baseline owned files unchanged,30 intentional changes,
  7 additions,0 deletions/unexpected paths; baseline READMEs unchanged,116 valid
  local links and clean git diff --check. This is not a claim that earlier local
  user work equals HEAD or that excluded dependencies were audited.

## Native-input continuation — 2026-10-31

- Owner pointed to `D:/leonardommello/PS5`; repository/core discovery remains
  read-only, no personal keys or console operations. SDK v0.42 is present as a
  verified vendored ZIP; generic SDK-unavailable descriptions are superseded.
- `native-sdk-audit-01/receipt.json` completed with 5/13 native object units
  passing. Eight UI units failed on the audit's missing `GL/glcorearb.h` include
  path. Keep that failed receipt; it is not eight proven application defects.
- `native-filesystem-abi-01/` captured actual v0.42 archive disassembly. `openat`
  and `fstatat` wrappers do not visibly translate carry/errno. Native content I/O
  remains disabled until the actual linked providers and semantics are qualified.
- `native-sdk-audit-02/` completed16/21 objects, including five UI units and all
  supporting audio/core/gfx/native-audio units. SDK2923 files matched the ZIP,
  allowed-input hashes remained unchanged. Five units stopped at a missing
  pinned JSON include path in the audit, not five established source defects.
- SDK symbol census found `fdopendir` in LibcInternal, `openat`/`fstatat` in
  kernel_sys/static libc, and IME lifecycle symbols in ImeDialog. The production
  link uses plain libkernel plus static libc: ABI behavior remains unqualified.
- `native-sdk-audit-03/` completed PASS21/21: nine real UI units, twelve
  audio/core/gfx/native-audio/frontend units, existing pinned JSON v3.12.0 and
  production warning/RTTI/overflow policy. All object hashes were read back and
  matched; no allowed-source mismatch before/after. Receipt3833d71a….
- No jobs/reviewers remain running. The core, main, real EdenServices provider,
  system-font/HarfBuzz and complete native link/package are still unverified.
  Earlier failed audits01/02 remain immutable; no native-I/O gate was enabled.
- No new reviewer, EGM round, native package, staging/commit/deployment or service
  contact. DE-05-01 and the five-round non-converged state are unchanged.
- `native-discovery-receipt-01.json`: all1533 current owned files exactly match
  the preceding preservation audit, no new/changed/removed owned paths, clean
  git diff --check, external runner syntax passed and toolchain/source hashes saved.

## Documentation migration — 2026-11-02

- Migrated33 permanent Markdown documents and added the index, external evidence
  map and migration record under `docs/feature-roadmap/`. Original copies and
  existing repository files are preserved; future documentation updates belong here.
- Code, user data, raw receipts/logs/screenshots/build objects and immutable
  source snapshots were not changed or incorporated. No new build or EGM round.
- The earlier1533-file census describes the pre-migration tree; these new docs
  are intentional additions, not proof of a new native or full-roadmap acceptance.

## Fifty-agent campaign — 2026-11-07

- Owner requested50 agents across12 fronts and specialist roles. All50 actual
  background dispatches returned unique sessionIDs; the
  [dispatch register](../campaign-20261107/DISPATCH.md) records them.
- Candidates are isolated, source integration belongs to the coordinator, and
  native/host builds are centralized in A49/A50. No production source change
  or feature completion is implied by dispatch.
- [Received results](../campaign-20261107/STATUS.md) distinguish analysis,
  blockers, candidate patches, executed tests and accepted integration.
  This is a separate engineering campaign, not an implicit EGM round06.
- A49 delivered actual native object results for font/SystemFonts/HarfBuzz and a
  relocatable link resolving21 hb_ imports (60 symbols still unresolved), plus
  A11 extracted SDK syntax matrix8/8 expectations. Coordinator readback matched
  all three object and final-receipt hashes. Initial depfile-validator failure
  remains preserved; no full-main/core/native executable or runtime pass.
- Network TB-01 remains BLOCKED: recipe intends the true override, but effective
  production selection lacks final core-TU build/dependency provenance. No header
  was changed to fabricate agreement, and no online capability was enabled.
- All50 agents have now returned. A50's first host stage passed10/10 baseline
  regressions, Python syntax751 files, A05 differential cancellation/gates,
  A02 candidate40/40 vs24 baseline security reds+16 controls, A11 four extracted
  host modes and focused2/2, and A12+A36 all29 methods. A02 original aggregate
  exit1 remains preserved for four wrong expected baseline diagnostics, separately
  interpreted; A14-05 remains OPEN. A01 raw-acquisition tests do not validate an
  Amiibo dump or justify integrating a header without a production consumer.
- Coordinator applied only A05/A02-r1/A11 source deltas serially. All three live
  destination hashes equal the tested overlays; A02's CRLF patch was applied as
  line changes preserving LF. Permanent fixtures were added without byte drift;
  research CLI/JSON unchanged, two Python tests relocated with assertions intact.
  A50 was reused for a new bounded Docker combined validation, completed below;
  no51st agent, staging/commit/deploy or console operation. See
  [integration](../campaign-20261107/INTEGRATION.md) for current receipts/hashes.
- Final `A50/integrated-04` passed10/10 regressions, Python syntax755 files,
  A05/gates absent+0, A02 all40 cases plus separate A14-05 OPEN witness, A11
  four extracted modes/boot policy, and all29 relocated research methods/CLI0+1.
  All68 test assertion calls and pins remained intact. Removing documentary
  input caused mandatory exit1/FileNotFoundError, not a skip. Strict preservation
  passed before/after, three live sources+nine promotions matched their hashes,
  550 older artifacts were unchanged and the container was removed.
- Coordinator verified177 indexed final artifacts and12 live pins; result hash
  `8c9abcaf1060b4501f283dfbc20a8478e25c21e03a8df26f5951398f358ee7a8`.
  Earlier integrated-01/03 refused preflights and integrated-02's strict-census
  false remain preserved separately; they were not relabelled as final passes.
  Actual integrated fixtures ran against `/repo` headers/main, not overlays.
- A10 host compilation/link completed but baseline rendering was intentionally
  stopped to prioritize A02 (exit137 retained); candidate/pt-BR/GL/rollback/video
  gates remain NOT_RUN, not evidence of OOM. A46 ownership-only-v1 was approved
  statically for tests by A17 but remains NEXT_WAVE/NOT_RUN and unintegrated;
  comparator optimization stays separate. A01/A08 remain HOLD without consumers.
- A17 Settings::ReadFile Major, A26 internal-type/read/close gaps, TB-01 and
  the capped EGM DE-05-01 independent-closure requirement remain unresolved.

## Continuing cycles — wave02

The owner requested continued team cycles unless blocked; the same50 sessions
are reused, not an EGM round06. [Wave02](../campaign-20261107/WAVE-02.md) records
active development/review/QA boundaries and sequential merge requirements.

- A49 compiled the real A10 candidate settings.cpp unit for the PS5 SDK target,
  with22 current app headers/1,001 inputs and qualifier0. Coordinator verified
  receipt/object hashes and unchanged live baseline. No link/runtime qualification.
- A50 completed A46 ownership-only FD/JPEG differential: eight exact witnesses
  across gates absent/0, baseline10 vs candidate0 with required hits/controls,
  plus live Descriptor availability/import probe. A17 gave limited acceptance.
- A10 completed20 tuples per language en-US/actual pt-BR, quiet/error/rollback/
  look/FSR-AA/video-rescan/GL;10 regressions+755 syntax. A22 accepted only this
  three-line delta. Eight current host PNGs inspected, limits retained; full480
  remains NOT_RUN. Native object is not link/runtime evidence.
- After A49/A50 terminal handoffs, coordinator integrated **only A10+A46** as
  minimal hunks. Destination hashes6a631c2f…/11c59337… equal tested overlays;
  all12 integration01 pins retained. New live combined validation **HOST PASS**:
  14 pins,10 regressions/755 syntax/40 writers/4 dev modes/29 research, A46 four
  exact witnesses and A10 fresh19-TU build/20 tuples per language/GL0/glyph.
- A02-r2 passed68/68 real API cases, five supplements and11 receipt-checker
  negatives. Library/FakeServices TUs and a verbatim bridge compiled; actual UI
  event/full-provider/link/runtime NOT_RUN. A14 accepted only source/API host and
  specified UI-EVT-01; A02 delivered30 real Launcher-to-API events and five
  negative mutants, under A14/A26 review and still NOT_RUN. New
  external r2+A46 composition excludes failed ReadFile and passed its own API/
  ownership/regressions/TU gates. UI-EVT derived TEST compiled but **FAIL** before
  events: GLSL4.60 unavailable in the test context,0/30 and0/5 accepted, verifier
  NOT_RUN. A02 repairs only the test recipe; no full-provider or UI claim.
- A05 ReadFile candidate **FAIL** on synthetic fread-wrapper exception, one red
  out of69; original failure preserved. Five A29 supplements passing do not
  erase it. Old composition8db59af9… blocked/NOT_RUN despite native extracted
  object compiling. A05 delivered a frozen diagnostic matrix; A17 authorized
  collection only; completed67 phases/36 runtimes/10 LLVM modules+2 sidecars.
  Runtime+CFG support H1 in this host experiment, not natural libc exceptions or
  general compiler bugs. A17 confirmed raw evidence and fixed its reviewed proof;
  A05 delivered r3's minimal replay gate; A17 approved the delta/pins and A50
  received14 replay phases after current jobs/cleanup, no repeated matrix.
  Replay v2 remains unauthorized; original FAIL untouched, no assertions waived.
- A46 permanent harness v2 passed36+16 controls,2 proven adoption/reap probes
  including setsid,4 exact candidate FD/JPEG witnesses and2 gates. Final hashes
  after drain. A29 accepted limited promotion of eight exact files.
- Integration03: after terminal handoff, coordinator applied seven clean r2
  hunks under A14's post-QA limited acceptance, plus eight byte-identical harness
  additions. Settings7f9089ac…/providerf2bfa4d6…, A10 unchanged. Fresh live combined
  execution completed: core HOST PASS including promoted harness/27pins/757 syntax,
  but A10/global FAIL/BLOCKED due to the UTF8-corrupted wrapper oracle. Both raw
  C++ locale runs exited0; no retry/waiver or retroactive PASS. A26 delivered an
  oracle-only packet over160 closed artifacts; A22 approved its strict UTF8 and
  seven negatives for A50 execution after current jobs. Execution is NOT_RUN;
  no new C++/render or retroactive PASS is proposed.
- UI GL r2 then passed its real same-binary prerequisite and30 events+5 exact
  mutants with original strict verifier. A14 independently closed A14-05 only in
  source/API/host-event scope with TEST adapter + real persistence, seven pins;
  not full EdenServices/native/natural IO or P02/global completion.
- ReadFile r3 completed14 phases/6 dep bindings,69+5/gates/nm with original
  failures and proof retained; A17 accepted isolated source+host and approved
  the new composition only for tests. A50 received a separate compound payload
  after the texture job/cleanup, NOT_RUN so far. External
  current-r2 + ReadFile preparation equals old8db59 bytes explicitly, not an
  automatically accepted old composition. Production ReadFile is unchanged.
- A08 count-r1 rejected for new maintenance/logout regression A20-L01; r2 and
  A32 QA not delivered because tool calls returned cyber_policy. No disguised
  retries/new agents or USB qualification; A20-03/04 still open.
- Known-blocked contracts/network production provenance remain explicit; no
  generic redispatch fabricates progress on those fronts or silently closes them.

Do not describe the whole requested roadmap as finished. Application progress,
selected native compilation, runtime ABI and attended console outcomes are
different evidence states. The constrained unavailable features are not silently
removed from scope or represented as working by configuration/UI labels.
