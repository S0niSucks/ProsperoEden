# Provider-owned witnesses — permanent-regression-v2

Destination: `tools/feature-roadmap-regressions/provider-owned/`, eight files only.
**New Python, syntax checks, gap controls and runtime/builds: NOT_RUN by A46.**
V1 is frozen. Four C++ fixtures remain byte-identical to the separately HOST-tested ownership-only-v1 package; that historical PASS does NOT qualify this adapted runner. No source snapshots, deps/SDK or raw artifacts belong in the promoted tree.

## Decisions after A29 H01/H02/H03

**H01:** Selection scans lexically from the START of the source, not from a byte-counted signature. Comments, ordinary/character and supported raw literals are masked. Backslash-newline/CRLF splicing has an original-offset map; slices are always the real original bytes, including physical newlines. The unique signature must be in code at a supported declaration boundary. Macro/directive payloads are not definitions; targets inside conditional preprocessing or with directives inside their body are refused. A commented/raw-only signature cannot become active output.

This is a small fail-closed cutter, not a C++ parser/preprocessor: unclosed literals/comments/conditionals, ambiguous definitions, trigraph/digraph syntax, unsupported declaration contexts and raw literals containing splices require review. It does not evaluate `#if`, prove namespace/type semantics or compile the full provider. Valid but unsupported new syntax may be blocked; do not weaken rejection or rewrite the provider algorithm to obtain green.

**H02:** Include/directive checks use the spliced lexical view, with comments masked and literal operands preserved. `include_next`, unknown directives, nonliteral/macro include operands, extra first-party include names, unsupported pragmas/preprocessing intrinsics and unreviewed helper includes reject BEFORE compilation. Legitimate pinned literal includes remain supported, including valid continuations and comments. First-party search uses `-iquote`, not broad `-I`; host stb uses `-idirafter`. Metadata-only preflight resolves supported quoted headers in actual search order, refuses aliases/shadows without reading unknown header contents, and rejects latent first-party standard/codec header shadows. It runs before every compiler command.

Depfiles retain lexical alias evidence and require an **EXACT per-unit first-party set** across checkout/harness/output roots, not merely a subset: real source/fixture and its allowed live headers/fragments only. Extra pinned-but-wrong-unit files also fail. The host codec must appear explicitly in its depfile; ordinary toolchain/system dependencies remain a trusted preprovisioned host-image input, not production/SDK snapshots or permission to reach protected core. Late dependency checks are defense in depth, never authorization to compile an unreviewed header.

**H03:** A50's strict EXTERNAL ENCLOSURE IS MANDATORY. `/.dockerenv` is only an execution guard, NOT proof of read-only mounts, network/PID/resource isolation, subreaping, absence of survivors or stable log hashes. The runner requires `--ack-external-enclosure`; acknowledging a flag is not evidence of that enclosure. It keeps safe timeout/interruption killpg+bounded direct-leader reap, and **never signals a PGID after completed leader reap**. A normal leader may leave descendants, including another session. A50 must contain/adopt/drain the whole process tree/container and capture final log hashes/receipt AFTER that drain. Internal hashes are explicitly PROVISIONAL until then. Internal witness PASS does not close enclosure acceptance.

## Real source and immutable fixtures

The two complete bodies are extracted verbatim from LIVE `headless/prosperoeden/eden_services.cpp` (ListEntries / EdenServices::load_image). No repair is synthesized, no live file is reverted, and no old provider is packaged. The actual LIVE dependencies are pinned Image header/source and `headless/user_content.h`/Descriptor. Compiler depfiles must prove those paths were consumed. Gate absent/zero retains HOST `PS5_NATIVE=1`, actual Descriptor, runtime checks and NM without openat/fdopendir/readdir/fstatat (including usual 64-bit aliases).

The unchanged four C++ fixtures still call real Linux libc/getdents64 and the real preprovisioned HOST stb codec/deleter; use actual Image/TGA/DDS code and actual allocator failure points; observe fd close/EBADF or counted stbi_image_free BEFORE baseline cleanup. AppFile/ValidAssetsDir remain fail-stop unexercised seams. Their old comments referencing `prepared/provenance.json` are unchanged for byte identity; current provenance is `result.json.extracted_functions`.

This is NOT full-provider/settings/ReadFile/native ABI/SDK/link/console/performance qualification. The A46 production ownership patch is separate. A05 ReadFile/blocked old compositions are not repaired, composed or waived here.

## Identification is NOT a test

Only after a stable reviewed source window, identify four LIVE inputs into a NEW EXTERNAL directory:

```sh
python3 -B /repo/tools/feature-roadmap-regressions/provider-owned/extractor.py \
  --identify --repo /repo --out /evidence/provider-owned-pins-v2-001
```

This produces `source-inputs.json` with `INPUT_IDENTIFICATION_ONLY_NOT_TESTED`. No candidate SHA is hardcoded; changed versions need new reviewed pins. Pin/hash/extraction/compile/NM checks cannot substitute for witness runtime or enclosure evidence. No install/fetch fallback for missing tools/stb.

## Execution — A50 only, strict external Docker Linux x86_64 enclosure

The caller supplies the existing image, RO source/harness, `--network=none`, CPU/memory/PID/fd/output limits, bounded timeout, whole-tree cleanup/adoption and final hashes. Runner child limits additionally cap CPU120s, address space2GiB, fds256, and file size8MiB (this includes objects/binaries, not just logs), or lower inherited hard limits.

```sh
python3 -B /repo/tools/feature-roadmap-regressions/provider-owned/run.py --execute \
  --ack-external-enclosure --repo /repo \
  --source-manifest /evidence/provider-owned-pins-v2-001/source-inputs.json \
  --stb-header /usr/include/stb/stb_image.h --compiler clang++-18 \
  --out /evidence/provider-owned-candidate-v2-001
```

Candidate-only is default: EXACTLY four FD/JPEG runtimes (absent/zero), each exit0, controls0, regressions0, **hits16 FD / hits8 JPEG**, plus both gate runtimes/depfiles/NM. A pre-fix live candidate fails; there is no automatic overlay. Existing output directories and outputs inside checkout/harness reject.

Optional red/green adds BOTH `--baseline-provider /external/authorized-old-provider.cpp` and `--baseline-sha256 <explicit-reviewed-SHA256>`. No implicit lookup, baseline snapshot in repo, or undo of live repairs. Both function versions intentionally use the same LIVE pinned helper headers/source. Baseline requires exit10, controls0, regressions16/hits16 FD and regressions8/hits8 JPEG for both gates; candidate requirements are unchanged. Eight exact runtimes plus both gates are mandatory. Compile error, syntax/import error, crash, timeout, unreachable fault or generic exit10 is not accepted red.

All fragments/synthetic JPEGs/depfiles/objects/logs/results remain EXTERNAL. Source/harness/manifest/codec/baseline hashes and include preflight are checked before/after commands. Stable read-only inputs remain required: point-in-time hashes are not atomic snapshots or continuous mutation detection. `result.json` separates prerequisite commands from actual witnesses and declares external enclosure/final-hash evidence outstanding.

## New gap tests and promotion

Delivery metadata includes a relative eight-file promotion manifest/pathmap and four original/copy hashes. Supplemental `tests/gap_controls.py`, `tests/enclosure_probe.py` and their README live OUTSIDE the eight-file promotion tree in this v2 bundle; A50 receives them explicitly. They were authored independently; no A29 script or partial output was copied.

The 36 prepared H01/H02 controls use synthetic lexical/filesystem inputs, including a spy requiring runner rejection before Popen. They are NOT provider FD/JPEG tests and must not inherit their PASS. H03 probes deliberately let a normal leader leave a log-owning survivor, with/without setsid; only the external supervisor's completed receipt/drain/final hashes can accept them. All are NOT_RUN in this delivery, including syntax checks. A29/central review and A50 execution are required before promotion; no assertions were loosened and no C++ bytes were changed.
