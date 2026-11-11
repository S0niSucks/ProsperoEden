# Native inputs and allowed integration surfaces — 2026-10-31

Relocation note (2026-11-02): this study now lives in the repository. Runner,
receipt, object and snapshot names below still identify external artifacts,
not files beside this Markdown. See the [evidence map](../../EVIDENCE.md#artifacts).
The relocation does not change the original inspection/build date or results.

This continuation is read-only on the repository. SDK materialization, compiler
objects, disassembly and receipts are external. No console, network service,
core modification, staging, commit, package, publication or deployment occurred.
The native compilation below is not an EGM round or closure of DE-05-01.

## Inputs actually located

| Input | Location and observed scope |
| --- | --- |
| Selected Eden pin | `tools/deps.json:5-12`: `5f142c7926d0c7fcbbd0ce30794d72f638a43b2a`; original archive in `vendor/objects/eden`, source in `vendor/sources/eden`. The protected original implementation was not semantically inspected. |
| Encore sources | `vendor/encore-active/README.md:3-31,65-96`; maintained overrides and shipping backports are distinct from the original. The README's older counts differ from the newer catalogue, so no exhaustive count is asserted here. |
| Application SDK | `tools/deps.json:123-152`: boilerplate `dd44bbdc75437332ed22e3ba95126733419ef25a`, public PayloadSDK v0.42. |
| Graphics SDK inputs | `tools/deps.json` pins OpenGL `ad2807d41cef2681882a9ee0d20808779f74b7cd`; `vendor/sources/opengl-source/dependencies.json:2-5,24-28` pins Mesa 26.2.0 and the same PayloadSDK archive. The graphics boilerplate revision is different; do not collapse the two into one template. |
| SDK binary archive | `vendor/objects/boilerplate-setup-1/0000.chunk`, 8,816,075 bytes, SHA-256 `8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da`. ZIP: 3,153 entries, 29,431,341 expanded bytes, no symlinks. |
| Public SDK source checkout | `D:/leonardommello/PS5/ps5-payload-sdk`; useful source/stub discovery, but not the installed SDK and not assumed identical to v0.42. |
| Docker toolchain | `prosperoeden-release:recovery-5211718` has Clang/LLVM 18, CMake, Ninja and ccache. Inspection found no prepared `.deps`, `.local/headless-cache` or extracted SDK in that image. |

The earlier generic assertion that the SDK is unavailable is superseded. Its
archive is present, verified and safely extracted under `native-sdk-audit-01/`.
Runtime/import qualification is a separate, still-open question.

## Native object audit 01 — completed, failed (preserved)

Receipt: `../native-sdk-audit-01/receipt.json`; runner: `../audit-native-sdk.py`.
Docker used the repository read-only, external evidence writable, no network,
uid/gid 65534, no extra capabilities. Target `x86_64-sie-ps5`, C++20, `-Werror`.

- Passed 5/13: content default, explicit `QUALIFIED=0`, isolated `QUALIFIED=1`
  header probe, forwarder parser and appearance header.
- The default/zero content objects do not import `openat`, `fstatat`, `fdopendir`
  or `readdir`. The isolated qualified object does reference those functions.
  It is an audit object only; the production gate remains disabled.
- All eight UI units failed before semantic compilation at the same missing
  `GL/glcorearb.h` include (`ui-*.log`). This is an audit include-path omission,
  not evidence that those eight sources have eight distinct native defects.
- `headless/CMakeLists.txt:1691-1703,1712,1741-1747` shows the production graphics
  include/link and `GL_GLEXT_PROTOTYPES=1` wiring. The follow-up probe uses pinned
  Mesa graphics headers rather than host GL headers or a fabricated replacement.

## Native object audit 02 — completed, failed (preserved)

`../native-sdk-audit-02/receipt.json`, runner `../audit-native-ui.py`:
16/21 units passed, including launcher/browse/widgets/textures/mapping, all three
audio helpers, strings, six graphics helpers and native audio. The reused SDK's
2,923 files matched the ZIP; allowed input hashes had no before/after mismatch.

The four remaining UI units and native frontend stopped at the pinned
`nlohmann/json.hpp` dependency, not at a proven source semantic defect.
The dependency is present at
`vendor/sources/eden-cpm/nlohmann_json/v3.12.0/include`, with a lock record in
`vendor/sources/eden-cpm/VENDOR-CPM-LOCK.json:85-92`. Audit03 adds that path plus
the production warning/RTTI/overflow policy; no application code was changed.

Audit02's `sdk-symbol-providers.json` inventories declarations from the actual
verified SDK libraries. `openat`/`fstatat` are found in `libkernel_sys.so` and
`libc.a`, not plain `libkernel.so`; `fdopendir` is in `libSceLibcInternal.so`.
The selected production link recipe uses `libkernel.so`, `libSceLibcInternal.so`
and `libc.a` (`tools/link-headless-native.sh:37-53`). This makes the static raw
wrapper concern material; it does not establish full-link resolution or runtime
behavior. IME init/abort/status/result/term symbols are present in
`libSceImeDialog.so`; symbol declarations alone do not establish the structs,
callback ABI, module lifetime or guest-keyboard integration.

No full selected-core/native link is claimed by these independent object probes.

## Native object audit 03 — completed, passed 21/21

Runner: `../audit-native-ui-deps.py`; receipt:
`../native-sdk-audit-03/receipt.json`, SHA-256
`3833d71a46eb97a6a79c56a6ebbfa7cde1cc85bb43567802d2c12035be28efe8`.

- Passed all nine UI translation units: launcher, library, settings, browse,
  home, switch_theme, widgets, textures and mapping.
- Passed mixer/sounds/wav, strings, bidi/draw_list/font/gl_batch/gl_program/image,
  native audio and the real native launcher frontend (12 supporting units).
- All21 object SHA-256 values were independently read back and matched the
  receipt after completion. Before/after allowed source hashes had zero mismatch;
  the original source snapshot from audit02 also still matched at audit03 start.
- Uses the verified v0.42 SDK, pinned Mesa GL headers and pinned JSON v3.12.0,
  `--target=x86_64-sie-ps5`, C++20, `-Wall -Wextra -Werror`, production
  `-Wno-unused-parameter -fno-rtti -fwrapv`, and SDK exception/TLS flags.
- No running jobs or reviewers remain in this continuation. Audits01/02 failures
  remain immutable historical evidence; audit03 does not relabel them as passes.

Not compiled by this audit: the selected core, `headless/main.cpp`, actual
`eden_services.cpp`, system-font/HarfBuzz units, complete graphics libraries or
native packaging. No full native executable was linked or run. Object passes
do not close the virtual-Amiibo adapter, content I/O, forwarder argv, lifecycle,
firmware or runtime qualification boundaries.

`../native-discovery-receipt-01.json` records exact owned-byte preservation:
1,533 files match the previous preservation audit, no new/changed/removed owned
paths, clean `git diff --check`, syntax checks for all four external runners,
cited allowed-source hashes and Docker image ID
`sha256:f54e66776e386c9792ffbdd593048732c74ec3bc85482878ada26682b6e96873`.
This is equality to the preserved working tree, not to clean Git HEAD.

## Filesystem ABI inspection — v0.42, not v0.43

`../native-filesystem-abi-01/receipt.json` records captured SDK disassembly.
The verified v0.42 `libc.a` hash is
`17b22ab7baec2107ee4ce4189257abbd773a9bb9ce854203e08e0581e10b72b3`.
`filesystem-wrappers.log:458-476` shows `fstatat` and `openat` in `syscalls.o`:
load syscall number, move fourth argument, syscall, return. No carry test or
errno translation appears in those wrappers. This is static evidence, not a
claim about the runtime provider actually selected by a complete application.
Do not treat successful compilation or symbol names as POSIX return/errno proof.

Corpus cross-check, fully read through QMD (not saved as a catalogue review):
`Documentation/docs/knowledge-base/originals/docs/LMDL_FEATURE_A4_STORAGE_LIBRARY_STUDY_2026-09-30.md:17-36,143-170`.
That study concerns SDK **v0.43**, a different `libc.a` hash and a separate LMDL
module. It reports the same class of wrapper uncertainty but does not qualify
Encore v0.42, its native title provider, no-follow behavior or firmware 13.60.
`knowledge_related` returned general scope/host-versus-console lessons, not a
specific conflict resolution or runtime observation.

## Amiibo .bin integration — permitted partial source evidence

- `vendor/encore-active/eden-core/src/hid_core/frontend/emulated_controller.cpp:180-205`
  maps `engine:virtual_amiibo,nfc:1` and output slot 3 only for Player1/Handheld.
- `headless/CMakeLists.txt:1016-1028` selects this maintained controller source
  instead of the original; this is a build selection, not runtime proof.
- Controller source `:1427-1450` gates support by initialization, connection,
  guest controller style and virtual-device support. `:1469-1511` dispatches
  polling/reading to physical and virtual outputs; `:1546-1558` dispatches writes.
- `vendor/encore-active/eden-backports/src/core/hle/service/nfp/nfp.cpp:15-45,53-85`
  exposes guest NFP handlers including detection, application area and flush.
  These are service-registration tables, not the dump parser or load/remove API.
- `headless/main.cpp:1748-1767` selects HLE cabinet/applets by default and
  `:1940-1945` installs only controller/error frontend replacements there.
  The cabinet default is not an interactive virtual Amiibo presentation feature.

Still missing: permitted evidence for the virtual backend's load/remove methods,
supported dump formats, validation, writeback/path ownership, notification threading
and session teardown. No adapter, `.bin` picker or success label was fabricated.
The presence of a guest virtual NFC path does not reintroduce physical NFC
readers, antennas or drivers into P01.

## Safe next build boundary

`tools/prepare-build.sh:14-47,62-92` writes checkout-local cache pointers,
materializes/mutates a build-only Eden tree, creates SDK/build artifacts and
invokes graphics preparation. `tools/build-headless-native.sh` likewise consumes
that mutable layout. Do not run these entry points against the preserved live
checkout. A complete native build needs a disposable external workspace, pinned
dependency materialization, source-selection proof and separate receipts.

Serena was attempted but has no registered/active project and cannot resolve
the Windows project path in its server environment. No config or onboarding
artifacts were added to the repository as a workaround.

The adjacent `any-pad/README.md:15-19,75-87` describes a Bluetooth controller
bridge and firmware10.01 testimony. Its HCI-over-USB source communicates with
the console's Bluetooth chip; that is not a verified wired Pro Controller
transport for Encore or firmware13.60. No AnyPad code or hardware was operated.

## Campaign extension — 2026-11-07

[A49's native integration report](../../campaign-20261107/agents/A49.md) records
three actual native objects: font (reproducing an earlier unit), SystemFonts and
HarfBuzz12.3.2 (two previous gaps). Verified SDKv0.42/2,923 files and pinned
HarfBuzz492 files were used with the protected original Eden masked empty.

The first aggregate run remained ERROR because its depfile validator rejected
an unnormalized relative spelling. A separate immutable follow-up normalized
dependency paths and linked the same objects with `ld.lld -r`, without recompiling.
It resolved21 hb_ imports;60 symbols remain undefined. Final receipt SHA-256:
`f9e03b813558e66849e17c4161977825c78c126a7f7b03cb02350e93c13f6674`.
No native executable, system-font runtime or console result follows.

A11's SDK probe met8/8 syntax expectations on extracted preamble/consumers,
including the baseline PROFILE-only failure and four successful candidate modes.
It used a declared marker for development_input.h, not that header's native ABI.
Receipt SHA-256:
`d803b0968f18dfd728b5ea923b86759df4bd0699d359553d2456f6cf4db56aa0`.
Full main, argv/handoff/exit and core/link remain unverified.

TB-01 is still BLOCKED: maintained policy headers disagree (true/false); CMake's
native include recipe intends the true override, but final BSD/DNS/NIFM flags,
generated tree and include-resolved production dependencies were unavailable.
Inventory and recipe analysis are not production-selection/enforcement proof.
No header was edited, network enabled or I/O qualification gate raised.

### Wave02 — real A10 settings unit

The continuing campaign compiled the **real candidate** `settings.cpp` unit with
SDKv0.42 for `x86_64-sie-ps5`, using22 current app headers and1,001 dependency
hash records. Candidate source remains
`6a631c2fd977542daacea79c4b724b5868089b8e6885c980626094ceeb2bffda`;
checkout baseline remains unchanged. Flags derive from the prior UI-settings
audit recipe, not final core BSD/DNS/NIFM production selection.

New receipt: `A49/wave02-a10-settings-native-01/native/receipt.json`, SHA-256
`bb67bbcf12d2a9156f97448ac930b87c558906161c50ce8d4027207492c3236a`.
Object SHA-256:
`3c2e97eaa5503f37ed833d78cde5e3b29eb01573007bf4e67c131b9949ab317a`.
Coordinator checked14 delivered files, dependency-record count and baseline in
`integration-02/A49-settings-readback-01.json`. Docker cleanup completed.

This closes only the object-compilation gate for these candidate bytes, not
host render/locale/GL/rollback gates, native link/runtime or PS5 qualification.
The proposed extra A46 SDK probe stayed NOT_RUN because it did not cover the
real JPEG deleter. Native I/O qualifier remained explicitly0; TB-01 stays blocked.
