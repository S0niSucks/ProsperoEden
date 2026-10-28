# Composed Amiibo / USB launcher UI validation

## Exact scope and source

This is **UI-only, fake-service evidence**, not a real USB writer, Amiibo worker, NFC, PS5,
hardware, filesystem, job-drain or service-lifecycle qualification. The coordinator owns the
real admission/USB stop-and-join composition and its actual service tests. Nothing here edits
production services, controllers, rendering, backend code, locale catalogs or release metadata.

Branch `team/amiibo-usb-ui-1006000` was created in the assigned exclusive worktree from exactly
`8e8bdd6392c2c4e1cb33fb23b29524d550d180da`. The immutable test source is:

- Test commit: `9e75ea3f6088de193bee1a67c83d370d274e074a`.
- Git tree: `c48242383d2c88309c1830085b19e5938c7fc612`.
- Tracked source (`git archive --format=tar`) SHA-256:
  `6cf5d32a38a19ad0b16bd796266e75d2fa0a5c0bc400b27ebc9e4170f7aead87`.
- Complete private build snapshot, adding the 502 retained renderer-input files and their
  manifest, SHA-256: `b1012fbb390f0d7b543a766e9d19dfcdd678e9cd9fc8755259f3d9dbd9287720`.
- Dependency manifest SHA-256:
  `d368a14bf3725cfd017de4192d98bc57ce15c05625d5306f9175da2bc0ee5500`.
- Existing image `prosperoeden-host:features`:
  `sha256:e6137ec556fe8e63ebf25e7fabe36e573d089c469d0e9d391c3072f2ae04d551`.

All feature tests run from this **one archived source**, not the changing coordinator checkout.
The helper checks its archive hash, creates a fresh private source/build directory, builds one
Mesa runtime-preview binary, and records its SHA-256 in every feature report. The test-only
`prepare-ui-snapshot.py` adds existing HarfBuzz/UI-kit inputs from this workstream's retained
`/work/source/.deps` directory. Every UI-kit file is checked against its archived SHA-256 pin;
HarfBuzz's retained version is checked as 12.3.2 and **all** input file hashes are recorded.
The original HarfBuzz download archive is absent, so its published archive pin is **not**
reverified during this preparation. `.ui-test-dependencies.json` inside the complete snapshot
records that limit and the exact retained inputs. No installation/download, full emulator
build or native console build is needed. The Cover Flow
driver links the same compiled renderer/runtime/fake-service objects and Mesa libraries; it
does not rebuild the launcher. Later coordinator service changes are not silently included.

## Adaptations and checks

- Both feature scripts use a common **test-only** source-bound Settings navigator. It reads
  the category labels and grid-column count from this snapshot, then sends ordinary controller
  keys from fresh builtin Home. In the composed 16-category layout, USB is index 14 (bottom
  third) and Amiibo is index 15 (bottom fourth). The opened modal's state is still asserted;
  no test hook directly opens a modal or changes the production layout.
- Existing USB **20** checks and Amiibo **26** assertions are preserved, including 720p/1080p,
  Lua's separate physical confirmation, no startup Amiibo request, offline browsing, cancellation,
  estimate/Cancel default, restore warning and restart lock. Native-size Settings captures show
  both adjacent tiles before opening either workflow.
- The extra cross-feature script exercises Lua startup refusals, unsupported USB action names
  with forged confirmation, native Amiibo confirmation owning input/draw, blocked builtin
  replacement while consent is pending, host-consumed physical Cross, resumed Lua's inability
  to create a USB session, cancellation and builtin return. A synthetic USB estimate/copy then
  blocks picker/launcher switching, updater/navigation inputs and launch completion. Navigation
  attempts must not change the default Cancel choice or create extra synthetic requests/copies.
  A direct preview `picker` probe can queue a hidden picker during Amiibo consent; it cannot
  replace the consent view or receive input until consent closes. The test then closes that
  picker and cancels the synthetic Amiibo request before returning to builtin.
- Existing focused Cover Flow rendering/controller and Lua API checks run against this same
  composed UI. Its projected-card, selection, save-failure, empty/large library, accessibility,
  theme and launch assertions remain its own fake-service regression scope.

Command transcripts, stderr, source metadata, compiler identity, linked-library listing and
compiled-object hashes are retained alongside captures. Renderer C++ uses the established
ASan/UBSan preview recipe, UBSan halts on errors, and Mesa process-global leak accounting is
disabled. Every preview command must return `GL=0`. A final `run.json` is written only after all
tests pass and indexes the hashes of the reports/logs/captures/source archive.

## Reproduce in the private volume

The snapshot input and export folders live under this assigned worktree's ignored
`.local/amiibo-usb-ui-20261027-9e75ea3/`. The runner accepts archive, provenance JSON, private
volume root and export directory. For the exported snapshot:

```powershell
$evidence = 'D:/leonardommello/PS5/ProsperoEden/.local/agent-worktrees/amiibo-sync/.local/amiibo-usb-ui-20261027-9e75ea3'
docker run --rm --name prosperoeden-team-amiibo-usb-ui-9e75ea3 --cpus=2 --memory=3g --pids-limit=256 `
  --network=none --cap-drop=ALL `
  --mount "type=bind,source=$evidence/input/run.sh,target=/runner.sh,readonly" `
  --mount "type=bind,source=$evidence/seeded-input,target=/input,readonly" `
  --mount "type=bind,source=$evidence/final,target=/export" `
  --mount type=volume,source=prosperoeden-team-amiibo-sync-20261027,target=/work `
  --entrypoint bash prosperoeden-host:features /runner.sh /input/source.tar /input/source.json /work /export
```

Two attempts stopped before compilation when the disconnected container tried to obtain
HarfBuzz: the default download cache and an explicitly selected private download cache were
both empty. Their exports are preserved separately in `export/` and `validated/`; neither is
counted as validation. The retained **extracted** inputs were then packaged offline with
`prepare-ui-snapshot.py <git-source.tar> <source.json> /work/source <fresh-output-directory>`.
That operation preserves the committed test/production source verbatim and records both code
archive and complete build-snapshot hashes. Only the complete snapshot under `seeded-input/`
is used for the actual combined build, with output exported separately to `final/`.

## Run and visual inspection results

**PASS**, in one complete snapshot/build under `/work/amiibo-usb-ui.68YUIV/`, with Docker
limited to 2 CPUs / 3 GiB, no network, all capabilities dropped, and no extra agents:

| Focused check | Result |
| --- | --- |
| Existing USB UI, 720p / 1080p | 20 checks passed |
| Existing Amiibo builtin 720p / 1080p and Lua | 26 assertions passed |
| Combined Amiibo / USB ownership, 720p / 1080p | 32 assertions passed |
| Cover Flow controller / Mesa driver, 720p / 1080p | Both runs passed; all recorded frames `GL=0` |
| Cover Flow Lua API / translation discovery | Passed; no locale catalog writes |

Compiler: Ubuntu Clang 18.1.3. No ASan/UBSan diagnostic occurred. The existing stb initializer
warnings and miniz large-file warning remain visible in the build logs; they were not hidden
or fixed as part of this test-only task.

- Single runtime-preview SHA-256:
  `e97740895d4c2e9b40c5e82d67e5a47a7ea7f8cdc4da665cb9555b196dbe234b`.
- Cover Flow test-driver SHA-256:
  `c954f002298da7f71da9a6bd2d4021244a62bf467b4d7062bcbe26dca32baaf1`.
- Final `run.json` SHA-256:
  `46ad2e75c2f90847518e6a700ab62f1b1d3f03bf5754a06655c443a3b30065c9`.

Exports are in `.local/amiibo-usb-ui-20261027-9e75ea3/final/`: **63 original PNG captures**,
feature `report.json` files, command/stderr/build/Cover Flow logs, source archive/provenance,
linked-library listing, object hashes and `run.json`. All **178** indexed artifact hashes were
checked again after export. The full container output is retained in the evidence folder's
`container.log`. The private volume retains the source, compiled binaries/objects and results.

**Actual visual review:** 22 representative original captures were opened, covering both
native sizes rather than accepting the JSON reports alone. Specific observations:

- `usb/usb-{720,1080}-settings-combined.png` and
  `amiibo/builtin-720-settings-combined.png`: all 16 tiles fit; USB and Amiibo are distinct
  bottom-third/bottom-fourth actions and the focus ring lands on the intended tile.
- Amiibo builtin catalog/photo confirmation and `amiibo/lua-native-confirmation.png` plus
  `ownership/ownership-1080-amiibo-host-owned.png`: source/privacy/local-data disclosures and
  separate physical Confirm download / Back hints are legible and not clipped. The host view
  remains the only visible owner after navigation probes.
- Amiibo progress/failure and `ownership/ownership-1080-amiibo-after-usb.png`: cancellation,
  offline fallback and the separate post-USB Amiibo list remain readable.
- `ownership/ownership-720-usb-confirm-{before,after}-navigation.png`: both show the USB
  estimate in front with **Cancel still highlighted**. Restore, low-space and restart captures
  retain their warnings and controls within the modal.
- `ownership/ownership-1080-usb-copy-gate.png`: the copy view retains its Cancel control;
  its saturated progress bar comes from the synthetic slow-mode fixture, not a real-copy
  throughput/completion claim. `ownership/ownership-720-builtin-return.png` shows clean Home
  after closing the prior consent/picker/request, without a leftover overlay.
- `cover-flow/{720p,1080p}/cover-flow.png` and the missing-cover/large-text and 5,000-game
  captures retain card focus, perspective, details and footer. The long detail title is
  deliberately ellipsized rather than overflowing.

No new composition/layout blocker was observed in those captures. The evidence folder's
`visual-inspection.json` lists the exact inspected images, hashes and observations (SHA-256
`6e7862a10c9081bc65ada89c84bf14a419df9888ceedd3a6722edee33f936974`). This is a bounded
representative inspection, not a claim of exhaustive visual/accessibility qualification.

The follow-up commit adds only this evidence record and the offline preparation helper that
was executed to package the retained inputs; the tested feature scripts remain exactly those
in `9e75ea3`. No coordinator-root, production API/service/UI/backend or locale file was edited.
Real job admission, writer shutdown/drain and hardware acceptance remain coordinator-owned.

Coordinator review imported the two commits as `88c4c06` / `62e99ba`. The exported `run.json`
and indexed file hashes were reviewed separately; Settings 1080p and host-owned Amiibo consent
1080p were also opened by the coordinator. The 16 tiles, USB focus and separate visible consent
were intact in those images. This bounded review does not extend the worker snapshot to later
Nextendo/security/localization changes; the final all-feature preview still needs a fresh run.
