# Goldfish returned findings — normalized transcription

Target: target.md, full file 1–198, SHA-256
58a8ee8adeed9c00d459a34350f9769a9b1df3e29718bd4a52fb36d980c6ddf4.
All three requested reviewers completed. This transcription retains findings,
severities, source scopes and receipt limits; formatting is parent-normalized.
The actual completed child responses were delivered in the session.

## egm-codebase-details

Session: ses_e6f71fdbcfferzMe04x2Sn4yF6. Completed, strict read-only.
Counts: 3 Blocker, 1 Major. Execution/independent tests: NOT_RUN.

1. **Blocker CD-01** — Target 86–88. Explicit developer launcher intent loses
   precedence. dev_launch_policy.h:8–19 and main.cpp:774–778,801–813,855–892:
   launcher=first/autoboot=off force the launcher only for developer autoboot;
   a valid forwarded request remains pending and launches at 880–882. Only a
   positive developer autoboot selection takes precedence. Specify and honor
   forced-launcher precedence; test both tokens with forwarded requests.
2. **Blocker CD-02** — Target 49–50,176. Claimed EGM synchronization retains
   the fixed DTO defect. headless/prosperoeden/EGM.md:21 says FSR/AA are not
   copied; eden_services.cpp:1974–2002 copies both at 1984–1985. Replace obsolete
   limitation with actual round-trip behavior and host-only evidence boundary.
3. **Blocker CD-03** — Target 101–105,176. Library documentation defines the
   wrong identity base. docs/LIBRARY_INTERCHANGE.md:24–26 says file is relative
   to game_files/roms; eden_services.cpp:1605,1633–1641,1715–1724 and
   library_file.h:27–40 use GamesDir and export games_folder. Consumers can
   resolve the wrong directory. Document games_folder, legacy fallback, file/path
   and both roots correctly.
4. **Major CD-04** — Target 173–174,188–193. Preview-provider differences are
   insufficiently documented. fake_services.hpp:119–139 inherits location_folder
   that never activates saved independent roots; wallpaper_image ignores
   content_io_available=false. preview_main.cpp:204–222 takes refusal screenshot
   without asserting refusal/unchanged state. These are mock limitations beyond
   hardware/font/art limits. State exact semantics and assert saved-versus-active
   roots and unchanged state on refusal.

Mandatory full reads: ancestor AGENTS 1–9 first; persona codebase_details.md1–69;
implementation-readiness-standard.md1–34; target1–198; manifest1–152. Manifest
SHA string matched supplied value; no independent byte hash computation.
Serena initial instructions/config: no active project, no activation/config.
Source read scope: relevant integration/predicates/persistence/gate/palette/
dispatch/mock infrastructure bodies, not every file; simple unit bodies excluded.
No live checkout, vendor, history/memory, console/network, writes or nested agents.
External 1,526-file baseline and historical passes not independently verified.

## egm-implementation-readiness

Session: ses_e6f71fdb5ffeR9HfbbnN3mOgZu. Completed; standard not met.
Counts: 4 Major, 1 Nit; no Blocker. Execution: NOT_RUN.

1. **Major IR-01** — Target47–57,155. Malformed appearance objects bypass
   rollback. settings_store.h:351–358,413–418; launcher.cpp:239–264;
   settings.cpp:625–636. appearance=[] or non-null scalar loads defaults but
   later string-key indexing throws, rather than returning false to rollback.
   Static counterexample, not executed. Specify rejection policy preserving
   disk bytes and local look; test scalar/array containers, not just leaf values.
2. **Major IR-02** — Target32–178,180–198; standard20–23. No ordered stable
   implementation checkpoints, per-phase file sets/prerequisites/invariants/
   acceptance criteria. Fresh implementer must choose how coupled DTO/provider/
   settings/UI edits remain buildable. Define ordered checkpoints and host
   assertions, with final native gate disabled; native qualification may remain
   a separate gate.
3. **Major IR-03** — Target113–128,180–193. Application acceptance assertions
   unspecified. check-content-roots.py:62–98 lacks 4,096/128 discovery boundaries
   and total pixels; reopen33–37 checks GamesDir and empty image, not independent
   state. preview_main.cpp:217–222 refusal is only a picture. Specify boundary,
   reset-preservation, refusal-state and reopen assertions separate from native
   qualification.
4. **Major IR-04** — Target104–105,176. Library compatibility contradicts
   independent roots: library_file.h:27–40, provider1718–1724 versus library doc
   4–5,24–26. Explain additive games_folder and legacy field absence, behavior
   for readers ignoring additions, and distinguish interchange from new launch
   handling. Schema version1 alone does not resolve wrong identity reconstruction.
5. **Nit IR-05** — Target49–50,176. Launcher EGM21 still reports FSR/AA omission,
   whereas provider1984–1985 copies both. Update contract and name exact path.

Mandatory full reads: ancestor AGENTS1–9 first; persona1–62; standard1–34;
target1–198; manifest1–152. Independent hash matched target and all146 listed
source hashes; hash verification is not full semantic reading.
Full source reads: appearance/forwarder/user-content/assets/storage/library
headers; three new checks; browse.cpp; all five feature/interchange docs and
four EGM contracts. Partial: settings1–465,675–752; provider1590–1737,1932–2254,
2330–2390; main731–938,2450–2576; launcher45–114,220–299,380–632;
library1–265,1200–1394,1590–1735; services1–260,300–406; preview50–234;
fake header1–185. CMake/fake implementation targeted searches only.
Serena unavailable without setup; fallback used. No external corpus/transcript/
memory/network/live source/vendor/nested agent. Historical passes not reproduced.

## egm-design

Session: ses_e6f71fdfeffeNAtfY1lEQ2Bg7V. Completed read-only.
Counts: 2 Major, 1 Nit. Execution and independent hash computation: NOT_RUN.

1. **Major DE-01** — Target36–38,59–62,169–170. Switch palettes make advanced
   selections nearly unreadable: apply_switch_palette218–225 sets kInk to
   near-white theme::kText(0xf4f2f8), but advanced_row159–170 retains focused
   background0xe7faff and draws labels/values in kInk. Game settings682–686,
   mods707–712 and mapping739–745 call it. Use coordinated selected-surface/text
   roles preserving Original; cover focused game/mod/mapping rows in all palettes.
2. **Major DE-02** — Target107–119. Wallpaper admission performs full-file I/O
   on UI/input owner: browse.cpp120–130 calls provider.set_wallpaper2230–2235,
   which reads up to16MiB via UserContent104–121 without cancellation/timebound.
   browse79–95/provider2226–2228/UserContent76–94 also synchronously discovers
   up to4,096 entries. Async textures326–363 does not protect earlier calls.
   Work-volume limits are not I/O-latency bounds. Move discovery/full admission
   into cancellable ownership, apply results before commit, keep checks/limits,
   document latency and exercise slow/failing media interactions. This is static
   risk, not a measured native stall.
3. **Nit DE-03** — Target99–107. About guidance browse.cpp536–550 constructs
   games path from files_folder()+/roms instead of location_folder(games),
   potentially directing players to a directory no longer scanned. Distinguish
   the games directory from keys/firmware/mods root.

Mandatory full reads: ancestor AGENTS1–9 first; persona1–63; standard1–36;
target1–198; manifest1–152. SHA string matched; not independently recomputed.
Full: appearance/forwarder/user-content/assets/storage/game/library headers;
theme.hpp,browse.cpp,APPEARANCE/CONTENT_LOCATIONS/FORWARDER docs, three checks.
Partial main733–1042,1118–1257,1983–2062,2168–2262,2420–2580;
settings99–419,650–752; provider1115–1297,1369–1513,1591–1745,1868–2055,
2144–2243,2346–2390; launcher68–467,592–798,1191–1259;
library1–700,743–842,1123–1413,1600–1772; settingsUI433–637;
switch1–205,213–296,570–728,730–881; widgets45–139,282–421;
services315–358;textures264–369. Discovery searches also scoped to snapshot.
No live source/vendor/transcripts/memory, files written or nested agents.

## Consolidation and resolution

12 original findings: 3 Blocker, 7 Major, 2 Nit. Consolidate CD-02/IR-05 and
CD-03/IR-04 as duplicate actions WITHOUT changing original severities. This
leaves10 actions (highest action severity:3 Blocker,6 Major,1 Nit). Every action
is PROPOSE FIX, accepted under owner AUTH-02 delegated resolution. No contest,
escalation or accepted-risk settlement. Author fixes are not fresh closure.

Round cleanliness is **false before repairs**. Consecutive clean rounds0;
converged=false. A fresh immutable target/source round is required after repairs
and their focused checks. This is only the bounded increment, not the parent
twelve-group design's convergence or complete feature readiness.
