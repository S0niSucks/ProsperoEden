# ProsperoEden 1.006.000 — real source-material preparation

> Historical preparation record. The [policy follow-up](RELEASE_SOURCE_POLICY_FOLLOWUP.md)
> preserves these materials RO and implements reviewed export/restore support.
> Earlier refusals and “not executed” statements below describe this original trial.

## Qualified result (2026-10-27)

This is an **integration preparation handoff**, not a release source bundle,
final-build receipt, successful relink trial, legal opinion, or compliance certificate.
The immutable snapshot studied/restored was:

- Commit: `3e0fb734116c663cf2a1596f8bc9bfb300b0d8a3`.
- Tree: `82b24ffe3e8a98c7ae782a50c37ad6138e2f2ad0`.
- Original worker collector commit `b037e543e0ce4fb70aef509c545748c02fc85d1c`
  is preserved; its collector and tests were **not edited** in this continuation.

All **81 initial requirements** are mapped: **59 actual verified inputs** and
**22 operator-declared CPM non-use decisions**. Some inputs are binary packages,
not source; these counts do **not** establish corresponding-source closure.
An additional **15 versioned source/recipe inputs** were collected for selected
SDK, pacbrew, OpenGL, RADV and LLVM dependencies.

The unchanged collector refuses five genuine initial inputs. Consequently **no
integration source TAR was published**, and relink remains **`PENDING`**. No old
native object or binary was collected as final evidence.

## Custody and reproducible inventories

All archives, Git object stores, source checkouts and restored trees are in the
worker-owned Docker volume **`prosperoeden-team-source-materials-20261027`**.
The paths below assume it is mounted at `/work`. Do not publish the whole volume:
it also contains private scratch, a partial-history Git snapshot and restore trials.

| Path | Content |
|---|---|
| `/work/materials/archives/` | Nine direct archives, including binary SDK/pacbrew/OpenGL inputs |
| `/work/materials/files/` | Seventeen individually pinned public files |
| `/work/materials/git/` | Six direct and seven supplemental exact-commit repositories |
| `/work/materials/historical/` | All five original `UPSTREAM.json` artifacts, unchanged |
| `/work/materials/cpm/` | Twenty-two selected Eden CPM archives, verified against SHA-512 pins |
| `/work/materials/closure/` | Eight selected transitive source/patch archives |
| `/work/inputs-integration.json` | Actual collector allowlist for the integration snapshot; relink `PENDING` |
| `/work/inputs-integration-with-source-supplements.json` | Same allowlist plus eight real source/patch archives in the existing `extra_sources` schema; seven supplemental Git stores still need a reviewed export/delivery route |
| `/work/material-hashes.json` | All 59 initial inputs: original hashes, measured SHA-256/size or Git commit/tree, explicit cache/public origin |
| `/work/material-verification.json` | Independent byte/pin checks, unchanged collector-policy results and discovered CPM definitions |
| `/work/closure-source-hashes.json` | All 15 supplemental inputs and their exact provenance |
| `/work/closure-source-recipes.json` | Versioned package recipes, source URLs/hashes and source-versus-binary limitations |
| `/work/closure-material-verification.json` | Supplemental hash/policy audit |
| `/work/savednative-selected.json` | Allowlisted cache options/link-library paths, hashes and selected source-overlay evidence; planning only |
| `/work/integration-real-plan.json` | Regenerated initial requirement map for the integration snapshot |
| `/work/material-blockers.json`, `/work/closure-source-blockers.json` | Acquisition gaps only; empty does not mean policy/closure/relink ready |
| `/work/restore-trial-01/restore-result.json` | Actual offline direct-source restore and patch-application result |

Inventory hash anchors are listed in the final evidence section below. They bind
this handoff's bytes, not a future coordinator HEAD. Regeneration against final
HEAD must not reuse this integration snapshot identity as release identity.

## Direct and historical provenance

Direct inputs retain the exact pins and extraction rules in snapshot
`tools/deps.json`; no dependency/license source was rewritten or repinned.
Six Git inputs were fetched by exact commit from explicit read-only native
repositories into private Git stores (not copied as working/cache volumes):

| Input | Commit |
|---|---|
| Boilerplate | `dd44bbdc75437332ed22e3ba95126733419ef25a` |
| Lapy owned helper | `57fcc19b7f57e46c823d6f2a6c6233ebf3288079` |
| PS5 Vulkan recipes | `5b5e4fc2d80fdb67a4f61ba9e6a8a424026bb8a5` |
| PS5 Mesa/RADV | `7b59ef27c1b09b9671bc4153c41940c3155c3af2` |
| RADV SDK fork | `b83202be73e930050e00de0f1b4d9ec46c0391bf` |
| MihawkGen | `c57232a9f16bcb6df5fe07c9bb631518f2b1cc0e` |

The SMB and NFS archives were recovered from explicit existing caches:

- `libsmb2-6.3`: SHA-256 `5011003fd091513f897e1ebbb8e1a40af634386df7199716348b7f77569a2790`.
- `libnfs-8.0.0`: SHA-256 `bc91216e927a85142b5de611c7f711558e02119a7daad67620ea631634038350`.
- Eden `67bada77f8a43a90da2e94e89b8e7da73c256989`: SHA-256 `d42b6fb492c65eb5e98ab72a786dff01fd64384d073a739b9c9ff475414366ee`.

Previously absent CPM archives were downloaded only from the real upstream
release/tag/commit URLs, with the original SHA-512 checks. GitHub codeload is
used only as an equivalent archive endpoint with the **same** required hash.
All five historical artifacts were retained, including the exact
`libboost1.90-dev_1.90.0-6ubuntu1_amd64.deb` recovered from the public Ubuntu
`pool/main/b/boost1.90/` index and verified against the existing `UPSTREAM.json`
SHA-256. No newer Ubuntu package was substituted.

### Boost reconciliation and applied overlays

The selected savednative cache is
`/native/cache/ps5-eden-headless.6d6053a486f3/native-local` on the read-only
release volume. It is **not** the final build. Its `Boost_SOURCE_DIR`, include
paths and filesystem/atomic/date_time/container/context link targets use unified
**Boost 1.92.0**. Native `tools/build-headless-native.sh` forces bundled Boost;
Eden's top-level recipe provides `Boost::headers` from that unified provider and
only requests standalone `boost_headers` (1.91.0) if that target is absent.

`savednative-selected.json` compares the applied Eden `cpmfile.json` with the
verified upstream file, records the selected Boost version header and hashes
representative applied cache inputs. These are sampled evidence, **not** a full
savednative source-delta inventory. Historical Boost 1.90 artifacts remain
historical inputs, not proof of the headers/objects actually compiled. None of
the 1.90, 1.91 or 1.92 pins was silently overridden.

Measured here: applied/pristine `cpmfile.json` are byte-identical (SHA-256
`e1a263a2825e1204a243998cd5254c600201a0d3ed9b542f7a25ef99f2bdb157`),
and the selected header defines `BOOST_VERSION 109200`.

Preserve the entire pinned Eden source (including `.patch`), tracked
`tools/eden-patches`, `headless` CMake/source overlays, compiler/linker wrappers
and native dependency recipes. For example `headless/jit-alias.cmake` modifies
the Xbyak cache source, and the headless overlay changes Boost.Process linkage.
Pristine archive hashes alone cannot identify the resulting compiled source.

## CPM applicability: explicit, provisional non-use decisions

Used/retained source providers are: `boost`, `enet`, `ffmpeg`, `fmt`, `frozen`,
`gamemode`, `httplib`, `lz4`, `nlohmann`, `tzdb`, `opus`, `reshade`, `sdl3`,
`simpleini`, `sirit`, `spirv-headers`, `vulkan-headers`,
`vulkan-memory-allocator`, `vulkan-utility-libraries`, `xbyak`, `zbic`, `zstd`.
The savednative source/provider list corroborates selection; it is not used
alone to declare an omission. FFmpeg is supplied by the app's explicit source
build. Retaining sources for a configured but ultimately dead-stripped library
does not claim that its objects are in the shipped binary.

The 22 decisions have individual reasons and tracked recipe paths in the actual
allowlist; their technical basis is:

| Omitted CPM entries | Recipe/configuration basis |
|---|---|
| `biscuit`, `oaknut`, `lagoon` | Eden `externals/CMakeLists.txt` gates RISC-V, ARM64/Dynarmic tests and LoongArch respectively. `tools/ps5.cmake` targets x86_64; savednative `DYNARMIC_TESTS=OFF`. |
| `libadrenotools`, `linkernsbypass` | Eden's Android driver and its Android-only linker dependency, not the PS5 x86_64 OpenGL/RADV route. |
| `llvm-mingw`, `moltenvk`, `vulkan-validation-layers` | Windows/MinGW, Apple, and Android validation-layer providers respectively; PS5 toolchain/driver recipes select none of these. |
| `catch2` | Native `BUILD_TESTING=OFF`, `YUZU_TESTS=OFF`; savednative Dynarmic tests also OFF. |
| `cubeb`, `libusb` | Native recipe explicitly sets `ENABLE_CUBEB=OFF`, `ENABLE_LIBUSB=OFF`; native audio/controller implementations are tracked separately. |
| `cpp-jwt` | Eden requires web service or update checker; both savednative options are OFF, and native recipe disables web service. |
| `discord-rpc`, `quazip` | Qt is explicitly OFF. Eden's Discord option depends on Qt; QuaZip belongs to that frontend. |
| `boost_headers` | Unified bundled Boost supplies `Boost::headers`; the fallback gate is not selected (see above). |
| `openssl`, `openssl-cmake`, `zlib` | CPM source providers are bypassed by explicit pacbrew include/library paths and `OpenSSL_FORCE_SYSTEM=ON`. This does **not** omit those libraries from source-closure review. Their actual source recipes are supplemental materials below. |
| `ffmpeg-ci`, `openssl-ci`, `sdl3-ci`, `sirit-ci` | The four unhashed CI binary variants are bypassed by the explicit source/system providers; savednative bundled-variant flags are OFF. |

The preparation script refuses changes to the principal native architecture,
disabled-feature and source-provider assumptions. Nevertheless the coordinator
must recheck **every decision** against final recipes, configure and link inputs;
this savednative evidence cannot establish final non-use of a newer feature.

## Binary packages versus corresponding sources

Supplemental release tags were resolved publicly to exact commits and then
collected by commit. The tag observations are not authenticated build receipts.

| Binary/input family | Real source/recipe materials obtained | Remaining qualification |
|---|---|---|
| Official SDK v0.42 | `ps5-payload-dev/sdk` commit `4eb701204fc3f8d31e84cf8ca272974e2be9c867`; complete tracked tree including source, headers and runtime recipe | Compare actual SDK binaries/runtime/import stubs and local modifications; retained symlinks are currently a collector blocker. |
| SDK LLVM runtimes | LLVM **18.1.8** source archive, SHA-256 `0b58557a6d32ceee97c8d533a59b9212d87e0fc4d2833924eb6c611247db2f2a`; contains libc++, libc++abi, libunwind, compiler-rt and shared CMake sources | Version comes from pinned SDK `libcxx.sh`; hash is independently published in the pinned Homebrew formula cited in `closure-source-recipes.json`. Not an SDK binary/source match or a runtime rebuild. |
| Pacbrew v0.40.2 | `pacbrew-repo` recipes at `1687e827675c66e1b040751c7f0811222bd3aea0`, plus hash-verified sources for OpenSSL **3.5.2**, zlib **1.3.2**, curl **8.18.0**, libpsl **0.21.5** | Version strings agree with the pinned binary package's selected `.pc` metadata. Matching versions do not establish matching build options, patches or resulting binaries. curl/libpsl are retained planning inputs, not claimed final linked dependencies. |
| OpenGL SDK v1.0.2 | `darkxex/ps5-opengl` at `6b6bb2039f741287ed7f1bc6bc4b2c589709a0ba`; its `dependencies.json`; hash-verified Mesa **26.2.0**; exact Git inputs for `opengnm`, `opengnm-psbc`, SPIRV-Headers and Vulkan-Headers | Compare static archive/compiler/generated-shader provenance and actual recipes. Example/UI/CTS dependencies were not fetched; their non-use and any additional library transitives still need review. |
| RADV | Exact direct Mesa fork, Vulkan recipes and SDK fork above; source and Meson patch archive from pinned `subprojects/zlib.wrap` (**zlib 1.3.1**) | This is distinct from OpenGL Mesa 26.2.0 and pacbrew zlib 1.3.2. Full Meson/tool/generator closure and actual local modifications still need review. |
| Native FFmpeg | Direct and CPM pin `c7b5f1537d9c52efa50fd10d106ca015ddde1818`, with tracked `tools/build-headless-ffmpeg.sh` and compiler wrapper | Native recipe disables autodetection, networking, unrelated libraries/programs and uses the small decoder/filter set; savednative links its explicit avcodec/avfilter/avutil/swscale outputs. The large prebuilt pacbrew FFmpeg package is not a substitute for that source build. Final config/source match still pending. |

The OpenGL dependency commits are recorded without shortened revisions in
`closure-source-recipes.json` and `closure-source-hashes.json`. No package
manager or upstream build/install recipe was executed to obtain these sources.
Having fetched these selected inputs does not imply recursive closure for the
SDK, drivers, packaging tools, generated data or every pacbrew package.

## Concrete collector blockers (unchanged policy)

`check_release_source_materials.py` rehashes real inputs and invokes the
unchanged collector's validators. Its diagnostic wrapper only observes path
names and **rethrows** refusals; it never accepts a rejected member.

1. Boilerplate commit contains tracked `.env.example`: private-path policy.
2. PS5 Vulkan recipes commit contains tracked `.env.example`: same policy.
3. PS5 Mesa commit contains **10** tracked symlinks: Git entry-type policy.
4. RADV SDK fork contains **15** tracked symlinks: same policy.
5. Pacbrew binary archive contains a hardlink: archive entry-type policy.

The supplemental official SDK source also contains 15 symlinks and receives
the same refusal. Exact offending paths/special Git entries are retained in
the verification JSONs. Public source trees/archives were kept intact: no
flattened symlinks, dropped templates, edited licenses, invented override, or
alternate bundle format was used to hide these failures. A coordinator-owned,
tested policy decision is needed before this mechanism can export these real
inputs safely. Acquisition success is deliberately reported separately.
Private Git stores also pass `git fsck --full` and tracked-checkout checks;
none of the Git stores is a wholesale copy of the coordinator's repository.

## Actual offline trial and tests

`restore_release_sources.py` exported the declared app Git blobs into a new
private folder, verified the six source archives using unchanged collector
validation, seeded **only** the original dependency tool's exact cache names,
and executed the snapshot's unmodified `tools/deps.py fetch` with Docker networking
disabled. Restored groups:

- Source archives: libsmb2, libnfs, Eden, FFmpeg, fmt, HarfBuzz.
- Individually pinned source files: compiler-rt (6), UI kit (10), ps5log (1).
- All **5** tracked Eden patches applied to pristine restored Eden, each with
  forward dry-run, real application and reverse dry-run checks.

**Result: PASS for these 9 dependency groups / 17 files / 5 patches only.**
The tool does not execute Git dependency setup hooks or binary SDK tools.
It refuses an existing restore destination instead of overwriting it.

The new offline preparation tests passed **17/17**. They cover exact hashes,
corrupt/missing materials, deterministic repeat metadata, path/symlink/FIFO
refusal, stale-partial preservation, public URLs, exact CPM URL construction,
and real local Git-pin collection/dirty-checkout refusal. The existing
`test_source_bundle.py` was neither modified nor claimed rerun here.

Not executed: CPM cache reconstruction/configure, full recursive restoration,
SDK/runtime compilation, host/native emulator rebuild, final-package generation,
final-binary comparison, object-closure collection or modified-library relink.

## Running the narrow preparation tools

Use `prosperoeden-host:features`, a private volume, `--cpus 2 --memory 1g
--pids-limit 128 --cap-drop ALL`, `TMPDIR=/work/tmp`, and `python3 -B`.
Mount the worker source at `/repo:ro`. The gatherer additionally reads only
explicit inputs from release `/native:ro`, integration `/coordinator:ro`, and
network-worker `/network:ro` volumes. No writable coordinator/native mount,
credentials, home-directory/config scanner or bulk cache-volume copy is needed.

Inside that container, with the private Git snapshot already available:

```sh
python3 -B /repo/tools/ci/gather_release_sources.py \
  --workspace /work --snapshot-repo /work/snapshot.git \
  --snapshot 3e0fb734116c663cf2a1596f8bc9bfb300b0d8a3
python3 -B /repo/tools/ci/gather_source_closure.py --workspace /work
python3 -B /repo/tools/ci/check_release_source_materials.py --workspace /work
python3 -B /repo/tools/ci/check_release_source_materials.py --workspace /work \
  --manifest closure-source-hashes.json --output closure-material-verification.json
python3 -B /repo/tools/ci/restore_release_sources.py --workspace /work \
  --destination /work/NEW-private-restore-trial
python3 -B /repo/tools/ci/test_release_source_preparation.py -v
```

Use Docker `--network none` for all audits/restores/repeats. Only the two gather
commands accept explicit `--download` for missing public inputs. Downloads
disable proxy inheritance, accept credential-free HTTPS origins, reject unsafe
redirects, check pins, and cap individual files at 512 MiB. Git source fetches
disable credential helpers/global configuration and use explicit commits.
These bounded scripts are release-layout specific, not a race sandbox or DLP
system; inputs and private output volumes must be reviewed and quiescent.

For final HEAD, the coordinator must prepare a new private Git snapshot and
regenerate/review allowlists, omissions and supplements before using
`source_bundle.py`. **Do not edit JSON snapshot identities to reuse old evidence.**

## Final integration/relink blockers

- Resolve the real-input policy refusals above; rerun both synthetic and real
  collector/verification tests without dropping or changing upstream source.
- Close/review all actually used transitive/generated-source and toolchain
  materials, including local driver/SDK patches and packaging/runtime inputs.
- Incorporate the supplemental archives after binary-match review and arrange
  source-preserving delivery of the seven supplemental Git inputs. The existing
  `extra_sources` schema accepts archives, not additional Git checkout paths;
  those Git sources are available but have not been packaged by this collector.
- Reconstruct CPM/overlay state offline; the successful direct-source restore
  is not a claim that complete offline configuration or a build works.
- Compare every selected input, option, overlay and omission with the final
  clean app HEAD and final build, not this savednative planning cache.
- For static libsmb2/libnfs, collect the **actual final-build** non-LGPL objects
  (including matching LLVM-compatible ThinLTO where applicable), actual link
  commands/response files, scripts/SDK/driver closure and binary-bound receipt.
  No substitute objects or borrowed old-build evidence are provided here.
- Independently verify provenance and complete a modified-library relink trial.
  Until then preserve `PENDING`/the collector's qualified statuses, never `READY`.

No push, tag, release, version change, installation or full build was performed.

## Evidence SHA-256 anchors

These paths are in the private volume, not committed archives or userdata.
Repeated offline preparation produced identical allowlist, initial/supplemental
material inventory, package-recipe and savednative-evidence bytes.

| `/work/` path | SHA-256 |
|---|---|
| `inputs-integration.json` | `6f48d2d0c9bdf92321863f71865a130f7763c318bdc91aacee71ed2b16905960` |
| `inputs-integration-with-source-supplements.json` | `4a13c8a80ee72762440cbc4b1b7d4b14fb7b8be58204d586ff590ab86d6fd5e0` |
| `material-hashes.json` | `9114198f1768201f56b4959ea835419504c00d852718dad1862d5fc14e757a6b` |
| `material-verification.json` | `59386a4e59ce8cf46e8fe5eef48cb5ac9d69d26cd057fe512284028031ae11ae` |
| `closure-source-hashes.json` | `4b5c752a0fab45598c3825dff986226970d271cbc3a6f5d60a41db7cc3434db1` |
| `closure-source-recipes.json` | `8bd128e1937328de26fa01df7d06336ba7aeecc62310e5098ba86f297e1269c2` |
| `closure-material-verification.json` | `278bf165455c2c6172c8dbf3c355eb5de23333dc79f4275c2f6b30802bf0e5b4` |
| `savednative-selected.json` | `ecbca17ab65f89a409384333d64a6141f7cf8ea39f4031db236e038a8b75190b` |
| `integration-real-plan.json` | `437e2940c714806549418d1f14c15062a3d0cca197ba8af0a281f08bf1e02c4b` |
| `restore-trial-01/restore-result.json` | `45f12348e9849188ae90822f09ba1313c1511510b95d0b16965511dda9a679d5` |

Local worker logs are under `.local/` in the `cover-flow` worktree:
`source-materials-final-audit.log` (real hashes/policy, repeat metadata, plan,
actual create refusal and no output TAR), `real-source-restore-trial.log`,
and `source-preparation-final-recheck.log` (final Git object audits, source
preparation tests and repeated offline restore). These ignored logs are not
final-release evidence and contain no environment or raw native-build log dump.

| Worker `.local/` log | SHA-256 |
|---|---|
| `source-materials-final-audit.log` | `18c4dee8e1ab87b5f5f272c3587cfd782eeb86b48321821dedda1cefbecf3268` |
| `source-preparation-final-recheck.log` | `8e9099da8d787704fcec963b20d5fc88c6a8314a2d55763d03b44be3ff1cd949` |
| `real-source-restore-trial.log` | `3cd23d07cac71884cb66603adbca64b1ec88c0d1f1da2a01e3c74c2854d36248` |

The second restore at `/work/restore-trial-02/restore-result.json` is
byte-identical to the first. The unchanged collector SHA-256 is
`8bb2c55dcf5a4437293ca42c75c0f255c4aa9433ba39db33c8e5dd12cd036a05`.
