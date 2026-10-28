# Nextendo actual-core compilation qualification

## Integrated native TLS follow-up: both genuine-header gaps closed

2026-10-27, integrated base **`62e99ba13563ac3d7f12947857c8e072c4afce74`**
(includes security JSON `d96e8cc` and lifecycle gates `dd3865c`), same pinned Eden
`67bada77f8a43a90da2e94e89b8e7da73c256989`. Final follow-up run:
`/private/nextendo-native-tls-20261027/run-04/`.

**PASS: six actual host and six actual PS5-target translation-unit compilations**, with no
remaining missing-header blocks. This supersedes the earlier native TLS/producer compile
blocks below, not their historical evidence. No production compatibility fix was needed.
Only the qualification runner and this document change in this follow-up.

| Actual source/body | Host | SDK 0.42 target |
| --- | --- | --- |
| Derived DNS / ACC / composed native network | PASS (three objects) | PASS (three objects) |
| Current hardened `nextendo.cpp`, using actual `nextendo_json.h` | PASS | PASS |
| Derived `ssl_backend_openssl.cpp` | PASS, both host and native-CA macro variants (two objects) | PASS, native-CA macro enabled |
| Current `update_check/console_curl.c` producer | Not a host source | PASS |

The release volume was mounted **read-only** at `/native`, never aliased over the host cache:

- Genuine prefix: `/native/cache/ps5-eden-headless.6d6053a486f3/cdeps`.
  Its target headers report OpenSSL **3.5.2** and libcurl **8.18.0**. Saved native CMake facts
  select this prefix's `include`, `lib/libssl.a` and `lib/libcrypto.a` under their original
  `/work/cache/...` mount. No cache rewriting or symlink repair was necessary.
- Genuine SDK: `/native/cache/ps5-eden-headless.6d6053a486f3/sdk`. Its `libc++.a` matches
  the previously qualified SDK v0.42 archive hash below.
- Portable target JSON: the saved native source's
  `source/.cache/cpm/nlohmann_json/v3.12.0/include`, selected with `--json-include`.
  Only `nlohmann/` is staged into private scratch; the enclosing include prefix is not added
  to SDK search paths. Host objects retain the actual saved host compilation configuration.

The maintained runner now records the producer's full compiler dependency list, refuses
writable coordinator SDK/PacBrew inputs when `--cdeps` is supplied, and requires the genuine
headers/archives to exist. Missing dependencies are failures, not accepted BLOCKED results,
when an explicit prefix is supplied. Target dependency lists reject Linux system-header
paths; target symbols reject checked Linux ABI markers. All **3,520** input hashes were
rechecked before success, including **80** genuine cdeps headers and all linked archives.

### Actual native symbol/link contract

Both existing host partial links and the PS5 feature partial link pass. The genuine SDK
`libc++.a` supplies the shared-pointer atomic runtime symbols. Additional focused checks:

1. Partially link the freshly compiled **actual producer** with genuine `libcurl.a` and
   `libpsl.a`, then apply the exact `update_own` / `update_rename` CMake block extracted from
   the current production `headless/CMakeLists.txt`. This mirrors production isolation order:
   both libc definitions and references inside the combined producer/curl unit are renamed.
   It is not the complete production update object: unrelated updater/HTTP TUs are not rebuilt.
2. Link the actual Nextendo/core objects and isolated producer with genuine `libssl.a`,
   `libcrypto.a`, `libz.a` and SDK `libc++.a` as a relocatable subset. Strong definitions of
   `console_curl_getaddrinfo`, `console_curl_freeaddrinfo` and `console_curl_ca_file` resolve
   the real consumer references; no unprefixed producer resolver definitions escape isolation.
3. Confirm actual TLS-object imports of `SSL_set_verify`, `SSL_set1_host`, `SSL_ctrl`,
   `X509_STORE_new` and `X509_STORE_load_locations`, and their resolution by the genuine
   archives. No Nextendo / `SSL_` / `X509_` / `curl_` / `psl_` imports remain in this subset.
4. Inspect genuine SDK dynamic exports: `libSceNet.so` supplies `sceNetPoolCreate/Destroy`
   and `sceNetResolverCreate/Destroy/StartNtoa`; `libkernel.so` exports
   `sceKernelGetFsSandboxRandomWord`. This checks symbol availability, not runtime permissions.

**Remaining contract:** the relocatable subset has **343 import entries**, deliberately not
a completed executable link. `native-unresolved.json` contains the demangled list and
`ps5-native-contract-undefined.log` preserves exact mangled/C symbols. These include the
unlinked Common/Core/Service/Network, fmt/STB and zstd implementations, C++ ABI/unwind,
SDK libc/POSIX and module imports. In particular the four `ZSTD_*` streaming imports and the
six verified `sce*` imports still require the app's normal compression/platform link inputs.
Unprefixed `getaddrinfo/freeaddrinfo` imports remain for ordinary native-off/network/library
paths; only the opted-in bridge uses the isolated producer, as in production. No Linux
header/ABI leak or new source/type defect was established. ELF headers are exported too;
their SystemV OSABI field alone is not evidence of a PS5 runtime.

This follow-up does **not** execute TLS, resolve a hostname, inspect console CA-file access,
boot a guest, execute lifecycle gates or rerun parent-owned storage/security behavior tests.
Those remain independent. There is no full emulator/native build or complete executable link,
no tool installation, new agent, network service or writable coordinator volume.

### Exact follow-up command

```powershell
docker run --rm --cpus=2 --memory=3g --network=none `
  --mount 'type=volume,src=prosperoeden-integration-20261027,dst=/work,readonly' `
  --mount 'type=volume,src=prosperoeden-integration-20261027,dst=/root/.cache,readonly' `
  --mount 'type=volume,src=prosperoeden-release-20261027,dst=/native,readonly' `
  --mount 'type=volume,src=prosperoeden-team-pro-controller-20261027,dst=/private' `
  --mount 'type=bind,src=D:/leonardommello/PS5/ProsperoEden/.local/agent-worktrees/pro-controller,dst=/repo,readonly' `
  --entrypoint python3 prosperoeden-host:features -u /repo/tools/check-nextendo-core.py `
  --build /work/ps5-eden-headless.2YUJmY/build `
  --out /private/nextendo-native-tls-20261027/run-04 `
  --sdk /native/cache/ps5-eden-headless.6d6053a486f3/sdk `
  --boost /private/pro-controller/boost/boost-1.92.0 `
  --fmt /native/ProsperoEden/.deps/fmt-12.1.0 `
  --json-include /native/cache/ps5-eden-headless.6d6053a486f3/source/.cache/cpm/nlohmann_json/v3.12.0/include `
  --cdeps /native/cache/ps5-eden-headless.6d6053a486f3/cdeps
```

Use a **new** `--out` directory when reproducing. The image is the same pinned
`prosperoeden-host:features` ID recorded below; versioned Clang/LLVM 18 tools were present.
No release-image fallback was needed. Earlier follow-up `run-01` passed compilation with
the previous runner; `run-02/03` passed the growing symbol checks with host portable JSON.
The final `run-04` includes every check above and the saved native nlohmann 3.12 dependency.

### Follow-up hashes (SHA-256)

All individual source/header/library/object hashes and exact commands are exported in the
final `provenance.json`. Principal bindings:

| Input / evidence | SHA-256 |
| --- | --- |
| Final input manifest | `63d7ebcc7185ffa9c2bc55dfbe1df6b8327bb59c0c94ad7aeb501116c9a42c7a` |
| Final provenance JSON | `4357a568c654bb45a31989c1d009c6492bd65e3da92bd9ec8b2cc02f3a009969` |
| Tested runner | `ddc356c1d23fec88213cb4546a1c62f996a25ee266e20afb8be98423007cc177` |
| Current hardened `nextendo.cpp` | `d55b9527e560f221c88c9d3d0215514aacf78aaf1af5707ba13b3576928987cd` |
| Current `nextendo_json.h` | `692b22b5cbe8192543c397a204d86a71a10a1341396ba725cce8e1fb4eb38a23` |
| Actual `console_curl.c` | `a3aea8633297f7e104d080798468f29a3499285629f7f363d8a38be966c110d8` |
| Current production `headless/CMakeLists.txt` | `9e8d59365d92f7295aba4227edda5e9c152d2b9c77889ae97728e5ba2ca86a63` |
| Native nlohmann 3.12 `json.hpp` | `88d7eb42b19599cab48856576dd4ea52538df72ad7c57200c50de98bda246458` |
| Genuine `openssl/ssl.h` | `927a3a2ba4ee19a90b36b8d7c34e159c2f15d53fca52ed28b614bc1080361e1c` |
| Genuine `curl/curl.h` | `697555e3f5829c8fbea3fc3ef783fb6003219f3263c464b6de4019f3b09ea604` |
| `libssl.a` | `db58e5bb95ee7dba9e83d192dbd3258b976956c05bb1876e1f7a0763d512807e` |
| `libcrypto.a` | `e8fe42a8159e30d4c8a2ab1be74ee10b119c3f547265e3cc872bdaecda620944` |
| `libcurl.a` | `c29815b2e111e12c68975626b7d53418533d0718b18ee229943081068c11527d` |
| `libpsl.a` | `8ae4064de68c29f74095dec10c3db61ee85fd0b33c4d75be197c0adf59ea4220` |
| `libz.a` | `6838f30dd02e47e3fb46ed64b83f38a6067378428616edef38dee76935868d69` |
| Fresh actual PS5 TLS object | `30f20ccda5b961ed5629ee1aba2c42b9ea412969f0878541b59138999ab9af92` |
| Fresh actual PS5 producer object | `8078ce96f54ef9978e1fea1c13f9fd59b4ef0d063325ab53e12250bbf5eb91da` |
| Native contract partial-link object | `2ed89be4d438caa634dabd09048922632f211539ebd7d05ee330063832d9ac47` |

Read-only native resource-discovery facts, not substituted compile inputs:
`native-local/CMakeCache.txt` hashes to
`af953f722c9c165e490799feb7424ef223e97a259a98bae4d000ab5fc1f1116b`;
its `compile_commands.json` hashes to
`f761aaabad2fa8b9f83eb41a85f136df51dd155a54c458947f14bbe4385e12f9`.
The source/evidence ZIP includes those facts, final logs/dependency lists, the exact tested
repository sources and derived bodies, both unresolved-symbol lists and object hashes.
No object/library, key, account fixture or console content is copied into the bundle.

**Integration surface:** only `tools/check-nextendo-core.py` and this feature-local document
change. No shared service/UI/HTTP/OAuth/store/overlay files or cached archives change. Parent
changes to CMake composition or isolation markers require a fresh run, not reuse of this
base-specific result.

## Earlier qualification: result and scope

2026-10-27, base `30724082bcca4824d64bcebfc84e2891493a1686`, pinned Eden
`67bada77f8a43a90da2e94e89b8e7da73c256989`. Final run:
`/private/nextendo-core-20261027/run-04/`.

**PASS WITH DOCUMENTED TARGET BLOCKS**: actual changed bodies compile against genuine
core headers, not mock core/API/type declarations. This is focused object and partial-link
evidence, **not** a complete emulator/native build, executable link, console test or guest
authentication acceptance. No game dump, real credentials or external service was used.

| Actual translation unit | Saved host configuration | SDK 0.42 PS5 target |
| --- | --- | --- |
| Derived `sfdnsres.cpp` | PASS | PASS |
| Derived `acc.cpp` | PASS | PASS |
| `nextendo.cpp` | PASS | PASS after the compatibility fix below |
| Derived `ssl_backend_openssl.cpp` | PASS; also compiled with `EDEN_NEXTENDO_NATIVE=1` | BLOCKED: genuine target `openssl/ssl.h` unavailable |
| Composed native `network.cpp` | PASS | PASS |
| Actual `update_check/console_curl.c` producer | Not a host source | BLOCKED: genuine target `curl/curl.h` unavailable |

`tools/check-nextendo-core.py` executes the real `nextendo.cmake` and multiplayer
generators, then extracts and executes the exact production CMake native-network composition
block and `write_derived` function. The resulting network body retains the multiplayer poll
derivative, socketpair wakeup adaptation and Nextendo resolver/free pairing together. It does
not replace these with a stand-alone header test.

Host compilation reuses each relevant command from the coordinator's saved compile database,
redirects the source/output/dependency paths, prepends current feature/derived includes, and
uses `-O1` for this focused check. The compiler's dependency files hash real core headers,
including `common/uuid.h` and `acc/profile_manager.h`; no guest identity representation is
copied into a substitute header. The saved command's existing `-Wno-error-all` produces one
Clang unknown-warning-option warning per host object; no source compilation errors remain.

Two host partial links resolve all Nextendo imports between the actual core/feature bodies.
The SDK partial link likewise resolves Nextendo imports and rejects observable Linux
libstdc++/errno ABI leakage. A further partial link against the genuine SDK `libc++.a`
resolves `std::__1::__get_sp_mut` and `std::__1::__sp_mut::{lock,unlock}`. Other core/platform
imports are intentionally unresolved. The native `console_curl_getaddrinfo/freeaddrinfo`
imports remain weak/unresolved until the real producer is linked; this is not proof of a
working console resolver.

The PS5 commands use the real SDK sysroot/libc++, private Boost 1.92.0 and fmt 12.1.0,
and real saved core include/config paths and definitions, with explicit PS5 compiler flags.
Only portable nlohmann headers are staged from the host. Linux system include directories
and Linux OpenSSL/curl headers are not inserted into target commands. These are targeted
cross-compiles, not a claim that a complete native CMake configuration has been qualified.
Supply `--cdeps` with a genuine PS5 PacBrew prefix to retry the two documented dependency
blocks. No fake declarations, altered TLS implementation or header-only replacement was
used to turn those blocks into passes.

## Proven compatibility defect and minimal fix

The original SDK compile of `headless/nextendo.cpp` failed at the real declaration
`std::atomic<std::shared_ptr<const Session>> guest`: SDK 0.42 libc++ has no such C++20
specialization, so `_Atomic`/the trivially-copyable assertion rejects the shared pointer.
The retained `run-01/ps5-nextendo.log` records the actual diagnostic.

After explicit coordinator approval, only snapshot representation/access was changed:
`std::shared_ptr<const Session>` with `std::atomic_load_explicit` and
`std::atomic_store_explicit`. All accesses remain atomic and sequentially consistent,
matching the original defaults; immutable shared ownership is unchanged. Account validation,
storage, OAuth, HTTP, routing and launch policy are not changed.

Separate existing host behavioral regressions also PASS in the final run:

- `tools/check-nextendo.py --source <pinned-source>`: checked overlays, profile/private
  storage, explicit import/off/unlink, routing and cancellation; real TLS hostname/chain
  and bypass refusal; service adapter guards; actual libcurl bearer/redirect behavior.
- `tools/check-nextendo-oauth.py`: ASan/UBSan actual loopback browser callback and local
  HTTPS synthetic provider, PKCE, registered-client header, state/parser bounds,
  expiry/cancellation, strict TLS, replacement/default-off and failure preservation.

The pre-existing host adapter-contract test uses synthetic bridge definitions. Its PASS is
only a separate regression result, not evidence that the native producer compiled or ran.
The actual-object results in the table do not use that test's bridge definitions.

## Reproduce without a full build

Run from PowerShell, changing only the checkout and fresh private output paths as needed:

```powershell
docker run --rm --cpus=2 --memory=2g --network=none `
  --mount 'type=volume,src=prosperoeden-integration-20261027,dst=/work,readonly' `
  --mount 'type=volume,src=prosperoeden-integration-20261027,dst=/root/.cache,readonly' `
  --mount 'type=volume,src=prosperoeden-team-pro-controller-20261027,dst=/private' `
  --mount 'type=bind,src=D:/leonardommello/PS5/ProsperoEden/.local/agent-worktrees/pro-controller,dst=/repo,readonly' `
  --entrypoint python3 prosperoeden-host:features -u /repo/tools/check-nextendo-core.py `
  --build /work/ps5-eden-headless.2YUJmY/build `
  --out /private/nextendo-core-20261027/review-01 `
  --sdk /private/pro-controller/sdk-v0.42/ps5-payload-sdk `
  --boost /private/pro-controller/boost/boost-1.92.0 `
  --fmt /private/pro-controller/source/.deps/fmt-12.1.0 --regressions
```

Omit the SDK/dependency options for host-only compilation. The runner requires a read-only
coordinator build mount and a new, separate output directory. It does not run ninja, configure
the emulator, install tools, mutate the saved cache, or start another agent. Temporary
regression fixtures also stay in private scratch. Python assertions must remain enabled;
unexpected compile/link/regression failures exit nonzero. Only the two exact missing target
dependency diagnostics may be recorded as BLOCKED rather than failed.

## Input/evidence hashes

The final `provenance.json` contains all commands, individual logs/checks, **3,383** hashed
inputs (including compiler-reported system/core/SDK dependencies) and object hashes. Inputs
are checked again before success. Coordinator data is protected by read-only mounts; no
coordinator archive is linked or modified. Absolute paths participate in the manifest, so a
new scratch path produces a new manifest; compare individual input hashes when reproducing.

| Input | SHA-256 |
| --- | --- |
| Input manifest | `8a54a75e1f1ba714751ba77ed3bb1bd69f28e43af462604372e2be7f66f53025` |
| Final provenance JSON | `04436868e3653a52d00e5194856c3a34c66e6497b65c49efe0fd3ebe103d372a` |
| `tools/check-nextendo-core.py` | `545fd10a84ceef9cd97ed7a48d037b8e0d0c427f85469765971a2ea151189193` |
| Tested `headless/nextendo.cpp` | `0b1d35076eb7f4c6baeb1c91df221d4ec23d60e6db49abd73f88fd0968901351` |
| Unchanged `headless/nextendo.cmake` | `2a462530c8820dcdbc9f06cbeefbed51c62d7ccdc87f5e4249aae9952bd6e394` |
| Saved `compile_commands.json` | `03ccb7dc90faca63e31accadfc83b9143220bfd32cd549ada47d725bd280cd73` |
| Saved `CMakeCache.txt` | `7a5d3fada656c076bd5b52077cc2b7ecb8402e62240511c86541993de37df0b2` |
| SDK v0.42 ZIP | `8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da` |
| SDK `libc++.a` | `b1480e6edcb7cffba5ea999cb13da0d40b0def4c215ead442649bba71d32414f` |

Image: `prosperoeden-host:features`,
`sha256:e6137ec556fe8e63ebf25e7fabe36e573d089c469d0e9d391c3072f2ae04d551`.
Clang 18.1.3, Python 3.12.3; tool binaries/versions are included in provenance/logs.
The handoff ZIP exports final logs, provenance, dependency lists, tested repository sources
and derived core bodies, plus the original SDK failure diagnostic. Native objects remain
in private scratch and their hashes are exported. No TLS private key or account fixture is
included in the bundle.

Earlier attempts are not final acceptance: `run-01` exposed the shared-pointer defect and
used a wrong Boost-root argument; `run-02` passed the actual-object subset after correction;
`run-03` also passed actual-object/SDK-runtime linking but the optional existing regression
launcher failed because the image only supplies versioned Clang executables. The final
runner supplies `CC=clang-18` and `CXX=clang++-18`; `run-04` passes both regressions.

## Integration/conflict surfaces

- The only shared production file changed is `headless/nextendo.cpp`: the anonymous-namespace
  snapshot declaration, the two `BeginGame` stores, `EndGame` and `GuestSession`. Coordinate
  these small hunks with USBsession's independent storage/security work.
- No `nextendo_http`, OAuth, service/UI, CMake integration, backup lifecycle, aggregate docs,
  locales or release metadata are changed. No overlay modification was necessary.
- The runner deliberately binds to current production CMake composition markers. Parent
  CMake/overlay changes require rerunning it against the newly composed source, not relabeling
  these base-specific results. Console trust-file availability, native producer linking,
  guest networking/authentication and physical hardware execution remain unverified.
