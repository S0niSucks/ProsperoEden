# Opt-in native build preservation

This is a **capture API**, not source closure, a binary-bound provenance receipt,
compliance certification, readiness assessment or a modified-library relink.
Every run and phase remains `PENDING`. The coordinator alone runs the final
native build and real relink. Nothing here configures/builds/downloads inputs
unless an already-authorized build script is explicitly invoked by that caller.

## Enable once, before the final build

Use Linux, the final clean coordinator checkout and **new private storage**.
Do not point this at any old evidence/index/TAR/restore. `init` refuses an existing
destination, even if empty; its parent must already exist. No implicit init.
Declare roots individually at their actual final paths, including roots that
will be created during that build. Below, substitute real absolute paths:

```bash
export EDEN_NATIVE_EVIDENCE_DIR=/private/new-native-evidence
python3 -B tools/ci/native_evidence.py init \
  --root app=/approved/ProsperoEden \
  --root scratch=/approved/native-work \
  --root boilerplate=/approved/ps5-native-app-boilerplate \
  --root sdk=/approved/ps5-native-app-boilerplate/.deps/native/ps5-payload-sdk \
  --root vulkan=/approved/mihawk-vulkan-review \
  --root radv-source=/approved/mihawk-vulkan-review/.deps/work/radv-src \
  --root radv-release=/approved/mihawk-vulkan-review/.deps/native/radv-release \
  --root radv-sdk=/approved/mihawk-vulkan-review/.deps/native/ps5-payload-sdk \
  --root opengl=/approved/ProsperoEden/.deps/ps5-opengl-sdk-1.0.2/sdk
# Now invoke the coordinator's already-approved final native build normally.
```

The root names are arbitrary lower-case identifiers. Add exact approved roots
for any actual compiler-resource archive, external object/response directory,
driver dependency or tool executable directly named by final lld argv. A root
is an **authorization boundary, not a directory to copy**. Never approve `/`,
`/tmp`, `/usr`, a home directory or a whole volume; top-level roots are refused,
and broader scope is not an excuse to collect unrelated material. The helper
does not discover roots, inspect user caches or read `.local/headless-cache`.
The existing build scripts retain their own normal cache-location handling.

`.deps`, `.cache`, `.local` are permitted only as components of explicitly
declared storage anchors. Relative selections below an anchor use the unchanged
`source_bundle.safe_name` gate. To select a file below a nested storage anchor,
declare that exact approved sub-root; do not weaken the gate. There are **no
`.env.example` exceptions in this capture API**. The old hash-reviewed source
bundle exceptions are unchanged, not reused as general permissions.

With the variable unset/empty, shell wrappers run the original commands directly;
no Python, initialization or evidence directory is involved. With opt-in,
validation/capture failure is fatal rather than silently losing evidence. Test
the declared paths **before** authorizing the single final build. Missing files
are recorded as `missing`, never filled from a previous build. Existing outputs
have both before/after hashes; unchanged outputs and reused stubs stay explicitly
unqualified. No hook forces a rebuild just to produce evidence.

When initializing, optional repeatable `--phase-file PHASE=/canonical/approved/file`
adds an **exact file** to that named hook's before/after input snapshots. Use this
for concrete generated outputs once the coordinator knows their real paths, or
separately approved symlink target files needed to close that phase's link graph.
It never follows a link or discovers targets. For example, add
`--phase-file radv-produced=/approved/radv-build/src/amd/vulkan/libradv_cl.cpp`
and a matching exact approved build root **if that is the real producer path**;
the API does not assert the example path exists. Selections are frozen at init,
checked for privacy/aliases before metadata, and missing files stay gaps. Extra
files for other phases are not copied by this phase. Unobserved phase selections
are not evidence and do not constitute completed production.

## Minimal API (explicit files only)

```bash
# Observe exact applied/generated files at the appropriate producer boundary.
python3 -B tools/ci/native_evidence.py capture --phase selected-generated-final \
  --file /approved/native-work/native-local/headless/emutls.c \
  --file /approved/native-work/native-local/headless/app_heap.c \
  --pending source_object_correspondence_pending

# Wrap an approved command, with explicit before/after inputs and outputs.
# This is an example, NOT permission to run additional native production.
python3 -B tools/ci/native_evidence.py run --phase approved-producer \
  --input /approved/input/source.c --output /approved/output/object.o -- \
  /approved/toolchain/bin/real-compiler -c /approved/input/source.c -o /approved/output/object.o

# Read-only integrity check of retained evidence, without accessing source roots.
python3 -B tools/ci/native_evidence.py check
```

`run --link` additionally parses explicit lld file arguments, `-T`, version
scripts, `-o`, `-Map[=]` and UTF-8 POSIX-quoted `@response` files (bounded to
16 MiB each, nesting <=8, no repeated/cyclic response). Response bytes are
retained verbatim; parsing is only for additional selection. Relative command
paths are interpreted from the recorded actual cwd; nested responses use that
same cwd. Command syntax may contain conventional `repo/../sibling` paths, but
the original ancestors are checked as well as the normalized path. Explicit
`--file/--input/--output` paths and root declarations must be canonical absolute
POSIX paths without `..` or symlink ancestors. Command executables must be absolute.

No `-L` directory search, linker `GROUP`/`INCLUDE` evaluation, compiler dependency
discovery, SDK wrapper inspection/execution or provider version probe is claimed.
Unresolved search/scripts/options/providers remain in the `pending` ledger.
Actual input argv and response bytes are private evidence: never pass credentials
or secret values to this API. It rejects the shared private-name classes and
credential-like options, but cannot determine whether arbitrary innocuous-named
file contents are confidential. Only explicitly approved public/build artifacts
belong in the selections. There is no environment dump or environment receipt.

## Implemented hooks and exact changed paths

| Existing script changed | Added opt-in preservation |
|---|---|
| `tools/build-agc-driver-stub.sh` | compile source, compiler wrapper, temporary `.o`; link wrapper, `.o`, temporary `.so`; installed `.so`, or explicit reused-stub gap. |
| `tools/build-common-dialog-stub.sh` | Same boundaries for the CommonDialog facade. |
| `tools/link-headless-native.sh` | Actual expanded final wrapper/lld argv, explicit libraries/objects/scripts, response bytes and named inputs, before/after output ELF/map. |
| `tools/build-radv-dependencies.sh` | cross-file pre/post rewrite, generated Ninja wrapper, upstream recipe bytes, ten selected RADV pre/postimages, six adaptation recipes/includes, final archive/provenance/four stamps. |
| `tools/prepare-build.sh` | prepare recipe/pins; selected FFmpeg generated config/archives, packaging tool, isolated RADV archive/map/manifest (not whole SDK/cache trees). |
| `tools/package-headless-native.sh` | pre/post stamped ELF, four selected native-generated source/header paths, actual converter/signer argv/tool bytes/ELFs/eboot, explicit `--stub` inputs and selected final package artifacts. |

New files only: `tools/ci/native_evidence.py`, `native-evidence.sh`,
`native-evidence-README.md`, `native_evidence_test.py` in that same directory.
No CMake/runtime/UI/service/USB/admission/catalog/version or delivered source
collector/preparation/restoration helper changed. The native inventory document
is intentionally untouched; its independent snippet correction belongs to the
coordinator.

The two stub scripts use a new `build/stubs/native-evidence.XXXXXX` temporary
directory **only with opt-in**, so it lies in the approved app root. Capture
completes before the existing EXIT cleanup. Original compile/link command
arguments and `PS5_PAYLOAD_SDK` setting remain unchanged. The default `mktemp`
behavior remains unchanged without opt-in.

## Retained representation, integrity and failure behavior

`run.json` binds approved roots and permanently `PENDING` qualification. Each
append-only `events/phase-UUID/` has exclusive-create `before.json`, then
`after.json`, each with SHA-256 sidecar. The latter binds the former; the former
binds `run.json`. Bytes live in opaque **regular** `files/UUID` records. A row
retains logical root/path, original permission mode, kind, size and SHA-256.
Local final symlinks retain **link-text only** (not their targets); all targets
must be separately explicitly selected in a closed same-root graph. Case/prefix
aliases, escapes, cycles, dangling links, special files and directory selections
fail closed. Intermediate local symlinks are never traversed. Root and file
privacy/alias gates run before those inputs' metadata probes; in particular
`.ENV.local`, `config/network-shares.json.bak`, `config/nextendo/token.json` are
refused using the common gate. No general template/secret/special-file exception.

Original file modes are metadata, **not applied to transport bytes**: evidence
directories are 0700 and files 0600. Files are streamed in 1 MiB blocks, not
read as full archive/object trees into memory. Capture checks inode/mode/size
and mutation during reads; it does not make a concurrently modified build
filesystem atomic. Keep producers/consumers and capture boundaries serialized;
do not modify roots, symlinks or inputs concurrently. Every event gets a fresh
UUID directory; there is no overwrite/resume/repair of failed events.

An interrupted or failed capture leaves an incomplete event (possibly orphaned
opaque bytes). `check` refuses incomplete/empty/corrupt evidence and reports
failed producer commands, missing selections and `PENDING` qualification. A
nonzero producer status is retained and returned. Missing selections alone are
an integrity-valid **gap**, not successful production. Check hashes do not confer
authenticity: the coordinator must freeze/re-hash this new evidence externally
and independently review final-HEAD/source/binary correspondence.

## Remaining work for the coordinator

* Preserve actual generator outputs, command lines, used headers/depfiles,
  toolchain/resource providers and compatible object/bitcode closure with
  additional **exact** `capture`/`run` selections at their real build boundaries.
  In particular, RADV host CLC/VTN, Meson/Ninja outputs, shader/SPIR-V generation,
  other CMake-generated source and SDK/runtime source-to-binary matching are
  **not** closed by these hooks. Upstream setup/build internals are not wrapped.
* Explicitly capture real used import stubs when converter argv uses
  `--stub-dir`; no directory enumeration is added. Conditional native generated
  source may legitimately be absent and stays missing rather than fabricated.
* Preserve actual effective build options/environment identities using a
  reviewed allowlist, not an environment dump. An executable wrapper hash does
  not identify the compiler/linker/provider it chooses.
* Bind final clean coordinator HEAD, source policies/omissions, final package
  identity, all consumed SDK/library/link-script/archive providers and actual
  compatible relink objects. Independently verify provenance and perform an
  actual modified-library relink before any readiness claim. These hooks emit
  **no source-bundle relink receipt and never `READY`**.

## Offline synthetic validation only

The test module uses generated mock tools and disposable fixtures, **never the
SDK or a real configure/native build**. Example resource-capped invocation:

```bash
docker run --rm --network none --cpus 2 --memory 1g --pids-limit 128 --cap-drop ALL \
  --mount type=bind,src=/final/checkout,dst=/inputs/repo,readonly \
  --mount type=volume,src=NEW_PRIVATE_TEST_VOLUME,dst=/work \
  --workdir /inputs/repo --env TMPDIR=/work \
  prosperoeden-host:features python3 -B tools/ci/native_evidence_test.py -v
```
