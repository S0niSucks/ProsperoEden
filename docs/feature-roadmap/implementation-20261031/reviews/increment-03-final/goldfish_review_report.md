# Goldfish Annotated Review Report — round03

Target: target.md FULL1–347, SHA256
5ef5d73f77d43612523ea4f47486379f83dce0465ea1fea3ecb7b7a9de83bfdb.
Status: author repairs under delegated AUTH-02; not fresh closure/convergence.
All three reviewers completed:12 original findings,2 Blocker/7 Major/3 Nit;
8 consolidated actions (highest severity2 Blocker/5 Major/1 Nit).
No original severity changed. Every action PROPOSE FIX; no contest/escalation/
decline/accepted risk. Round03 was unclean BEFORE repairs; clean streak0.

## A31 — Failure receipts (Blocker action)

- CD-01 **Blocker**, target293–300; IR-04 **Major**, target264–268,297–300.
- harness/run-focused-checks16–31 runs child with timeout120, saves stdout only
  after completion, parses syntax without catching exceptions and writes result
  last. Timeout loses available output; syntax/start error can omit final receipt.
  Ordinary nonzero exits are recorded. Documented complete retention is false.
- Repair: initial not_run receipt, per-test status/log updates, partial timeout
  output, start_failed errors, collected syntax file/errors, nonzero terminal
  failed result. Refuse overwriting old run directories. A supplemental actual
  runner fixture tests timeout/start/syntax and preservation. Applied externally;
  self-check passed, complete suite is being rerun.

## A32 — Firmware fallback parity (Blocker action)

- CD-02 **Blocker**, target168–170,339–342; IR-05 **Nit**, same scope;
  DE-04 **Nit**, target168–170,339–341.
- settings draw_settings293–304 uses only Not observed, unlike draw_dialog855–866
  and Switch category535–539/dialog895–900 with current-build-unqualified suffix.
  Provider diagnostics2096–2117 leaves host_firmware empty, exposing this path.
  The short label is truthful, but contradicts claimed presentation parity.
- Repair: reuse full qualified fallback in Encore/Collection category. Applied.
  No host probe or compatibility claim added; captures need current-byte inspection.

## A33 — Executable P10/decode-entry coverage (Major action)

- IR-02 **Major**, target237–243,248–256,314–320; CD-03 **Major**,
  target236–243,314–320.
- Combined preview145–319 returns before other gallery tests; no integrated
  partial grid, Options, Square, Recent identity/order or scan-count assertions.
  Services arm_safe_launch285–287 is no-op and fake has no observable override.
  Content fixture116–118,147–152 changes dimensions in30-byte TGA and checks false;
  that alone does not prove pre-decode rejection despite correct provider guard
  at2373–2376.
- Repair: observable mock safe-launch/mod/scan counters and ten-item truncation;
  controller paths assert partial-grid indices7/9, exact selected file, one Safe
  Launch request, title-bound mods toggle, first/second provider Recent identity,
  and unchanged scan count on appearance-only edits. Instrument the single
  extracted provider stbi_load_from_memory call with a counting wrapper that
  forwards real decoding; dimension rejection must not increment it. Applied;
  new combined suite/preview pending.

## A34 — Stable phase-local gate (Major action)

- IR-01 **Major**, target234–247,311–320.
- Stage restart80–85 always calls activate_content_snapshot; pictures269–317
  includes P05, harness always selects that mode. Earlier checkpoint2 requires
  it before checkpoint3 supplies content APIs/mocks; no partition is defined.
- Repair: make the coupled P10 UI+P05 provider/owner/mock changes ONE coherent
  checkpoint2. Its combined preview runs only after those APIs exist. Preference
  safety remains checkpoint1 with explicit --preferences-only fixture mode;
  forwarders3 and final documentation/evidence4. No fictional P10-only gate.

## A35 — Metadata-error policy (Major action)

- IR-03 **Major**, target121–126.
- UserContent::Entries96–120 skips fstatat failure at107 and can return partial
  success, contradicting target's failure-clears-both-lists statement. No error
  acceptance test exists in fixture162–173.
- Repair: fail the whole walk on metadata errors and clear both outputs. Add
  injected real-provider fstatat EIO assertion. Malformed/unsupported names/types
  may be filtered; they are not a failed metadata syscall. Applied; test pending.

## A36 — Owned settings staging (Major action)

- DE-01 **Major**, target58–63,144–150,248–256.
- WriteFile197–211 uses predictable .tmp plus fopen(wb). A pre-existing symlink
  or hardlink to settings/another writable file can be truncated before final
  cancellation, without any concurrent writer. Returning false cannot then
  guarantee preservation. Protected content descriptors do not secure staging.
- Repair: new WriteOwned for the three protected preference/content writers,
  same-directory unique-sequence O_EXCL/O_NOFOLLOW candidates,0600 mode,8 retries,
  remove only own created temp; reuse existing LibraryFile open/write/close import
  family rather than add unqualified fdopen/fsync. Cancellation cutoff after raw
  write/close, before rename decision. No power-loss durability claimed. Fixtures
  retain legacy .tmp symlink/hardlink targets and test actual candidate collisions
  without truncating/deleting others' nodes. Other legacy writers not retrofitted.

## A37 — Filled Switch selection contrast (Major action)

- DE-02 **Major**, target36–38,65–68,199–204,235–239.
- theme66–84 gives pale focus colours (Slate#f0f2f8); Switch apply218–225 uses them
  as kFocus, but Settings482–486 and Files554–568 fill selection with kFocus and
  white text. Advanced rows159–170 already separate dark fill/bright outline.
- Repair: dark selected text/icons in both category and legacy Files for new
  palettes/high contrast; retain Original normal behavior. Category defect was
  independently observed in actual screenshots after snapshot03. Add Files rows
  to every palette/accessibility capture and a focused role-pair contrast witness.
  Applied live; latest complete matrix rerender pending.

## A38 — Switch refusal explanations (Nit action)

- DE-03 **Nit**, target47–63,156–157,237–239.
- launcher save260–267 and settings473–478,625–637 set warning/rollback state,
  but Switch dialog804–907 and category472–544 never draw message_. Encore886–891
  does. State preservation is correct but user cannot see why an edit was refused.
- Repair accepted as worthwhile: render bounded shared feedback in Appearance/
  Storage dialogs with readable warning ink, add cases already starting in Switch
  for save refusal and unavailable content. Applied; captures pending.

## Review identities and required-read receipts

- IR ses_e6f4f95e7ffeLB9IsY0GBugT14: completed,4 Major/1 Nit, readiness FAIL.
- DE ses_e6f4f96a5ffe1hb8dYC9fJ1Ol3: completed,2 Major/2 Nit.
- CD ses_e6f4f969affegAp56UfECk6KW7: completed,2 Blocker/1 Major.

All fully read ancestor AGENTS1–9 first; persona (IR1–62,DE1–63,CD1–69), assigned
standard (readiness1–34, feature-spec1–36 for DE), target1–347, manifest1–164.
All153 source and3 harness hashes matched, with independently verified target.
Hashing is not a semantic read receipt. Local AGENTS-directed knowledge skill1–52
was read; no corpus/history/memory/live checkout/vendor access was used.

IR fully read harness runners1–33/1–37 and Dockerfile1–5; relevant stated source
paths cited above. It did not assert full semantic reading of all153 files.
CD inspected bounded settings/root/admission, UI dispatch, lifetimes, feature
docs/mocks/runners; simple unit bodies excluded. Some unchanged dependencies and
extraction inputs are absent; not inferred or obtained elsewhere.
DE full reads: five feature/interchange docs; assets/dev/forwarder/game/appearance/
library/settings/storage/user headers; provider declaration; fake header; services,
launcher declaration, theme/textures; three new fixtures; both runners and recipe.
DE partial: main740–1015,1118–1261,1578–1752,1798–1863,1980–2051,2490–2581;
provider1250–1736,1860–2407; fake330–554; preview1–415; launcher1–590,612–753,
1150–1271; library1–980,1123–1427,1590–1809; settings1–650,790–914;
browse1–531; home162–271,412–446; Switch1–290,465–586,790–931;
widgets1–455/declaration1–145; pt-BR1–120,1757–1865. Other searches discovery-only.

Every reviewer: edits/tests/builds/previews/installs/network/hardware/nested agents
NOT_RUN. Serena no active project; no activation/configuration, fallback files.
Historical author passes and preserved baseline were not independently reproduced.
