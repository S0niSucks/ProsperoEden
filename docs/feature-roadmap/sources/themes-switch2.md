# Themes, interfaces and Switch 2 — scope intake

Date: 2026-10-31. The owner explicitly requested these additions **inside the
EGM design**, not production implementation. P10 is expanded; P12 is new.

This is a documentation-level intake, not a source-body compatibility audit,
fresh Goldfish review, build or PS5 qualification. The earlier source reports
still cover the original eleven groups and their original scope.

## P10 reference: ps5-homebrew-ui

Local root: `D:/leonardommello/PS5/ps5-homebrew-ui`.
Read-only Git inspection found HEAD
`bbe19054ee2e7ec069254775273aeb8846c4bd3e`, branch `main`, with no working-tree
changes at intake. No files in that repository were changed.

### What was read

The following were delivered in full: `AGENTS.md` (355 lines), `CLAUDE.md`
(21), `README.md` (309), `docs/ADOPTING.md` (210), `docs/THEMES.md` (567),
`docs/ARCHITECTURE.md` (129) and `docs/CRAFT.md` (187). No source bodies,
third-party code, generated pictures, tests or native execution receipts were
audited. File hashes and the source HEAD are recorded in
[input-provenance.json (external receipt)](../EVIDENCE.md#provenance).

### Evidence and transfer limits

- [README](../../../../ps5-homebrew-ui/README.md), lines 5–16 and
  40–52, describes an OpenGL kit, reusable components and complete example
  designs; its documented counts are 21 designs, 30 themes and 99 components.
  Those are source-document counts, not a verified Encore delivery list.
- [THEMES](../../../../ps5-homebrew-ui/docs/THEMES.md), lines 3–14
  and 73–104, distinguishes bespoke screen design from theme data and themed
  widgets. Therefore the owner's request cannot be met by palette changes alone.
- [ARCHITECTURE](../../../../ps5-homebrew-ui/docs/ARCHITECTURE.md),
  lines 3–7, 44–90 and 99–105, describes an immediate-mode OpenGL path,
  state updates separate from drawing, layer composition and accumulated
  controller input. It does not establish binary/API compatibility with Encore.
- [ADOPTING](../../../../ps5-homebrew-ui/docs/ADOPTING.md), lines
  8–31 and 83–97, lists different adoption mechanisms. It is not an instruction
  to copy the entire kit, platform runtime, build tree or app into Encore.
  Lines 117–125 specifically report text/shaping limitations encountered by
  another adoption; Encore's current multilingual font path must not be lost.
- [CRAFT](../../../../ps5-homebrew-ui/docs/CRAFT.md), lines 12–30,
  61–65 and 143–166, supplies source design guidance on TV readability, visible
  focus, controller hints, reduced motion and nonblocking input. No exact visual
  style, animation values or default interface is selected by this intake.
- README lines 244–255 reports hardware measurements for that project's build
  `044d909` on 2026-10-02. Those source-reported measurements do not qualify
  Encore, the current UI reference HEAD, or the new interface combinations.
- README lines 285–287 and ADOPTING lines 205–210 describe GPL-3.0-or-later and
  separate third-party asset terms. Actual file/asset attribution and permitted
  reuse need a later scoped review; no fonts, music or artwork were imported.

### Requirement captured

Provide selectable themes **and** alternative interface layouts/navigation,
using this project as an explicit construction reference. Existing Encore
presentation must not be the only available interface. Preserve working game
services and earlier ports; retaining a recovery presentation does not select
the default appearance forever. Concrete layouts, default choices, theme/layout
compatibility, persistence and compiled versus external-pack mechanisms remain
owner decisions. No requirement to import all examples or all thirty themes is
inferred from selecting the reference project.

## P12 reference: Eglantine for Switch 2

Repository: [Djihads80/Eglantine](https://github.com/Djihads80/Eglantine).
Inspected branch: `nx2`; pinned commit:
`b40ad217ab829226841cb71f6e4c5cb05977a225`.
The GitHub repository description at intake was
“VERY experimental Nintendo Switch 2 emulator”.

### Delivered sources

These pinned documents were read in full and saved as reference bytes:

- [README](https://github.com/Djihads80/Eglantine/blob/b40ad217ab829226841cb71f6e4c5cb05977a225/README.md), 73 lines.
- [NX2 notes](https://github.com/Djihads80/Eglantine/blob/b40ad217ab829226841cb71f6e4c5cb05977a225/NX2_NOTES.md), 275 lines;
  the initially truncated delivery was completed with the remaining lines.
- [GPU-thread split notes](https://github.com/Djihads80/Eglantine/blob/b40ad217ab829226841cb71f6e4c5cb05977a225/NX2_GPU_THREAD_SPLIT.md), 63 lines.
- [Build guide](https://github.com/Djihads80/Eglantine/blob/b40ad217ab829226841cb71f6e4c5cb05977a225/docs/Build.md), 195 lines.

The GPLv3 license file was saved, but only its first 25 lines and the README's
license statements were read; this is **not** a complete legal/provenance audit.
No repository clone, dependency fetch, source-body audit or executable download
was performed. Known root-file Git blob identities were verified; all saved
documents have SHA-256 provenance. Original relative links in raw snapshots
retain their upstream meaning; the snapshot directory is not a complete checkout.

### What those sources establish, and what they do not

- README lines 18–20 describes a C++ project derived from Eden/Yuzu/Sudachi
  and names Windows, Linux and macOS builds. Lines 40–44 claims game execution
  and performance. This records upstream claims, not independent reproduction
  or PS5 availability.
- NX2 notes contain experiments at different stages. Lines 147 and 186 record
  task/mesh limitations and a desktop run needing approximately 20 GiB of free
  commit for a particular configuration. Lines 207–213 report GPU-thread and
  video work, including later fixes and progress. These are versioned host
  observations, not requirements proven identical on PS5.
- NX2 notes lines 234–275 also records partial title outcomes, automatic stubs
  and open issues. Earlier headings and later progress rows are not one uniform
  compatibility result; old limitations must not be presented as permanent,
  and later success must not be generalized across all titles or platforms.
- The GPU split notes describes CPU/GPU ownership and threading work. Sharing
  Eden ancestry does not establish compatible memory/JIT/kernel/GPU services,
  shader pipelines, callbacks, SDK imports or packaging for Encore.
- License identity alone does not approve every inherited file/dependency or
  authorize redistribution of third-party assets. No proprietary game, key,
  firmware or SDK material is included by this design addition.

### Requirement captured and feasibility gate

The requested capability is **Switch 2 support**, not merely adding a platform
label or launcher tile. Eglantine is a candidate to evaluate, not a selected
replacement core or an already working backend. Determine permitted integration
interfaces, correctness and compatibility, CPU translation, guest/host memory
requirements, GPU features/shader translation, audio/input/save behavior,
dependencies/licensing and native PS5 feasibility before an implementation claim.

Keep the requirement visible if blocked. Record discovery, load/boot, graphics,
audio/input/saves and sustained gameplay as separate future evidence states.
A desktop run, a Switch 2 filename or UI selection is not Switch 2 emulation on
PS5. Existing Switch support and the excluded Eden subtree must remain untouched.
