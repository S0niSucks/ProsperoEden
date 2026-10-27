# Upstream integration audit — 2026-10-27

Source: [Swordpdf/ProsperoEden](https://github.com/Swordpdf/ProsperoEden).
Base: `a26ba78` (1.005.000). Local branch: `integrate/upstream-prs-2026-10-27`.

**Experimental integration, not a qualified PS5 release.** No issues were closed, no upstream
PRs were merged on GitHub, and nothing was pushed. Host checks are not evidence that a reported
game, GPU, firmware or performance problem is fixed.

## Inventory and history

- Snapshot: **157 PRs** (128 merged, 17 open, 12 closed without merge), **123 issues** (71 open,
  52 closed). The original JSON inventories, including issue bodies and comments, are retained
  locally in `.local/upstream-audit/{prs,issues}.json` (not committed).
- All PR heads were fetched to `refs/remotes/upstream/pr/<number>`, using GitHub's pull refs.
  `upstream/main` was fetched too. The local `main` initially matched it exactly and was left alone.
- All **17 open PR head commits are ancestors of the integration branch**. Grouped contributions
  keep their original commits and authors: #276 includes #272/#274; #284 includes #275.
- Of the 128 historically merged PRs, 125 recorded merge commits are ancestors. The recorded merge
  SHAs for **#4, #8 and #9 are not**. Do not confuse that with three missing current features:
  main carries the reworked Lapy integration (`996fdf1`, `5a9c350`, later `10b1626` and `dfda802`).
  The old embedded helper was superseded by the pinned one-shot helper. Their old merge commits
  were not replayed on top of that implementation.

The fetch can be repeated without modifying the working tree:

```bash
git fetch https://github.com/Swordpdf/ProsperoEden.git '+refs/heads/main:refs/remotes/upstream/main' '+refs/pull/*/head:refs/remotes/upstream/pr/*'
```

## Open PRs integrated

| PR | Change | Integration / remaining qualification |
|---|---|---|
| [#272](https://github.com/Swordpdf/ProsperoEden/pull/272) | Skyline hooks | Through #276; synthetic hook fixtures, not proof of every mod |
| [#274](https://github.com/Swordpdf/ProsperoEden/pull/274) | Safe post-processing paths | Through #276; failed-canonicalization fallback |
| [#275](https://github.com/Swordpdf/ProsperoEden/pull/275) | Opt-in adaptive clock | Through #284; normal ticks by default; game A/B still needed |
| [#276](https://github.com/Swordpdf/ProsperoEden/pull/276) | Hooks / effect-path integration | Preserve its combined CMake transformation and timing-stop fix |
| [#284](https://github.com/Swordpdf/ProsperoEden/pull/284) | Adaptive idle polling / atomic interrupt access | **Author explicitly requests PS5 A/B before production merge**; local experimental branch only |
| [#285](https://github.com/Swordpdf/ProsperoEden/pull/285) | NSZ/XCZ readers | Synthetic NCZ checks; real console files not tested here |
| [#286](https://github.com/Swordpdf/ProsperoEden/pull/286) | Nested recent-game paths | Shared naming for history and loading covers |
| [#287](https://github.com/Swordpdf/ProsperoEden/pull/287) | Newest update metadata | Combine with #285 in one `registered_cache.cpp` derivative |
| [#288](https://github.com/Swordpdf/ProsperoEden/pull/288) | Game-folder mods scoped by title | Keep update/DLC lookup from inheriting the base game's local mods |
| [#289](https://github.com/Swordpdf/ProsperoEden/pull/289) | PCM overflow recovery | Drop oldest queued audio, retain buffer/sample correspondence |
| [#290](https://github.com/Swordpdf/ProsperoEden/pull/290) | Forwarders after a game moves | Prefer original path; refuse ambiguous one-level matches |
| [#291](https://github.com/Swordpdf/ProsperoEden/pull/291) | Count nested game folders | Reuse #285's `IsGameFile`, including NSZ/XCZ |
| [#292](https://github.com/Swordpdf/ProsperoEden/pull/292) | Local `library.json` export | Keep both `game_files.h` and `library_file.h`; test GCC include ordering |
| [#293](https://github.com/Swordpdf/ProsperoEden/pull/293) | Hide unused trailing CPU cores in HUD | Host formatting tests; no CPU/FPS improvement implied |
| [#296](https://github.com/Swordpdf/ProsperoEden/pull/296) | Refused guest-write accounting | Targets part 1 of #246, not its GPU/heap/launch problems |
| [#298](https://github.com/Swordpdf/ProsperoEden/pull/298) | ExecuteProgram relaunch | Added callback/transition regression test; real multi-program games untested |
| [#302](https://github.com/Swordpdf/ProsperoEden/pull/302) | Swedish translation | Translation content only |

Merge conflict resolution kept every added regression check. Additional integration checks
cover NSZ/XCZ game counts and verify that newest-update selection does not erase the compressed
container adapter. `tools/check-execute-program.py` compiles the actual callback and transition
fragments and exercises program zero, first-request wins, cancellation, previous-program
metadata, and multiple handovers without spending a repeat/soak cycle.

## Closed, unmerged PRs

These heads are available locally, but closed alternatives and unfinished prototypes are not
automatically safe additions to current main.

| PR | Disposition |
|---|---|
| #37 | Historical macro-JIT recovery alternative; not replayed over newer macro handling |
| #38 | Competing historical rollback; not combined with #37 |
| #43 | Historical FW740 documentation correction; not treated as a current runtime patch |
| #123 | Explicitly superseded by #124 |
| #124 | Closed work-in-progress performance/memory branch; not treated as a qualified fix |
| #127 | Closed preset/reporting prototype; no evidence approving its presets for this integration |
| #131 | Closed version branch/template, not a separately qualified feature |
| #140 | SDK 1.0.2 change already incorporated in the later version integration (`d7f2764`, #141) |
| #174 | Closed theme overhaul; current theme/launcher architecture has moved on |
| #196 | Closed rumble alternative; not replayed over current HD-haptics changes (#208) |
| #240 | Closed scene-theme/Themezer draft; #239 is not implemented by this integration |
| #241 | Closed IPC-ring diagnostic proposal; not a fix for #238 by itself |

## Open issue triage (all 71)

**Candidate** means relevant code is integrated, not that the complete report is resolved.
**PS5** means reproduction and console evidence remain necessary. **Feature** means the requested
capability is not implemented here. No entry below authorizes closing an upstream issue.

| Issue | Status / next evidence |
|---|---|
| #42 Chinese font detection | PS5: retest current elevation/font access on FW 9.20; older font-access fix is already in main |
| #46 Breath of the Wild FPS | PS5: same warmed scene, settings, frame times and logs; not a generic clock-fix claim |
| #48 Jamboree cookie-cutter freeze | PS5: reproduce minigame and collect GPU/guest logs |
| #50 Separate game files and emulator data | Feature: existing data separation does not provide independent/multiple ROM, key and firmware roots |
| #52 Xenoblade Chronicles 3 console freeze | PS5: guest/GPU hang evidence needed |
| #56 Amiibo files | Feature: file selection and NFC presentation are not implemented here |
| #59 Pokémon Z-A flicker | PS5: graphics capture and backend comparison needed |
| #60 3D All-Stars / Splatoon loading | PS5: retest existing pipeline fix; #298 is only a possible lead for program handovers, not a fix for both games |
| #61 Age of Calamity 1 FPS | PS5: matched scene and synchronization trace needed |
| #62 Link's Awakening FPS ceiling | PS5: timing and GPU bottleneck comparison needed |
| #63 Final Fantasy demo lined textures | PS5: exact demo build, screenshot and renderer trace needed |
| #64 Tears of the Kingdom freeze / hair | PS5: treat rendering and freeze as separate symptoms |
| #74 Cheat database manager | Feature: source licensing, build-ID matching and download UI remain work |
| #83 Naruto Storm 4 memory crash | PS5: current-version crash report and symbols needed |
| #84 Tomodachi frame generation | PS5: plugin/backend/configuration and real presentation measurements needed |
| #86 Tears of the Kingdom crashes | PS5: exact crash, settings and mod isolation needed |
| #89 Nextendo | Feature: guest networking/account services and external interoperability are not implemented |
| #93 Graphics memory exhaustion | PS5: memory trace at default resolution and failing settings needed |
| #97 Rest-mode freeze | PS5: controller reopen is only a lead from another fork; GPU/resume lifetime unverified |
| #99 Out-of-memory crash | PS5: distinguish host heap, graphics allocation and guest memory before changing limits |
| #107 Just Dance controls | PS5: required controller type/accessory and applet behavior needed |
| #109 Crash after game start | PS5: current build, title/update and logs needed |
| #111 SMB/NFS streaming | Feature: no network-backed VFS, caching or disconnect recovery added |
| #112 Pokémon Z-A city not loading | PS5: title/update, guest state and renderer trace needed |
| #116 Graphics glitches / lag | PS5: isolate visual and performance reports, with reproducible scenes |
| #118 Fighting Force Collection | PS5: #298 may be relevant only if the log shows ExecuteProgram; Vulkan crash not proven fixed |
| #119 Top Racer slowdown / Vulkan crash | PS5: failing service reply and GPU logs; #241 diagnostics alone would not fix it |
| #120 Mario Kart CTGDX | Candidate #272/#276 hooks; mod version, build ID and console retest still required |
| #122 Achilles Survivor | PS5: current title/update crash or loading trace needed |
| #128 In-game cheat toggle | Feature: no runtime cheat-menu/hotkey added |
| #129 The Stretchers | PS5: startup trace and current build required |
| #133 Cover Flow | Feature: no 3D library layout added |
| #148 Monster Hunter Generations Ultimate | PS5: current-version reproduction needed |
| #149 Separate ROM/update folders | Feature: related to #50; independently configurable roots not added |
| #150 Luigi's Mansion 3 | PS5: exact scene and graphics/guest logs needed |
| #157 USB data backup | Feature: no bulk backup/restore implementation added |
| #173 External/custom theme engine | Feature: investigation and existing launcher packs are not full acceptance evidence |
| #195 Metroid Prime 4 | PS5: exact failure and current-version evidence needed |
| #202 Switch Pro Controller | Feature: PS5 driver/input support and hardware tests required; MissionControl is not directly portable |
| #206 Castlevania Belmont's Curse | PS5: startup failure, key generation and firmware requirements needed |
| #219 Cheat GUI / Arceus cheats | Feature: GUI request remains; verify build-ID/file matching separately |
| #222 Title 0100D71004694000 | PS5: current-version game report and logs required |
| #224 Kirby Return to Dream Land Deluxe | PS5: reproduce the specific game problem; no blanket Kirby claim |
| #226 Graphical glitches | PS5: failing scene and backend comparison needed |
| #228 Black occluder shapes, PS5 Pro | PS5: capture failing rendering and Pro memory/output mode |
| #229 Slow games / high CPU after update | Candidate #275/#284; busiest-core HUD is not total host CPU; mandatory A/B before a performance claim |
| #232 Update thinks app still runs | PS5: identify remaining process/sandbox mount; do not bypass the updater's safety check |
| #233 Fixed-scene lost GPU device | PS5: device-fault/submission evidence required |
| #236 Luigi's Mansion 3 floor 10 | PS5: save/scene and GPU/memory trace required |
| #238 Abort 2010-0212 | PS5: identify malformed/failing CMIF reply; no guessed service-success patch |
| #239 Scene themes / Themezer | Feature: large scene/import system remains unimplemented |
| #242 NRO and NSZ | Partial candidate #285: NSZ/XCZ added; NRO library/launcher support is not included |
| #246 Memory write / GPU / heap / app restart | Partial candidate #296: repeated successful write-fault promotions only; remaining three parts unresolved |
| #248 Shared system-font mods | Feature: #288 title scoping does not itself add a shared-font archive loader |
| #260 Kirby and the Forgotten Land regression | Candidate #275: deterministic timing reproduction; same-scene PS5 A/B still needed |
| #262 Astral Chain | PS5: specific current-version reproduction and trace required |
| #263 Wallpapers on USB | Feature: no wallpaper path preference or USB lookup added |
| #264 Empty home after nesting games | Candidate #286: full relative game path retained; host path/history check available |
| #265 Crypt Custodian loading hang | PS5: identify waiting guest thread/service from current logs |
| #268 Post-processing canonical EPERM | Candidate #274/#276: nonthrowing canonicalization and fallback tested with real filesystem errors on host |
| #269 Freeze after PS home menu | PS5: input-handle recovery and actual GPU/app state need reproduction |
| #271 Super Mario Galaxy crash | PS5: #298 is a possible handover lead, not a demonstrated crash fix |
| #273 Tears of the Kingdom textures / UltraCam | PS5: isolate mod, memory pressure and texture invalidation |
| #277 Switch Sports console hang | PS5: stalled GPU submission/fence requires console/driver evidence |
| #278 60 FPS patch slows game | Candidate #275 only for counter-paced timing; mod version and real frame pacing still required |
| #279 Castlevania Belmont's Curse boot | PS5: correlate with #206, retaining independent title/build evidence |
| #280 App not mounted, FW 9.00 | PS5/install: confirm folder layout and ShadowMountPlus logs; no unproven SDK rollback |
| #295 Minecraft / error 21 | Keys/configuration: missing key-area-key generation is not fixed by bypassing decryption errors |
| #297 Recent games / shader rebuilds | Partial candidate #286 for history; repeated shader compilation needs separate cache investigation |
| #300 Project maintenance status | Maintainer communication, not a code defect an integration branch can resolve |
| #301 Metroid Dread write fault | Possible #296 match (guest write, trap 12/error 0x7); requires >30-minute console retest, with mods isolated |

The 52 already-closed issues were inventoried with their original state; they were not reopened
or presented as newly fixed. Related closed reports #270 (PCM overflow) and #281 (game-folder
mods/updates/forwarders) are addressed by the integrated PRs, subject to the same host/console limits.

## Validation record

- Docker Desktop Linux engine, Ubuntu 24.04 image, Clang 18.1.3, GCC 13, CMake 3.31.10.
- Initial configure found missing `autoconf`; added autoconf/automake/libtool to the Dockerfile
  and installed them **inside the container only**. No WSL toolchain installation performed.
- Focused checks passed: ExecuteProgram, nested game counts including NSZ/XCZ, recent paths,
  title-scoped mods, moved forwarders, library JSON export, HUD formatting.
- Fixed two integration-test defects: game-count extraction lacked `game_files.h`; explicitly
  putting `/usr/include` before libstdc++ broke GCC's `include_next` in the library export check.
- Generated update/NCZ composition, PCM queue, timing stop (with the old-code negative control),
  effect paths, NCZ byte-for-byte reads and guest-clock checks passed. Idle polling and the
  spin lock passed ASan/UBSan and ThreadSanitizer in a disposable capability-dropped container
  with `setarch -R` and relaxed seccomp; default Docker restrictions prevented TSan startup.
- Smash-mod staging: 8 unit tests passed. Shell syntax, modified Python syntax and
  `git diff --check` passed. The translation checker reports Swedish at 555 texts with no
  problems (3 existing length warnings); its global result fails because the other 29 catalogs
  still lack two frame-generation strings. Those catalogs are unchanged from the upstream base.
- Native-only checks were also attempted. Checks needing `native-local/` generated PS5 sources
  cannot run against a host-only configure; those failures are **not** native test passes.
- Full host compilation completed (1,512 Ninja steps), including `eden-headless` and the host
  check executables. Guest hook/code-memory fixtures passed in the compiled CPU core; derived
  sparse/multiplayer header checks passed too. This does not compile the PS5-only renderer.
- The first full suite then stopped at its heap stand-in: Linux `mmap` did not guarantee the
  2 MiB alignment promised by the console reservation API. Fixed the stand-in to overreserve
  and trim, without weakening the commit alignment assertion or changing the PS5 heap.
- **Full `tools/build-headless-host.sh` rerun: PASS**, ending 2026-10-27 17:32 UTC. The runner
  reached the final device tests and copied the binary/compile database. Heap (480,000 operations
  on eight threads), idle polling and kernel spin-lock checks passed under ThreadSanitizer.
  Memory-service repeated-write, shared JIT, fastmem, page-table, scalar IR, decoder, RomFS,
  save transfer/sync, mods, shader cache, settings, update and simulated HTTP/FTP checks passed.
  Test inputs intentionally provoking damaged caches, out-of-space downloads and guest faults
  are negative controls, not unchecked failures. Optional real-server RomM was explicitly skipped.
- Local evidence: `.local/upstream-audit/host-suite.log` and `host-results/`. Compiled host
  `eden-headless` SHA-256: `a7f31d373bd5f223e764c54223609e0b5f9e10b307b2fa84a25fa080712772e2`.
  The host binary is in Docker at `/work/ProsperoEden/build/headless-host/eden-headless`, not a
  PS5 ELF or a release ZIP. The replay used the same Linux cache paths from the initial build
  (also mounted from the volume at `/root/.cache`); a fresh documented runner uses `/work/cache`.
- Resident/one-shot Lapy client checks and single-threaded elevation startup passed on host
  with ASan/UBSan. This tests the client state machine, not actual elevation on a PS5.
- PS5 release/package build, firmware/game/GPU testing and real-server RomM tests: **not run**.
  This is not a complete `make test` or `make release` claim.

See [BUILDING.md](BUILDING.md#docker-host-tests-including-windows) for the reproducible Docker
runner. Before promoting this branch, build the native package and
perform the console comparisons requested by #284, #275, #285 and #298 as well as the issue
reproductions above. Do not close composite issues for one corrected symptom.
