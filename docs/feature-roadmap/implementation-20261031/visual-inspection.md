# Actual C++ screenshot inspection

Environment: Docker host C++20, Mesa/llvmpipe,1920×1080, synthetic art/providers,
distribution HarfBuzz8.3; no console/system-font/latency qualification.

## Inspected completed receipts

- `preview-round2-repairs/screenshots/collection-previous-crash.png`: pt-BR,
  larger text/high contrast/reduced motion. Ready setup plus crash status is
  visible above the footer; title/grid and warning text remain readable.
- Same run `appearance-chooser-saved.png` and `storage-independent-locations.png`:
  three property labels/values fit their rows and the translated modal guidance.
- `preview-round2-complete/screenshots/collection-forwarder-refusal.png`: generic
  launch-refusal detail is visible independently of setup readiness; no hidden
  notice or footer overlap in this captured case.
- Same run `switch-editors-midnight-diagnostics-dialog.png`: unobserved/current
  build unqualified host label and guest startup detail are visibly separate from
  logging/cache controls and fit the modal.
- Same run `switch-editors-original-diagnostics-category.png`: host label visible
  in category. This does not establish Original's broad contrast compliance.
- Same run `switch-editors-midnight-mapping.png`: focused text uses a dark selected
  surface, and the physical L1 keycap remains neutral/dark with visible light ink.

## Visual defect discovered after snapshot03 — author repaired

`preview-round2-complete/screenshots/switch-editors-midnight-diagnostics-category.png`
shows **white selected-category text on the new pale-blue focus background**.
Advanced editor rows were repaired earlier, but the category sidebar uses another
foreground/surface pair. This is an actual rendered defect, despite the scenario
fixture's PASS; it does not measure every text/background contrast combination.

Author repair applied to live source: neutral dark selected-category ink for
non-Original palettes/high contrast, preserving Original non-high-contrast.
The focused fixture now checks the new role pair against4.5:1 sRGB contrast.
`preview-round3-visual` passed after the category fix. Subsequent Files/refusal
improvements are visible in `preview-round3-repaired-recent-flow`: Slate Files
selection uses readable dark ink; Switch refusal and unavailable dialogs show
their warning text without row/footer overlap. The complete normal fixture
passed after correcting its Recent input sequence to the existing two-Cross
card→Hero→launch behavior. The earlier failed Recent fixture is retained under
`preview-round3-repairs/`; it is not silently reclassified as PASS. Do not
claim the immutable03 source already contains this repair or promote its clean
review (if any) to newer live bytes without another fresh round.

`preview-round4-ptbr-accessible` completed against04-final:71 captures, binary
4d6db2cb163ff60aa65288422a6bbdbff814aceb0174720fff8b2f230920ff74,
exit0, no source mismatch before/after. Inspected Slate Files selection,
unavailable-storage explanation, Original Diagnostics dialog and Collection's
partial last row. The long translated diagnostics label/value columns were
crowded, leading to the later computed32px gutter in Switch dialogs.

Round04's static audit found that text_ink lightened deliberately dark ink under
high contrast. The earlier raw-role contrast assertions and screenshots did NOT
establish correct emitted contrast. Current source now preserves selected ink
through text_shrink for Settings/Files/Language (including OK marker). The new
preview inspects the actual emitted glyph colours and contrast in each palette
with high contrast both off/on. Thus this older PASS is historical only; current
build/render completed104 captures under `preview-round4-repairs-normal/`.

## Current normal-build inspection (frozen05)

Actually opened Slate/high-contrast Language (selected English name and OK marker
are dark/readable), Slate/high-contrast Files (parent text/icon dark/readable),
different-kind deferred reentry (a usable parent row, current Games directory),
and cancelled-exception feedback (newer unavailable explanation remains visible).
These agree with the integrated glyph RGB/contrast and owner-count/navigation/
feedback assertions. Binary93001cc79ff88923720ab43bc05f0fd4e0d21ff250be8499240e942da70020b1.
Source/harness comparison after rendering matches05-final; that snapshot was made
after compilation, so no automated compilation-start hash receipt is claimed.

`preview-round5-ptbr-accessible/` completed104 captures on that same binary with
source verification before/after against05, no mismatches, exit0 and fixture PASS.
Actually inspected translated Original Diagnostics dialog (label/value gutter and
host/guest qualification detail fit), Slate Language/OK/category/Files (dark selected
ink remains readable), Switch unavailable explanation (full bounded wrapped text),
and same-kind browser reentry (folders/image/parent remain usable). The cancelled
exception fixture preserves the later unavailable message; Encore's existing
single-line notice ellipsizes it with large pt-BR text, so this is not a claim
that all legacy notices show every word or that every translation is certified.
No console, system-font, performance or broad legacy contrast compliance is inferred.

## Post-cap source revision

Round05 found an exception-safety Major in UserContent::Entries, not a visual
finding. Its post-cap stream/private-output repair changes current source hashes
after frozen05. Current focused checks passed, including actual future exceptions/
closed streams/empty outputs/EBADF; new normal render passed104 captures and
selected current Language and browser-reentry images were opened. Source/harness
post-render checks match the validation-only package. UI binary is unchanged;
the resource repair is executed by focused real-helper tests, not synthetic UI
I/O. The fresh current-source-bound accessible rerender passed104 captures, exit0,
no source mismatches before/after. Opened current translated Language/OK, browser
reentry and Diagnostics gutter images; they retain the inspected readable layout.
Do not relabel
the prior104+104 source-bound receipts as proof of these newer full-source bytes.
No round06 is being run. EGM remains capped/non-converged independently of the
eventual host validation result.
