# Switch 2 — research arithmetic, not support

This standalone Python/std-library tool checks a **declared model**, not an
emulator, backend, allocator or console. It is not imported by the launcher.
Every output has `switch2_supported=false`; exit 0 means toy arithmetic only.
See the [comparison](../../../docs/feature-roadmap/implementation-20261031/research/nx2-comparison.md)
and [campaign integration](../../../docs/feature-roadmap/campaign-20261107/INTEGRATION.md).

## Contract

- Commit and its limit must belong to the same host accounting domain. They are
  not resident memory and are not added to resident charges.
- Resident components must be **disjoint physical charges at a simultaneous
  peak**, including guest backing, graphics, JIT and runtime overhead. Count
  CPU/GPU aliases once. The tool does not infer discounts for sparse backing,
  compression or aliasing, or verify the supplied measurements.
- The application budget must be established separately. PS5's advertised total
  shared hardware memory is not the process budget. Explicit positive margin is
  required. Unknown memory/capabilities block; `null` is never zero.
- Virtual extents are validated but not charged as resident memory. Unknown VA
  does not block this arithmetic-only check or imply a verified mapping.
- Seven capabilities are declarations, not discovered properties. `unknown` and
  `pc_documented` block, `unavailable` rejects, `synthetic_only` admits only a toy
  model. There is no accepted `ps5_verified` or build-success status.
- A fixture has strict keys/types, integer byte quantities bounded to u64, and a
  64 KiB JSON read limit. Sums do not wrap; duplicate keys/non-finite JSON fail.

Statuses: `SYNTHETIC_ONLY` (exit 0), `BLOCKED`/`REJECTED` (exit 1),
`INVALID_INPUT` (exit 2). Rejecting one declared scenario is not proof that
Switch 2 emulation is impossible. None of these states may become a capability
flag or bypass a launcher/core gate.

## Fixtures and documentary input

`synthetic-exact-boundary.json` uses arbitrary **toy bytes**, not a PS5 estimate:
75 resident + 25 margin = 100, separately commit 70 <= 128.

`pinned-pc-commit.json` is deliberately BLOCKED. Its only documentary quantity is
20480 MiB = 21474836480 bytes, from Eglantine's pinned journal line 186. This is
an attributed backing-allocation failure, not an independently measured resident
peak or an established requirement of that whole revision. PS5 budget, margin,
residency and capabilities remain unknown. [provenance.json](provenance.json)
retains URL/commit/blob/hash/quote and the original targeted reading scope.

The complete upstream document is a separate raw test input, not a replaced or
abridged quotation. For offline tests, supply its exact bytes read-only through
`NX2_DOCUMENTARY_INPUT`, or at `inputs/G-NOTES.md` in a disposable test layout.
The default fails if absent: **no skip or weakened provenance check**. Tests check
SHA-256, Git blob and the quote at the original line. Acquisition, when needed,
must use the pinned URL in `provenance.json`, never an unpinned branch.

Campaign raw input remains under the approved external root at
`encore-campaign-20261107/A12/inputs/G-NOTES.md` (not executable instructions).
No implementation, games, keys or firmware are needed by this tool.

## Reproduce inside the approved Docker sandbox

Use the existing offline image with repo/input read-only, network disabled,
bounded CPU/memory/PIDs, fresh external outputs and container timeout/cleanup.
No dependency installation or source preparation is required. Example payloads
with repo mounted at `/repo` and the pinned document at `/inputs/G-NOTES.md`:

```sh
cd /repo/tools/research/nx2
NX2_DOCUMENTARY_INPUT=/inputs/G-NOTES.md python3 -B -m unittest -v test_nx2_gate test_a12_gate_additional
python3 -B nx2_gate.py fixtures/synthetic-exact-boundary.json
python3 -B nx2_gate.py fixtures/pinned-pc-commit.json
```

The second CLI must return 0/SYNTHETIC_ONLY; the third 1/BLOCKED. Do not convert
the documentary case to a positive fixture. The tests have 29 methods, including
16,384 capability combinations, CLI outcomes and input/provenance boundaries.

## Evidence and relocation

A50 ran all 29 original methods before integration. CLI and both JSON fixtures
are byte-identical to those candidates. Relocated tests change only documentary
input selection and the A36 default root; all original pins/assertions remain.
Their integrated rerun passed all 29 methods in A50/integrated-04, with the new
default root and mandatory pinned document. The missing-document control failed
with exit1/FileNotFoundError as required, never skipped. Results and original
failed/refused attempts are recorded separately in the campaign; original source
comments saying NOT_RUN preserve what their authors had executed, not a new
claim about the coordinator's later runs. Test success proves arithmetic and
failure behavior only, not backend correctness or platform feasibility.
