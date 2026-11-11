# Goldfish Annotated Review Report — round04

Target: immutable target.md FULL1–392, SHA256
862e4b10a81c1214fbdcf93389b2fe1701e304593060c6119b71b771083050d1.
Status: seven PROPOSE FIX actions author-applied under AUTH-02. Subsequent current
host gates passed: ten focused checks, runner self-checks, normal104 and pt-BR
accessible104 captures with inspected selected images. Round05 remains pending;
this is not fresh closure. No contest, waiver or severity change. Pending wording
inside the individual resolution entries records their earlier resolution-time state.
All three fresh reviewers completed. Original findings: **1 Blocker,4 Major,2 Nit**.
Round04 was not clean BEFORE repairs; clean streak0/converged=false. Round05 is
the last permitted round of this bounded loop. Even a clean05 cannot establish
two consecutive clean rounds. No silent extension beyond cap5 is authorized here.

Source paths below are relative to source/, except explicit harness references.

## CD-04-01 — Blocker — Actual high-contrast ink (A41)

- TARGET78–82,262,358–359; Switch22,218–225,482–487,559–570;
  widgets24–44,145–157; appearance fixture122–130.
- Switch chooses dark#161616 for selected categories/Files, but text_shrink goes
  through text_ink, whose neutral high-contrast lift changes it to about#b9b9b9.
  Against Slate focus this is about1.75:1, not the documented dark-on-focus pair.
  The raw colour regression bypasses the transformation and cannot establish the
  actual drawn contrast. This is a document/code contradiction, not a demand to
  certify unrelated legacy surfaces.
- PROPOSE FIX: add an explicit preserve_ink argument to text_shrink/draw_text,
  default false so existing text behavior is unchanged. Pass it for selected
  category/Files/Language on-surface roles only. Extend the real preview to inspect
  emitted DrawList glyph RGB (not only input constants) and contrast in all
  palettes/high-contrast states. Applied; complete preview pending at resolution.

## IR-04-01 — Major — Preview evidence replacement (A42)

- TARGET292–296,343–359; harness/build-preview29–36 uses exist_ok=True and w-log;
  preview127–139,178–210 emits fixed screenshot filenames.
- Target promises retained old receipts but says repeat with language/accessibility
  flags without explicitly changing PREVIEW_RUN. A fresh implementer can overwrite
  compared log/images because runner accepts old evidence directories.
- PROPOSE FIX: require PREVIEW_RUN, exclusively reserve a fresh evidence directory
  BEFORE compiler workers, and require distinct normal/ptbr-accessible/retry names.
  Self-check supplies an existing log/image with sentinels, verifies FileExistsError,
  zero compiler/process invocations and unchanged bytes. Applied and supplemental
  self-check passed. Existing historical receipts were not replaced.

## DE-04-01 — Major — Language filled-selection contrast (A43)

- TARGET8–10,34–45,65–82,246–248; Switch draw_switch_language602–608,
  apply_switch_palette218–225, theme70(Slate#f0f2f8), earlier Settings/Files485–487,
  567–570 already select palette-aware dark foregrounds.
- The Language name and OK marker retain white on the new near-white focus fill.
  New palettes must preserve multilingual usability; this is source-derived new
  palette regression, not rendered evidence or broad Original certification.
- PROPOSE FIX: same selected role pair and exact-ink preservation as A41, including
  marker. Add normal/high-contrast language/category/Files captures for all five
  palettes, inspect actual emitted glyphs and require marker separately. Applied;
  real C++ matrix pending.

## CD-04-02 — Major — Reentry before cancellation drainage (A44)

- TARGET139–148,282–284; browse enter_files65–80, browse_to87–106,
  finish_content_work133–159, press_files191–220; old preview369–376 settles before
  reopening, so does not exercise the race.
- Circle cancels and leaves the owned future. Immediate reopening clears entries,
  but browse_to refuses replacement while old query is valid. Old completion is
  discarded without scheduling the current directory. Browser stays empty/inert
  until another exit/reentry, relevant under admitted noninterruptible syscall
  latency. One-owner safety is retained but navigation completeness is not.
- PROPOSE FIX: record only current navigation reentry intent while old token stays
  cancelled; after get/drain, query current browse_dir/kind once if still in the
  independent browser. Leaving again clears intent; never queue stale mutations.
  Add a mock syscall hold that delays cancellation acknowledgement, same/different
  kind reentry BEFORE release, one-active-owner and exact call-count assertions,
  then actual parent navigation after completion. Applied; fixture pending.

## CD-04-03 — Major — Descendant containment on timeout (A45)

- TARGET329–336; harness/run-focused-checks13–36, self-check12–20;
  representative fixture descendant compilers at appearance135–142/nro164–171.
- subprocess.run times out its immediate Python child only. Compiler/executable
  descendants can retain stdout pipe, keeping timeout cleanup waiting for EOF
  before the handler can publish terminal failure. result.json can remain running.
  Existing self-check only sleeps in the direct interpreter.
- PROPOSE FIX: Linux Docker Popen with own session/process group, stdout to regular
  file (no pipe drain), child-subreaper enabled; kill group on timeout/parent exit,
  reap direct/adopted children with bounded waits. Cleanup errors are failures.
  Self-check creates a stdout-holding descendant, requires bounded terminal timeout,
  partial output and absence/reaping of descendant. Ordinary check/compiler family
  only, no claim for malicious session escape or kernel-uninterruptible tasks.
  Applied and supplemental self-check passed.

## IR-04-02 — Nit — Clear unavailable outputs (A46)

- TARGET128–131,274–285; UserContent81–87 clears lists, but native
  content_entries2203–2205 short-circuits unavailable before helper; default
  Services334–335 returns false without clearing. Fresh launcher vectors97–103
  avoid demonstrated stale UI, but broad failure contract and fixture are incomplete.
- PROPOSE FIX (worthwhile Nit): clear provided lists at default/native/mock boundary
  even when unavailable/cancelled. Preserve no-I/O unavailable path. Extract default
  method body and test prepopulated outputs; native fixture disables filesystem
  admission, actual preview tests unavailable mock. Applied; focused suite pending.

## DE-04-02 — Nit — Cancelled exception feedback (A47)

- TARGET139–143; browse finish_content_work135–158 guards successful result138–139,
  but catch158 unconditionally says cannot open. Circle193–197 can leave Settings
  while query stays owned. Exceptional completion can therefore replace new-screen
  feedback even though directory data remains guarded.
- PROPOSE FIX (worthwhile Nit): capture current screen/kind/cancel/reentry guard
  before future get and apply it to exception feedback too. Do not weaken drain/
  join. Mock releases a cancelled held query by throwing while later screen shows
  an unavailable explanation; compare emitted message glyph identity before/after.
  Applied; matrix pending.

## Review identities, required reads and limits

- DE ses_e6f3dccfcffe56cw0rIXOe5Q25: completed,1 Major/1 Nit.
- CD ses_e6f3dccd1ffeCzQy1hQw76d8le: completed,1 Blocker/2 Major.
- IR ses_e6f3dccb6ffez0Zed5wGZBCCb1: completed,1 Major/1 Nit, needs revision.

Every reviewer read AGENTS1–9 FIRST; persona FULL(DE1–63,CD1–69,IR1–62), designated
standard FULL(feature1–36 for DE,readiness1–34 for CD/IR), target FULL1–392 and
manifest FULL1–192. All four harness files read fully. All verified target hash.
DE/CD independently checked180 source+4 harness hashes with no missing/mismatch.
IR did NOT independently recompute manifest source hashes; no inferred receipt.
Hashing is never a claim of semantic full-file reading.

DE full sources: assets/dev/forwarder/game/appearance/library/settings/storage/user
headers; frontend.cpp; UI browse/services/theme; fake header; appearance/content/
forwarder fixtures; all five feature/interchange docs. Partial main450–739,750–960,
1790–1892,1980–2049,2420–2581; provider1369–1512,1591–1747,1974–2256,2363–2407;
launcher48–321,381–812/declaration120–398; library1–1065,1123–1369,1580–1757;
home1–541; settings100–204,282–303,425–642,820–869; Switch1–242,472–649,806–919;
widgets55–154,291–422; textures1–54,168–373; fake359–548; preview64–387;
pt-BR1749–1865. Remaining content search/hash-only.

IR full21 sources: assets1–54,dev1–25,forwarder1–164,appearance1–42,library1–92,
settings1–812,user1–162; browse1–660,services1–421,theme1–151; fakeheader1–275;
appearancefixture1–144,content1–282,forwarder1–302,recent1–58,library1–76;
docs appearance1–39,content1–66,firmware1–26,forwarder1–106,interchange1–47.
IR partial8: main748–952,1995–2039,2470–2549; provider1840–1919,1974–2053,
2144–2260,2363–2407; preview1–388,850–1122; fake350–554; launcher1–285,370–564;
library1580–1734; settings452–641; Switch1–55,145–234,680–779.
Remaining151 sources not content-read. Harness complete lengths37/54/31/5.

CD complete relevant reads: appearance/forwarder/content/settings/root/library
helpers; service/launcher declarations; launcher.cpp,browse,settings,theme; both
FakeServices; five feature/interchange docs; supplied headless/frontend/UI/host
EGM contracts. Large main/provider/Collection/shared-library/palette/textures/
preview/localization selectively read only. It did not supply invented full-file
line counts. Simple unit bodies excluded; extraction/wrappers/mocks/runners audited.

All: strict static read-only. Tests/builds/previews/screenshot inspection/installs/
network/hardware/nested agents NOT_RUN. Serena no active project; file fallback
without activation/configuration/memories. No prior reports/transcripts/live
checkout/vendor/network evidence. Historical author passes/baseline not reproduced.
No finding merely demands unrelated roadmap or native/core/firmware qualification.
