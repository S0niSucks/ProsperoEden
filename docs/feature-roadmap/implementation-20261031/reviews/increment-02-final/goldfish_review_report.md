# Goldfish Annotated Review Report

Target: target.md FULL 1–279, SHA-256
838e6aa46a20dc10f9903218a0a94ef7271183b484927d7103286db8cd2d9e1c.

Status: author resolution applied under AUTH-02; fresh closure not established.
All three fresh reviewers completed. Original counts: **5 Blocker, 11 Major**,
16 findings; consolidated into 8 actions without changing any original severity.
Round 02 was **not clean before repairs**; clean streak 0, converged false.
All actions PROPOSE FIX; no contest, escalation, decline or accepted-risk waiver.
Earlier increment-02/ was never dispatched; these findings concern 02-final only.

## Original returned findings and resolutions

### A21 — Collection recovery notices

- **CD-01 Blocker**, target23–25,88–93. **DE-01 Major**, target23–25,88–93,221–224.
- Source: main774–783,880–891; provider home1393–1415; library draw_collection1642–1720,
  especially1710–1714. Crash/admission errors become Home.status with launch_failed,
  but Collection tests setup readiness or unrelated message_warning. Valid setup
  therefore hides preserved crash, forwarder refusal and returned-session errors.
- Rationale: recovery feedback promised by the bounded increment does not survive
  interface selection, despite correct suppression/admission handling.
- **PROPOSE FIX:** show nonempty Home status independently of setup readiness;
  preserve warning priority and readable wrapping. Add ready-setup Collection
  captures and recorded-draw assertions after controller Settings round trip for
  all three conditions. Applied to live library.cpp/preview_main.cpp; not a native
  argv or hardware result.

### A22 — Top-level corruption refusal

- **CD-02 Blocker**, target56–59,215–217. **DE-04 Major**, target52–59,215–217.
  **IR-04 Major**, target52–59,215–217.
- Source: Settings::Load250–263, SavePreferences369–420, content writers722–748;
  check-appearance82–91 only tests malformed named sections. Load converts existing
  scalar/array/null JSON to empty object, so mutation silently overwrites bytes.
  Invalid JSON similarly loses its corruption identity.
- Rationale: nested refusal is implemented, but the broad document-container
  preservation claim is false; readers and writers need distinct load semantics.
- **PROPOSE FIX:** introduce LoadForMutation for these three writers, refuse
  existing malformed/unreadable documents, permit missing/legacy import in memory
  without a side publication. Retain read-time defaulting; do not claim every
  legacy writer was retrofitted. Add scalar/array/null/truncated byte-preservation
  assertions for preferences and both content writers. Applied and focused checks
  passed; fresh review still required.

### A23 — Cancellation cutoff and publication

- **CD-03 Blocker**, target131–137,234–236. **DE-03 Major**, target130–137,234–236.
  **IR-02 Major**, target130–137,235–236.
- Source: provider set_location2219–2230/set_wallpaper2242–2248; helpers722–748;
  WriteFile195–209. Token is dropped before JSON load/edit/serialization and
  temporary write/flush/close/rename. Cancellation during those steps can still
  publish even though it preceded rename. Reporting saved after success is right,
  but the stronger pre-commit preservation acceptance is unsupported.
- **PROPOSE FIX:** carry token through Settings::Write/WriteFile and define cutoff
  as final acquire-load after staging/flush/close immediately before rename
  decision. Observed cancellation removes temp without publication; after cutoff
  rename is non-cancellable and successful result wins. Add real libc wrappers
  injecting requests during fwrite/fflush and on rename entry, not only a paused
  mock before helper entry. Applied; actual host fixture passed. No hard syscall
  deadline, native durability, timing or power-loss claim is inferred.

### A24 — Switch qualification-label parity

- **CD-04 Blocker**, target153–158,186–187.
- Source: launcher draw1242–1255; settings draw293–304/dialog855–866; Switch
  category520–522/dialog877–879. Encore/Collection shows host observation/current
  build unqualified; Switch only exposes logging/cache controls.
- Rationale: this is a missing application presentation path, not a demand for
  actual firmware support or a new host probe.
- **PROPOSE FIX:** show identical unknown/observed host status in both Switch
  category and dialog, keep controls and bounds, add both captures per palette.
  Applied; current complete preview is pending at resolution time.

### A25 — Shared mock-state access

- **CD-05 Blocker**, target265–272.
- Source: fake preferences81–89 vs content locks123–169; catalog start_scan476–500;
  launcher save243–256. Catalog worker reads preferences while owner writes them;
  content mutex does not protect preferences_. Input is permitted during scanning.
- Rationale: blanket mock thread-safety statement exceeds code; ASan/UBSan does
  not establish race freedom.
- **PROPOSE FIX:** state mutex around preference/game-setting/docked/mod/error/
  saved-Files snapshots; use setters for error strings and atomic scalar fixture
  controls; keep content mutex for saved/active content. Generated catalog/art is
  immutable after construction; no concurrent language-catalog replacement. Add
  concurrent coherent preference-pair witness and document no TSan proof. Applied;
  complete preview pending at resolution time.

### A26 — Entire independent discovery must be bounded/cooperative

- **DE-02 Major**, target117–119,126–135,229–235. **IR-03 Major**,
  target117–134,225–236. **CD-06 Major**, target117–134,229–235.
- Source: browse87–113, especially97–105, calls legacy folders before Images;
  provider folders2144–2148 -> ListEntries1119–1166 accumulates/sorts all entries
  without token/cap. UserContent::Images78–103 has the bounds, but runs afterward.
- Rationale: futures protect rendering but not the bound/cooperative contract.
  Circle only suppresses result application while uncancellable work consumes
  the sole owner and can prolong teardown across many successful operations,
  beyond the documented single-syscall latency limit. Existing tests cover Images
  only, not the actual provider route.
- **PROPOSE FIX:** separate content_entries API backed by one no-follow Entries
  walk for folders/images, 4,096 entry cap, 256 folder/128 image display caps,
  cancellation between entries and clear outputs on failure/cancel. Preserve
  legacy Files enumeration. Test unchanged provider bodies through actual helper,
  including root ancestry, limits and cancellation after fourth entry. Applied;
  focused checks passed.

### A27 — Checkpoint fixture dependency order

- **IR-01 Major**, target212–224.
- Source: check-appearance27,104–108 consumes theme.hpp/ApplyPalette58–87, but
  target assigns theme implementation to checkpoint2 after fixture gate1.
- Rationale: no phase-specific fixture or prerequisite resolves this; fresh
  implementer must choose repartitioning, violating stable checkpoint readiness.
- **PROPOSE FIX:** assign standalone role/palette helper implementation to
  checkpoint1 alongside preference helpers; checkpoint2 wires existing helpers
  into UI. Applied in live contract, no invented historical commit claims.

### A28 — Executable final gate

- **IR-05 Major**, target243–263. **CD-07 Major**, target243–247.
- Source: only five checks are named/snapshotted; content fixture needs writable
  disposable /data (3,37–64,149–164); preview pictures145–146 needs
  PE_PREVIEW_APPEARANCE=1; preview main812–827; forwarder doc94–103 gives only its
  own invocation. Ten-test suite, complete commands/mounts/environment and preview
  build/execution are not specified.
- Rationale: fresh implementer must select suite/setup, not merely reproduce a
  known gate. External receipts were outside permitted package, not nonexistent.
- **PROPOSE FIX:** enumerate ten checks, exact Docker image/security/disposable
  mounts and invocations, preview compile/link flags and activation, expected
  exit/inner PASS/no GL error, plus pt-BR accessibility rerun. Include runner/image
  recipe bytes and all ten test sources in next immutable package. Applied to
  contract/snapshotter; latest invocation receipts remain external.

## Review identities, mandatory reads and integrity

- DE: ses_e6f5e19e8ffeRFbI5GorLUOEqA; 0 Blocker,4 Major,0 Nit.
- IR: ses_e6f5e199fffesTlC8j0CVOtO3a; 0 Blocker,5 Major,0 Nit; readiness FAIL.
- CD: ses_e6f5e19e0ffelwVEFU3AlSeV2s; 5 Blocker,2 Major,0 Nit.

All independently hashed target and all148 snapshot files: zero missing/mismatch.
Hash verification is NOT a semantic reading or executed-test receipt.
All read ancestor AGENTS1–9 first, assigned persona (DE1–63, IR1–62, CD1–69),
assigned standard (DE feature-spec1–36, others readiness1–34), target1–279 and
manifest1–154 in full. IR additionally supplied hashes: AGENTS
6b18c9623b06696974325dee9cf06016c6f4fa9408d2f5fc5251bdcefc81271c;
persona1f6aa61cfe7cb86dc94943ab3b3fd339e5baba533937198fd10edb06dfa64e8f;
standarded0d2a085eac0fd9581d504a2ec0df2a92d6b2538636a22b16e0b1370fba6dbb;
manifest174adce42b4a9bd8ac10e17871d74e988b277b8c392d652f2ab0a90cfa2f8c2c.

DE complete reads: four feature docs, assets/dev/forwarder/game/appearance/library/
native_directory/settings/storage/user_content headers, provider declaration,
fake header, browse/launcher declaration/services/theme, three new fixtures.
Partial: main726–927,2005–2034,2460–2575; provider1095–1216,1310–1513,
1591–1745,1868–2256,2359–2403; launcher1–711,1203–1271; library800–1024,
1123–1407,1600–1736; settings384–639,807–882; widgets55–147,288–423;
widgets declaration1–180; Switch205–283,316–333; home407–444,858–897;
preview50–274.

IR complete reads: five feature/interchange docs; root/runtime/frontend/UI/host
EGM contracts; assets/dev/forwarder/appearance/native_directory/settings/user
headers; provider declaration; services/browse/theme; fake header; five supplied
checks. Partial main745–934,2465–2564; provider1105–1209,1974–2063,2132–2281,
2350–2403; launcher55–254,345–499; library1590–1769; preview1–275;
fake implementation485–549.

CD complete reads: five feature/interchange docs; root/runtime/frontend/host/UI
contracts; assets/dev/forwarder/game/appearance/library/preferences/settings/
storage/user headers; provider declaration; both FakeServices files; preview;
UI launcher declaration/implementation/services/settings/Switch/theme/widgets
declaration/textures; appearance/recent fixtures. Partial main760–988,1960–2041,
2430–2581; provider1112–1334,1393–1512,1595–1744,1868–2403; browse1–554;
home130–203,400–453,865–885; library476–574,743–796,852–996,1123–1371,
1590–1730; widgets1–125,288–430; pt-BR1764–1865; content fixture1–62,106–165;
forwarder1–112,241–302; library fixture1–32,64–76. Simple unit bodies not graded;
shared mocks/extraction seams/infrastructure were audited.

All reviews were strict read-only; no builds/tests/previews/installs/network/
hardware/nested agents or live checkout/vendor/history/memory access. Serena
initial instructions read; no activation/configuration; fallback file tools.
Author-reported historical passes were not independently reproduced.
