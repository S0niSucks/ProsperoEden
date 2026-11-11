# Switch 2 upstream comparison for Encore — research only

Research date: **2026-10-31**, approximately 05:33–05:38 UTC. Pins and availability are observations from that window, not promises about future branch heads.

## Decision

**Keep Switch 2 unsupported in player-facing Encore builds.** The Eden-derived experiments are useful research inputs, not a demonstrated PS5 port. Prioritize a provenance review and a PS5 memory/graphics feasibility gate before planning implementation. Do not import a whole fork, turn on success-returning service stubs, or advertise compatibility based on these sources.

The most important findings are:

1. **Takoroka/eden-nx2 and chrisjenkynw/notx2 have the same experimental branch head.** They are not two independent confirmations of compatibility at these pins.
2. **Eglantine has an accessible, newer experimental head and matching Windows/Linux prerelease metadata.** This demonstrates publication of artifacts, not their execution, correctness, or PS5 suitability.
3. **The documented memory strategy is a major feasibility risk.** Eglantine notes report a 12 GB guest layout requiring 20 GB of free Windows commit at startup, while texture expansion can consume several GB more on the GPU. Commit, resident RAM, GPU allocation, and virtual address space are different quantities; none may be treated as a verified PS5 budget.
4. **The GPU work is substantially more than accepting a new game format.** The notes describe Ampere SM86 shader lowering, Turing MME macros, QMD compute, new graphics services, synchronization, sparse memory, ASTC handling, and missing task/mesh geometry.
5. **Oboromi explicitly cannot run commercial games or firmware.** Its ARMv9/SM86 work is an architectural reference, not a fallback game-running engine.

## 1. Exact accessible pins and lineage

| Requested upstream | Default branch at inspection | Relevant research branch and exact commit | Commit date | Provenance / interpretation |
| --- | --- | --- | --- | --- |
| [Takoroka/eden-nx2](https://git.eden-emu.dev/Takoroka/eden-nx2) | `master` | `eden-nx2` → **`50fa86bbbc403ad565eb44fcc281f16f77c69e15`** | Branch API timestamp `2026-10-29T19:23:21-04:00` = `2026-10-29T23:23:21Z` | Forgejo identifies its parent as `eden-emu/eden`. Experimental head message: “nx2: fix the Linux (GCC) build of the vi2 present stats”. A compile-fix message is not a Linux gameplay test. [E-META, E-BRANCHES] |
| [Djihads80/Eglantine](https://github.com/Djihads80/Eglantine) | `nx2` | `nx2` → **`b40ad217ab829226841cb71f6e4c5cb05977a225`** | Author/committer `2026-10-31T01:10:10Z` | README declares derivation from Eden, Yuzu and Sudachi. GitHub reports `fork: false`; that hosting flag does **not** establish independent authorship. Head is a merge titled “core: end device page runs where pages are unmapped or mapped again”. [G-META, G-BRANCH, G-README] |
| [chrisjenkynw/notx2](https://git.eden-emu.dev/chrisjenkynw/notx2) | `master` | `eden-nx2` → **`50fa86bbbc403ad565eb44fcc281f16f77c69e15`** | Same branch API timestamp as Takoroka above | Forgejo identifies parent `Takoroka/eden-nx2`. Same experimental commit; independently retrieved NX2 notes also have the same blob ID. No distinct implementation benefit is established for this branch. This is **not** an exhaustive claim that every ref in both repositories is identical. [N-META, N-BRANCHES, N-NOTES] |
| [Nikilite/oboromi](https://git.eden-emu.dev/Nikilite/oboromi) | `main` | `main` → **`b97ea0cb32a0ef564ebf039bd8878dc344776e13`** | Author timestamp `2026-10-29T04:19:15Z`; Forgejo repository HTML displays `2026-10-29T05:19:46+01:00` for the latest commit | Forgejo explicitly declares a mirror of [0xNikilite/oboromi](https://github.com/0xNikilite/oboromi), last synced `2026-10-30T23:52:19+01:00`. GitHub `main` independently resolves to the same SHA. Do not confuse mirror sync time with development time. [O-META, O-BRANCHES, O-GH-BRANCH] |

Both Eden Forgejo repositories also resolve `master` to **`a2aa4d97e10a8345babfe647e1abd5ed5909c623`**, timestamp `2026-10-29T17:10:37+01:00`. Their landing-page README identifies Eden as a **Switch 1** emulator. Its broad “most commercial games at full speed” wording is not Switch 2 evidence.

Oboromi additionally exposes `gpu/autogen-decoder` → `2d5f02d0bfd80ea1a4e54bf674ab78ae61a44549`, branch timestamp `2026-02-15T16:06:10+01:00`. Only the ref/message was inspected; its implementation was not reviewed and it is not the selected baseline.

Eglantine and Takoroka share the exact `cpmfile.json`, `docs/Build.md`, and `NX2_GPU_THREAD_SPLIT.md` blobs at the selected pins, but have different NX2 notes and top-level CMake blobs. This supports close documentary/build lineage, **not** an established whole-tree equivalence or a proven “Eglantine is a superset” relationship. The first 100 ancestor-list entries retrieved for Eglantine did not contain the Takoroka head; no complete ancestry proof was attempted.

## 2. Maturity: claims, documentary evidence, and what remains unproven

| Upstream | Stated maturity and claims | Evidence actually obtained | Limits / confidence |
| --- | --- | --- | --- |
| Takoroka/eden-nx2 | Repository description says experimental Switch 2 support. Release notes say it “needs a lot of fixes” and some mesh is missing. Publisher says only Windows can be tested reliably. | Detailed developer notes describe boot/rendering progress, approximate protocols, crashes and fixes, partial shader coverage, video-decoder work, and local A/B measurements. Release metadata lists one Windows x64 ZIP and marks it **prerelease**. | Substantial self-reported experimental work, not independently reproduced here. Inherited README and old summary sections must not be treated as current compatibility results. No demonstrated PS5 result. |
| Eglantine | GitHub description: **“VERY experimental Nintendo Switch 2 emulator”**. README names Mario Kart World at “decent/full speed depending on specs and configurations” and Splatoon Raiders at variable performance. | Pin-specific notes report much narrower conditions, failures and workarounds. Recent commit metadata includes device-memory, texture-view, text-rendering and loose-dump-listing fixes. `build-60` is a prerelease explicitly tied to the inspected SHA, with Windows x86_64 and Linux x86_64 assets. | README is a project claim, not a compatibility certification. No game, binary, screenshot sequence, benchmark, save progression, firmware boot, or AMD/PS5 runtime was tested by this research. Commit titles identify work, not proof of successful fixes. |
| notx2 | Same experimental project description; default README still describes Switch 1 Eden. | Same selected experimental commit and same NX2 notes blob as Takoroka. | No independent maturity increase established. Its release API returned **404**; that is not proof that no release or external artifact exists anywhere. |
| oboromi | Proof of concept / work-in-progress foundation. README explicitly: **“oboromi cannot currently run commercial games or firmware.”** | Rust core / Qt frontend manifests; declared Unicorn dependency; Vulkan bindings; documentation of future ARMv9 work. Forgejo release listing returned an empty array. | Clear non-player-ready scope. Source/dependency declarations do not prove complete CPU, GPU, kernel, or firmware support. No throughput or hardware minimum established. |

### Publication observations, not execution evidence

- **Takoroka:** [`nx2-2026-10-29`](https://git.eden-emu.dev/Takoroka/eden-nx2/releases/tag/nx2-2026-10-29), published `2026-10-29T23:39:33+01:00`, prerelease. Release API `target_commitish` is `b7ff03036c0eca49ed99d40973a6b94a410b88ae`, **not** the later branch head selected above. The tag object itself was not separately resolved. Asset metadata lists `Eden-Windows-nx2-2026-10-29-amd64-msvc-standard.zip`, 36,048,730 bytes. No archive was downloaded.
- **Eglantine:** [`build-60`](https://github.com/Djihads80/Eglantine/releases/tag/build-60), published `2026-10-31T01:59:10Z`, prerelease, explicitly names full SHA `b40ad217ab829226841cb71f6e4c5cb05977a225`. Asset metadata lists Linux AppImage and Windows ZIP, respectively 106,129,912 and 112,125,511 bytes. No macOS asset appears in the three newest release records inspected, despite the README's macOS claim. This is a reading limit, not a claim that macOS support is impossible or no older artifact exists.

### Internal documentation conflicts that must remain visible

The NX2 notes begin as an old “local experiment”, saying graphics are out of scope and nothing is pushed, yet later rows describe published graphics/video progress. Their “Where it stops now” and prioritized-next-steps sections also retain older title-screen problems and a six-core TODO despite later history describing six-core runs and further rendering fixes. The GPU split document says “off by default” in its status paragraph and subsequently says it became on by default after A/B runs. [E-NOTES, G-NOTES, GPU-SPLIT]

Treat those as a development journal with documentary drift, not a single consistent specification. Prefer identifying each row, setting, revision and host; resolve actual defaults and behavior in a separately authorized source/runtime review. Do not cherry-pick the fastest later row while concealing earlier/later failures.

## 3. Architecture and host/resource requirements

| Area | Takoroka experimental branch and same-pin notx2 | Eglantine selected `nx2` pin | Oboromi selected `main` pin |
| --- | --- | --- | --- |
| Structure | C++ Eden-derived emulator; NX2 services and GPU changes described in notes; Qt and CLI options in build manifest. | Same general C++ lineage; new fixes on an independently pinned revision. Shared documentation is not a complete source diff. | Primarily Rust core, C++17 Qt 6 debugging frontend, C/static-library boundary via Corrosion; not a drop-in replacement for Eden C++ components. |
| CPU guest / execution | Notes describe AArch64 instruction gaps beyond the inherited decoder: LSE atomics, CAS/CASP, LDAPR, PAC/RETAA/RETAB, MTE and some SIMD. Reported shortcuts include PAC as identity and MTE tags as zero. Early core folding is followed by six-core experiments. | Same inherited issue history. These approximations need semantic tests; boot progress does not establish general ARMv9 correctness. | README says a new ARMv9 emulator targeting T239-required extensions is **being developed**, credits vrtgs/rapid-arm-emu. Current `core/Cargo.toml` still declares `unicorn-engine = "2.1.1"`; CONTRIBUTING discusses moving from Unicorn to a custom ARM emulator. Future direction is not proof the replacement is integrated. |
| Host CPU | Public experimental binary is Windows x64. Root manifest has x86_64/arm64 platform handling, but NCE is only enabled for arm64 Android/Linux. | Latest inspected published assets are x86_64 Windows/Linux. An x86-64 PS5 would need translation/JIT rather than that arm64 NCE path. No supported PS5 JIT ABI is established here. | No gameplay-capable host CPU or core-count requirement stated. Rust edition 2024 and `rlib`/`staticlib` outputs are declared. |
| GPU model | Guest SM86 SASS lowered through the inherited shader machinery; Turing MME64 macros, QMD 3.0 compute, NVN2/new services and fences. Notes still identify absent task/mesh support and a bilinear substitute for a tensor-heavy upscale. | Same documented base issues, plus recent fixes. Shader lowerings, sparse textures, ASTC decode/recompression and new scheduler/thread behavior require independent correctness checks. | README targets NVIDIA SM86; core manifest declares `ash = "0.38.0"` (Vulkan bindings). Bindings and a target ISA are not evidence of a complete renderer. |
| Host GPU requirements | Release publisher specifies a **Vulkan GPU with 8 GB+ VRAM**. This is a published requirement, not a measured universal minimum. | Reviewed notes include RTX 4070 Laptop 8 GB and Ryzen 5 2600 + RTX 3050 6 GB examples; the latter is an uneven, tuned anecdote, not a recommended minimum. No vendor-independent or PS5 result demonstrated. | No GPU capability level, VRAM minimum, playable workload or supported PS5 graphics API established. |
| RAM / address space | Publisher specifies **16 GB+ RAM**. Notes use a **12 GB emulated layout**, describe added backing for graphics regions and a **64 GiB device address space**. GPU virtual space is not resident VRAM. | Notes explicitly report a failed **20,480 MiB backing allocation** and “20 GB of free commit up front” for the 12 GB layout (line 186). This is documentary evidence of one configuration, not a measurement of this head's RSS or PS5 peak. | No trustworthy minimum RAM/commit budget specified for actual games, because games/firmware are unsupported. |
| Texture memory | Notes describe 8192×8192×68 ASTC virtual arrays: around **1.5 GB guest data** versus roughly **23 GB expanded RGBA8** per array; trimmed mip levels/unbacked images and BC recompression are workarounds, with quality/coverage tradeoffs. | Same documented pressure; reports of multi-GB texture use/collection do not transfer directly to a unified-memory PS5 budget. | No validated game-sized texture residency budget established. |
| Documented build hosts | Inherited README lists Windows/Linux/macOS/Android/FreeBSD and more; experimental release author explicitly limits reliable testing to Windows. A Linux GCC fix is visible at the head. | README claims Windows/Linux/macOS. Latest Linux AppImage notes require **glibc 2.39+**; that packaged Linux userspace is not a PS5 ABI. | CONTRIBUTING provides Windows/Linux/macOS setup. No documented PS5 target or verified console build was found in the material reviewed. |
| Build dependencies | Root CMake declares **CMake 3.31**, **C++20**; Qt optional, CLI optional; manifests list SDL3, FFmpeg, SPIR-V/Vulkan headers and Vulkan Memory Allocator among dependencies. | Same listed `cpmfile.json` blob. Build guide is inherited, not a PS5 cross-build recipe. | Stable/latest Rust, Ninja, C++ compiler and Qt 6; CONTRIBUTING recommends Qt **6.12+**. Root CMake minimum **3.22**, Qt Widgets/Concurrent, Corrosion tag `v0.6`. Core depends on Unicorn, libc, memmap2, log and ash. |

**Guest versus host distinction:** emulating a T239/ARMv9/Ampere guest does not require an ARM host or an NVIDIA host GPU in principle. Conversely, an AMD host with sufficient advertised TFLOPS does not automatically provide an implementation of NVN2, SM86, mesh stages, sparse mappings, Vulkan extensions, or guest memory semantics. An adapter/backend and verified semantics are required.

**Performance reading limit:** the notes include low frame rates, later faster attract-loop/light-course figures, and an explicit warning that differing courses varied means by about ±15%, with background test loops interfering with earlier measurements. GPU split figures are local self-reported A/B data. Neither those data nor the README's “full speed” wording supports a PS5 frame-rate promise, complete-game claim, regression-free setting recommendation, or generic hardware ranking. [E-NOTES rows 97–99, 122, 130, 138–149; G-NOTES line 186, rows 130, 138–149]

## 4. License and provenance gates

### Eden-derived line

- Takoroka and Eglantine README declarations say **GPLv3 or later**. Both README ownership headers identify Eden and inherited Yuzu material, with GPL-3.0-or-later / GPL-2.0-or-later declarations. Root CMake declares Eden ownership and GPL-3.0-or-later. The selected notx2 commit is the same commit as Takoroka's, not newly independent ownership.
- Readable root `LICENSE.txt` files contain the GPL version 3 text and have identical blob ID `f288702d2fa16d3cdf0035b15a9fcbc552cd88e7`. A root license does not settle all individual files, generated tables, dependency terms, asset rights or contributions.
- The journal repeatedly credits code from **`changes.patch`** without establishing that patch's complete author/source/license chain in the documentation reviewed. That is an **unresolved attribution/provenance question**, not an accusation of improper origin. Require per-file and per-commit origin tracing before importing affected code.
- `cpmfile.json` provides versions/hashes for many dependencies, but is not a complete license audit or proof of every vendored modification. Its exact blob is shared by Takoroka and Eglantine: `00327347128e1a53ea61997183f970f89c670b74`.
- GitHub reports Eglantine's merge signature verified; Forgejo's selected Takoroka branch record reports an unsigned commit. Signature status concerns a commit's signing metadata, not code correctness, clean-room provenance, legal compatibility, or permission to redistribute game content.

### Oboromi

- README, core Cargo manifest and CONTRIBUTING declare **GPL-3.0 / GPLv3**. The root GPL text includes the standard example for applying “or later”; that boilerplate example alone must not be promoted into an explicit project-wide or-later grant. Clarify exact per-file grants if reuse depends on that distinction.
- `core/Cargo.toml` names Nikilite and Elizabeth. README credits vrtgs for the new ARM emulator direction. The external rapid-arm-emu repository was not audited or pinned by this task; importing it would require a separate provenance/license/dependency review.
- `.gitmodules` still declares `0xNikilite/dynarmic` and `0xNikilite/ext-boost` URLs, while the inspected core manifest declares Unicorn and the README describes future custom ARM work. Do not assume all named backends are active, interchangeable or already integrated. No submodule contents were fetched.
- Rust crates, Unicorn, Qt and Corrosion need their own exact version, license, ABI and redistribution review. A C ABI/static library is an integration mechanism, not an exemption from copyleft obligations.

**Distribution boundary:** preserve attribution, licenses, modification records and applicable source-offer/corresponding-source obligations. Confirm compatibility with the actual Encore distribution and SDK/toolchain terms in a separately authorized review; no private SDK/configuration was inspected. Do not bundle proprietary firmware, keys, shaders extracted from games, game binaries or account credentials. Public source hosting is not proof of rights to all such material. These are engineering/release gates, not a legal opinion.

## 5. PS5 port blockers, prioritized

Sony's public baseline PS5 specification is **x86-64 Zen 2, 8 cores/16 threads, AMD RDNA 2 graphics, and 16 GB GDDR6 system memory**. This is total hardware memory, not a guaranteed Encore process allocation. No PS5 Pro assumption, private SDK limit, current Encore allocator limit, backend capability or runtime budget is asserted here. [PS5-SPECS]

| Priority | Blocker / uncertainty | Why the upstream evidence matters | Required evidence before advancing |
| --- | --- | --- | --- |
| **P0** | **Backing and residency strategy** | A documented 20 GB Windows-commit startup configuration is not a drop-in fit for a 16 GB shared-memory machine; graphics residency and emulator/OS overhead also matter. However, committed virtual backing is not identical to physical RSS, so this is a feasibility risk rather than a proof that emulation is impossible. | Separate guest virtual reservations, sparse physical backing, aliasing, committed pages, shader/JIT heaps and texture/buffer residency. Establish actual available PS5 memory and a bounded peak with safety margin, including map/unmap/reuse and failure behavior. Never equate the 64 GiB GPU address space with 64 GiB resident allocation. |
| **P0** | **Graphics API / compiler / feature bridge** | Reviewed working reports use desktop Vulkan, SPIR-V/VMA-related tooling and PC driver behavior. A PS5-compatible graphics path with equivalent required semantics was not established. RDNA 2 is not SM86, and no compatible Vulkan stack may simply be assumed. | Explicit feature mapping and an authorized PS5 backend plan: shader translation, resource/view formats, descriptor/buffer rules, sparse residency, barriers, fences, queries, presentation and cache visibility. An OpenGL build option alone does not prove the experimental path works without Vulkan. |
| **P0** | **Unresolved guest rendering behavior** | Task/mesh programs are documented as skipped/rejected; tensor-heavy upscale uses a bilinear substitute. Missing geometry and quality substitutions are correctness limitations, not just optimizations. | A deliberate task/mesh implementation or validated compute/indirect fallback; faithful or clearly labeled quality mode; shader semantic tests and images covering these paths. No promise of PS5 native mesh support is made. |
| **P0** | **Source/dependency rights and traceability** | `changes.patch`, inherited ownership, fork/import history and many third-party components need traceability. No whole-tree license audit occurred. | Per-change provenance, selected dependency pins, notices and a compatible release/source-compliance plan before any code transfer. |
| **P1** | **CPU/JIT and host OS adaptation** | Guest atomics/PAC/MTE/SIMD and six-core scheduling are incomplete or historically approximated in the notes. The notes also introduce an x86-64 Turing MME JIT. Matching PS5 host ISA does not supply executable-memory management, signal/fault handling, cache flushes, TLS, synchronization or console ABI glue. | Synthetic instruction/concurrency tests, explicit supported ISA and memory model, safe JIT code allocation/permissions/invalidation, and verified PS5 host primitives. Do not apply arm64 NCE assumptions to x86-64 PS5. |
| **P1** | **Kernel/services and dishonest success stubs** | Notes say all recorded runs use auto-stubbing; several protocols are acknowledged guesses. Successful initialization can hide absent behavior, invalid handles, incomplete camera/chat responses and later corruption. | Keep a declared capability matrix and per-service status. Unsupported operations must remain visibly unsupported outside a tightly isolated research harness. Use semantic protocol/handle/lifetime tests; do not connect to account/game services for this work. |
| **P1** | **Synchronization/lifetime regressions** | Journal reports shared translation-cache races, async presentation crashes, map/unmap corruption and GPU-split scheduler ownership problems. Eglantine head metadata specifically concerns page-run boundaries. | Deterministic synthetic map/unmap/reuse, ownership and synchronization tests; repeated scene-transition-equivalent workloads; validation of cache invalidation and one-owner GPU recording rules. Commit titles and a few clean runs are insufficient. |
| **P1** | **Toolchain, frontend and multimedia portability** | Windows ZIP/Linux glibc AppImage are not console builds. Qt/SDL/frontend packaging, FFmpeg decode, Rust/Unicorn/Corrosion for Oboromi and host memory APIs need separate adaptation. | A headless/research-only portability boundary and explicit ABI/dependency plan. Validate input, audio/video, timing, file I/O and lifecycle independently; retain Encore's player frontend rather than importing upstream desktop UI wholesale. |
| **P2** | **Evidence and compatibility debt** | Loose ExeFS boot experiments, game-specific workarounds and attract-loop figures do not cover packaged content, firmware, progression, updates, saves or a representative corpus. No PS5 runtime evidence was collected. | A separately authorized, lawful evaluation plan with fixed revisions, repeatable workloads, render correctness, bounded memory, failure reporting and long-session stability. Preserve Switch 1 baseline behavior and player saves. |

## 6. Recommended research order and safe integration boundaries

1. **Takoroka experimental baseline first:** retain the exact `eden-nx2` pin as the origin/reference for the documented C++ experiment. Resolve attribution of imported patches and write down memory/renderer contracts before selecting implementation pieces. Use its Windows-only testing admission and experimental requirements as constraints, not as a PC-to-PS5 performance estimate.
2. **Eglantine next, as the active revision watchline:** use `b40ad217...` and its matching prerelease metadata to track newer fixes. Request/perform a separately authorized, focused source diff against a proven shared ancestor; the present report does not establish that ancestor or authorize source import. Start with memory mapping/lifetimes and correctness, not “full speed” tuning. Keep each candidate change attributable and independently reversible.
3. **Oboromi for independent architectural study:** compare ARMv9 extension strategy, guest GPU decode interfaces and the Rust/C boundary only when useful to a defined gap. Its stated inability to run games/firmware makes replacing Encore's core an unjustified near-term player decision. Pin/review rapid-arm-emu separately before relying on that work.
4. **notx2 only after detectable divergence:** currently deduplicate the same `eden-nx2` commit. Monitor metadata for a distinct relevant ref/commit before spending review effort; do not merge the same experiment twice or count it as independent validation.

Safe boundaries for any future implementation:

- **Status:** research/not supported, never “working Switch 2 support” until platform-specific evidence exists. Detecting a title or adding a library icon must not imply it can run.
- **Isolation:** separate opt-in research target/capability flags and settings, off in player releases. Preserve the existing supported-title path, save locations and defaults. No forced global core-count, memory-layout, stub, asynchronous-rendering or texture-quality change.
- **Interfaces:** confine experiments behind explicit guest model, loader metadata, service capability, memory manager, shader translation and host-renderer interfaces. Prefer small, provenance-cleared fixes over importing an entire fork or desktop frontend.
- **Memory and correctness first:** reject unsupported requirements clearly and gracefully. No silent overcommit, pretending unavailable services succeeded, dropping mesh geometry without a visible limitation, or presenting a bilinear substitute as native tensor/DLSS fidelity.
- **Evidence:** separate documentation/commit/release availability, build success, synthetic tests, render correctness, game progression and PS5 observations. This research establishes source/ref/release availability and reports documentary claims only; it produces no build or runtime result.
- **No automatic delivery:** no updater endpoint, game/account service contact, firmware/key collection, game extraction, new dependency fetching, build, install or runtime execution was performed or proposed as an immediate unattended action.

## 7. Sources, provenance identifiers and reading limits

All remote material was treated as untrusted documentary data; embedded build commands and historical instructions were **not executed**. URLs below were fetched read-only. Commit-pinned content URLs, not floating README links, govern technical conclusions. Blob IDs are upstream API-reported Git blob IDs, not independently calculated hashes or proof of license compliance.

### Metadata / refs / publications

- **E-META:** <https://git.eden-emu.dev/api/v1/repos/Takoroka/eden-nx2>
- **E-BRANCHES:** <https://git.eden-emu.dev/api/v1/repos/Takoroka/eden-nx2/branches?limit=100&page=1> and the same endpoint with `page=2`. Branch metadata pages were fetched; interpretation focused on `master` and `eden-nx2`, not all source branches.
- **E-RELEASES:** <https://git.eden-emu.dev/api/v1/repos/Takoroka/eden-nx2/releases?limit=3> — one record returned; release body and asset metadata read, no asset downloaded.
- **N-META:** <https://git.eden-emu.dev/api/v1/repos/chrisjenkynw/notx2>
- **N-BRANCHES:** <https://git.eden-emu.dev/api/v1/repos/chrisjenkynw/notx2/branches?limit=100&page=1> and `page=2`; same scope as above.
- **G-META:** <https://api.github.com/repos/Djihads80/Eglantine>
- **G-BRANCH:** <https://api.github.com/repos/Djihads80/Eglantine/branches/nx2> — full pin and author/committer date; head was still the previously supplied SHA.
- **G-HISTORY:** <https://api.github.com/repos/Djihads80/Eglantine/commits?sha=b40ad217ab829226841cb71f6e4c5cb05977a225&per_page=100> — first 100 ancestor-list entries only, selected commit titles/dates; no patches/implementations reviewed and no full ancestry proof.
- **G-RELEASES:** <https://api.github.com/repos/Djihads80/Eglantine/releases?per_page=3> — three records, bodies and asset names/sizes; no binaries downloaded.
- **O-META:** <https://git.eden-emu.dev/api/v1/repos/Nikilite/oboromi>
- **O-BRANCHES:** <https://git.eden-emu.dev/api/v1/repos/Nikilite/oboromi/branches?limit=100&page=1> and `page=2`.
- **O-GH-META:** <https://api.github.com/repos/0xNikilite/oboromi>
- **O-GH-BRANCH:** <https://api.github.com/repos/0xNikilite/oboromi/branches/main> — independent canonical-host pin check.
- **O-RELEASES:** <https://git.eden-emu.dev/api/v1/repos/Nikilite/oboromi/releases?limit=3> — empty list, not a full artifact search across every host/tag.
- **PS5-SPECS:** Sony Interactive Entertainment, published March 18, 2020, updated March 20, 2020: <https://blog.playstation.com/2020/03/18/unveiling-new-details-of-playstation-5-hardware-technical-specs/>. Read official technical-specifications table; surrounding news/comments not used. No login/account link was followed.

### Commit-pinned documentary/build/license content

| ID | Exact URL | API-reported blob / scope read |
| --- | --- | --- |
| E-README | <https://git.eden-emu.dev/api/v1/repos/Takoroka/eden-nx2/contents/README.md?ref=50fa86bbbc403ad565eb44fcc281f16f77c69e15> | `c0d8b65d8a08a71c3f1ac77932954ac03c199bf3`; 91 lines, complete. |
| E-NOTES | <https://git.eden-emu.dev/api/v1/repos/Takoroka/eden-nx2/contents/NX2_NOTES.md?ref=50fa86bbbc403ad565eb44fcc281f16f77c69e15> | `a50b037b85f39af4837cd953d44466254fe0a1ca`; 265 lines. Targeted reading: lines 1–6; rows 3, 11, 30, 53–54, 64, 80, 85, 87, 89, 94, 97, 99, 122, 128, 130, 138, 140–149; selected core/memory rows; lines 207–265. Not an independent full-source audit. |
| N-NOTES | <https://git.eden-emu.dev/api/v1/repos/chrisjenkynw/notx2/contents/NX2_NOTES.md?ref=50fa86bbbc403ad565eb44fcc281f16f77c69e15> | Same `a50b037...` blob, 56,174 bytes; identity metadata checked rather than rereading duplicate text. |
| GPU-SPLIT | <https://git.eden-emu.dev/api/v1/repos/Takoroka/eden-nx2/contents/NX2_GPU_THREAD_SPLIT.md?ref=50fa86bbbc403ad565eb44fcc281f16f77c69e15> | `75fd0e4105ffaba8dcf3242e562f2c4699e9307e`; 64 lines, complete. Eglantine same-named file at its pin independently reports the same blob. |
| E-BUILD | <https://git.eden-emu.dev/api/v1/repos/Takoroka/eden-nx2/contents/docs/Build.md?ref=50fa86bbbc403ad565eb44fcc281f16f77c69e15> | `84a3784fc06b2b99728210dfc68fab2b2641a23f`; 196 lines, relied on displayed lines 1–190. Eglantine same-named file independently reports the same blob. |
| E-CMAKE | <https://git.eden-emu.dev/api/v1/repos/Takoroka/eden-nx2/contents/CMakeLists.txt?ref=50fa86bbbc403ad565eb44fcc281f16f77c69e15> | `f647204a4b978d3c24ae415bd82c4c083e37cca1`; 750 lines; selected ownership/toolchain/frontend/platform/ISA/NCE declarations only, especially 1–29, 74–80, 244–269, 302–304, 325–344, 371–372. Not executed. |
| G-README | <https://api.github.com/repos/Djihads80/Eglantine/contents/README.md?ref=b40ad217ab829226841cb71f6e4c5cb05977a225> | `0c6177ad0fe656870cbf0d0ee6890d14a0efdd8b`; 74 lines, complete; authorship/license 1–6, derivation/platform claims 18–19, compatibility claim 42, license 73. |
| G-NOTES | <https://api.github.com/repos/Djihads80/Eglantine/contents/NX2_NOTES.md?ref=b40ad217ab829226841cb71f6e4c5cb05977a225> | `8da5873a5cea4f96880aefe2150c80e6d4b0bfd3`; 276 lines. Targeted upstream reading plus supplied local reference lines 1–207; relied especially on 61, 88, 138, 186, 194, 204–213, 215–276. No claim that journal measurements were independently reproduced. |
| G-CMAKE | <https://api.github.com/repos/Djihads80/Eglantine/contents/CMakeLists.txt?ref=b40ad217ab829226841cb71f6e4c5cb05977a225> | `fb2b96ed05306dfbb57c81e01ee156fcb0172507`; same selected declaration categories as E-CMAKE; not a full implementation comparison. |
| E-CPM | <https://git.eden-emu.dev/api/v1/repos/Takoroka/eden-nx2/contents/cpmfile.json?ref=50fa86bbbc403ad565eb44fcc281f16f77c69e15> | `00327347128e1a53ea61997183f970f89c670b74`; 364-line dependency manifest delivered. |
| G-CPM | <https://api.github.com/repos/Djihads80/Eglantine/contents/cpmfile.json?ref=b40ad217ab829226841cb71f6e4c5cb05977a225> | Same `003273...` blob; dependency declarations read, dependencies not fetched. |
| E-LICENSE | <https://git.eden-emu.dev/api/v1/repos/Takoroka/eden-nx2/contents/LICENSE.txt?ref=50fa86bbbc403ad565eb44fcc281f16f77c69e15> | `f288702d2fa16d3cdf0035b15a9fcbc552cd88e7`; GPL v3 header and standard example lines 639–640 inspected, not a whole-tree license audit. |
| G-LICENSE | <https://api.github.com/repos/Djihads80/Eglantine/contents/LICENSE.txt?ref=b40ad217ab829226841cb71f6e4c5cb05977a225> | Same GPL text blob; same reading limit. |
| O-README | <https://git.eden-emu.dev/api/v1/repos/Nikilite/oboromi/contents/README.md?ref=b97ea0cb32a0ef564ebf039bd8878dc344776e13> | `95b3940f2978299e30d2a65336676d7655257dbe`; 48 lines, complete; direction/unsupported status 16–24, requirements 35, license 41. |
| O-CONTRIBUTING | <https://git.eden-emu.dev/api/v1/repos/Nikilite/oboromi/contents/CONTRIBUTING.md?ref=b97ea0cb32a0ef564ebf039bd8878dc344776e13> | `5eb6fc7946d006e6682b80637526fc8cd641cc51`; 107 lines, complete; source data only, instructions not executed. |
| O-CARGO | <https://git.eden-emu.dev/api/v1/repos/Nikilite/oboromi/contents/core/Cargo.toml?ref=b97ea0cb32a0ef564ebf039bd8878dc344776e13> | `a661e1db7f6094bc08ab041c7391710284b74af0`; 23 lines, complete. Root Cargo workspace manifest also read (`23091ace4418f3548e9552bb68b184f2bc5df84e`, 9 lines). |
| O-CMAKE | <https://git.eden-emu.dev/api/v1/repos/Nikilite/oboromi/contents/CMakeLists.txt?ref=b97ea0cb32a0ef564ebf039bd8878dc344776e13> | `0fb6caf6d775d7c53d5a1549f470b3ca92a76427`; 70 lines, complete, not executed. |
| O-SUBMODULES | <https://git.eden-emu.dev/api/v1/repos/Nikilite/oboromi/contents/.gitmodules?ref=b97ea0cb32a0ef564ebf039bd8878dc344776e13> | `ab44223f6525ed945fad15573e47d505c1503ccc`; 7 lines, complete; URLs/declarations only. |
| O-LICENSE | <https://git.eden-emu.dev/api/v1/repos/Nikilite/oboromi/contents/license.txt?ref=b97ea0cb32a0ef564ebf039bd8878dc344776e13> | Same `f288702...` GPL text blob; same partial license-text reading limit. |

Additional scope: repository landing pages and selected root/document-directory listings were inspected to discover canonical links. Oboromi `docs/` listing contained a PDF; **it was not downloaded or read**. No upstream implementation files, large repository clones, dependency internals or scripts were inspected/executed. No exhaustive issue/PR, branch, history, benchmark, platform or license audit was performed.

The explicitly supplied local Eglantine reference directory was used for initial notes. Canonical HTTP metadata/docs were then independently obtained for the other requested upstreams. `sources/themes-switch2.md` was not needed. The PS5 knowledge project lookup for `Encore` returned no current context; this report does not invent one or use unrelated corpus files. Local Encore `vendor`/`third_party`, selected Eden implementation internals, private configs, `.serena` memories and unrelated user files were not read.

### Access outcomes and blocked endpoints

- **No entire requested host was inaccessible:** all four requested repository landing pages and relevant branch metadata were reachable.
- Forgejo API discovery was through the linked <https://git.eden-emu.dev/api/swagger>, which disclosed <https://git.eden-emu.dev/swagger.v1.json> and `/api/v1` endpoints. Only documented read-only endpoints were used.
- Discovered Takoroka raw commit URLs for at least `README.md` and `NX2_NOTES.md` returned **HTTP 403 / Cloudflare challenge**. They were treated as blocked, not parsed as repository contents, and no challenge script was executed. The canonical contents API succeeded and provided base64 documentary content instead.
- <https://git.eden-emu.dev/api/v1/repos/Takoroka/eden-nx2/git/commits/50fa86bbbc403ad565eb44fcc281f16f77c69e15> and <https://git.eden-emu.dev/api/v1/repos/Nikilite/oboromi/git/commits/b97ea0cb32a0ef564ebf039bd8878dc344776e13> returned **403**. Commit dates above therefore identify their actual accessible branch/GitHub metadata source; no inaccessible commit response is claimed as read.
- <https://git.eden-emu.dev/api/v1/repos/chrisjenkynw/notx2/releases?limit=3> returned **404**. Release availability on that endpoint remains unavailable/unknown, not a fabricated release result.

**Outcome:** a pinned research comparison and decision gates only. No Encore source modification, upstream code import, build/test/install, game/account-service request, compatibility certification, or performance guarantee was made.

## Campaign research increment — 2026-11-07

The outcome above describes the original comparison, not later work. A12 supplied
a bounded arithmetic tool, reviewed by A24 and independently extended by A36.
A50 executed all29 methods, including16,384 capability combinations. This proves
declared arithmetic/fail-closed behavior, not CPU/GPU/service implementation,
native feasibility or games. Every output retains `switch2_supported=false`.

The coordinator promoted the [tool and provenance](../../../../tools/research/nx2/README.md)
outside the launcher/core. CLI SHA-256 remains
`4400b287c78a4a3d6282f34898d58eb9ffd73ff95871bcc540664ff311028389`;
both JSON fixtures retain their tested bytes. The only changes to the two test
modules are relocatable documentary-input selection and default input root.
The integrated rerun in A50/integrated-04 passed all29 methods and exercised the
new default input root. CLI outcomes remained0/SYNTHETIC_ONLY and1/BLOCKED, both
unsupported. An intentionally absent documentary input caused a real
exit1/FileNotFoundError, never a skipped provenance test. All68 self.assert*
calls and pins remained intact. See the
[campaign integration](../../campaign-20261107/INTEGRATION.md) for distinct
candidate/integrated receipts and preserved earlier refusals.

The documentary fixture attributes a PC backing allocation failure at Eglantine
commit `b40ad217ab829226841cb71f6e4c5cb05977a225`, `NX2_NOTES.md:186`:
`Failed to allocate 20480 MiB of backing memory`. This is21474836480 bytes of
that reported allocation, not observed RSS, simultaneous GPU residency or a PS5
process requirement. The complete input's SHA/blob/quote/line checks remain in
the test, with an explicit read-only raw-input path; no provenance test is skipped
when the raw document is absent. Unknown PS5 application budget stays unknown,
never16GB hardware total or a guessed subtraction for the OS. No upstream core,
game, firmware or keys were imported by this increment.
