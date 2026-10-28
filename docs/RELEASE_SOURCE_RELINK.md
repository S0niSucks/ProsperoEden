# 1.006.000 source and static-relink delivery mechanism

> **Policy follow-up:** [schema 2, exact reviewed exceptions and safe restore](RELEASE_SOURCE_POLICY_FOLLOWUP.md)
> supersedes the original blanket Git-link/template refusal below. Historical
> schema 1 descriptions remain baseline documentation, not current exceptions.

## Status: mechanism/evidence, not a completed release delivery

`tools/ci/source_bundle.py` is a new, offline Python 3.11+ / Git CLI. It exports
an explicit immutable application snapshot, verified pinned inputs and optional
hash-inventoried final-build relink materials. It does not call a downloader,
build system, packaging tool or relink script. No existing collector is changed.

**No legal compliance, complete corresponding-source delivery, successful relink,
reproducible application binary, console qualification or final package readiness
is certified here.** The coordinator must run this mechanism again at the final
HEAD, supply/review the actual final build materials and perform the checklist below.
Source-only collection always records relink as **PENDING**. Source is not presented
as a substitute for application objects under a chosen static-relink route.

Implementation baseline: `8d91f4a5dbcf851b3c32fcd89c4bf8c3e34715ff`, tree
`360fd3cfba5ae330fbee159fe48e53713fb25b75`. This is not the final release snapshot.
The notices from `ff47099`, integrated by `f6c3f45`, are preserved unchanged.
See also [release license boundaries](features/RELEASE_LICENSE_NOTICES.md).

## Actual inputs and build boundaries inspected

- `UPSTREAM.json` and `tools/deps.json` agree on Eden commit
  `67bada77f8a43a90da2e94e89b8e7da73c256989`, archive SHA-256
  `d42b6fb492c65eb5e98ab72a786dff01fd64384d073a739b9c9ff475414366ee`.
  The tool rejects disagreement, missing materials or any mismatching hash.
- `tools/deps.json` declares nine archives, six Git repositories and seventeen
  individually pinned files (32 logical inputs). Archive SHA-256/SHA-512 pins
  are checked on the copied bytes. Git HEAD and all allowed tracked working bytes
  must match the pinned commit; exports use Git blobs, never working-tree filters.
  Gitlinks require separately supplied repositories at their exact Gitlink commits.
- Eden's **verified archive**, not an extracted working cache, contains its own
  `cpmfile.json`: 44 entries, 40 with SHA-512 and four unhashed CI variants
  (`ffmpeg-ci`, `openssl-ci`, `sdl3-ci`, `sirit-ci`). Included hashed entries must
  match. Unhashed entries cannot be included by inventing a pin. Every unused CPM
  entry needs an explicit reason and an existing tracked build-evidence path;
  these are operator declarations, **not automatic proof of non-use**. Nested
  `cpmfile.json` definitions in included sources are discovered recursively.
- All five `UPSTREAM.json/archives` pins are retained as explicit requirements,
  conservatively including the older Boost 1.90/header/Context artifacts and
  xbyak pin. Eden's current CPM Boost declaration is **1.92.0**, with separate
  `boost_headers` at **1.91.0**. Those historical pins are not silently substituted
  for the sources actually used by the final build. Missing historical artifacts
  currently block `create`; reconciling stale repository pin records belongs to
  their owner, not an invented override in this tool.
- Several inputs are **binary SDK/distribution packages**, notably pacbrew,
  OpenGL SDK and the official helper Payload SDK. A verified binary archive is
  not corresponding source. Their source/build closure, plus transitive SDK,
  Mesa/RADV, FFmpeg/CPM and other actually linked material, requires review and
  separately hash-inventoried source archives where needed. `extra_sources`
  collects such reviewed additions without pretending their binary match is proven.
- `make package` runs `tools/prepare-build.sh`, `tools/build-package.sh`,
  `tools/build-headless-native.sh` and `tools/package-headless-native.sh`.
  `tools/eden-patches/*.diff` apply in name order. `headless/inject.cmake`,
  `headless/CMakeLists.txt` and other tracked overlays generate/adapt Eden sources.
  All tracked application sources, patches, build/install recipes, license files,
  source assets and headers are exported, with their Git executable modes.
- `headless/network/CMakeLists.txt` builds static `smb2` and `nfs` from the two
  pinned sources, not system-library fallback: libsmb2 tag `libsmb2-6.3`, archive
  SHA-256 `5011003fd091513f897e1ebbb8e1a40af634386df7199716348b7f77569a2790`;
  libnfs tag `libnfs-8.0.0`, archive SHA-256
  `bc91216e927a85142b5de611c7f711558e02119a7daad67620ea631634038350`.
- Final linking goes through `tools/link-headless-native.sh`, the boilerplate's
  `prospero-lld` and `tooling/native/ps5-pie.ld`, `tools/unwind.ld`,
  `tools/app-symbols.map` and `tools/radv-link-eden.sh` / upstream RADV recipes.
  It uses SDK C/C++/unwind libraries and import facades, not just `libsmb2.a`
  and `libnfs.a`. Some application targets (`dynarmic`, native `video_core`) use
  ThinLTO even though the main configure command sets `ENABLE_LTO=OFF`.
  Preserve the matching LLVM/linker and bitcode-compatible objects; do not
  promise that a different linker or plain ELF objects can replace these inputs.

## CLI and inventory

Run in an offline Docker container with the repository and input roots read-only,
a **private** writable scratch directory, and only the designated output directory
writable. Use Python's standard library and Git; inspecting Boost's `.tar.zst`
also uses the Docker image's existing `libzstd` through `ctypes` (streamed private
scratch, no package installation or decompressor shell command). Missing libzstd
or unsupported archive formats fail closed. It is already in the tested host image.

```bash
COMMIT=$(git rev-parse HEAD)
TREE=$(git rev-parse 'HEAD^{tree}')
# These must identify the frozen FINAL build snapshot, not an earlier integration.
python3 -B tools/ci/source_bundle.py plan \
  --repo /checkout --commit "$COMMIT" --tree "$TREE" \
  --materials-root /inputs --inputs /evidence/inputs.json \
  --output /evidence/requirements.json
```

The input JSON is a private operator-authored allowlist, **not** a cache scanner.
It is never copied verbatim into the bundle; local absolute paths are not recorded.
Minimal input for `plan` (replace both hash values with the declared snapshot):

```json
{
  "schema": 1,
  "snapshot": {"commit": "FULL_40_HEX_COMMIT", "tree": "FULL_40_HEX_TREE"},
  "materials": {
    "deps/eden": {"path": "eden-67bada77.tar.gz"}
  }
}
```

Each `path` is relative to `--materials-root`, never an absolute path or `..`.
That root may be a read-only dependency-cache mount; the tool does not walk it.
For each `requirements.json` key supply the actual archive/file or Git repository.
Examples of logical keys are `deps/libsmb2`, `deps/libnfs`, `deps/boilerplate`,
`deps/compiler-rt/emutls.c`, `upstream/boost-context-1.90.0.tar.gz`, and
`cpm/deps/eden/mirror-67bada77f8a43a90da2e94e89b8e7da73c256989/cpmfile.json/fmt`.
The Git path is a checkout directory; its ignored/untracked files are not exported.

Optional top-level fields:

- `cpm_omissions`: object keyed by exact CPM requirement ID, each value
  `{"reason":"Specific, reviewed non-use reason (12–500 characters)",
  "evidence":"tools/build-headless-native.sh"}`. Evidence must be an existing
  file in the declared app tree; its SHA-256 is recorded. Do not omit a dependency
  actually used by the final binary. Direct `deps/` and `upstream/` requirements
  cannot be omitted. Included and omitted IDs cannot both be supplied.
- `extra_sources`: array of `{"id":"reviewed-source.tar.gz", "path":"local.tar.gz",
  "sha256":"FULL_64_HEX", "origin":"https://public.example/source.tar.gz"}`.
  Each source archive is copied, hash checked and path inspected. Public provenance
  URLs cannot contain credentials, query strings or fragments. These supplements
  remain explicitly operator-supplied and subject to binary/source-match review.
- `relink`: absent or exactly `{"status":"PENDING"}`, unless final-build materials
  and the receipt described below are supplied.

`plan` verifies/exports the app privately and verifies Eden to discover initial CPM
requirements; it is **not** a source bundle. More nested CPM/gitlink requirements
can appear when the remaining sources are processed. `create` requires the full
fixed-point inventory, rejects unknown/unused inputs and writes nothing at the
final output path if any validation fails:

```bash
python3 -B tools/ci/source_bundle.py create \
  --repo /checkout --commit "$COMMIT" --tree "$TREE" \
  --materials-root /inputs --inputs /evidence/inputs.json \
  --output /evidence/ProsperoEden-1.006.000-source.tar
sha256sum /evidence/ProsperoEden-1.006.000-source.tar
python3 -B tools/ci/source_bundle.py verify \
  --bundle /evidence/ProsperoEden-1.006.000-source.tar \
  --sha256 RECORDED_OUTER_SHA256 --commit "$COMMIT" --tree "$TREE"
```

Exit 0 means only the operation's stated result; validation refusals exit 2.
Existing outputs are never overwritten. Plan/create success prints the outer
SHA-256. `verify` requires that independently retained hash AND the expected
commit/tree, checks every member hash/size/mode against `SOURCE_BUNDLE.json`, and
does not extract or execute the contents. A manifest beside mutable files with
no externally retained hash is not an authenticity proof.

### Bundle layout and reproducibility

- `app/`: exact allowed Git blobs, including overlays, ordered patches, generated
  source recipes, build scripts and notices. No `.git`, worktree diff, arbitrary
  untracked file or ignored directory is exported. Dirty application working bytes
  are deliberately ignored, never attributed to the declared snapshot. **Therefore
  do not use this snapshot to represent a dirty final build.**
- `materials/deps/`, `materials/upstream/`, `materials/cpm/`, `materials/gitlink/`:
  unchanged pinned archive/file bytes or exact Git source trees. Archive inputs use
  an `.archive` suffix; original URLs and expected hashes are in the manifest.
- `extra-sources/`: explicit supplemental source archives, if supplied.
- `relink/`: explicitly inventoried artifacts and receipt only, if collected.
- `SOURCE_BUNDLE.json`: snapshot, collector hash, input provenance/pins, manual CPM
  decisions, every payload file SHA-256/size/mode, honest source/relink status and
  outstanding reviews. It excludes itself from the recursive file-hash list.

The outer archive is **uncompressed deterministic TAR**, sorted by member name,
regular files only, modes 0644/0755, uid/gid/mtime zero and empty owner names. No
clock, hostname, absolute input paths or build-cache metadata is generated into it.
The identical snapshot, input bytes, decisions and collector produce identical
bytes on the tested Python/tar implementation; changing the collector changes its
recorded hash. This is archive reproducibility, **not binary reproducibility**.

For restoration use the retained `app/tools/deps.json` extraction/save/destination/
strip rules and Git sibling layout, plus the CPM recipes and recorded pins.
Supplying a tarball is not proof that offline restore/configure/build succeeds.
The coordinator must document and test that restoration in an empty directory,
including binary-package source supplements and modifications not expressible by
the retained tracked recipes. Do not simply rerun a network fetch and call it a
verification of the delivered source materials.

## Final-build static-relink collection (optional; otherwise PENDING)

Do not reuse an integration/host-check object, sample ELF or an earlier release
artifact. Preserve the final native build before cleanup. Prepare a narrow,
reviewed inventory of:

1. All non-LGPL application objects/archives necessary for the final executable,
   including generated inputs and ThinLTO-compatible IR as actually linked.
2. The final build's `libsmb2.a` and `libnfs.a` as comparison libraries, their exact
   source/configuration/local changes, and all other required static libraries,
   import stubs, link scripts, response files and generated symbol/version maps.
3. A portable relink command/script with its working-directory/layout, toolchain
   versions and acquisition/source/build recipes. Capture narrow build-system link
   commands, **not** an unreviewed environment dump, entire Ninja cache or log tree.
   Inspect/review absolute-path rewriting; do not alter object/library bytes.
4. The final packaged `eboot.bin` hash and evidence mapping the actual link output
   (`native-local/bin/eden-headless`) through stamped `llvm-pie.elf`, package
   conversion and release identity to that exact eboot. Preserve matching link maps
   and build identity as explicitly inventoried files. An eboot hash alone does
   not prove which source or objects produced it.

Replace the optional `relink` object with:

```json
{
  "status": "COLLECT",
  "snapshot": {"commit": "FULL_40_HEX_COMMIT", "tree": "FULL_40_HEX_TREE"},
  "final_binary": {"path": "final/eboot.bin", "sha256": "FULL_64_HEX"},
  "receipt": {"path": "final/relink-receipt.json", "sha256": "FULL_64_HEX"},
  "files": [
    {"name":"application.o", "role":"non_lgpl_object", "path":"reviewed/application.o", "sha256":"FULL_64_HEX"},
    {"name":"libsmb2.a", "role":"lgpl_library", "path":"reviewed/libsmb2.a", "sha256":"FULL_64_HEX"},
    {"name":"libnfs.a", "role":"lgpl_library", "path":"reviewed/libnfs.a", "sha256":"FULL_64_HEX"},
    {"name":"relink.sh", "role":"commands", "path":"reviewed/relink.sh", "sha256":"FULL_64_HEX"},
    {"name":"ps5-pie.ld", "role":"link_script", "path":"reviewed/ps5-pie.ld", "sha256":"FULL_64_HEX"},
    {"name":"toolchain.txt", "role":"toolchain_recipe", "path":"reviewed/toolchain.txt", "sha256":"FULL_64_HEX"}
  ]
}
```

This is a schema example, **not a sufficient real link inventory**. Other allowed
roles are `other_link_input`, `generated_source` and `link_map`. Names are unique
relative destination paths. `libsmb2.a` and `libnfs.a` must each be present under
the `lgpl_library` role. At least one object, command, link script and toolchain
recipe must be supplied. No role's existence proves the complete link closure.
Explicit hash-allowlisted generated objects can be collected even though ordinary
build outputs are ignored by Git; ignored **source-tree scanning** remains forbidden.

The reviewed receipt file has exactly these fields:

```json
{
  "schema": 1,
  "snapshot": {"commit": "FULL_40_HEX_COMMIT", "tree": "FULL_40_HEX_TREE"},
  "final_binary_sha256": "SAME_FINAL_BINARY_SHA256",
  "lgpl_sources": {
    "libsmb2": "5011003fd091513f897e1ebbb8e1a40af634386df7199716348b7f77569a2790",
    "libnfs": "bc91216e927a85142b5de611c7f711558e02119a7daad67620ea631634038350"
  },
  "files": {
    "application.o": {"sha256":"SAME_OBJECT_SHA256", "role":"non_lgpl_object"}
  }
}
```

The receipt's `files` map must enumerate **every** inventory file, not just the
example object. Snapshot, LGPL pins, final binary bytes and every relink input hash
must match; stale receipts, missing/corrupt objects, incomplete required roles and
unknown receipt fields fail closed. The binary is checked but **not bundled**.
The receipt is an operator claim, not a signature or proof of authentic build
provenance. Even on success the only possible collected status is
**`COLLECTED_UNVERIFIED_RELINK`**. No `READY` or legal-compliance status exists.
Independent provenance/closure review and a modified-library relink trial remain
pending; synthetic fixtures cannot qualify those steps.

## Privacy, filesystem and trust limits

- Names are checked before input-child metadata/opens. Private `config/nextendo`
  subtrees and `config/network-shares.json*` aliases (case, alternate separators,
  trailing dot/space) are refused, as are `.local`, `.deps`, `.cache`, `.git`,
  `.env*`, `.ssh`, `.aws`, known key files and `oauth.json`. The tool does not open
  or descend into a denied node. Tracked denied paths fail the export rather than
  silently creating an incomplete source claim. Ignored/untracked app and Git
  dependency paths are never discovered by a recursive walk.
- Local roots/parents/files cannot be symlinks. Git symlinks, special files,
  traversal/absolute/device paths, case aliases and duplicate archive members are
  refused. Pinned TARs may retain relative internal symlinks **inside the original
  archive bytes**; escaping links/hardlinks/special members and ZIP symlinks fail.
  No upstream archive is extracted here. Do not later extract one unsafely.
- The read-only inputs must remain quiescent. These checks are not a filesystem
  race sandbox. Byte verification uses private staged copies before publishing the
  output; publication is same-filesystem no-clobber. Failures clean private staging.
- This is a bounded name/allowlist policy, **not semantic DLP or a guarantee that
  arbitrary tracked source/explicit command files contain no secret**. Review any
  new committed material, omission reason, source supplement, receipt and relink
  script before invocation. Never point the allowlist at real credentials, game
  data, user configuration, tags/keys or downloaded artwork/catalogs. No such data
  is required by this mechanism. Do not ship an entire build/cache volume.

## Validation evidence and coordinator gates

The real CLI synthetic suite is `tools/ci/test_source_bundle.py`; run without any
dependency download or compiler:

```powershell
docker run --rm --network none --cpus 2 --memory 1g --pids-limit 128 --cap-drop ALL --tmpfs /work:rw,size=512m -e TMPDIR=/work -e PYTHONDONTWRITEBYTECODE=1 --mount "type=bind,source=$($PWD.Path),target=/repo,readonly" --entrypoint python3 prosperoeden-host:features -B /repo/tools/ci/test_source_bundle.py
```

At implementation time, **39 real CLI tests passed**: missing/corrupt direct and
SHA-512 inputs, pin disagreement, snapshot/tree mismatch, traversal/absolute/device
paths, private aliases before stat, local/Git symlinks and FIFO, ignored/untracked/
dirty-app exclusion, dirty Git dependency refusal, duplicate JSON/archive/member
manifests, nested CPM/gitlinks, omission evidence, output no-overwrite, externally
anchored verify/tamper refusal, supplemental sources, deterministic output, PENDING
status, collected-but-unverified synthetic relink and stale/missing/corrupt receipt/
object refusal. Valid multi-buffer `.tar.zst` source inspection and truncated Zstd
refusal are included. All object/binary fixtures are synthetic, never release artifacts.

Actual baseline `plan` was also executed with the existing read-only integration
Eden archive (hash above). It reported 81 initial requirements (32 direct, five
historical UPSTREAM artifacts, 44 Eden CPM). Its canonical plan SHA-256 is
`7b51d496bdfed971cbb705174577faa1dc12e061a0039cc35eecef0d1ba2b038`.
Only Eden was supplied: actual `create` refused missing CPM biscuit and left no
bundle. This is a tested refusal, **not a complete real source/relink bundle**.
Private logs live in the exclusive worker's `.local/source-bundle-cli-tests.log`
and `.local/source-bundle-real-plan.log`; the plan is
`.local/source-bundle-real-plan.json`. No coordinator volume was mounted writable.

Before describing any final deliverable as the source/relink companion to 1.006.000:

- [ ] Freeze final HEAD/tree and an immutable, clean final-build snapshot; verify
  its relation to the actual shipped eboot/package identity. Rerun final tool/tests.
- [ ] Supply every declared direct/upstream material and recursively required used
  CPM/Gitlink source, with the genuine archive/hash/commit. Reconcile stale pins
  with their owner. Independently check each non-use decision against final build
  configuration/link inventory, not the existence of its evidence file alone.
- [ ] Resolve sources, notices, patches and recipes for actually redistributed
  binary dependencies and transitive components. Audit local upstream changes and
  generated source not captured by the tracked recipes; add reviewed supplements.
- [ ] Restore/configure from delivered materials in a new directory and verify
  the claimed build path, with no private cache fallback. Retain procedure/results.
- [ ] Select/review the applicable LGPL/GPL distribution route. If supplying a
  static-relink route, deliver complete non-LGPL objects and link closure and show
  that recipients can relink a modified library with the supplied commands/toolchain.
  Keep applicable modification/reverse-engineering-for-debugging rights; do not
  claim dynamic replacement for this static build.
- [ ] Bind the receipt to that final snapshot/binary and all inventoried artifacts;
  independently validate provenance and perform/record the modified-library trial.
  Until then retain PENDING / COLLECTED_UNVERIFIED_RELINK, never READY.
- [ ] Verify final source/relink archive hashes and extracted safe inventory,
  retain notices/license/source-delivery information, privacy review, and record
  which requirements remain unresolved. Final packaging/integration/publication
  is coordinator-owned and is not performed by this change.
