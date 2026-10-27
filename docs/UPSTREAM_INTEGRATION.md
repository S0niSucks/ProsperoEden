# Upstream integration audit — 2026-10-27

Source: [Swordpdf/ProsperoEden](https://github.com/Swordpdf/ProsperoEden).
Base: `a26ba78` (1.005.000). Local branch: `integrate/upstream-prs-2026-10-27`.
Target release designated by the user: **1.006.000**, after integrating the complete scope.
Do not publish an intermediate feature batch as that completed release.

**Experimental integration, not a qualified PS5 release.** No issues were closed, no upstream
PRs were merged on GitHub, and nothing was pushed. The full host suite and Docker native
build/package checks passed; the local ZIP is recorded below. Neither result is evidence that
a reported game, GPU, firmware or performance problem is fixed.

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

## Follow-up implementation

Reviewed individual changes from [Oliv3rmoon/Nightshade](https://github.com/Oliv3rmoon/Nightshade),
another public fork by the author of several integrated PRs. The fork was **not** merged wholesale:
no rebranding, version bump, release notes or unrelated driver/heap changes were imported.
Selected commits were cherry-picked with `-x`, retaining their authorship and original IDs:

| Original commits | Work integrated | Remaining qualification |
|---|---|---|
| `b6c568a`, `fa6be98` | Independent `games_folder` for `roms/` and `updates/`; pause downloads if the drive is absent | Configuration-file setting, not an independent folder picker; USB and download behavior on PS5 |
| `74c2365`, `9869653` | Shared-font mods from `mods/<archive ID>/romfs/` or a named mod below it; BFTTF and shared-memory checks | Actual Vietnamese font and game on PS5 |
| `675c0df`, `b3f7d61`, `33e89d7`, `f839bff` | Reopen failed controller handles, retain stale player slots and back off retries | Rest mode, home menu and real controllers on PS5; no GPU-resume fix claimed |
| `cbc26a6` | Decode loader failures and identify missing key generations in the launcher/log | Better diagnostics, not replacement keys or a game-compatibility fix |
| `4494c89` | Missing frame-generation strings in 29 catalogs | Catalog checker passes; existing length warnings and unverified console-font glyphs remain |

Local follow-up work:

- Combined independent game paths with NSZ/XCZ filtering, moved forwarders, nested history,
  loading covers and `library.json`; updated the regression checks rather than dropping them.
- Bound shared-font file allocations **before** reading; a 1 GiB sparse input, truncated files
  and bad headers are rejected in the host check.
- Implemented #263: portable `wallpapers/` in the game files folder, or the console-wide
  `wallpapers_folder` setting. Existing local wallpapers remain the fallback. Host tests cover
  missing drives, invalid paths, filesystem-access denial and profile isolation of the image choice.
- Implemented the NRO library/path portion of #242 using the existing core loader: case-insensitive
  discovery, nested paths, counts, history and moved forwarders. NROs use file-name metadata for
  now; the launcher's setup check still applies. Added a three-cycle real loader/JIT/guest-exit
  fixture check, separate from extension/path tests and from PS5 homebrew compatibility.
- Added the catalog checker to host and native source checks, so missing translations fail builds.
- Added the Docker native-build image and runner, reusing the CI tool versions and a separate
  Linux volume. These do not install a toolchain in WSL or deploy to a console.

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
| #50 Separate game files and emulator data | Implemented `games_folder` setting for ROMs/updates apart from keys/firmware and internal caches/saves; independent picker and PS5 qualification remain |
| #52 Xenoblade Chronicles 3 console freeze | PS5: guest/GPU hang evidence needed |
| #56 Amiibo files | Implemented file-backed NFC factory and in-game selection/presentation; new driver/menu checks in progress, actual PS5 game scan unverified |
| #59 Pokémon Z-A flicker | PS5: graphics capture and backend comparison needed |
| #60 3D All-Stars / Splatoon loading | PS5: retest existing pipeline fix; #298 is only a possible lead for program handovers, not a fix for both games |
| #61 Age of Calamity 1 FPS | PS5: matched scene and synchronization trace needed |
| #62 Link's Awakening FPS ceiling | PS5: timing and GPU bottleneck comparison needed |
| #63 Final Fantasy demo lined textures | PS5: exact demo build, screenshot and renderer trace needed |
| #64 Tears of the Kingdom freeze / hair | PS5: treat rendering and freeze as separate symptoms |
| #74 Cheat database manager | Implemented explicit HTTPS/cache/confirmed-install UI and build matching; integrated regression and console qualification in progress ([record](CHEAT_DATABASE.md)) |
| #83 Naruto Storm 4 memory crash | PS5: current-version crash report and symbols needed |
| #84 Tomodachi frame generation | PS5: plugin/backend/configuration and real presentation measurements needed |
| #86 Tears of the Kingdom crashes | PS5: exact crash, settings and mod isolation needed |
| #89 Nextendo | Feature: guest networking/account services and external interoperability are not implemented |
| #93 Graphics memory exhaustion | PS5: memory trace at default resolution and failing settings needed |
| #97 Rest-mode freeze | Partial candidate: controller reopen/backoff integrated with mock coverage; GPU/resume lifetime and PS5 reproduction remain |
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
| #128 In-game cheat toggle | Implemented session-only quick strip and timing-owner mailbox; parser/VM/concurrency checks in progress, actual PS5 games unverified |
| #129 The Stretchers | PS5: startup trace and current build required |
| #133 Cover Flow | Implemented selectable perspective library view; Grid remains default. Integrated host/controller/render checks passed at 720p/1080p; PS5 qualification remains. See `COVER_FLOW.md` |
| #148 Monster Hunter Generations Ultimate | PS5: current-version reproduction needed |
| #149 Separate ROM/update folders | Implemented `games_folder` in the settings file (ROMs and updates together); independent picker and PS5 USB checks remain |
| #150 Luigi's Mansion 3 | PS5: exact scene and graphics/guest logs needed |
| #157 USB data backup | Feature: no bulk backup/restore implementation added |
| #173 External/custom theme engine | Feature: investigation and existing launcher packs are not full acceptance evidence |
| #195 Metroid Prime 4 | PS5: exact failure and current-version evidence needed |
| #202 Switch Pro Controller | Feature: PS5 driver/input support and hardware tests required; MissionControl is not directly portable |
| #206 Castlevania Belmont's Curse | Improved missing-key diagnostics integrated; PS5 startup/key-generation evidence still required |
| #219 Cheat GUI / Arceus cheats | Launcher switches plus live in-game switches implemented; specific Arceus build/cheat compatibility remains unverified |
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
| #242 NRO and NSZ | NSZ/XCZ and NRO library/launcher paths, bounded embedded metadata, key-free NRO setup and recursive `switch/` import implemented; actual homebrew on PS5 unverified |
| #246 Memory write / GPU / heap / app restart | Partial candidate #296: repeated successful write-fault promotions only; remaining three parts unresolved |
| #248 Shared system-font mods | Implemented shared archive-font override, validation and bounded reads; actual translation/font/game on PS5 still unverified |
| #260 Kirby and the Forgotten Land regression | Candidate #275: deterministic timing reproduction; same-scene PS5 A/B still needed |
| #262 Astral Chain | PS5: specific current-version reproduction and trace required |
| #263 Wallpapers on USB | Implemented portable folder lookup and `wallpapers_folder` setting; host path/profile checks pass, PS5 display unverified |
| #264 Empty home after nesting games | Candidate #286: full relative game path retained; host path/history check available |
| #265 Crypt Custodian loading hang | PS5: identify waiting guest thread/service from current logs |
| #268 Post-processing canonical EPERM | Candidate #274/#276: nonthrowing canonicalization and fallback tested with real filesystem errors on host |
| #269 Freeze after PS home menu | Partial candidate: input-handle recovery integrated; actual GPU/app state and console reproduction remain |
| #271 Super Mario Galaxy crash | PS5: #298 is a possible handover lead, not a demonstrated crash fix |
| #273 Tears of the Kingdom textures / UltraCam | PS5: isolate mod, memory pressure and texture invalidation |
| #277 Switch Sports console hang | PS5: stalled GPU submission/fence requires console/driver evidence |
| #278 60 FPS patch slows game | Candidate #275 only for counter-paced timing; mod version and real frame pacing still required |
| #279 Castlevania Belmont's Curse boot | Improved missing-key diagnostics; correlate with #206 while retaining independent PS5 title/build evidence |
| #280 App not mounted, FW 9.00 | PS5/install: confirm folder layout and ShadowMountPlus logs; no unproven SDK rollback |
| #295 Minecraft / error 21 | Improved missing-key diagnostics; no bypass of decryption errors or claim that required keys were supplied |
| #297 Recent games / shader rebuilds | Partial candidate #286 for history; repeated shader compilation needs separate cache investigation |
| #300 Project maintenance status | Maintainer communication, not a code defect an integration branch can resolve |
| #301 Metroid Dread write fault | Possible #296 match (guest write, trap 12/error 0x7); requires >30-minute console retest, with mods isolated |

The 52 already-closed issues were inventoried with their original state; they were not reopened
or presented as newly fixed. Related closed reports #270 (PCM overflow) and #281 (game-folder
mods/updates/forwarders) are addressed by the integrated PRs, subject to the same host/console limits.

## Initial validation record (see follow-up results below)

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
- At this initial snapshot, the PS5 release/package build, firmware/game/GPU testing and
  real-server RomM tests were **not run**. The subsequent native build result is recorded below;
  console testing and real-server RomM remain unexecuted.

See [BUILDING.md](BUILDING.md#docker-host-tests-including-windows) for the reproducible Docker
runner. The native package has now been built, but before promoting this branch,
perform the console comparisons requested by #284, #275, #285 and #298 as well as the issue
reproductions above. Do not close composite issues for one corrected symptom.

## Follow-up validation — host and native build passed

- The first follow-up full host suite passed, including shared-font anchors, missing-key
  diagnostics and the new controller reopen/backoff fixtures. Evidence is retained in
  `.local/upstream-audit/followup-host-suite.log`. This run predates the local wallpaper,
   bounded-font and NRO additions; the final full-suite run below supersedes it.
- **Final full host suite: PASS**, ending 2026-10-27 18:50 UTC, including all local runtime
  changes through `5cbae36`: bounded-font checks, wallpaper path/profile tests, NRO discovery,
  controller reopen/backoff, ThreadSanitizer heap/idle/spin-lock checks, hooks, memory/JIT,
  updates, mods, saves and simulated HTTP/FTP. The last step successfully ran three real
  NRO loader/JIT/guest-exit cycles. Real-server RomM was explicitly skipped. The newly wired
  catalog check was also run separately and passed for all 30 catalogs.
  Evidence: `.local/upstream-audit/final-host-suite.log`, `devices-host-final/` and
  `nro-host-final/`. Host binary SHA-256:
  `e09c51ee5b236eb09565407ce329fa4eb91570b21f6cc3b7199e55ec2c0df13a`.
  This is a Linux host executable, not the PS5 app.
- Current focused checks pass: portable/explicit wallpaper folders and profile settings;
  bounded font reads (including the sparse 1 GiB negative input); settings/history; nested
  game counts with NSZ/XCZ and NRO; moved NRO forwarders; 555 texts in all 30 catalogs.
  Catalog glyphs requiring unavailable console fonts remain explicitly unchecked.
- `tools/check-headless-nro.sh` passed a real Eden loader/JIT/guest-exit run for three NRO
  fixture cycles, with the receipt checker's negative controls. Evidence:
  Docker volume `prosperoeden-integration-20261027`,
  `/work/ProsperoEden/results/nro-host-20261027-184608.mglAbz`.
  The first smoke runner attempt read the wrong receipt path; it was corrected to extract
  the host stdout receipt while retaining all lines except the one known JIT diagnostic.
- Docker release image construction and toolchain checks passed: LLVM 18/21, Meson 1.10.1,
  CMake 3.31.10 and the pinned SPIR-V translator. A mocked `make` failure confirms that the
  runner preserves exit status 37 and saves the log; this is not a package-build pass.
- First native run: preparation built FFmpeg, reproduced the clean-room runtime hashes and
  built the isolated RADV dependencies. All **1,704 native Ninja steps completed and linked**
  `eden-headless`. Generated timing, sparse/multiplayer-header, audio and failed-load checks
  passed. It then stopped in the old `check-recent-games.py` from that initial snapshot:
  the test still looked for `AssetsPath`, while the independent-folder integration uses
  `GamesPath`. The current-source check was already corrected in `c9cf898` and passes on host.
  Evidence: `.local/upstream-audit/native-first-build.log`.
- **Current-source Docker `make release`: PASS**, using the native build cache and all changes
  through `9ef8012`, including wallpapers, bounded-font reads, NRO and updated regression checks.
  The native executable linked; source/ABI/link checks, Lapy helper verification, loader ELF
  framing and package inventory/fixture/alignment/import checks passed. Checks using host
  backends or mocked native APIs remain host evidence, not execution on PS5.
  Evidence: `.local/upstream-audit/native-final-release.log`.
- Independently compared all **676 tracked files** with the build volume: only the subsequent
  documentation update in `docs/UPSTREAM_INTEGRATION.md` differed. A checksum-based dry-run
  sync also found no extra source files in the reused volume. Verified the ZIP's CRC, unique
  safe paths, exact 74-file inventory, 0777 permissions and byte-for-byte package contents.
  The exported ZIP/checksums/notes match the volume; the exported symbols match the native
  unstripped executable. Evidence: `.local/upstream-audit/native-release-verification.json`.
- Local artifacts (ignored, not published):
  - ZIP: `.local/docker-release/20261027/ProsperoEden-v1.005.000.zip` (**42,714,656 bytes**).
    SHA-256: `08d8b5d3812467584877c8204b2e08c9c7cbc356f9f43f8ae86e5fee3dc32333`.
  - Packaged `eboot.bin` SHA-256:
    `0a97ce5eb33e8ef440ee145f3f79c4650504aa8e4ebe2aea0eab8bdae0711708`.
  - Symbols: `.local/docker-release/20261027/symbols/ProsperoEden-v1.005.000.elf`.
    SHA-256: `8c64d24d9f604c178dc4c7451237f24d61b3b1c792476d302304e8748b02e4fc`.
  - `SHA256SUMS` and the upstream-version `release-notes.md` are beside the ZIP. The version
    number was deliberately not changed; these notes do not describe all integration changes.
    `INTEGRATION-BUILD.md` beside them identifies this as an experimental local build.
- This is a **build-verified integration candidate**, not an official or PS5-qualified release.
  In particular, #284 remains experimental pending the author's requested console A/B tests.
- No console was accessed. No issue was closed or branch pushed. Outstanding features and
  console-only failures in the triage remain work, not implied fixes from a passing build.

## Follow-up implementation, after the user's console smoke test

The user reported that the previous local package worked normally on their PS5. No
firmware/game/scenario matrix was supplied, so this is a smoke-test report, not
qualification of every integrated patch or of #284.

- Implemented **Settings > ROMs and updates**, independent save/reset through the real
  folder browser, service and Lua API. Reset follows Game files; it moves no files.
  Existing settings indices remain unchanged (the category is appended).
- Implemented bounded NRO ASET icon/NACP title/program-ID/language reading and key-free
  homebrew launch, without bypassing encrypted-container setup or storage requirements.
  `roms/switch/` can be copied intact and searched recursively within documented limits.
  Symlink loops/aliases and hidden entries are not imported; containers stay one level deep.
- First follow-up Docker host suite **passed**, including actual NACP metadata bridge,
  storage preferences/browser regression, 557 texts in 30 catalogs, TSan/memory/JIT,
  device/audio/mod/save tests, simulated HTTP/FTP and three real NRO loader/JIT/exit cycles.
  Evidence: shell `sh_17ee2542f0015yvlCWi0XfAqPf`; Docker results
  `devices-host-20261027-194805.Kb7NX6` and `nro-host-20261027-194808.684ON2`.
  Later recursive scanning, bounded runtime ASET loading, GPU guard and identity changes
  require the next run; that earlier pass is not assigned to those later edits.
- Dedicated audit of the supplied 23-file ZIP found **four crashes, one signature**, from
  a 17 October diagnostic binary. #237 is directly relevant but remains **candidate,
  not retested** for these reports. No false symbolization or universal crash-fix claim.
  See [Crash validation](CRASH_VALIDATION.md) for the evidence boundary and retest matrix.
- Added specific generated GPU-table regression/failure controls, null-allocation guard,
  allocation-stage diagnostics and build identity checks. New builds bind logs to the
  exact unstripped ELF; release manifests also bind eboot and source hashes.
- The next full host run passed with the generated runtime NRO loader, icon/NACP/languages,
  GPU refusal controls and build identity. Evidence: shell `sh_17eeb379f001o2nN8XXAmytA7P`,
  device results `devices-host-20261027-195909.IefzdV`, NRO results
  `nro-host-20261027-195912.hm3WCy`. Later quick-menu/NFC/cheat edits are not covered by it.
- The native build/package of that snapshot passed too, including the post-link identity,
  `build-identity.json`, loader framing and import/inventory checks. Evidence: shell
  `sh_17eef0a7d0012GGt5NUO0Bttva`. It is an intermediate build, not the requested final release.
- Launcher preview with ASan/UBSan passed the independent folder category/browser save/reset
  and the NRO/no-firmware/storage/container scenarios. Fixed the category count (14, not 13)
  and the test's parent-dialog exit. Evidence: shell `sh_17ef3cccd001X5V6Ub369z8UBQ`.
- Added isolated post-link identity/LTO/tamper/manifest and source-copy provenance checks;
  both passed. The next host image exposed an accidental FFmpeg X11/libdrm requirement;
  the headless runner now disables X11 discovery explicitly, without host/WSL installs.
- Implementing #56/#128: file-backed NFC registered before controller binding; serialized
  real Eden NFC driver, bounded tag files, an in-game Amiibo/cheat strip and guest-input
  capture. Live cheat changes go through a mailbox consumed by the VM timing owner, not
  direct concurrent mutation of the pinned engine's vector. Tests include the real parser/VM.
- Native readable-file checks use `open(O_NOFOLLOW)`/`fstat`, not denied `lstat`; this also
  corrects recursive NRO discovery on those console mounts without accepting symbolic links.
  Host recursive-count/link/depth checks with forced `lstat` refusal passed.
- The NFC/live-cheat snapshot passed the complete host suite with normal, ASan/UBSan
  and TSan targets: `sh_17f08ca13001g7FkfoQ97N7p68`, devices
  `devices-host-20261027-203437.Vvhp9b`, NRO `nro-host-20261027-203440.QLrC06`.
  Its native package also passed (`sh_17f08ca5c001JT9XcoIgck5WV6`). These precede
  the descriptor-backed NFC read/write derivation and are not evidence for that edit.
- The subsequent descriptor-backed NFC snapshot passed the complete host suite too:
  `sh_17f1256e4001Q753Kpyb1ipAZH`, devices `devices-host-20261027-204427.OvYEQw`,
  NRO `nro-host-20261027-204431.GppizT`. This includes the real driver/factories and
  normal/ASan/UBSan/TSan checks for padding, signed dump normalization and a symlink
  substituted after scan. RomM real-server tests were explicitly skipped.
- The matching native build/package passed (`sh_17f125738001X1hSkPgI0Eqs28`), including
  identity/provenance, ELF framing, import/inventory and package checks. Preserved
  locally in `.local/docker-release/amiibo-io-20261027/`: ZIP SHA-256
  `6b57e43f7b7b84fb914b9a7def6d07e522235635ba712675f75fb5a26b6777a1`, matching
  symbol ELF `3b223a19d6a32d4c3598e32c158c9be03eea9b470244b48b56560540f22483e0`.
  This intermediate package is neither the final requested release nor PS5-qualified.
- Researched the user's Amiibo folder/photo request against Eden, a Ryujinx-derived
  source mirror, TagMo and maintained AmiiboAPI. Added bounded offline catalog/full-ID
  lookup, folder labels and PNG candidate association; no tag/key/artwork download.
  The first host/native compile found a missing private core-header include path;
  corrected it without suppressing the failure. Dedicated host catalog/NFC/cheat checks
  then passed normal/ASan/UBSan/TSan (`sh_17f1e6240001ypDJxmlbccQyCn`).
- A later hardening uses `openat` to reject ancestor symlinks as well as leaf symlinks,
  and discovery enumerates verified directory descriptors. Standalone actual helper
  tests passed normal and ASan/UBSan: interrupted/short/failed reads/writes, 532-byte
  padding, all 32 signature bytes preserved, held descriptor reads, and malformed native
  directory records. The catalog and final directory changes still need the next
  complete host/native run. See [Amiibo catalog study](AMIIBO_CATALOG.md); photos and
  online metadata synchronization are not yet implemented.
- The full offline-catalog/directory snapshot subsequently passed both runners:
  host `sh_17f220842001lSzaqYxEqiVm0R` (devices `devices-host-20261027-210213.hyesjZ`,
  NRO `nro-host-20261027-210216.ssV3ex`) and native `sh_17f2208640013Nx1l2Lv0usy9V`.
  This includes normal/ASan/UBSan/TSan catalog and NFC regressions and normal/ASan/UBSan
  descriptor/directory fault injection. Preserved ZIP SHA-256
  `96575539b7c4eee76e9fc0ea4d9af2365a4a00df5e785c5b1cf0f7efcf4d1647`, symbol ELF
  `73f2849497a91c93ea4892be3e236ed321aad9488795f299ad054d1aa816a916` in
  `.local/docker-release/amiibo-catalog-20261027/`. It predates photo decoding/rendering.
- The user confirmed retaining ProsperoEden's existing visual identity, without TGS
  branding, for the photo extension. Added a selected-item-only PNG decoder and
  immutable text/picture exchange, with OpenGL/Vulkan photo draw paths. The real
  OpenGL HUD method passed Mesa rendering/visibility/orientation/replacement/fallback
  checks at 720p/1080p (`sh_17f2dbb02001mp8njmv25lnR6k`); captures use synthetic colored
  pixels, not third-party artwork. Complete native, PNG sanitizers and Vulkan checks
  are still pending for these later edits; no PS5 photo-renderer claim yet.
- Subsequent photo evidence: native `sh_17f2d250d001D0Jn63FK1OmHh3` passed compilation,
  linking, imports/inventory and packaging. Actual ZIP CRC/modes and correspondence of
  manifest, source snapshot, ELF identity, symbols and eboot were verified, then saved in
  `.local/docker-release/amiibo-photos-20261027/`: ZIP
  `c1b53d6d3abd81e56d40f6a277cd3fa16d6f2caf31d4ddc81a375a41c7c5c48f`, symbol ELF
  `b351ea34b704ef5e179d808f52005a496bc501328399c32cf67aa3c5bd31d1c9`, eboot
  `d4589aab987773caa07cf1a3a13ab03798d7594b8722918095975f772dc6a46d`.
  Vulkan draw/shaders passed Mesa at both sizes with zero validation errors, including
  the producer-normalized text follow-up (`sh_17f313d43001mHu3W5p0geaRQ4`). Updated PNG
  tests with decoder-allocation refusal and exception fallback passed normal/ASan/UBSan/
  TSan (`sh_17f38f241001cEbhVy0oYtjEgN`). The native artifact predates that exception
  fallback and the PC helper, and remains unqualified on the PS5.
- Added an opt-in PC cache-preparation helper, `tools/sync-amiibo.py`: configurable HTTPS
  catalog/image sources, explicit public IDs only, bounded metadata/PNG downloads,
  cache reuse and validated atomic file replacement. Synthetic offline checks pass,
  including redirect/body/deadline controls, malformed compressed images and failed-batch
  preservation. It does not read user tags, and is not an in-launcher PS5 sync feature.
- Full photo host suite `sh_17f2d250d002SEdc5zUn7mQ4SY` passed all checks, including NFC,
  decoded photo selection, live-cheat mailbox and PNG sanitizers. Device evidence:
  `devices-host-20261027-211827.JXusIb`; real-core NRO cycles:
  `nro-host-20261027-211831.fqfGQv`. RomM real-server checks were explicitly skipped.
  This run predates the PC helper and the later C++ allocation-exception fallback,
  whose separate normal/ASan/UBSan/TSan decoder check is recorded above.
- The hardened-photo snapshot passed native runner `sh_17f3a276a001VLJdXcXaFj3OLR`.
  It includes the C++ allocation-exception fallback but predates the latest PC-helper edits
  and cheat-database work. ZIP/ELF/eboot/manifest/source correspondence, CRC and 0777 ZIP
  modes passed and were preserved in `.local/docker-release/amiibo-photos-hardened-20261027/`:
  ZIP `1e482fa99694e2076c1fc57a5e754d090f6b621443be62ddd4afce77049eb392`,
  symbols `95991452635d3db18ce7d8c73dfb8cc0ca32d3b8675a257a32b22cc9108beff3`,
  eboot `25373693cdbf5ab242ecf9c564cf4aec38570af8aabf3a7cd4e889369b35b155`,
  build identity `bc7ef855c1395c46da7425063b957b186df2519f9a7ff6069c2401bfa3b51f44`.
- The later complete host suite `sh_17f3b288e0012Qb6LYPnOiUD5s` passed, including the
  current PC-helper checks, photo decoder and NFC/catalog/mailbox sanitizers. Evidence:
  `devices-host-20261027-213103.LZU6ox`, `nro-host-20261027-213107.H38JpV` and
  `guest-hooks-euwlzkbp`, copied before reuse to
  `.local/docker-release/amiibo-photos-hardened-host-20261027/` with tested source hashes.
  Real RomM servers were explicitly skipped. This run predates cheat-catalog ingestion.
- Started #74's bounded TitleDB ingestion, checked against the inspected pinned database
  (15 matching entries for one exact title/build; no codes installed). Synthetic parsing
  passed normal/ASan/UBSan (`sh_17f41e93f001Ntx5gri8965xSL`, extended cases
  `sh_17f45e8b60013MAEktw2fay2Fs`). Later switch-key-alias and zero-prefix checks,
  real-core ingestion and the new shared effective-ExeFS lookup require their own runs.
  See [cheat manager record](CHEAT_DATABASE.md); this is not a completed downloader.
- #74's effective-main identity and generated-boot shared resolver passed real-core checks
  `sh_17f4f5370001x4mnyQisFf6M3c`: six update insertion orders with synthetic plaintext NCAs,
  replacement/disabled main, retained original program ID and malformed NPDM/main refusal.
  Catalog text also passed the actual parser/VM normal/ASan/UBSan/TSan targets. This is not
  qualification of encrypted commercial containers or a PS5 game/update/cheat combination.
- The explicit cheat transport passed synthetic normal/ASan/UBSan and real loopback
  libcurl/TLS cases (`sh_17f54e811001iet0Dz6g6ayVGT`). The bounded cache/no-clobber managed
  installer and default-off masters/single-code policy passed normal/ASan/UBSan/TSan
  (`sh_17f580819001epOwoS3KVz6aqD`). Later cache-depth/checksum edits require a rerun.
  Backend operations are not yet launcher actions; #74 is still in progress.
- Later cache-depth/checksum hardening, managed-name recognition and manual-mod preservation
  passed standalone normal/ASan/UBSan/TSan `sh_17f5bf08d001XGdLYq3FE74We0`. Complete host
  and native backend snapshots were started as `sh_17f59d6880010hKq69QlDJXEcj` and
  `sh_17f59d69e001GH1cQjpsW0141i`; their results are pending. Both precede the subsequent
  early-NPDM-reader guard, extracted-NSP fix, configurable probe paths and expanded
  NSP/CNMT/external-update identity fixtures; none of those later edits is qualified by them.
- The combined libcurl/TLS -> catalog -> offline cache -> default-off install flow passed
  `sh_17f61016b001wH2i0jsEKRO9JW`, with invented program/build/code only. It still does
  not establish a launcher action or encrypted-game/PS5 compatibility.
- Backend native snapshot `sh_17f59d69e001GH1cQjpsW0141i` completed packaging/import checks;
  ZIP, symbols, manifest, source snapshot and hashes are preserved under
  `.local/docker-release/cheat-backend-before-npdm-20261027/`. It remains a v1.005.000
  intermediate build, not the requested final v1.006.000, and precedes the new manager wiring.
- Host `sh_17f59d6880010hKq69QlDJXEcj` printed successful checks through final device/NRO
  cases but exited 1 in the bind-mounted runner (`y: command not found`) after that file was
  edited during execution. Do **not** record it as a successful overall suite. Evidence for
  device/NRO/guest hooks and tested source hashes was preserved under
  `.local/docker-release/cheat-backend-host-runner-error-20261027/`. The rerun uses a fixed
  private runner copy, rather than editing the executing mounted script.
- Isolated actual-core link `sh_17f6746d1001IAphtUmQ2rtAJ2` passed expanded synthetic
  plaintext NSP/CNMT, external updates, extracted NSP, state restoration and early-NPDM
  refusal. That execution preceded the later allocation-limit negative control.
- `CheatManager` normal/ASan/UBSan/TSan passed `sh_17f704d5c001Up4anXAOKPhLTJ`.
  The launcher now wires explicit check/cache/confirmed-install, status/progress/cancel,
  default-off selection and a metadata/launch lifetime barrier. UI and actual service/native
  compilation are pending their next runs; #74 is not console-qualified.
- Allocation-limit negative control and expanded plaintext probes passed isolated actual-core
  link `sh_17f7317db00114Vy96zy3tBrm5` (guarded 2 MiB synthetic capability vector is refused;
  original reader does not satisfy the bounded contract). The preceding attempt
  `sh_17f71b044001u9UFrSoNnxz5Dc` failed to compile that test-only control due to a missing
  loader enum include; it was fixed without changing the guard.
- Native `sh_17f713ac4001Ir3E84MJfOrq77` compiled/linked the early NPDM guard and actual
  manager/service/built-in/Lua UI, but packaging stopped at 19 missing strings per catalog.
  All 30 catalogs now contain those translations, and the normal string check passes
  (576 texts, no problems; existing layout warnings and unqualified console-font glyphs remain).
  New fixed-snapshot host/native reruns are `sh_17f76fd180017gleRpy2dMvTFd` and
  `sh_17f76fd44001d1gWDxi93hcd4V`. Their results are pending, and they precede the later
  optional `cheats.catalog.v1` capability declaration and preview-only completion command.
- UI `sh_17f7a0a7d001qjYujv5XPvxPKQ` passed 24 controller/API checks and the existing
  23 runtime-workflow checks, real Mesa at 720p/1080p under address/undefined sanitizers
  (Mesa process-global leaks excluded). Evidence/source snapshot preserved in
  `.local/docker-release/cheat-ui-actions-20261027/`. Subsequent named-preview/other-build
  rows need their own rerun, as do the latest service rechecks/profile locks.
- `sh_17f7ba158001446LHYgucJfgG3` passed the actual worker manager + libcurl TLS +
  parser + explicit cache + default-off installation together, including all transport
  negative cases. No commercial codes, remote services, console filesystem or gameplay tested.
- Latest #74 additions retain optional cheat names before install and up to 32 informational
  other-build/version-integer labels on a mismatch, never code from those other builds.
  The request now derives its local folder from the **selected file**, not a title-ID map
  that can conflate two installed copies. Two native-VFS local-copy fixtures cover their
  separate updates/mods; their run is pending. This remains part of the full #74 delivery,
  not a declaration of complete v1.006.000 integration.

- Named-preview and other-build UI rerun `sh_17f828cf6001LjtWI29yRw9r5U` passed **32**
  controller/API assertions in 720p and 1080p, plus 23 existing workflow regressions.
  Actual renderer, simulated backend; evidence retained in
  `.local/docker-release/cheat-ui-named-builds-20261027/` and captures inspected.
  Extended parser and manager checks passed too (normal/address/undefined, plus thread
  for the manager): `sh_17f828c05001rDl24ojpeBw9g2`, `sh_17f828c94001i4IcFpoXclPHJg`.
- Translated native incremental build `sh_17f76fd44001d1gWDxi93hcd4V` packaged successfully;
  archive/symbols/manifest/source/logs retained in
  `.local/docker-release/cheat-launcher-native-translated-20261027/`, ZIP SHA-256
  `75a9da0a3f156f53c3f5e6242a5293b3d3ad3232a52501da8c4e443406d4082b`.
  Its copied source snapshot does **not** prove all incremental objects were rebuilt: host
  `sh_17f76fd180017gleRpy2dMvTFd` exposed an old probe object with the new source, stopping
  at link. To avoid concurrent volume jobs, all three Docker runners now reject a busy lock
  (three forced-lock negative tests passed). New recovery runs explicitly invalidated
  port-dependent objects before rebuilding: host `sh_17f8ee1c4001e29O13pI2US2mN` and native
  `sh_17f8ee1ec001Zf9nzTqqubihCm`. The native recovery subsequently passed all 738
  build steps and source/link/import/inventory/package checks. Preserved in
  `.local/docker-release/cheat-fresh-objects-native-20261027/`: ZIP SHA-256
  `dac5e83ec81289a129d148530d28f863b79ff5ce6dd2fc3fc3b7cec7cbcd63d7`, matching
  symbol ELF `5df516231a42f8688b824988fbf7db61945d1910314085880b655575c5e6cb62`, eboot
  `d4fdd9bf514cb1acce8ff3e371edf1d8d94be6233ed22b78a9e1064bd89b8fa9`.
  The host recovery compiled/linked all 648 steps, including the new probe overload, but its
  suite stopped at the existing guest-idle timing assertion `IdlePollBudget(2) == 0`.
  The standalone ASan/UBSan/TSan idle/wakeup/lock run passed
  (`sh_17fadd14e001azk7cH1JRTYcNZ`); interference is suspected, not proven. No runtime
  idle policy was weakened. Host binaries, source snapshot, compile database and preceding
  evidence are in `.local/docker-release/cheat-fresh-objects-host-20261027/`; remaining
  saved-snapshot tests are being run without recompiling the core.
  This does not include later encrypted fixtures or any
  team implementation, is not PS5-qualified, and retains the intermediate v1.005.000 version.
- Per-copy local-update selection passed the isolated probe until its local-mod assertion.
  The host still used the upstream BIS resolver; CMake now derives the same game-folder
  resolver on host and console, keeping only the firmware-root extension native-specific.
  Recovery host tests include those cases. Independent OpenSSL encrypted header/section/
  CNMT/update fixtures with invented keys were added **after** that snapshot; their run
  and BKTR qualification remain pending.

Still active: verify the newest edits on host and native Docker builds and the real
launcher preview; continue the outstanding feature implementation. No new version or
release is declared complete yet, and no remote publishing/issue closure was performed.
