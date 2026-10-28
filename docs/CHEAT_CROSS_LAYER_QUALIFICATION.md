# Issue #74 — focused cross-layer qualification

## Scope

`tools/check-cheat-flow.py` links a maintained synthetic-container test against
the coordinator's **read-only host archives**. It compiles only the current
identity/probe, manager, download, parser, store, HTTP and test translation units,
plus the real pinned NPDM reader with the current bounds guard. `ninja -t commands`
is used to *read* the existing link command; no build target is executed.

There is no fixed-build identity callback or fake HTTP transport. The manager's
worker calls `ProbeCheatIdentity` on actual local files through `RealVfsFilesystem`,
NSP/CNMT/NCA/NPDM and the real effective-ExeFS/PatchManager path. Network traffic
uses `remote/http.c` and libcurl against an ephemeral loopback TLS server; peer and
hostname verification stay enabled. The server returns only invented catalog
entries/code. Container headers, executable bytes, encryption keys, CTREx and BKTR
tables are synthetic, independently serialized using the existing fixture writers.

The runner reuses the writer-only prefix of `cheat_identity_check.cpp` verbatim as
a generated private header. It asserts the extraction boundaries and records the
entire source hash. This avoids duplicating the encrypted/BKTR serializers or
changing production behavior. The original identity test also runs freshly linked,
including effective wrong-key reload and mixed base/patch byte assertions.

## Assertions

- Online worker preview for a real base/update identity; no automatic installation.
- The same end-to-end flow for encrypted header/section NCAs with BKTR. The actual
  base/patch files inside the same on-disk NSPs must merge to the expected two
  distinct byte ranges, not merely expose a build ID.
- Two native-VFS copies of one title select their own local update and ExeFS mod,
  including persisted mod disabling. They share a cache/mod destination, not an
  identity result. A cache for copy A cannot authorize copy B.
- Offline preview makes no HTTP request and requires the exact source and full
  32-byte build, including a changed suffix with an unchanged public catalog prefix.
- Explicit installation reprobes the selected real files. Changes to an update,
  enabled replacement mod, title or full build **while TLS is held open** refuse
  publication when installation revalidates. No cheat text or pending file remains.
- Wrong catalog title/build and a malformed NPDM are refused. A wrong expected
  program fails before any HTTP request. No unknown-build fallback is supplied.
- Real cache/parser/installer output is read through `Mods::List` and
  `Mods::Switches`. Managed downloads remain default-OFF even with global Mods and
  Cheats switches on; installation does not change saved choices. A real persisted
  individual choice is required to enable an entry.
- Cancellation interrupts a held real TLS transfer without caching/installing.
  A cancelled probe queued behind the metadata bridge never enters it. Delaying
  the return of an **actual** probe result verifies that `busy` remains true and
  `Stop()` joins before the owner can cross its launch/lifetime boundary.

This tests the manager's launch-barrier contract, not a guest boot or a rendered
launcher interaction. Existing launcher/UI tests cover the owner integration
separately. The core/device archives are the existing host configuration, not a
new full emulator/native build or a fully sanitizer-instrumented core.

## Reproduction

Reference prerequisites (already present on the coordinator): `9d6bc6b` effective
key-reload controls and `2d6b596` encrypted BKTR fixtures. In this exclusive
worktree they are `90340e1` (test file only, no aggregate docs) and `4b81d36`.
**Do not reimport these prerequisites** when taking the new qualification commit.

Use a new output directory on every run. No toolchain installation, external
network service, real dump/key/cheat or writable coordinator mount is needed.

```powershell
docker run --rm --cpus=2 --memory=2g --network=none --entrypoint python3 `
  --mount 'type=bind,src=<exclusive-worktree>,dst=/checkout,readonly' `
  --mount 'type=volume,src=prosperoeden-integration-20261027,dst=/work,readonly' `
  --mount 'type=volume,src=prosperoeden-integration-20261027,dst=/root/.cache,readonly' `
  --mount 'type=volume,src=prosperoeden-team-pro-controller-20261027,dst=/private' `
  prosperoeden-host:features /checkout/tools/check-cheat-flow.py `
  --build /work/ps5-eden-headless.2YUJmY/build `
  --out /private/cheat-flow-20261027/<new-run>
```

The private output includes `run.log`, `provenance.json`, `source-before.json`,
fresh/derived objects and sources, TLS files, the request log and runtime fixtures.
Provenance records exact compile/link/run commands, fresh-source/header SHA-256s,
compile-database configuration and archive hashes before/after execution. The
reported source-manifest SHA-256 is over sorted compact JSON of that input map.
Every subprocess failure and C++ assertion propagates as a nonzero result; no
assertion failure is accepted as success. A failed/incomplete run is not evidence
for the flow.

## Execution record

Container image for the focused runs: `prosperoeden-host:features`, image ID
`sha256:e6137ec556fe8e63ebf25e7fabe36e573d089c469d0e9d391c3072f2ae04d551`.
The first runner attempt failed linking the prerequisite-only binary because it
unnecessarily included the HTTP object without its TLS setup. The runner now
links that identity-only binary with identity objects only; no stub was added and
no assertion failure was suppressed. Its failed provenance/log remain in
`/private/cheat-flow-20261027/run-01` for audit, not as passing evidence.
Run `run-02` passed the freshly linked prerequisite identity assertions and
compiled/linked the cross-layer binary, but stopped before TLS setup on a Python
module-shadowing error. That harness error was corrected; `run-02` is also failed
evidence, not a cross-layer pass.

**PASS — `/private/cheat-flow-20261027/run-03`.** The freshly linked prerequisite
identity test passed, followed by the complete cross-layer test: **29 actual
worker probe callbacks**, exactly **11 loopback HTTPS requests**, and all **34
reused archive/library hashes unchanged**. The run used the bounded, network-none
Docker command above. Production sources and controller behavior were unchanged.

Exact tested inputs:

- Source-manifest SHA-256:
  `d2c3ac7fb9319e926bb1ab62a14088fa2a625c4880c4439a0596efb2289860fa`
- `headless/cheat_flow_check.cpp` SHA-256:
  `536f5ead211254c1e77839aac8cb5e5b3e8b6bfad606bb8db3fd0520a517e541`
- `tools/check-cheat-flow.py` SHA-256:
  `59b9099165e7828bbc8305a0dab18b013bc22fbc2fb073d8d2478d30b5564194`
- Reused `src/core/libcore.a` SHA-256, before and after:
  `f2e442c067d1b8ff022938febc97b04809f8afbda986a477de5a5d09e505f9cc`

The full log, exact command/input/archive provenance, pre-run source manifest,
request log, tested test/runner/fixture source, generated NPDM reader, certificate
and per-artifact hashes were exported to:

`C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/cheat-flow-74-20261027-run-03.zip`

Bundle SHA-256:
`ea6d83a767ed4d1eab9d180dcf9b549b705920ced430f2aed1110f195fe7b666`.
The same bundle is retained as `evidence.zip` in the private run directory. No TLS
private key or commercial material is included in the export.

This focused host test cannot establish console TLS behavior, cheat effectiveness in
a commercial title, encrypted retail-content compatibility, or #74 hardware
acceptance. No production behavior change is part of this qualification task.

## Coordinator integrated-HTTP repeat

After the Nextendo HTTP/session/OAuth commits landed, the maintained runner passed again on
the coordinator's synchronously frozen **820-file clean `ab5f70d`** snapshot, not the worker
tree (`sh_17fdabfef00167L7A3GHZyk3uO`). It repeated the independent identity and complete
cross-layer assertions: 29 actual probes, exact offline binding, update/mod/title/full-build
mutation refusal, real TLS cancellation and default-off installation. Source-manifest SHA-256:
`2144b97a2c8fb1184c7b7e7c416e0d2e32db3694d1d6bd4312f8e9929bef7af0`.

The input snapshot, raw output and exit-0 status are in
`.local/docker-release/cheat-cross-layer-20261027/` (`source-snapshot.json`,
`integrated-http-flow.log`, `integrated-http-status.json`). Full private commands/hash evidence
is retained in Docker volume `prosperoeden-cheat-flow-nextendo-review-20261027`, under
`/private/cheat-flow-nextendo-20261027/run/`. The reused older host-core archives were mounted
read-only at their original paths; this is neither a rebuilt Nextendo core nor PS5 gameplay.
