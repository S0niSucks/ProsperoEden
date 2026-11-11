# Encore firmware compatibility: evidence and conservative implementation boundary

**Read-only assessment: 2026-10-31.** Repository: `D:/leonardommello/PS5/ProsperoEden-Encore`; observed HEAD: `21fdbc1ca7f5a74e4f4a67c0b721349cf1353ffb`. References below describe the working-tree bytes inspected, not an assertion that a later concurrently edited tree or published binary is identical.

## Outcome

- **There is no evidence for all-PS5-firmware support.** PS5 **13.60 is the repository's historically hardware-exercised target**, not qualification of every current feature or this working tree. Other host versions remain unqualified. No native firmware version was independently verified in this assessment.
- **Host PS5 firmware, the native SDK/loader/kernel contract, and guest Switch firmware/content are different axes.** Neither a guest SystemVersion NCA nor a key revision certifies the PS5 host; a PS5 SDK offset case does not certify guest compatibility.
- App-owned compatibility reporting, typed capability states, conservative admission, clearer setup labels and safer guest-firmware validation can be implemented without inventing offsets. Broadening native compatibility requires pinned SDK/loader/graphics evidence and exact-artifact hardware qualification. Switch 2 requires positive guest-core evidence that is unavailable within this task's permitted scope.
- This task changed no repository source and ran no builds, tests, installs, console operations or feature-service requests. The only authored file is this report.

## Scope and evidence discipline

Read `D:/leonardommello/PS5/AGENTS.md`; no additional `AGENTS.md` was found in the scoped application/source/tool/documentation subtrees inspected. Loaded the PS5 knowledge guidance and Serena initial instructions. Serena symbol overview returned **“No active project”**, with no known projects; no activation, onboarding, configuration or memory operation followed. Used bounded local reads/searches instead. No Graphify pipeline was run because its writes/install/delegation would violate this task.

Did **not** inspect `vendor/`, `third_party/`, selected Eden internals, private runtime configuration/credentials or `.serena` memories. App integration files may name excluded dependencies; those references were not followed. No SDK source tree or generated/local build cache was inspected. Source findings below are therefore application-boundary findings, not a completed core/SDK audit.

Local Documentation was consulted through `ps5_knowledge` and QMD. No Encore semantic project context was returned by project discovery; project README/install/support sources supply its context. Related-conflict lookup was consulted but returned no Encore-specific resolution. Search snippets were not promoted to evidence. Historical commands, permissions and claimed results remain documentary data, not authority; no review/classification/index write was made.

## 1. Current host-side assumptions and guards

| Source / symbol / lines | What the inspected application actually establishes | Limit |
| --- | --- | --- |
| `README.md:32–37,143–155`; `SUPPORT.md:3–8`; `docs/FORK_NOTES.md:16–25` | 13.60 is the primary historically tested target; other versions are not qualified. Offset availability explicitly is not Encore compatibility. | Historical selected-title sessions are not current-artifact acceptance or a universal playability claim. |
| `headless/prosperoeden/version.h:6–15` | `kAppVersion`, optional `kBuildVersion`, and `kPackageVersion` identify Encore/package metadata. | `R1` / `01.000.041` are not host firmware or guest firmware. |
| `headless/main.cpp:417–475`, `main` | Boot/source trace precedes one filesystem elevation request. Captures UID/EUID/GID/EGID before/after. `rollback_failed` or changed identity on a failed request causes `_Exit(125)` before filesystem use. Successful reply still requires `ProbeWritableRoot(kDataDir)`. | No application host-firmware allowlist/check is visible before this request in the inspected startup path. Failures in loader/mandatory imports may prevent reaching `main` at all. |
| `headless/filesystem_probe.h:13–47`, `ProbeWritableRoot` | Rejects non-directory/symlink root; exclusively creates a per-PID probe, writes a token, fsyncs, closes and removes it. All must succeed. | Proves this root's tested write lifecycle, not arbitrary path access, safe kernel layouts, free-space accuracy, graphics, JIT execution or future I/O. It is intentionally a write probe, not a passive detector. |
| `headless/storage_paths.h:27–33,44–81`; `headless/assets_dir.h:13–28` | `FilesystemAccessStatus()==0` selects elevated data paths. Otherwise uses `/app0` and `/download0`; assets root is resolved once. | “Sandbox only” is a path/availability state, not evidence that the external privilege mechanism is universally safe. A newly selected root takes effect after restart. |
| `headless/main.cpp:145–185,188–345,348–405,476–482` | Guarded legacy migrations; unsafe source types/conflicts rejected, move rollback attempted, layout creation conditional on filesystem access. | Does not prove every external root is writable; default-root proof must not be treated as a global storage grant. Filesystem elevation is not a post-elevation sandbox. |
| `headless/elevation/elevation.cpp:52–96,99–143,147–173`, `exchange`, `submit`, `request` | Packages local helper, uses loopback elfldr endpoint, bounded per-operation socket timeouts, same-UID credential-clone handshake and validated response. One request path, no persistent service. | These are per-operation timeouts, not a demonstrated whole-boot deadline. Successful protocol framing is not host support. This assessment made no connection. |
| `headless/elevation/protocol.hpp:14–32,47–78,81–94` | Exactly one wire capability (`filesystem`); fixed 24-byte little-endian v1 messages; validates kind/PID/status/capability and handles split transfers. | `Status::unsupported_version` means **protocol version**, not PS5 firmware. Message contains no firmware, SDK capability inventory or qualification receipt. |
| `headless/elevation/helper/main.cpp:28–57,64–109`, `find_target`, `read_state` | Checks exact title ID through assumed `AppInfo` ABI, bounded process traversal, canonical-looking kernel pointers, SDK-supplied process/credential/filesystem layout macros. | Pointer shape/read success does not prove semantic correctness of a layout on an unqualified kernel. The layout and `AppInfo` declarations are assumptions, not version-independent proof. No offsets were inferred or added. |
| `headless/elevation/helper/main.cpp:112–149,165–207`, `write_state`, `grant_filesystem`, `handle_request` | Requires kernel payload context; re-finds same process/filedesc; rejects unchanged credential pointer or clone unequal to original; writes intended state, verifies readback; attempts verified restoration on apply failure. | Narrow protocol name does not mean least-privilege sandboxing: helper modifies credentials/authority/capabilities and filesystem roots. Depends on valid loader/kernel R/W/SDK semantics before these checks are meaningful. |
| `headless/elevation/helper/main.cpp:211–230`, helper `main` | Logs `kernel_get_fw_version()` with the result **after** `handle_request`. | This is firmware observation/logging, **not an admission guard**. No explicit app-owned firmware-support selection occurs in `handle_request` before SDK-address/layout use. The SDK CRT may have its own earlier checks; those were not independently inspected. |
| `headless/main.cpp:906–925,1093–1096` | Queries current direct-memory headroom; current shipping native route selects sparse JIT and rejects title launch if `Common::ProbeSparseJitAlias()` fails. Vulkan selection is rejected when not compiled. | Safe Launch does not bypass native JIT alias admission. Backend compiled-in availability is not firmware-qualified graphics support. |
| `src/memory_pages.cpp:315–392`, `ProbeSparseJitAlias` | Disposable 2 MiB shared-backing mapping, RX protection, cross-alias byte visibility, cleanup; aborts on ambiguous mapping ownership/failed cleanup. | **Does not execute generated code**, prove cross-core I-cache correctness, stress/lifetime stability or long-session memory savings. Do not run it as a harmless firmware detector. |
| `headless/prosperoeden/eden_services.cpp:2089–2110`, `diagnostics` | Exposes full-filesystem versus sandbox mode. Explicit 13.60 safety comment forbids libc `statfs/statvfs` after prior `SYSTEM_ILLEGAL_FUNCTION_CALL`; uses directory inventory. | Do not add speculative “capability probes” using these calls. Inventory/storage display is not physical SSD capacity. |

### Important source/document discrepancies

1. `headless/elevation/README.md:48–50` still says reported success with unexpected identity terminates. Current `main.cpp:460–469` deliberately proves writable-root capability instead of rejecting success based on identity; `tools/check-startup-contract.py:21–24` asserts that UID/GID-based success rejection is absent. Preserve the actual failure/rollback checks; correct the documentation rather than restoring the old rule by accident.
2. `src/memory_pages.cpp:315–317` says alias failure “keeps the dense path.” Current caller `headless/main.cpp:920–924` throws and rejects the title. Preserve fail-closed shipping behavior and correct the stale comment.
3. `docs/FORK_NOTES.md:8–14` carries historical branch names, while newer product documentation describes `main`. It is useful historical evidence, not a reason to switch branches or inherit old qualification.

## 2. Native SDK, ABI, import and package boundaries

| Source / lines | Actual guard or assumption | Not proved |
| --- | --- | --- |
| `tools/deps.json:123–151` | Pins application boilerplate `dd44bbdc75437332ed22e3ba95126733419ef25a`, SDK v0.42 archive and SHA-256. | Does not prove which installed SDK built an arbitrary helper, nor availability of every host firmware. |
| `tools/deps.json:156–177` | Separate pinned Vulkan/Mesa/PayloadSDK-fork trio: `3f3ee696…` / `0b2d6d1a…` / `95c08f27…`. | Not interchangeable with application SDK v0.42. A single-member upgrade is not a verified firmware port. |
| `tools/ps5.cmake:1–17`; `headless/ps5.cmake:1–10` | FreeBSD cross-toolchain configuration, LLVM 18, target `x86_64-sie-ps5`, SDK sysroot/emulated TLS; target-only library/include/package lookup. | `CMAKE_SYSTEM_VERSION 9` is a toolchain value, **not** “PS5 firmware 9 supported” or an established runtime kernel version. |
| `tools/link-headless-native.sh:14–18,25–53,56–89` | Native-title `libkernel`, not web-process provider; static frontend's optional loader hooks (`__dlopen`, `__dlsym`, etc.) resolved to null; final link rejects unresolved weak imports. | Cannot assume generic `dlopen/dlsym` feature discovery works here. Required module/export absence can fail before a UI capability screen. |
| `tools/package-headless-native.sh:23,28–46` | Runtime libc hash check; selected AGC/driver stubs; module SDK `0x02000009`, companion SDK `0x08050001`; self conversion/signing. | SDK metadata numbers and stub SONAMEs do not establish minimum/maximum host firmware or runtime ABI behavior. Do not edit them to claim all-version support. |
| `tools/package-headless-native.sh:133–137`; `headless/elevation/helper/Makefile:5–18,25–28` | Builds packaged helper using `PS5_ELEVATION_SDK` or `/opt/ps5-payload-sdk`; helper includes that SDK's toolchain. | Packaging script alone does not bind that external SDK's actual bytes/version to the application SDK archive pin. No ambient SDK was read in this assessment. |
| `headless/elevation/validate-helper.py:11–23` | Validates ELF64/little-endian/section extents and stream framing. | Not a target-ABI, kernel-layout, authentication, SDK-provenance or hardware test. |
| `headless/check_package.py:50–93` | Checks package inventory, metadata/alignment, expected needed-module list, no unresolved weak imports, fself integrity, pinned OpenGL manifest. | Does not certify all per-firmware required symbol/NID exports, runtime signatures or launch contexts. These checks were **not run** here. |
| `tools/package-headless-native.sh:148–159,174–184`; `headless/check_package.py:134–148` | Numeric package content version; 120 Hz declaration and flexible-memory request; package frontend records `hardware_qualified: false`; receipt creation hardcodes target `firmware='13.60'` while `hardware_run=False`. | Metadata target and offline receipt are not measurements. 120 Hz declaration does not prove working output; memory declaration is not physical RAM/guest/JIT backing. |
| `tools/check-startup-contract.py:12–27,47–56` | Source-order/string assertions for startup/elevation, actual root probe and title identity. | Name includes 13.60, but this is not an on-console firmware check. Full script reads excluded core files; do not run under this task's scope. |

`docs/BUILDING.md:164–180` explicitly separates preflight/native checks from 13.60 boot and gameplay. `docs/FORK_NOTES.md:307–320` keeps PS5 FPKG entitlement/PPR/kstuff and next-release startup qualification separate. Neither should become an Encore “firmware support” toggle.

## 3. Guest Switch firmware/content: what “Firmware ready” means today

**Current provisioning is user-supplied extracted firmware NCAs under `<selected-root>/firmware/`, with user-supplied keys under `keys/`.** `INSTALL.md:17–42,74–76,121–131` and `headless/assets_dir.h:13–28` define this contract. This is not PS5 firmware installation or an automatic firmware downloader. No dedicated guest-firmware import workflow was established in the inspected app entry points; save import is a separate feature.

### Concrete startup validator

`headless/metadata_bridge.cpp:595–643`, **`eden_startup_error()`**:

1. Resolves guest `keys`/`firmware`; requires nonempty `prod.keys` and a key-manager NCA header key (`598–607`).
2. Enumerates directory; inspects entries with **exact `.nca` extension**, opens each and requires `FileSys::NCA` status success (`608–628`). Core implementation/meaning of that success remains outside this audit.
3. Looks for SystemVersion title ID `0x0100000000000809`, extracts RomFS and accepts a file called `file` if at least `0x100` bytes exist and `0x100` bytes can be read (`629–636`).
4. Returns one static cached error string for the entire process (`595–596,641–642`).

**It does not decode/report a guest firmware version here, enforce a verified guest-version range, demonstrate a complete required NCA set, reject mixed-version content through an explicit app-level manifest, or prove that a title's keys/services are supported.** Presence of the SystemVersion marker is not a compatibility check. Symlink/non-regular-file rejection is not explicit in this loop; do not describe it as a transactional safe importer. Such behavior in underlying VFS would need separate permitted evidence.

`EdenServices::EdenServices` caches `setup_` from that function (`headless/prosperoeden/eden_services.cpp:1368–1385`). `home()` turns empty error into `setup_ready` (`1399–1404`) and the label **“Firmware ready”** (`1505–1508`). `setup_details()` is more accurate: **“startup checks passed. Game-specific compatibility is checked at launch”** (`2079–2083`). `folder_info()` counts NCAs (`2143–2146`); count is not version/validity proof. `set_files_folder()` checks directory type/schema and persists the root, not firmware compatibility (`2160–2189`).

Loader errors/key-revision naming in `headless/load_failure.h:10–32` are diagnostics, not compatibility certification. `docs/PROSPEROEDEN_PORTS_2026-10-30.md:13–17,60–64,92–93` explicitly leaves selected-core API compilation/native boot unverified. Do not infer a guest firmware release solely from highest `master_key_XX`, extension, NCA recognition, or ZBIC/zstd decoding.

`headless/main.cpp:1733–1752` selects built-in/HLE guest applets in the normal profile. A development-only firmware-applet choice does not certify those applets or provide a native in-game keyboard. `docs/CONTENT_FORMAT_MATRIX_2026-10-23.md:7–21,43–51,96–100` separates formats/compression from bootability and native qualification.

**Switch 2:** No permissible source read in this task establishes Switch 2 CPU/OS/services/crypto/GPU/content compatibility. Treat as **not implemented/unsupported by the app's declared capability surface unless a future explicitly reviewed guest-core contract says otherwise**. Recognizing an NSO compression method or a shared-looking archive extension cannot enable Switch 2 support. Do not invent guest firmware-version ranges.

## 4. Local PS5 Documentation context (attributed, not current runtime proof)

Sources delivered through QMD, all research dated **2026-09-30**:

- `Documentation/docs/knowledge-base/originals/docs/research-2026-09-30/topics/07.md:3–9,34–54,62–64` — distinguishes payload SDKs and loader ABIs. Records v0.42's 13.60 case as kernel-CRT offset selection, not an exploit or universal payload qualification. Describes prerequisite kernel R/W payload arguments, unknown-firmware `-ENOSYS`, and v0.43 detection changes because kernel-reported versions may be altered. **This does not verify Encore's current SDK bytes or license mixing in a new build.**
- Same source `:40–42` — stubs name runtime modules; CRT must resolve actual module exports. Compilation does not establish available exports/context. This directly supports separating package imports from observed capability.
- `Documentation/docs/knowledge-base/originals/docs/research-2026-09-30/topics/09.md:3–11,40–67` — websrv/bootstrap, application/foreground context, graphics and specific firmware execution remain separate layers; an HTTP response/TCP accept or SDK firmware branch is not launched-application evidence. Its websrv issues/versions are **not Encore tests**.

The corpus lookup provides no independent current Encore hardware receipt. No historical instructions in these sources were executed. No claim that SDK v0.43 or moving upstream HEAD is the correct upgrade follows from this research.

## 5. Recommended conservative capability matrix

Keep **availability** (`unknown / unavailable / available`) and **qualification** (`unverified / source / host-fixture / native-package / hardware-observed`) separate, with a reason and exact source/artifact reference. A capability that was observed once is not globally “supported.” Invalid/unsafe-state rejection is separate from an optional feature being absent.

| Axis / scenario | Conservative product state now | Admission / UI behavior |
| --- | --- | --- |
| PS5 13.60, historical matching artifacts | Historically observed startup/launcher/selected titles; current working-tree artifact unverified | Preserve existing guarded path. Show exact artifact/SDK/loader and per-capability results; no blanket stability badge. |
| Any other PS5 firmware, even if an SDK names it | **Unqualified**; native availability unknown until relevant evidence exists | Do not derive support from a version list. For a newly broadened firmware feature, refuse unreviewed privileged layout use; sandbox/diagnostic-only behavior is conditional on eboot already being loadable. |
| Unknown/unreadable/spoofed or conflicting host identity | **Unknown**, not “latest” or 13.60 | No guessed layout, nearest-version fallback or privileged probing. Reject the new privileged path before target kernel access; preserve safe diagnostics if reachable. |
| Missing helper/elfldr or failed filesystem proof | Filesystem capability unavailable | Retain sandbox paths; disable external-root operations/import/migration requiring that grant. |
| Rollback failure / identity mutation on failed request | Unsafe terminal state | Preserve immediate termination, not Retry/Continue. Do not automatically resend an uncertain request. |
| Sparse-JIT alias preflight failure | Required guest-runtime capability unavailable | Preserve current title rejection; Safe Launch does not bypass it. Successful byte alias probe is not JIT hardware qualification. |
| Vulkan not compiled, missing native module or failed backend startup | Backend unavailable | Distinguish compile/package/runtime failures. Do not advertise a backend merely because a setting exists. No firmware-number-only fallback. |
| Guest Switch NCAs pass current startup checks | Guest setup structurally readable through selected APIs | Say “Guest firmware startup checks passed”; version, service and title compatibility remain separate. |
| Guest firmware absent/malformed/mixed, key missing, or core version unknown | Setup unavailable/unknown for the specific reason | Keep error diagnostics; do not auto-download keys/firmware or claim all guest versions. A known unsupported format/version must be rejected. |
| Switch 2 | No declared implemented support / unverified core dependency | No enabled support checkbox or arbitrary “all firmware” option; explicit unavailable explanation. |
| Themes/interface/accessibility | App feature on an already functioning launcher | Choice does not qualify host kernel/SDK or guest OS. Keep independent from version allowlists. |
| Online metadata vs guest networking/online service | Separate capabilities and policies | Successful Nlib/HTTP metadata is not guest connectivity, account authentication, LAN/LDN or Nextendo support; no firmware-wide Online=true. |

No other named native firmware versions or numeric guest firmware ranges can be responsibly added to a supported table from this evidence.

## 6. Concrete app-owned changes feasible without guessed offsets

All items below are **recommendations, not changes implemented by this task**.

### P0 — truthfulness and safety

1. **Introduce one small compatibility snapshot, not a universal firmware switch.** Define separate host observation, artifact/SDK identity, filesystem result, renderer compiled/runtime status, required JIT preflight result, guest setup and guest-core support. Pure policy/serialization can be implemented now with explicit unknown defaults. Store bounded reason codes plus display text; never key material, kernel pointers or credentials. Wire existing facts from `main.cpp:426–475,920–924,1093–1096`, `storage_paths.h:27–33` and `EdenServices::diagnostics:2089–2110`.
2. **Do not turn the helper's version log into implicit support.** Add an explicit helper eligibility decision before `find_target`/`read_state` in `handle_request` (`helper/main.cpp:165–180`). Reuse only the already-declared SDK detector and a reviewed mapping/policy tied to the exact helper SDK; unknown/mismatched cases return unavailable without app-owned target kernel reads/writes. No new offsets. Verify the raw version encoding and SDK initialization semantics against an explicitly permitted pinned source before choosing actual admitted values. A kernel version observation can be patched; retain provenance/ambiguity. This local gate cannot prevent earlier loader/SDK CRT effects and therefore cannot by itself make unknown-firmware execution safe.
3. **No ad-hoc native ABI declarations for a host detector.** Current app path does not expose an authoritative host version to the launcher; helper v1 response carries none. Either display “host firmware not observed” or add a separately reviewed typed SDK adapter/versioned response. Do not overload protocol `unsupported_version`; it already means wire-version mismatch. Preserve v1 fixed framing and rejection unless both endpoints/fixtures are deliberately versioned together.
4. **Make package provenance cover the helper SDK.** In `tools/package-headless-native.sh:133–137` bind `PS5_ELEVATION_SDK` to a verifiable build-input manifest; record helper hash, compiler/sysroot/CRT/kernel API source identity, application SDK identity and graphics trio independently. Fail release packaging on unknown/mismatched provenance rather than accepting an ambient `/opt` directory. Extend application-owned package guards; do not inspect or update dependencies under the current task's exclusion.
5. **Clarify user-facing labels and stale comments.** Change “Firmware ready” to “Guest firmware: startup checks passed,” and add a separate “PS5 host: [observed/unknown], qualification: [historical/current-artifact status]” line. Use existing `setup_details()` wording. Correct the two stale elevation/alias descriptions noted above. Do not label all versions supported merely because the new report UI exists.

### P1 — guest provisioning/diagnostics and independent feature choices

6. **Replace boolean marker readiness with a bounded structured guest-validation result.** Refactor `eden_startup_error()` into a read-only validation operation returning status + detected version only when decoded via an existing, typed, reviewed core contract. Keep unknown when unavailable; do not reverse-engineer a new SystemVersion byte layout here. Preserve current NCA/key errors and per-title core loading checks. Replace process-static result only if adding explicit Refresh/Import, with a generation/root identity and no revalidation racing a running title. Existing restart-only storage contract otherwise remains authoritative.
7. **A future guest-firmware importer must be staged, not a host updater.** Only user-supplied lawful input; regular-file/no-link admission, bounded entry/count/bytes/paths, complete staging validation, no live replacement while a guest runs, conflict/rollback policy, no silent root/key replacement. Per-volume capacity must be known via an approved native interface or admission must remain unknown/refused; do not add prohibited libc `statfs/statvfs` probes. Validate through the actual selected guest core before publishing. An import success proves copied/accepted bytes, not all guest versions or Switch 2.
8. **Keep UI/network choices capability-specific.** Theme/interface preferences can be implemented independently on the launcher. Metadata, shares, online guest service, native keyboard/USB and other external providers need their own compiled/available/qualified states and visible disabled reasons. Do not enable privileged/guest-core paths on the basis of firmware labels or external metadata success. `docs/PROSPEROEDEN_PORTS_2026-10-30.md:26–42` explicitly lists remaining features and core exclusions; those are work items, not existing capability proof.

### Mandatory-import constraint

An in-app capability screen cannot recover from an unresolved required native import before `main`. Optional native APIs need either existing supported, typed platform integration or a deliberately qualified packaging/link architecture; the current static frontend intentionally nulls dynamic-loader hooks (`tools/link-headless-native.sh:32–44`). Do not paste generic `dlsym` probes or fabricate signatures/NIDs to implement “works everywhere.”

## 7. Verification plan — all future work, NOT_RUN here

1. **Read-only provenance gate:** bind exact source revision/dirty-tree digest, application/helper hashes, symbol package, SDK/sysroot/CRT manifests, graphics trio and loader context. Reject stale receipts; separate target labels from measurements.
2. **Pure policy fixtures:** known historical 13.60 metadata, other SDK-recognized firmware, unknown/unreadable/conflicting firmware, missing module, filesystem unavailable, failed/uncertain helper, missing guest setup and undeclared Switch 2. Assert no universal-support state and no nearest-version fallback. These can use explicit synthetic adapters without console APIs.
3. **Helper/transport fixtures:** fragmented/invalid wire frames, mismatched PID/capability/version, transport loss at prepare/final reply, changed identity on failure, apply/rollback failures, policy denial before any mocked target read/write. Preserve exact fail-closed paths. Do not execute the production helper or a kernel-write path on a host.
4. **Guest validator fixtures:** missing/empty keys, header-key absence, unreadable/invalid NCA, absent/truncated SystemVersion, malformed or mixed sets, unknown version, uppercase extensions, symlink/special-file/path replacement, excessive count/size, cancellation and refresh generation. Core stand-ins prove boundary behavior only; selected-core compilation remains an independent gate.
5. **Native package gate:** authorized build elsewhere using actual pinned target headers; import/NID inventory, all strong/weak providers, alignment/helper framing, exact SDK/graphics provenance and optional-feature exclusions. Static type correctness, linking and package inspection remain distinct from runtime ABI availability. Do not run existing gates that traverse forbidden dependencies until scope permits them.
6. **Supervised exact-firmware hardware qualification:** separately authorized; record full host revision/source-of-version, model, jailbreak/loader/SDK/graphics context and exact artifact+symbols. First safe startup/import resolution, helper outcome and filesystem proof; then selected storage, launcher input/audio/display, JIT admission, renderer startup, basic guest launch/exit, repeated title sessions and recovery. No injected failed kernel writes/rollback experiments as a casual smoke test. Unknown firmware is not admitted by a destructive exploratory probe.
7. **Guest-version/title qualification:** own independent matrix of guest dump version, key revision identifiers (never bytes), core revision, title/update/DLC, compression, renderer and applets/services. Exercise actual guest progression/HUD/content freshness and teardown, not compositor FPS alone. Switch 2 stays unavailable until the guest-core/platform contract exists and is separately qualified.
8. **Publish narrow outcomes only:** source fixture passed / native package passed / exact hardware scenario observed. Promote one specific host+artifact+capability combination at a time; maintain explicit unverified rows for every other native firmware and guest-core dependency.

## Blockers / handoff

- **All-host-firmware implementation is not substantiated:** unreviewed SDK/kernel layouts, native exports/ABI and loader/application context cannot be supplied from a version detector or app UI.
- **Current native hardware qualification is absent in this task**, including 13.60 for these working-tree changes and current sparse-JIT behavior; historical testing does not transfer automatically.
- **Selected guest-core APIs/crypto/services and any Switch 2 support are outside permitted inspection**, so guest firmware ranges and new core-dependent functionality remain unknown/unqualified.
- **Actual helper SDK provenance and runtime host-version adapter are not established** from source manifests alone. No private/local SDK configuration was read.
- Serena unavailable without project activation; deliberately not activated. This limited symbol tooling, not the ability to deliver the source-pinned read-only assessment.

**Delivery:** this report only; no implementation, build/test PASS, hardware support expansion, release acceptance or external-service verification is claimed.
