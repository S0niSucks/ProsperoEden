# Reference projects pinned (2026-11-10)

Pinned by the coordinator from public repositories shared by the owner.
These define the proven path for the remaining fronts.

## Eglantine (upstream Switch 2 core)

- https://github.com/Eglantine-Community/Eglantine (branch `nx2`, 5.8k stars)
- FOSS Switch 2 emulator derived from Eden/Yuzu/Sudachi (GPLv3).
- Runs Mario Kart World at decent/full speed; Splatoon Raiders variable.
- Source layout: src/, externals/, CMakeModules/, cpmfile.json.

## EdGlantine-PS5 (Switch 2 emulation ON PS5 — the proof)

- https://github.com/Portablelle/EdGlantine-PS5 (release 00.069.000, PPSA99862)
- "Runs the Eden NX2 engine natively on the PS5. Built on ProsperoEden."
- Current state (Nov/2026, console-reported): Pokopia PLAYABLE 45-50 FPS;
  Bananza 5-20 FPS; Welcome Tour ~10 FPS; OoT menu; MKW stops at first
  load (graphics memory, issue #20).
- Architecture: ProsperoEden base + Eden NX2 core pinned 67bada77 +
  patch layers (eden/tools/eden-patches/*.diff, applied in order,
  refresh-core-patch.py re-extracts on any layer change) + deps.json
  + remote Linux build server (build-remote.sh / package-remote.sh).
- Key tech: native shader translator (Maxwell->GNM lowering), Mihawk
  RADV Vulkan, macro JIT (NX2 MME), hardware H.264/VP9 via
  libSceVideodec2, dav1d AV1, NUP packages, six guest CPU cores.
- Known bottleneck: CPU draw preparation (8.65s in a 20s capture);
  GPU split front/back threads experimental (layers 99zzzt1/t2).

## EglantinePS5 (sister port, feature reference)

- https://github.com/heydemoura/EglantinePS5 (v1.000.000, 493 commits)
- Eden port for PS5 homebrew; fork lineage of ProsperoEden.
- Features WE can reference: local-wireless multiplayer via Eden rooms
  (docs/MULTIPLAYER.md + MULTIPLAYER_IMPLEMENTATION.md), save sync to
  RomM servers with QR pairing, download sources (RomM) via console FTP
  loopback, per-game controller type, self-update via payload loader,
  attested release ZIPs, crash reports.
- Game files layout: save-import/, ryujinx/ import, save-export/,
  mods/<title ID>/{exefs,romfs,cheats}, updates/, .remote-downloads/.

## Implications for ProsperoEden-Encore

1. Native link front: our repo ALREADY has the full build machinery
   (tools/deps.json pins Eden 5f142c79; vendor/sources; encore-active
   patch layers: eden-core/gpu/shader/vulkan/surface-lifecycle/sync/
   threading/backports/build). The executable comes from OUR build
   system (make deps + layers), not hand-assembled objects.
2. Switch 2 on PS5: proven feasible by EdGlantine-PS5 on this exact
   foundation. Path = add NX2 patch layers (eden-nx2 core pinned,
   native shader translator, NX2 services) following their recipe.
   Watchline criteria U1-U5 are MET by this public evidence.
3. Save data: EglantinePS5 documents ryujinx/ import + save-import/
   + save-export/ layouts that match our pinned c-saves spec (F-A/F-B).
4. Multiplayer: Eden rooms local-wireless documented in the sister
   port; reference for our 4-player front evolution.
