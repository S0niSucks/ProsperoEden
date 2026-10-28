# Nextendo actual-core compilation qualification

## Result and scope

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
