# Goldfish Annotated Review Report — round05 / cap

Target: immutable target.md FULL1–427, SHA256
4dcc8cf9cad9f2f86ecc9bab469b15e10b0239babfaaab056d44e866f4baa866.
Status: **review completed, cap reached, not converged**. All three reviewers
completed. Original findings: **0 Blocker,1 Major,0 Nit**. No severity changed.
One PROPOSE FIX action author-applied; no contest, escalation, decline or waiver.
The round was unclean BEFORE repairs. All five rounds were unclean; clean streak0.
Under delegated cap policy, return to drafting/non-converged rather than silently
start a sixth review or accept the risk. Author repair/testing cannot establish
fresh closure. Current source now differs from this reviewed immutable snapshot.

## DE-05-01 — Major — Exceptional directory-stream lifetime (A51)

- TARGET133–141,147–162.
- Source UserContent::Entries81–120: fdopendir91–94 gives the duplicate descriptor
  to a raw DIR*. String/vector allocations101,108–109 can throw before the sole
  closedir111 and final output clearing. Browse finish_content_work141–169 catches
  the future exception but cannot close that duplicate or free its stream.
- Discrepancy: original descriptor has RAII, directory stream does not. Allocation
  failure can leak stream/duplicate and leave partial caller outputs. Repeated
  failures undermine bounded resource/lifetime behavior. Static exception-safety
  finding, not observed runtime leakage or native qualification evidence.
- PROPOSE FIX: give the stream immediate unique_ptr/scope ownership. Build both
  result lists privately, publish via nonthrowing standard-vector moves only after
  success/sort/final cancellation check, and convert exceptional discovery to
  failure with entry-cleared caller outputs. Preserve no-follow/bounds/native gate.
- Applied post-cap: unique_ptr<DIR,local close lambda> owns stream; original FD
  remains Descriptor-owned. Failure/cancellation/exception returns no partial
  results. New owned-future fixture injects std::bad_alloc at the fourth readdir
  after acquisition/partial private enumeration, repeats8 times, counts actual
  real-stream acquisition/closure and verifies fcntl(F_GETFD)==-1/EBADF on duplicate.
  It is a deterministic exception seam, not an actual system-OOM experiment.
- Validation currently running in focused-checks-post-cap-stream-repair and
  preview-post-cap-stream-repair-normal. No fresh Goldfish review of repair is
  performed within exhausted cap. This remains the Major pending fresh closure.

Update: focused-checks-post-cap-stream-repair completed PASS for all ten plus
Python syntax751; the new future/stream/empty-output/EBADF witness passed all8
iterations. Current normal104 and pt-BR accessible104 previews also passed,
with selected actual images inspected. Accessible source checks matched before/
after; no native qualification follows. A separate validation-only package
records current bytes under increment-post-cap-stream-repair-validation; it is
NOT a sixth round and does not close the finding's fresh-review requirement.

## Other reviewers

- IR ses_e6f2f1fbaffeN3F0UFv1tcKt2N: **completed,0/0/0**. Full result/read receipts
  preserved in readiness-review.md. Actionable bounded contracts/checkpoints/gates;
  no execution/native PASS inferred.
- CD ses_e6f2f202fffex2QAxbV8Xu3XHG: **completed,0/0/0**. Full result/read receipts
  preserved in fidelity-review.md. Inspected implementation matched stated
  persistence/admission/root/ownership/runner boundaries; not full native audit.
- DE ses_e6f2f2034ffesgvrVCOpB1fL5c: **completed,0/1/0**, finding above.

## DE mandatory complete-read receipts

- AGENTS FULL1–9 FIRST, hash
  6b18c9623b06696974325dee9cf06016c6f4fa9408d2f5fc5251bdcefc81271c.
- Persona design_critic FULL1–63, hash
  cee6294735ce2bd4fea19245852b6eb07cbe8ede7e72a4bfc829f586e4a39d0f.
- Standard feature-spec FULL1–36, hash
  b3e4960fd884fde880a41da782fc73342fe232769e5835a4470b3c8c517644c4.
- Target FULL1–427, verified hash above; manifest FULL1–192, hash
  d2c71728d68f13f621dc2249a144ac277354b730a22c36c45d6cf721669963df.
- AGENTS-required local knowledge skill FULL1–52; no corpus/memory consulted.
- All180 source+4 harness hashes matched; hashing is not semantic review or PASS.

DE18 full source reads: four feature docs(APPEARANCE1–43,CONTENT1–71,FIRMWARE1–26,
FORWARDER1–106); assets1–54,dev1–25,forwarder1–164,gamefiles1–88,appearance1–42,
library1–92,storage1–102,user1–162; frontend1–475; fakeheader1–293;
UI browse1–677,theme1–151; appearancefixture1–144,contentfixture1–297.
DE14 partial: CMake1510–1538,1700–1768; main732–956,1980–2052,2400–2581;
settingsstore101–481,711–810; provider1240–1519,1592–1748,1860–1899,
1960–2275,2310–2409; fake359–552; preview65–479; launcher1–550/declaration200–353;
library852–966,1211–1368,1600–1859; services145–372;
settingsUI120–204,266–329,440–644; Switch430–496,500–547,880–913;
textures174–378; widgets20–124. Forwarderfixture/home additional search snippets
only. Remaining snapshot searched/hashed, not comprehensively read.
All four harness files read FULL: focused1–93,preview1–38,self-check1–68,recipe1–5.

All three mandatory reviewer inputs were fully delivered. All independently
verified target and184 manifest hashes. No failed/not-run reviewer is counted clean.
All reviews strictly read-only: tests/syntax/builds/previews/image inspection/
installs/network/hardware/native qualification NOT_RUN. Historical author receipts
not promoted to independent PASS. External baseline, excluded core/SDK/dependencies
and unrelated roadmap remain outside review. No prior reports/transcripts/memory/
live-checkout/vendor/network evidence or nested agents; Serena fallback without
activation/configuration. The post-cap repair was not present in reviewed bytes.
