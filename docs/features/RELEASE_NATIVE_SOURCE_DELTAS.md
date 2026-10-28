# Native source deltas: final-build preservation inventory

2026-10-28; feature-local follow-up to
[the source-policy handoff](../RELEASE_SOURCE_POLICY_FOLLOWUP.md).

**Outcome: the concrete source/recipe routes are inventoried; final applied
bytes, used-source closure, binary matching and relink remain PENDING.** This
is neither a final release identity/receipt nor a compliance certification.

## Scope and custody

Recipe review base: `ee0bfc263416c9223631fb91b50c43d923ff038d`.
The delivered integration bundle still identifies application snapshot
`8ee544eb5077ad89bfbc848ea22c4cf4a375477d`, tree
`93665566189319ff7a072cf353e4cb3be242e1c1`, **not the future release HEAD**.
The application recipe bytes inspected here match that bundle's original
file hashes. Regenerate these comparisons against the coordinator's final HEAD.

Only static reads, hashing, JSON/AST parsing and public archive-member reads
were performed. No SDK wrapper, upstream generator, patch script, compiler,
configure, build, installation, network request or packaging ran. No old native
build/savednative was inspected or promoted. The coordinator's independent
review was not polled. The symlink/CRLF area needs no correction and is unchanged.

The old worker volume was mounted **RO** at `/policy`; the feature worktree
was mounted **RO** at `/repo`. All new private inventory is in the **new** volume
`prosperoeden-team-native-deltas-20261028`, `/work/native-delta-inventory`.
Containers used the existing `prosperoeden-host:features` image, `--network none
--cpus 2 --memory 512m` or `1g`, `--pids-limit 64` or `128`, `--cap-drop ALL`.
No received/coordinator/native volume was mounted writable. The old index23,
TARs, restores and their outputs were not modified.

Terminology below:

* **Retained original**: immutable source/public input already present, with a
  recipe or provenance reference. This is not proof that a binary used it.
* **Mapped**: a static recipe establishes an input → transformation → output →
  consumer route. Branches, exact object paths and include precedence require
  the actual final build graph; they are not inferred from filenames alone.
* **Missing evidence**: absent from this delivered evidence set or not bound to
  the future final build. It does **not** mean that a file is absent on the
  coordinator's machine. No such machine/cache/config scan was performed.

## Immutable original inputs, not replacements

Full trees/blob IDs/modes and individual SHA-256s remain in `SOURCE_BUNDLE.json`.
The private inventory adds selected recipe hashes, not new dependency pins.

| Logical source | Retained identity |
|---|---|
| Native boilerplate | `dd44bbdc75437332ed22e3ba95126733419ef25a` |
| Official payload SDK v0.42 source | `4eb701204fc3f8d31e84cf8ca272974e2be9c867` |
| RADV payload SDK fork | `b83202be73e930050e00de0f1b4d9ec46c0391bf` |
| RADV build/link recipes | `5b5e4fc2d80fdb67a4f61ba9e6a8a424026bb8a5` |
| RADV Mesa fork | `7b59ef27c1b09b9671bc4153c41940c3155c3af2` |
| OpenGL v1.0.2 source | `6b6bb2039f741287ed7f1bc6bc4b2c589709a0ba` |
| OpenGL PSBC / OpenGNM | `a92a1228ea3a64e4be9f0e61c2a65a5aa7ffed92` / `4b295ca54c82c83acf308d1c646a2dfa9ae57350` |
| OpenGL SPIRV-Headers / Vulkan-Headers | `0d25db97cb9b8f725e4c95e4553001710e7fc39d` / `b51f6b865c18fc5b33990d12f75e8dfd672cede6` |
| Pacbrew recipes v0.40.2 | `1687e827675c66e1b040751c7f0811222bd3aea0` |

| Original archive | SHA-256 |
|---|---|
| Payload SDK v0.42 ZIP | `8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da` |
| LLVM project 18.1.8 source | `0b58557a6d32ceee97c8d533a59b9212d87e0fc4d2833924eb6c611247db2f2a` |
| OpenGL SDK 1.0.2 public package | `30d8e51dd062739b629346102be5932728ba714bc50b1bdcdf08306c8843d9a4` |
| OpenGL Mesa 26.2.0 source | `efd4bb08cdb7c365a812cd4e6c9202ab55b2f22cdcd13c7d6c4f9647b799a4ef` |
| Pacbrew public package | `a85f65de418a8e6a898c6c3e3c870d50fff7618a200e4dd59ea9692af6ecec4d` |
| RADV zlib 1.3.1 / Meson wrap patch | `9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23` / `e79b98eb24a75392009cec6f99ca5cdca9881ff20bfa174e8b8926d5c7a47095` |

The retained SDK/package binaries are inputs, **not corresponding-source
substitutes**. All original 59 inputs, 15 supplements, five historical artifacts
and 22 provisional CPM omissions remain preserved. Boost selection is still
**1.92.0**; historical 1.90 and fallback 1.91 are not substituted or repinned.

## 1. SDK, runtime and import facades — routes N01–N06

Paths beginning `boilerplate:`, `vulkan:`, `sdk-fork:` and `opengl:` refer to the
pinned repositories above. `scratch` is the explicitly approved final native
workspace, not a search of the user's cache.

| Original → transformation | Generated/applied file → consumer | Final evidence to preserve |
|---|---|---|
| SDK ZIP → `boilerplate:tools/setup-native-dependencies.sh` | `boilerplate:.deps/native/ps5-payload-sdk`; its `target/` is copied by `tools/prepare-build.sh:67–71` to `scratch/sdk/target` → native compilation/FFmpeg and SDK imports | Both installed and copied used-file hashes, link-text/modes, and deltas against ZIP members. `.complete` is an existence guard, not an inventory. |
| SDK ZIP + fork → `sdk-fork:platform/tools/setup-sdk.sh:69–85` | Removes the release's empty `target/include_common`, installs fork compiler wrappers/headers and `libps5platform.a` → RADV build and Eden's RADV link | Exact installed overlay, source-file/object list and platform build commands. The script preserves base CRT/libc/C++/stub binaries; it does not rebuild them. |
| Fork `platform/src/*.c`, regex sources and `src/dlmalloc/mspace.c` → `platform/Makefile` | `platform/build/ps5/*.o`, regex/dlmalloc objects → `target/lib/libps5platform.a` | `Makefile.inc`'s actual compiler/archiver selection, all used headers and object/archive relation; not just `.ps5-sdk-revision`. |
| Official `crt/*`, `libc/*.c`, `sce_stubs/*.c`, `host/*` → official Makefiles | SDK CRT objects, libc archives, `.so` facades, wrapper/toolchain scripts → SDK users | SDK source exists; missing is the actual release/local binary build match. Toolchain files install from **`host/toolchain/`**, not source-root `toolchain/`. |
| LLVM 18.1.8 runtimes → official `libcxx.sh` | `libc++.a`, `libc++abi.a`, `libunwind.a`, C++ headers → final link | CMake cache, exact options/compiler/resource files, generated headers and archive/member hashes from the actual producer, or an explicit unresolved binary-match entry. |
| `tools/deps.json` compiler-rt `emutls.c` → `headless/CMakeLists.txt:117–124` | `EMUTLS_SKIP_DESTRUCTOR_ROUNDS 0 → 1`; `headless/emutls.c` → `common` | Postimage hash and its actual object. The separate `cxa_thread_atexit.reference.cpp` is a retained reference, not proof it was compiled. |
| `third_party/app_heap.c` + `heap_arenas.inc`; boilerplate `app_crt.cpp` → `headless/CMakeLists.txt:3331–3392` | `headless/app_heap.c`; conditional `headless/radv_app_crt.cpp` adds `ps5_fp_ieee()` → `eden-headless` objects | Heap arena/guard branch values, original/postimage hashes and compile commands. Do not confuse the app CRT with SDK payload `crt1.o`. |
| Boilerplate `libc_builder.cpp` + `runtime/api-surface.txt`, `imports.txt` → `build-host-tools.sh`, `rebuild-libc.sh` | `build/runtime-shim/libc-{a,b}.raw.elf` → signed `libc-{a,b}.prx` → `runtime/libc.prx` → packaged `sce_module/libc.prx` | Host builder sources/binaries, host zlib, commands, both comparisons, actual artifacts and final package equality. This clean-room runtime is **not** SDK `libc.a` or LLVM libc++. |
| `tools/stubs/libSceAgcDriver.c`, `libSceCommonDialog.c` → the two tracked `build-*-stub.sh` scripts | Temporary `-fPIC -c` objects → `build/stubs/*.so`; sonames `libSceAgcDriver.prx`, `libSceCommonDialog.sprx` → link and converter | Compiler/wrapper/linker bytes, exact argv, `.o` and `.so` hashes. `mktemp` EXIT traps delete the intermediate objects; preserve them during their final production, not by reusing old objects. |

The official/fork immutable source comparison is concrete: **1,986 identical
entries, six changed and 66 fork-only**. Changed paths are `Makefile`,
`host/bin/prospero-clang`, `host/bin/prospero-clang++`, `include/Makefile`,
`include/freebsd/sys/_ucontext.h`, `include/freebsd/sys/mman.h`. This describes
two source trees, not the actual installed SDK delta. The platform additions
and installed overlay still need final byte comparison.

Do not collapse these distinct runtime recipes: Pacbrew's `libcxx/PKGBUILD`
sets `LIBCXXABI_HAS_CXA_THREAD_ATEXIT_IMPL=NO`; official SDK `libcxx.sh` does not
explicitly set it. Both name LLVM 18.1.8. Equal versions do not establish equal
options or binary identity. Also, official `Makefile.inc` can select LLVM 22
downwards, and installed `prospero-llvm-config` prefers 21 downwards unless
overridden. `clang-18` on an app compile line therefore does not identify the
linker's provider or a driver generator's compiler.

Official `sce_stubs/Makefile` builds retained `.c` facades. Its optional `stubs`
regeneration route uses `genstub.py`, `.sprx` input and an unpinned NID URL.
Do **not** invent or obtain proprietary module dumps/SDK materials to fill that
route. Retain the existing source facades/tables and leave any unsupported
regeneration/provenance claim pending.

The runtime reproducer's expected raw hash is
`8ee6e124993e1af26420cb455890fd002f5d6c7e78883c860ce45734e7d002bb`;
expected signed hash is
`e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036`.
These are **script expectations**, not a reproduction performed here.

## 2. RADV applied source and generated driver — routes N07–N10

Exact order in `tools/build-radv-dependencies.sh:30–52`:

1. Rewrite only the C/C++ entries in `vulkan:tooling/radv/ps5-cross.ini` from
   `sdk / 'bin/prospero-clang[++]'` to the `['ccache', ...]` form. Generate
   `build/radv-tools/ninja` with the chosen bounded job count.
2. Upstream setup overlays the fork SDK. Upstream `build-radv.sh release`
   exports pinned Mesa into `.deps/work/radv-src`, builds host CLC/VTN tools,
   configures the target and may reuse an existing provenance guard.
3. Apply WSI → GS-compute → disk-cache → transfer-split adaptations.
4. Build upstream RADV release again, then write four `EDEN_*_SHA256` stamps.
   Changed source causes the patchers to set aside the upstream provenance
   file, allowing its cached-by-pin build to re-enter Ninja.

The exact **ten unique original source targets** are individually hash-bound
in `baseline-delta-targets.json`. Prefixes below are in `.deps/work/radv-src`:

| Applied path | Ordered transformation | Object/library consumer |
|---|---|---|
| `src/vulkan/wsi/wsi_common_videoout.c` | `patch-radv-wsi.py` + `radv-videoout-lifecycle.inc` | Vulkan WSI objects → RADV archive |
| `src/amd/vulkan/radv_cmd_buffer.c` | WSI resets from the immutable original; GS sink; transfer-split | RADV command-buffer object → RADV archive |
| `src/amd/vulkan/winsys/ps5/radv_ps5_cs.c` | WSI submission-statistics gating | PS5 winsys object → RADV archive |
| `src/amd/vulkan/radv_physical_device.c` | WSI initialization diagnostics | RADV physical-device object → RADV archive |
| `src/amd/vulkan/winsys/ps5/radv_ps5_winsys.c` | WSI queue diagnostics | PS5 winsys object → RADV archive |
| `src/amd/vulkan/winsys/ps5/radv_ps5_platform.c` | WSI + `radv-host-memory.inc` | PS5 platform object → RADV archive |
| `src/amd/vulkan/cl/radv_gs_compute.cl` | GS-compute sink changes | `libradv_cl.spv` → `libradv_cl.cpp/.h` → RADV objects/archive |
| `src/util/disk_cache_os.c` | Remove only the elevated-user cache disable check | Mesa util objects → RADV archive |
| `src/amd/vulkan/meta/radv_meta_buffer.c` | Transfer-split call sites | RADV meta object → RADV archive |
| `src/amd/vulkan/meta/radv_meta_copy.c` | Transfer-split call sites | RADV meta object → RADV archive |

Preserve the **final** postimage of multiply-written files, not only the first
WSI output. Its `--digest` hashes the adapted display text plus a fixed adaptation
name; the other stamps hash patcher source. These stamps do **not** hash all ten
postimages, the `.inc` closure, generated shaders or final object contents.
Keep them as supplementary data alongside actual file hashes/diffs.

Additional exact generator routes:

* `vulkan:tools/build-radv.sh:75–119` builds host `mesa_clc` and
  `vtn_bindgen2` in `.deps/work/radv-clc-build`; `.deps/work/radv-clc-bin`
  contains links used through PATH. Record both link-text and resolved,
  separately approved tool inputs. Host LLVM/Clang, LLVMSPIRVLib, SPIRV-Tools,
  OpenCL resource headers and any selected libclc data need actual identities;
  target `-Dllvm=disabled` does **not** remove host generator dependencies.
* Mesa `src/poly/cl/{geometry,tessellation}.cl` → `mesa_clc` → `libpoly.spv`
  → `vtn_bindgen2` → `libpoly.cpp/.h` → `libpoly_nir` → RADV. The edited
  `radv_gs_compute.cl` has its own `libradv_cl.*` route. Preserve command-line
  include/define arguments and depfile inputs, not SPIR-V alone.
* Mesa NIR/ACO, tracepoint, entrypoint, drirc, register and Vulkan generators
  contribute additional source/headers. The exact final Ninja/Meson graph,
  generated outputs, Python/Mako/PyYAML/packaging versions and build options
  are missing; this static list does not replace that graph.
* `subprojects/zlib.wrap` requires the retained **1.3.1** source and patch ZIP;
  preserve applied Meson overlay/configuration and its objects. It is distinct
  from Pacbrew/host zlib **1.3.2**.

Target options are recorded by upstream `.radv-options`: AMD-only PS5 winsys,
LLVM/GL/EGL/GBM disabled, forced zlib fallback, shader cache enabled,
`mesa-clc=system`, `buildtype=debugoptimized`, release `b_ndebug=true`, build ID
set to the Mesa pin. Preserve actual `.radv-options`, `.radv-build-id`, both
cross files, `compile_commands.json`, Meson introspection/coredata, Ninja rules,
response files and setup/build logs; defaults and `PROVENANCE.txt` alone are
not proof of the used configuration.

The target `src/amd/vulkan/libvulkan_radeon.a` is installed as
`radv-release/lib/libvulkan_radeon.ps5.a`. `tools/isolate-radv.py` reads it
without modifying it, uses **host** `nm`, `c++filt`, `objcopy`, `ranlib`, and
produces `build/radv-isolated/{symbols.map,libvulkan_radeon.ps5.a,manifest.json}`.
Preserve original/isolated archive bytes, member order, mapping and actual tool
hashes. A namespace-check manifest is not a source/binary or relink receipt.

At final link, `tools/radv-link-eden.sh` consumes the upstream platform recipe,
flattens group delimiters and removes its heap-wrap subset while retaining the
Eden heap. It also emits the `vkGetInstanceProcAddr` alias. Upstream creates
`vulkan:build/radv-platform-local.map` and chooses builtins from the resource
directory of **`PS5_CLANG`, otherwise unversioned `clang` before `clang-18`**.
Capture that concrete builtins archive, fork platform/C++/ABI/unwind archives,
all imports and actual expanded argv. The outer linker additionally names the
application SDK C++/ABI/unwind archives. Do not silently deduplicate the two SDK
sets by basename. Eden's outer link actually uses boilerplate `ps5-pie.ld`,
`tools/unwind.ld` and `tools/app-symbols.map`; the upstream recipe's separate
returned unwind-script array is not consumed by this outer link script.

## 3. OpenGL retained sources versus applied/binary proof — routes N11–N14

**Do not report the public package's embedded sources as absent.** Static
inspection of the already-retained OpenGL binary package verified:

* All **45** `sdk/manifest.sha256` file hashes, and all **ten** nested source
  archive hashes declared in `provenance.json`.
* `sources/ps5-opengl.tar`: **626** regular files, same bytes and executable
  bits as the supplemental OpenGL Git source; `sources/opengnm-psbc.tar`:
  **1,264** regular files, same bytes and executable bits as its supplemental
  Git source. Tar raw permissions add group-write; this is recorded separately,
  not mislabeled as a source-content delta. No missing/extra regular source
  paths were found in those two comparisons.
* Package provenance names source commit
  `a519561626933257213d64a4a8bd5e02b55dceb2`, whereas the supplied Git pin is
  `6b6bb2039f741287ed7f1bc6bc4b2c589709a0ba`. The byte comparisons above
  reconcile the two delivered source file sets; they do not prove a commit's
  ancestry or the original builder's execution.

The public package carries runtime archive hash
`7fd5d7a1beb7bf5af42e6a6936b88b1266cba3f224d3a60c3a6a09285e339ae1`,
PSBC archive hash
`32699de1cdd3f51e879d9c54ed3b750a76832f7c3435578812c3098b654b6c57`,
and SDK manifest hash
`0d432edc9d648d6e0cccb4d5eb3db8484f10ba4b075b5ccf23edda3251b66360`.
Its `consumer-validation.json` records a wrapper reporting **Clang 21.1.8**;
that is upstream consumer-check data, not a measurement of the future Eden
builder or proof of how every library was produced.

| Retained original → exact recipe | Applied/generated output → consumer | Still missing |
|---|---|---|
| Mesa 26.2.0 + `toolchain/mesa-ps5.patch` → `apply-mesa-ps5-patch.sh`, `build-mesa-ps5.sh` | **25** original patch targets; Mesa config/NIR/GLSL/GLAPI/format codegen → installed Mesa component libraries → OpenGL GROUP script | Actual postimages, generated bytes, cross/compiler/diagnostic options and binary build match. All 25 originals and patch bytes exist. |
| PSBC pin + `toolchain/opengnm-psbc-ps5.patch` → `tools/fetch-sources.py` patch/tree verification | **66** patch targets: 50 original paths, 16 test additions created by patch → patched compiler tree | Exact applied tree/postimage hashes; tests added by patch are not missing original source. Preserve the entire patch, not just runtime hunks. |
| PSBC patched tree → `build-opengnm-psbc.sh` + upstream Makefile | 17 `GENERATED` outputs (NIR/ACO/SPIRV/registers) plus 11 explicitly materialized format/builtin/stats/Vulkan/packet outputs → `.ps5.o`, `.ps5.cpp.o` → `libpsbc.ps5.a` | Generator/module versions, Make command expansion, generated bytes, actual target compiler and `libpsbc.ps5.identity`. Default absent `config.mak` is overridden by the retained host/PS5 `.mak` files, not a missing dependency. |
| OpenGL `src/egl`, `src/gallium/ps5`, `src/platform` → `tests/ps5/native-app.mk`, `toolchain/ps5-opengl-core33.mk` | Six runtime objects → `libps5_opengl_core33.a` | Actual `runtime-config.txt`, defines and source/object/archive relation. `ps5_agc_runtime_backend.c` includes the native runtime source; retain included source too. |
| Mesa `gl.xml` + GLAPI bridge/shared archives → `tools/generate-gl46-entrypoints.py` | `gl46_entrypoints.c` → `gl46_entrypoints.o` → runtime archive | Input archive/registry/tool hashes plus generated C; XML alone does not describe the generator's symbol inputs. |
| Runtime config, screen/cache sources and PSBC/Mesa archives → `tools/shader-cache-build-id.py` | `shader-cache-build-id.h` → `ps5_screen.o` → runtime archive | Exact generated header and all hashed inputs. |
| `native-app/agc_link_stub.c`, `agc_driver_link_stub.c` → runtime Make fragment | `agc*_link_stub.o` → SDK `libSceAgc.so`, `libSceAgcDriver.so` | The package already contains the public facades; final consumed facade list and any replacement need comparison. |
| Runtime/PSBC/Mesa libraries → `install-ps5-opengl-gl46.sh` → `install-ps5-opengl-core33.sh` | Materializes thin archives with `ar -M`; installs headers, imports, aliases and manifest → `PS5OpenGL::OpenGL` | Pre/post install/archive member relation and final app prefix equality or explicit local delta. |

`sdk/lib/libPS5OpenGL.a` and `libPS5OpenGLCore33.a` are **text GROUP linker
scripts**, not ordinary archives. Resolve and preserve their 19 named component
libraries; do not run `ar` on those scripts or treat their small hash as the
whole OpenGL link closure. The interface also imports Agc/AgcDriver, VideoOut,
kernel_web and SystemService. Eden replaces kernel_web with kernel in its link
wrapper; when Vulkan is enabled it substitutes its superset AgcDriver facade
both in CMake and package-converter selection (`headless/CMakeLists.txt:3816–3821`,
`tools/package-headless-native.sh:26–41`).

The package runtime defines include dynamic scanout with 2160/120 capacity;
the installed header declares 1920/1080/60 startup. The installer deliberately
normalizes the startup header in dynamic mode, so these fields are not by
themselves a contradictory build receipt. Retain both; the exact final profile
still needs confirmation. Upstream describes this package as host-checked,
**not console-validated**; this work does not upgrade that claim.

## 4. Eden/CPM, derived sources and other native generation — N15–N20

The previous offline restoration already tested five Eden patches, selected
upstream CPM fetch/patch/reuse, and the **source-header prefix only** of Xbyak's
overlay. It did not evaluate the full native target configuration below.

| Source → transformation | Output family → consumer | Required final capture |
|---|---|---|
| Eden `67bada77` archive → five `tools/eden-patches/*.diff` in name order, then CPM's original patch route | Patched Eden/selected CPM sources, patch keys, `GIT-COMMIT`/`GIT-REFSPEC` → all native targets | Original and applied inventories at final HEAD; individual final evidence for all 22 CPM non-use decisions. |
| Patched Eden and tracked `.inc` bodies → `headless/inject.cmake`, `headless/CMakeLists.txt`, included `.cmake` recipes | Derived `.cpp` files and override headers → common/core/Dynarmic/video/audio/network/etc. | All actual source lists, options and include precedence; record before/after target mutations, not only generated `.cpp` hashes. |
| Dynarmic + Xbyak 7.40.1 → JIT alias, shared-jit, register allocator, fast paths, scalar memory, guest-code-cache, page reuse/table recipes | `block_of_code.cpp`, A32/A64 emit/interface files, `constant_pool.cpp`, `reg_alloc.cpp`, `callback.cpp`, optimizer/scalar/opcode/page-table outputs and `include/{xbyak,dynarmic}` overrides → Dynarmic/common/core objects | Remaining native derived postimages and consumers are untested. Xbyak's prior generated header hash is not final object evidence. |
| Boost 1.92.0 → native block `headless/CMakeLists.txt:3322–3330` | Remove `Boost::process` library link, retain `boost_process` interface include/link properties → core | Actual resulting target graph and header use; no claim that the remaining interface works offline. Do not replace with retained Boost 1.90 or fallback 1.91. |
| Patched Eden Vulkan files + `prepare-vulkan-port.py`/tracked `.inc` files | **39 static source-route entries, 38 unique output paths**, including a second-stage swapchain rewrite → video_core | Original hashes/routes are listed in the private inventory. Capture final postimages plus header depfiles; literal `adapt()` calls alone miss memory-allocator/pipeline-cache and loop-generated outputs. |
| `vulkan_hud.{vert,frag}` → `glslangValidator -V --target-env vulkan1.0`, `spirv-val --target-env vulkan1.0`, Python word packing | `vulkan_hud.{vert,frag}.spv`, `vulkan_hud_shaders.h` → derived `vulkan_window_adapt_pass.cpp` | Actual generator binaries/source identities, argv, SPIR-V and header hashes. The current helper performs this HUD stage pair twice; retain the actual invocation sequence. |
| Upstream shader GLSL/FidelityFX/templates → `src/video_core/host_shaders/CMakeLists.txt` | `StringShaderHeader.cmake` source headers and glslang `*_spv.h`, including integer MSAA variants, target `spirv1.3` → video_core | Final generated headers, template/includes and exact options; do not conflate with HUD's Vulkan 1.0 route. |
| `loading_{scene,text}.glsl`, `runtime_loading.glsl`, `sce_sys/param.json` → headless CMake | `include/loading_*_glsl.h`, `version/app_version.h` → eden-headless | Exact combined/generated bytes and final identity values; none are generated here. |
| FFmpeg `c7b5f153` → `tools/build-headless-ffmpeg.sh`, `ffmpeg-ps5-cc.sh` | Configure outputs, generated headers and native avcodec/avfilter/avutil/swscale archives → Eden FFmpeg consumers; **swresample is explicitly disabled** | Preserve `config.h`, `config_components.h`, `ffbuild/config.mak`, `ffbuild/config.log` when present, actual compiler commands and SDK headers/provider inputs. Decoder selection is h264/vp8/vp9, filters yadif/scale, cross x86_64/freebsd, static/PIC/pthreads, autodetection/network/programs/docs/avdevice/avformat disabled. |
| Pacbrew OpenSSL 3.5.2, zlib 1.3.2, curl 8.18.0, libpsl 0.21.5 originals + retained PKGBUILDs | Generated configuration headers/archives → app cdeps | Exact published/local build correspondence and selected `EDEN_CDEPS_SOURCE`; pristine sources/recipes already exist. Curl's recipe uses autoreconf and a network CA-bundle generation step; do not execute it here or replace its data with a local credential/certificate store. |
| Six tracked update-check/HTTP C sources + pacbrew curl/libpsl archives → `headless/CMakeLists.txt:3750–3788` | OBJECT members → `ld.lld -r` `update-check.o.all` → objcopy symbol renaming → `update-check.o` → eden-headless, with SSL/crypto/zlib | Preserve the intermediate aggregate, renamed object, actual rename argv and every input library; the final object alone hides this transformation. |
| Actual app ELF → `tools/build_identity.py stamp`, native converter/signing | Unstamped ELF → stamped `llvm-pie.elf` → `eboot.elf`/`eboot.bin` | Actual before/after hashes and tool/command inputs, under the coordinator's final workflow only. This inventory creates no final identity. |

`recipe-hashes-v3.json` binds **175** selected recipe/source files, and
`static-source-io-v3.json` indexes **446** CMake I/O/target call sites with
verified line contexts. The index is deliberately symbolic: it is **not** 446
generated files, an evaluated CMake graph, or a claim of comprehensive source
closure. The final compile graph must also identify any used generated CPM/
network-library headers and other unlisted generator inputs.

`ENABLE_LTO=OFF` is insufficient for object compatibility: the overlay adds
`-flto=thin` to Dynarmic, and to native video_core together with `-march=znver2`.
Keep the actual compatible bitcode/objects, linker/plugin/provider identity and
response files. Do not replace them with objects from an older build.

## 5. Concrete missing-evidence ledger

| ID | What is really not delivered/qualified yet | Minimal next observation |
|---|---|---|
| M1 | Actual installed/copied SDK headers/wrappers/runtime/import bytes and local deltas | Compare the final used-file list to the ZIP plus the documented fork overlay; preserve every changed/added/removed item, original mode/link-text and producer recipe. |
| M2 | Actual RADV cross-file adaptation, ten patched source postimages and source/object/archive match | Capture the final applied tree delta, generator graph/output and both archive forms before final link. Four stamps alone cannot close it. |
| M3 | Actual host generator/toolchain identities and their immutable source/recipe mapping | Identify actual glslang, SPIRV-Tools, translator, CLC/VTN, LLVM/Clang/resources, Python modules and binutils providers. The 74 materials do not separately pin that used host-tool closure. No version guess or LLVM-18 substitution. |
| M4 | OpenGL applied/generated build provenance matching the consumed binaries | First compare the final installed SDK to the already-retained 45-file public manifest. Preserve any local delta. Published sources/patches/flags already exist; generated-output/producer correspondence remains unqualified even if prefix bytes match. |
| M5 | Full native derived-source evaluation, include precedence, Boost.Process target treatment and used transitive source list | Record actual final CMake/Ninja graph, generated files and compiler dependencies; re-review omissions against it. Do not call the previous source-only restoration a native configure. |
| M6 | Final-produced clean-room runtime, temporary stubs, generator outputs and aggregate object/link closure | Preserve transient outputs during production and hash-bound producer/consumer records, not after a cleanup/reuse decision. |
| M7 | Final clean coordinator HEAD, final binary-bound provenance receipt and a successful modified-library relink | Coordinator-owned final build, independent provenance verification and actual modified-library relink. No `READY` status is justified by this inventory. |

Nothing above authorizes network acquisition, installation, executing unknown
SDK sources, or reconstructing proprietary source. Unsupported mappings remain
explicitly pending. Existing source policy, graph/privacy validation and
license/source preservation still apply; this document introduces no exception.

## 6. Minimal executable preservation plan for the next final build

**Future coordinator actions only; none of these build stages ran here.** Use
new private storage and approved explicit paths. Do not copy a volume/cache,
dump the environment, run shell tracing over setup/packaging, collect userdata,
or overwrite any existing evidence. Do not use `make -n` as a safe inspector:
Makefiles may execute `$(shell ...)` while parsing. Inspect already-produced
Ninja graphs without executing their rules instead.

### A. Freeze originals and the allowed capture list

1. Record final clean app HEAD/tree, approved upstream commits/trees, originals'
   hashes and source-policy identity. Keep the original materials mounted RO.
2. Build a **reviewed, explicit** `(logical root, relative path)` list using the
   ten RADV targets, 25/66 OpenGL patch paths and Vulkan routes in this private
   inventory, then the actual final graph's used files. Add required producer
   recipes, generated inputs/outputs, libraries, objects and link scripts.
   Do not expand it with `find`, globbing a user cache, symlink traversal or
   copying all of `build/`, `.deps/`, SDK installations or a volume.
3. Keep separate phase captures: `original`, `applied` (including all later
   rewrites), `generated`, `objects`, `link` and `package`. A reused binary needs
   its original producer evidence; an existence/timestamp stamp is not enough.

For final source deltas, retain before/after bytes **and** a patch/difference
description; hash both. For symlinks retain raw link-text, never target reads.
Use the delivered source-policy validation for component graph/alias/privacy
review; a reviewed path list is not a new `.env.*` approval mechanism.

Example capture-list format (paths supplied by the coordinator, not discovered):

```json
{
  "roots": {"radv-applied": "/approved/vulkan/.deps/work/radv-src"},
  "files": [
    {"root": "radv-applied", "path": "src/amd/vulkan/radv_cmd_buffer.c"},
    {"root": "radv-applied", "path": "src/amd/vulkan/cl/radv_gs_compute.cl"}
  ]
}
```

The following non-recursive capture command is suitable for **quiescent,
private, already-reviewed** roots/lists. It stores regular bytes in an opaque
hash store and symlink text as metadata; it does not follow links, create links,
restore an SDK or declare closure. The example two-file list must be replaced
with the reviewed full list for each phase. Roots themselves must be approved
real mount/directory paths without symlink ancestors.

```sh
python3 -B - /work/NEW-final-capture applied /work/reviewed-paths.json <<'PY'
import hashlib, json, os, re, stat, sys, tempfile
from pathlib import Path
out, phase, selection = Path(sys.argv[1]), sys.argv[2], Path(sys.argv[3])
assert re.fullmatch(r'[a-z0-9-]+', phase)
plan = json.loads(selection.read_text())
dest = out / phase
dest.mkdir(parents=True, exist_ok=False)
objects = dest / 'objects'
objects.mkdir()
records, seen = [], set()
for item in plan['files']:
    owner, rel = item['root'], item['path']
    parts = rel.split('/')
    assert parts and all(p not in ('', '.', '..') for p in parts)
    assert '\\' not in rel and ':' not in rel
    assert not any(p.startswith('.env') for p in parts)
    assert (owner, rel) not in seen
    seen.add((owner, rel))
    root = Path(plan['roots'][owner])
    assert root.is_absolute() and stat.S_ISDIR(root.lstat().st_mode)
    p = root
    for segment in parts[:-1]:
        p = p / segment
        assert stat.S_ISDIR(p.lstat().st_mode), 'No linked parents'
    p = p / parts[-1]
    info = p.lstat()
    row = {'root': owner, 'path': rel, 'mode': oct(stat.S_IMODE(info.st_mode))}
    if stat.S_ISLNK(info.st_mode):
        data = os.fsencode(os.readlink(p))
        row.update(kind='link-text', linktext_hex=data.hex(), size=len(data),
                   sha256=hashlib.sha256(data).hexdigest())
    else:
        assert stat.S_ISREG(info.st_mode), 'No special files'
        with tempfile.NamedTemporaryFile(dir=objects, delete=False) as target:
            temporary = Path(target.name)
            digest = hashlib.sha256()
            with p.open('rb') as source:
                while chunk := source.read(1024 * 1024):
                    target.write(chunk)
                    digest.update(chunk)
        value = digest.hexdigest()
        stored = objects / value
        if stored.exists():
            with stored.open('rb') as existing:
                assert hashlib.file_digest(existing, 'sha256').hexdigest() == value
            temporary.unlink()
        else:
            temporary.rename(stored)
        row.update(kind='regular', size=stored.stat().st_size, sha256=value,
                   object='objects/' + value)
    records.append(row)
(dest / 'CAPTURE.json').write_text(json.dumps({
    'status': 'CAPTURED_UNVERIFIED_NOT_CLOSURE_OR_RELINK',
    'selection_sha256': hashlib.sha256(selection.read_bytes()).hexdigest(),
    'files': records}, sort_keys=True, indent=2) + '\n')
PY
```

### B. Preserve during production, not by rebuilding afterward

* Final SDK/platform: preserve actual used headers/imports/libraries and
  wrapper/host-tool resolution before/after overlay and before the scratch
  copy is accepted. If using unreproduced public binaries, record that producer
  provenance gap instead of claiming the sources rebuilt them.
* Final stubs: the two tracked scripts use the same shape of command below.
  During their authorized final production, retain the object in private custody
  and the resulting facade at the build's expected path. Do not execute this
  example now, change its flags silently, reuse a stale timestamp result, or
  perform an additional final build merely to obtain evidence:

```sh
# FUTURE approved Agc stub production; SDK/out variables are reviewed final paths.
PS5_PAYLOAD_SDK="$sdk" "$sdk/bin/prospero-clang" -fPIC -c \
  "$root/tools/stubs/libSceAgcDriver.c" -o "$out/libSceAgcDriver.o"
"$sdk/bin/prospero-lld" --shared -soname libSceAgcDriver.prx \
  -o "$out/libSceAgcDriver.so" "$out/libSceAgcDriver.o"
sha256sum "$root/tools/stubs/libSceAgcDriver.c" \
  "$out/libSceAgcDriver.o" "$out/libSceAgcDriver.so"
```

  CommonDialog uses its own source/object/facade and soname
  `libSceCommonDialog.sprx`. Integrate the retained result into the approved
  final recipe once; the example alone is not evidence that the final link
  consumed it. For the runtime, keep the actual reproducer's `a`/`b` raw/signed
  files, host builder records and checks before packaging.
* Final RADV: after all four patches and generator runs, capture the ten
  postimages plus include/script/config closure and generated source. Preserve
  release and isolated archives and their mapping. Retain host CLC/VTN files,
  both build graphs and exact host dependencies. Do not restore over the
  existing source-only restore or convert old driver provenance into final data.
* OpenGL: if consuming the public SDK, compare its concrete installed files
  against the retained manifest; preserve local differences and actual GROUP
  expansion. If a local SDK is produced, additionally preserve the patch,
  generator and installer stages above, including pre-materialization thin
  archive member files. No local rebuild is requested by this document.

### C. At the single final build, retain graph and actual execution

For each **already-configured final** native/RADV/CLC/Mesa Ninja directory,
run the following inspection after its build. Here `b` and `name` are explicit,
reviewed paths/labels, not a directory-discovery loop:

```sh
ninja -C "$b" -t compdb > "$out/$name-compdb.json"
ninja -C "$b" -t commands > "$out/$name-commands.txt"
ninja -C "$b" -t deps > "$out/$name-deps.txt"
ninja -C "$b" -t targets all > "$out/$name-targets.txt"
```

Also retain each existing `compile_commands.json`, `build.ninja`, included
rules, `.ninja_deps`, `.ninja_log`, response files, generated source/headers,
depfiles when retained, Meson `meson-info/intro-*.json` and `meson-private/coredata.dat`,
and CMake cache/generated target data **selected/reviewed for this purpose**.
Keep exact options/argv and capture-tool versions/hashes. Do not scan or export
the environment, ccache contents, client configuration or credentials.

`ninja -t commands` records the outer wrapper command, **not** the evaluated
shell arrays or actual linker provider. Use the coordinator's existing trusted
exec/link recorder during the single final build. If an approved existing
`strace` is available, this is an alternative argv-only recording pattern:

```sh
# FUTURE only: coordinator sets final_command to its one approved build argv.
# No -v environment dump; no install/download to make this example possible.
strace -f -s 1048576 -e trace=execve -o "$out/final-execve.txt" \
  -- "${final_command[@]}"
```

Review logs privately before any sharing; reject truncated/unresolved execution
records. Response-file contents and the link map must be preserved separately.
Where the approved workflow permits, preserve Ninja response files during that
same build (`-d keeprsp`); a post-build tool listing cannot recreate a deleted
response file or prove it was consumed. If the recorder or a transient capture
is missing, leave that edge pending rather than simulate a second final build.

For each actual ordinary archive, retain its complete bytes and member order
with the actual approved archiver's `t` command; record duplicate member names
without flattening them through `ar x`. Thin archives additionally require their
referenced member files and path relation. GROUP scripts require their resolved
library list instead. Keep `update-check.o.all` and `update-check.o`, the actual
resource builtins, SDK/imports, linker scripts, symbol maps and every compatible
non-LGPL object required by the separate relink workflow.

### D. Bind and stop at the appropriate status

Hash every preserved original/postimage/generated file/tool/object/library,
command/log/response file, selected configuration and final output. Record for
each edge: original identity, ordered transformations/argv/options/tool hashes,
postimage hash/mode, generated file hash and actual consumer object/library.
Compare the final link inputs to the recorded compile/generator dependencies;
resolve every unmatched path or leave it pending. Preserve before/after identity
stamping, converted ELF/package hashes, final runtime and facade selections.

The coordinator must regenerate source inventories/omission reviews at final
clean HEAD, independently verify provenance, and actually relink with a modified
library. Captured objects/receipts alone do not establish `READY`; the delivered
collector's `COLLECTED_UNVERIFIED_RELINK` limitation still applies.

## 7. Private evidence and verification of this inventory

Final inventory index: `/work/native-delta-inventory/native-delta-evidence-sha256-v2.json`
in `prosperoeden-team-native-deltas-20261028`, SHA-256:

`069c8f97617abbb8d700307ee69038dce2834eb1db82da76988e10b3fa2b4e12`.

Only these **five selected JSON files** are the indexed handoff, not the whole
volume or intermediate diagnostic versions:

| File | SHA-256 |
|---|---|
| `recipe-hashes-v3.json` | `0f68d5cc4b86032c262fe1da08770d8ab5d3be8f892677f70e5413d06ed2ac7f` |
| `static-source-io-v3.json` | `de3e1f747752d4616ff039cdb2bcd0de2bf26295635e8105ca0d0f81022742dc` |
| `baseline-delta-targets.json` | `7ee6c9dff0c8be53ead8cb2907b6925a6542fae9121a927ed40ea2d90b78eaf2` |
| `native-delta-routes-v2.json` | `0fd2e85aeecfdabc9d972b33bc9ddb6d6f9c51b73dffa79831dc9e1342fd1f17` |
| `opengl-package-static-audit-v3.json` | `78fdf5d2395d03cf32e06419d0f3471041e652a5fe5fd4e4630daa5f01fff0c3` |

Static validation passed: 175 recipe/source hashes against their retained
proof entries and read bytes, 446 exact call-site line contexts, 20 routes,
ten RADV originals, 25/66 OpenGL patch-target paths, 39 Vulkan source routes,
45 public SDK manifest hashes, ten nested archive hashes and 1,890 compared
nested source files. **No native generator/build tests were run.** Original
collector/tests/helpers/build scripts are unchanged; previous 79-test results
remain historical evidence, not tests of a native build performed here.

The old index23 anchor
`5cc34f32f5e63998859abc79dbece599c4f00b93acb19b4f61a84b07a4cc34e5`
and old restore-result anchor
`0e820ddd9da565904b238f71f3ceec4d8cc490db43ed204dbb7ad3aa3922685a`
were rehashed from the RO mount. The old bundle SHA-256 remains
`540dcdae30b5045f7b4fc691960e9015b029e208e3fa755563bef0a4b2939ac5`;
this task did not repeat the coordinator's full verify/create/restore review.

To check only this new handoff, mount the new volume **RO**, use the same capped
offline image, and run this read-only index verification:

```sh
python3 -B - <<'PY'
import hashlib, json
from pathlib import Path
p = Path('/work/native-delta-inventory')
index = p / 'native-delta-evidence-sha256-v2.json'
assert hashlib.sha256(index.read_bytes()).hexdigest() == \
    '069c8f97617abbb8d700307ee69038dce2834eb1db82da76988e10b3fa2b4e12'
data = json.loads(index.read_text())
assert len(data['files']) == 5
for name, row in data['files'].items():
    f = p / name
    assert f.is_file() and not f.is_symlink()
    assert f.stat().st_size == row['size']
    assert hashlib.sha256(f.read_bytes()).hexdigest() == row['sha256']
print('PASS: five private static-inventory files; not final build/relink evidence')
PY
```

Only this new documentation is added to Git. No source bundle, prior evidence,
runtime/services/USB/UI/CMake/build-script/version/catalog changes, new agent,
push, tag or publication are part of this handoff.
