# Native app-folder notice staging

## Scope

Branch `team/native-license-packaging-1006000` starts exactly at
`00b1fdc3bbf98c73c9fd9566d13a079cc14785ba`. This change closes the notice-placement gap
for an app folder produced by `tools/package-headless-native.sh`; it does not build an
application, install anything on a console, generate/extract a PKG, or qualify legal
compliance, corresponding source or relink-material delivery.

The shell script invokes the new `tools/package-license-notices.py` after its existing
Lapy/runtime copies, before its final inspections and successful exit. Its existing
`licenses/Lapy-MIT.txt` copy command is unchanged. This placement applies to the default
headless folder and to the release/development app-folder paths supplied by the existing
build pipeline, before downstream ZIP collection or an app-folder install.

## Layout and byte preservation

```text
PPSA99008/
  LICENSE
  THIRD_PARTY_NOTICES.md
  third_party/licenses/
    SOURCES.json
    citron-nextendo/{LICENSE,SOURCE_NOTICES.txt}
    libnfs/{COPYING,LICENCE-BSD.txt,LICENCE-LGPL-2.1.txt,SOURCE_NOTICES.txt}
    libsmb2/{COPYING,LICENCE-LGPL-2.1.txt,SOURCE_NOTICES.txt}
    titledb/LICENSE
  licenses/Lapy-MIT.txt                 # existing native script copy, not touched by the CLI
```

The tool consumes the integrated `release_licenses.release_license_entries` without
modifying it. All 11 retained upstream license/notice files, raw `SOURCES.json`, and root
project `LICENSE` / `THIRD_PARTY_NOTICES.md` keep their exact bytes, line endings and
whitespace. The pinned `SOURCES.json` hash remains
`fd79de67ad2fd83a6cd781b31d672f408a6c35dd15d060af823f48d1ab0588dc`.

The notices' relative **retained-license links**, including `LICENSE` and
`third_party/licenses/...`, resolve inside the app folder independently of the ZIP's
outer root. Source-documentation links under `docs/` still refer to repository context;
this is not an offline documentation/site copy. Source archives, nested provenance
records and source excerpts are not interpreted as full source, fetched, or recursively
copied. The tool copies only the helper's explicit file mapping, never the repository,
an existing app tree or its own source/tool folder into the license directory.

The unchanged release ZIP collector now sees these same notices under `PPSA99008/` as
ordinary app files, while retaining the previous separate ZIP-root notices. Those are
two intentional usable roots, **not** a recursively nested `licenses/third_party/licenses`
layer. An extra/unexpected owned license-tree file or directory is refused.

## CLI and failure contract

Run on a POSIX packaging host (the native pipeline already uses Linux):

```sh
python3 -B tools/package-license-notices.py build/release/PPSA99008
python3 -B tools/package-license-notices.py build/release/PPSA99008 --check
```

- Destination must already be a directory. Within the source repository only `build/`
  or `dist/` staging is allowed; an external staging folder is also allowed. Source roots,
  their ancestors/other descendants, traversal, ambiguous dot/space/backslash/colon paths
  and private-config destinations are refused before destination traversal.
- Every destination ancestor is checked with descriptor-relative no-follow metadata and
  opened with `O_DIRECTORY|O_NOFOLLOW`. Tree enumeration does not follow links; symlinks,
  FIFOs and other special nodes are refused before payload open/enumeration.
- The existing shared helper retains its missing/malformed manifest, exact file inventory,
  raw size/hash, path and type checks. A name-first source-tree preflight adds the unchanged
  network/Nextendo privacy policy before that helper inspects source children. Extra source
  directories, including empty ones, are also rejected.
- App staging is **not an app-data sanitizer**: it refuses private nodes using the existing
  broad `config/network-shares.json*` and entire `config/nextendo` policy, including aliases,
  before metadata/read/descent. It does not silently keep credentials in a folder destined
  for raw install, delete anything, blacklist all config, or weaken the export collectors.
  Public config bytes and other normal app payloads are retained without being opened.
- Existing matching notices are verified and left untouched. Conflicting bytes, file vs
  directory conflicts, case/trailing-dot/space aliases and extra owned inventory fail
  before writes. Use a fresh staging folder when intentional notice updates conflict with
  older staged copies; the tool never deletes/overwrites unknown existing work.
- New files are created exclusively with `O_EXCL|O_NOFOLLOW`, raw bytes and mode 0777;
  newly created directories are also 0777. Existing files/directories, including the Lapy
  notice and unrelated payloads, are not chmodded or rewritten. `--check` never creates
  anything and refuses missing notices. Unsupported hosts fail closed, not a weak fallback.

Inputs must remain quiescent. These checks are not a concurrent-filesystem-mutation sandbox;
the reused source helper checks types then reads paths. IO failure during creation returns
failure and can leave some **new** notices/directories, never a success receipt. Re-run on
clean/fixed staging; no claim of whole-folder atomicity is made. The shell's other existing
native/build/copy operations are not requalified or made into a hostile-input sandbox here.

## Qualification method

`tools/check-native-license-notices.py` requires unprivileged Linux so chmod permission
controls are real failures, not root-bypassed simulations. It copies only maintained notice
bytes and the unchanged tools needed by miniature repositories; app payload and Lapy text
are explicitly synthetic non-executable sentinels. It runs the **actual new CLI** for initial
staging, idempotent staging, check-only verification, external staging and safe relative paths.
It verifies every retained byte/hash/size, the internal license links, new-file 0777 modes,
source immutability and unchanged existing payload/Lapy bytes, modes and mtimes.

The actual unchanged `make-dist.py` then collects the staged miniature app twice. Exact ZIP
membership, extracted bytes, both notice roots, lack of recursive duplication, fixed
timestamps, 0777 attributes, SHA256SUMS and repeat-byte equality are checked. No optional
`build/headless-native/llvm-pie.elf` is present; its symbol/build-identity branch is not
exercised or faked. The synthetic `eboot.bin` is text, not a built SELF/ELF.

Adverse cases run the actual staging CLI with missing tree/manifest/file/project notices,
changed sizes and same-size hashes, malformed/duplicate-key manifests, traversal/backslash
destinations, extra files/empty directories/recursive layers, conflicting target bytes/types,
case/trailing aliases, missing check-only entries, unreadable files/directories, unwritable
staging/deep parents, source/destination directory/file/manifest/project-license links/FIFOs,
project-notice/`third_party` ancestor links/FIFOs, app-root/ancestor links, payload and Lapy
file/directory links/FIFOs, and destination path guards. Synthetic private
file/directory/link/FIFO nodes cover mixed case, trailing spaces/dots, backslashes, nested
paths, the network-share broad-prefix rule and full Nextendo subtree on both input sides.

An independent child-process guard instruments open/read/enumeration and metadata calls,
including `dir_fd` operations resolved through already-open descriptors. Hostile type cases
allow metadata only; private-node cases allow **no** metadata either. Success requires zero
attempted forbidden operations, not merely a failure caused by a guard exception. Unrelated
app payloads/Lapy text are protected against opens on every staging invocation. Refusals
must also leave the test-owned source/app snapshot unchanged. Instrumentation is Python-level,
not kernel tracing or proof against a malicious concurrent process.

`tools/package-headless-native.sh` gets `bash -n` and a static single-call/ordering check;
it is **not executed**. The previous actual-CLI share/Nextendo privacy/launcher validation
suite runs unchanged as a separate regression with its historical baseline. No source
license, release collector, privacy helper, native runtime/build runner, version, or other
owner's file is edited.

## Evidence and remaining limits

Ignored evidence root: `.local/native-license-packaging-20261027/`. Linux runs use the existing
`prosperoeden-host:features` image, private volume `prosperoeden-team-amiibo-sync-20261027`,
2 CPUs / 2 GiB, UID/GID 65534, no network, no capabilities and read-only input mounts. Source
snapshots and test outputs stay private. No installation, extra agent, native/full build,
console transfer, push, tag, version change or release publication is involved.

The initial `linux-first/` attempt is preserved as a failed control: its guard detected an
attempted `open` on the synthetic source license-directory symlink before `O_NOFOLLOW`
refused it. Directory ancestry now checks no-follow metadata/type **before** that open.
No guard was loosened to hide the attempt. `linux-qualified/` holds an intermediate passing
run; `linux-final/` is the expanded final suite and the only final qualification export.
Baseline privacy ZIPs deliberately contain synthetic canaries as leak evidence; none of
these fixture ZIPs is an actual release asset or intended for publication.

### Final results (2026-10-27)

**PASS:** Linux Python 3.12.3, UID/GID 65534. The final new suite ran **122 actual Python
CLI invocations**: five staging/verification success controls, two actual release-collector
invocations and **115 adverse refusal cases**. The unchanged privacy suite ran **121** more
actual CLI invocations, including its historical leak/refusal controls. Every new guarded
run and every fixed privacy run recorded **zero attempted forbidden accesses**. Native
shell syntax/static placement, all 14 staged notice-file bytes, 11 manifest declarations,
internal license links, Lapy preservation, source hashes, exact ZIP membership, extraction,
0777 attributes, timestamps, checksums and reproducibility passed.

All **1,128** entries in the exported artifact index were rehashed locally. The final
reports' collector/test hashes match the current worktree bytes; all maintained license
files and out-of-scope collectors/helpers match their pinned HEAD blobs byte-for-byte.
`qualification.json` records the counts/hashes and private run root. `verify-evidence.py`
repeats that verification from the assigned worktree. Final exports contain per-invocation
commands/stdout/stderr/guard logs, reports, synthetic ZIPs/feeds/checksums and a full staged
app tar; no hostile-node fixtures are copied out of the private Linux volume.

| Artifact | SHA-256 |
| --- | --- |
| Tested source tar (`input/source-final.tar`) | `74c0f545f9ab875ca86481df2c3cebfd47ef149e68ed7036f349ef999f5166d4` |
| Historical privacy baseline tar | `5006ca8d5549347e39e0b0b52e7c77e191e3a3b0fee94c4a983b22577a07576a` |
| Final native staging report | `895e300ec9cb5c5fd436fe28070e115ced45f7118b56d29436049d6e06feb5e3` |
| Final privacy regression report | `a91c0290558b0753383d9b0d9fb91327cf46797776807810e74498608c299a5c` |
| Synthetic staged app-folder tar | `3672d5243c34dbf5777a2c86288aa5245d7b122576ba33abcb450f9217c5b367` |
| Both identical synthetic release ZIPs | `af89db8ca6362eb71c83d9e84094760eaa8a191bb60f82ba6b0d3f7295906d18` |
| Exported artifact index | `0c2fa5f520a5d5d0a7d8c8f7e4b82227a58e29b771f91958a713e337bec2bb61` |

The existing image identity was checked as
`sha256:e6137ec556fe8e63ebf25e7fabe36e573d089c469d0e9d391c3072f2ae04d551`.
The complete final fixtures remain at `/work/native-notices.sYREfw/` in the private volume.
Read-only source input is unpacked into that fresh owned directory; only miniature test
repositories are mutated for adverse controls, not the tested source snapshot or checkout.

The retained `input/run.sh` runs these commands in the fresh private directory:

```sh
python3 -B "$run/source/tools/check-native-license-notices.py" --output "$run/results/native"
python3 -B "$run/source/tools/check-network-privacy.py" --baseline "$run/baseline" --output "$run/results/privacy"
```

Exact final Docker invocation (use fresh output directories when reproducing):

```powershell
$evidence = 'D:/leonardommello/PS5/ProsperoEden/.local/agent-worktrees/amiibo-sync/.local/native-license-packaging-20261027'
docker run --rm --name prosperoeden-team-native-license-final --user=65534:65534 `
  --cpus=2 --memory=2g --pids-limit=128 --network=none --cap-drop=ALL `
  --env SOURCE_TAR=source-final.tar `
  --mount "type=bind,source=$evidence/input,target=/input,readonly" `
  --mount "type=bind,source=$evidence/linux-final,target=/export" `
  --mount type=volume,source=prosperoeden-team-amiibo-sync-20261027,target=/work `
  --entrypoint bash prosperoeden-host:features /input/run.sh
python -B .local/native-license-packaging-20261027/verify-evidence.py
```

Actual generation/extraction of a downstream **PKG remains unvalidated**. The parent owns
integrated/native pipeline execution and final package inventory. Final-binary licensing,
corresponding-source/relink materials and their delivery are still separate pending work;
notice staging is not a promise or compliance certification on the user's behalf.
