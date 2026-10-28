# Integration source-policy follow-up (2026-10-27)

Continuation of preserved `b037e54` and `8ee544e` (the latter imported by the
coordinator as `0bc00f9`). **Not** a final 1.006.000 receipt, build,
source-completeness assertion or relink qualification. Relink remains **PENDING**.

## Explicit reviewed policy

`source_bundle.py` adds `source_policy`, copied into the bundle manifest:

* `git[dependencyID]`: exact `commit`, `tree`, `reason`, `templates`, `symlinks`.
  Template entries require exact path, SHA-256 and individual reason. Only root
  `.env.example` is eligible; **app cannot opt in**. Other `.env.*`, private app/
  config paths, client tokens, network shares, keys and aliases remain refused.
  A wrong pin is refused before child metadata; an immutable template hash is
  checked before local template metadata. Unused approvals are refused too.
* `symlinks[path]`: exact Git link-text SHA-256. Only reviewed component pins can
  transport mode `120000`. Working links use `lstat`/`readlink`, never target
  reads; regular tracked bytes and executable modes remain checked.
* `archives[materialID]`: original SHA-256, reason, exact
  `hardlinks[path] = {target, reason}`. The archive-root-relative target must be
  an existing REGULAR member of that archive, independent of member order.
  Other hardlinks/special/FIFO, chains, cycles, aliases and symlink targets fail.

The coordinator inspected these defaults-only public root templates (example IP
`192.168.1.100`, FTP port `2121`):

| Dependency / full commit / tree | `.env.example` SHA-256 |
|---|---|
| Boilerplate `dd44bbdc75437332ed22e3ba95126733419ef25a` / `245988cd4afc2fddb7aab8b09aa6f46cf60d3581` | `3b0dff510ab8125bb130e4d2c997b2ec922720e1f5c9b6df4b2e5de2fae3a2d1` |
| Vulkan recipes `5b5e4fc2d80fdb67a4f61ba9e6a8a424026bb8a5` / `fb9253ee37641a79046aaafb12cc15d7e080bdd2` | `a4946581bc43f38d38f560df15d66e56c91d74edaa3d41130883814167ba4c65` |

Reviewed links: Mesa **10**, RADV SDK **15**, official SDK **15**, at the exact
pins in `RELEASE_SOURCE_MATERIALS_1006000.md`. Every path/hash is explicit in
`policy-inputs.json` and `SOURCE_BUNDLE.json`. Sole pacbrew hardlink:
`opt/ps5-payload-sdk/target/user/homebrew/bin/libdeflate-gzip` →
`opt/ps5-payload-sdk/target/user/homebrew/bin/libdeflate-gunzip`, bound to
`a85f65de418a8e6a898c6c3e3c870d50fff7618a200e4dd59ea9692af6ecec4d`.

Graphs are resolved without filesystem I/O. Absolute/Windows/backslash,
escaping/private/missing targets, case/device/prefix aliases, direct cycles and
directory-link descendant cycles fail. Redundant separators in relative POSIX
link-text (pacbrew has `../target//...`) are resolved normally but preserved
literally. Source member names still reject empty path segments.

## Schema 2 transport, verification and restore

The deterministic outer TAR remains **REGULAR files only**. Normal Git files
keep source paths/modes; exceptional templates/link-text use opaque safe
`git-blobs/` storage. `git_sources` records every original path, mode, Git blob
ID, SHA-256, size, storage path and link-text. Raw commit/tree objects are
rehashed recursively; export-ignore/export-subst cannot omit/mutate source.
Empty trees, `100755`, nested CPM and Gitlinks are preserved. The current
exporter/restorer is embedded in `bundle-tools/`, since the app snapshot
predates this implementation. Schema 1 lacks these proofs: use its historical
tool to inspect old schema 1 bundles, not this schema 2 verifier.

`verify` requires external TAR SHA-256 and commit/tree, rechecks objects and
every byte/mode/approval/graph, rediscovers direct pins/nested CPM/Gitlinks,
checks omission evidence and audits embedded archives against their pins.
It executes no recipes. `restore` verifies first, refuses existing output,
writes regulars first, hardlinks next, symlinks last, never uses `extractall`,
and rechecks restored Git hashes/modes/readlink text. Inputs/outputs must be
private and quiescent: this is not a concurrent-filesystem race sandbox.

Seven Git supplements now use `extra_git_sources` entries (ID, local path,
public origin, full commit/tree), through the same proof/discovery route.
Eight archive supplements remain original bytes. Neither proves binary matching.
The 22 original technical CPM non-use decisions remain unchanged/provisional.
All five historical artifacts remain, including Boost 1.90; selected CPM Boost
is 1.92. No repinning, omitted sources or license edits.

## Custody and canonical inputs

Received volume `prosperoeden-team-source-materials-20261027` was mounted
**RO** at `/received`; its 12-file index anchor is still
`35f79c0398074667598bab13bb5b7ce163521ffb7cc53bb52b1f5dcc0c2c2277`.
No original 59/15 inputs, inventories, Git stores or restores were overwritten.
New evidence is in **`prosperoeden-team-source-policy-20261027`**, under
`/work/policy-followup`.

The initial actual create correctly refused byte-dirty tracked files: **40
CRLF-only checkout conversions** differed from immutable LF blobs. The dirty
check was **not relaxed**. `canonical_source_inputs.py` exports all 13 selected
Git inputs to NEW private checkouts without filters, populating index/objects
without a working-tree checkout. This route refuses any non-CRLF received
difference rather than discarding a genuine local source modification. Original
bytes stay RO. `canonical-source-report.json` records both hashes per conversion;
its SHA-256 is `0fed3d425597198aadedc478b565b6ce12414f97a2ab6edb0e911a0fef497146`.

Integration snapshot: **`8ee544eb5077ad89bfbc848ea22c4cf4a375477d`**, tree
**`93665566189319ff7a072cf353e4cb3be242e1c1`**, not final release identity.
`policy-origin.json` records revalidated unchanged source/dependency/omission
recipe hashes versus received `3e0fb73`; it is not an edited final-build receipt.

The integration source-material TAR retains explicitly pinned **public binary
SDK/package archives** too. They are not corresponding-source substitutes,
final native objects, userdata or a relink receipt. “Source-only integration”
does not reclassify those public binary archives as source.

## Reproduce without building

Use existing `prosperoeden-host:features`, `--network none --cpus 2 --memory 2g
--pids-limit 128 --cap-drop ALL`, `/repo:ro`, private `/work`, and
`TMPDIR=/work/tmp PYTHONDONTWRITEBYTECODE=1`. Initial preparation reads
`/received:ro`; repeats/restores need no received/coordinator/native mount.
First prepare a private object snapshot of the integration commit above.

```sh
python3 -B /repo/tools/ci/prepare_source_policy.py --received /received \
  --workspace /work/NEW-followup --snapshot-repo /work/snapshot.git \
  --snapshot 8ee544eb5077ad89bfbc848ea22c4cf4a375477d
python3 -B /repo/tools/ci/canonical_source_inputs.py --received /received \
  --workspace /work/NEW-followup
python3 -B /repo/tools/ci/source_bundle.py create --repo /work/snapshot.git \
  --commit 8ee544eb5077ad89bfbc848ea22c4cf4a375477d \
  --tree 93665566189319ff7a072cf353e4cb3be242e1c1 \
  --inputs /work/NEW-followup/policy-inputs.json \
  --materials-root /work/NEW-followup/canonical-materials --output /work/NEW.tar
python3 -B /repo/tools/ci/restore_source_policy.py --bundle /work/NEW.tar \
  --sha256 EXTERNALLY_RECORDED_HASH \
  --commit 8ee544eb5077ad89bfbc848ea22c4cf4a375477d \
  --tree 93665566189319ff7a072cf353e4cb3be242e1c1 --output /work/NEW-restore
```

The trial restores Git and direct/supplemental archives, applies five Eden
patches with forward/apply/reverse checks, seeds 22 exact CPM artifacts and runs
the **unmodified upstream CPM source-fetch script** offline. It checks patch
keys/reverse application and repeats upstream fetch to test unchanged cache
reuse. Only the source-header prefix of the tracked Xbyak overlay runs in CMake
script mode: no target setup, top-level configure, compiler or linker.

## Concrete remaining blockers

* Complete actually used transitive/toolchain/generated-source closure and
  source-generator versions/options. A reproducible TAR does not establish it.
* Native offline configure and remaining headless derived sources/target edits,
  including Dynarmic and Boost.Process. No top-level configure success claimed.
* Actual local SDK/runtime/import-stub/RADV/OpenGL patches, generated-driver
  provenance and binary build options. Pristine pins and sampled savednative
  observations remain planning-only, not final applied-source or binary evidence.
  In particular, `tools/build-radv-dependencies.sh` rewrites the upstream
  `tooling/radv/ps5-cross.ini` compiler commands and invokes the WSI, GS-compute,
  disk-cache and transfer-split patch scripts. The resulting `EDEN_WSI_SHA256`,
  `EDEN_GS_SHA256`, `EDEN_DISK_CACHE_SHA256`, `EDEN_TRANSFER_SPLIT_SHA256` and
  actual applied files still need comparison with the final driver build; they
  were not borrowed from savednative. Likewise the SDK/runtime binaries must be
  matched to the retained LLVM 18.1.8 sources and exact runtime build recipe.
* Final clean HEAD recipe/omission review, source/binary match, coordinator's
  single final build, compatible actual non-LGPL objects/link closure, binary-bound
  receipt, independent verification and modified-library relink trial.

No full build/setup/install, runtime/services/native collector/CMake changes,
version change, config/secret scan, extra agent, push/tag/publication.

## Actual integration evidence

Implementation commit: `3b2ffdf` (collector/proofs, independent verification,
safe restore, offline CPM/selected-overlay trial and adversarial tests).
Original `test_source_bundle.py` and `test_release_source_preparation.py` are
unchanged: **39 + 17 + 23 = 79 tests PASS** in the capped offline container.

The real bundle contains **74 materials**, **88 requirements**, **22 provisional
CPM omissions**, **14 Git source proofs including app**, **40 reviewed Git
symlinks**, **two templates**, and **24,124 regular transport files excluding
the manifest**. It is **1,778,554,880 bytes**, with `relink=PENDING` and
`source_status=PINNED_INPUTS_COLLECTED_FINAL_BINARY_AND_TRANSITIVE_SOURCE_REVIEW_PENDING`.
Independent verification rediscovers requirements from actual source bytes.

The actual offline restore passed **17 direct/supplemental archives**, all 13
Git inputs (plus app), **22 original CPM source fetches**, five Eden patches,
upstream patch-key/reverse checks and byte/mode/link-identical CPM cache reuse.
The Xbyak source-header overlay ran without targets/configure/build. The sole
pacbrew hardlink restored to the same REGULAR inode as its reviewed target,
whose content SHA-256 is `647948a381d35b57fb481d5081d2a646fcbb7acbdb99948b07df84cdb397465f`.
Received **74 input hashes / 12-file index / 13 Git fsck checks** and all **13
new canonical Git fsck checks** passed; received storage was never writable.

Paths below are relative to `/work/policy-followup` in
`prosperoeden-team-source-policy-20261027`:

| Path | SHA-256 |
|---|---|
| `ProsperoEden-integration-source-policy-v2-PENDING.tar` | `540dcdae30b5045f7b4fc691960e9015b029e208e3fa755563bef0a4b2939ac5` |
| `ProsperoEden-integration-source-policy-v2-repeat-PENDING.tar` | `540dcdae30b5045f7b4fc691960e9015b029e208e3fa755563bef0a4b2939ac5` |
| **`policy-final-evidence-sha256.json` (23 files, including both TARs)** | **`5cc34f32f5e63998859abc79dbece599c4f00b93acb19b4f61a84b07a4cc34e5`** |
| `policy-reproducibility.json` | `0269724d1e1da9e29411cfaac405613e879253417581c2b8800a258398296078` |
| `policy-evidence-sha256.json` (18 evidence files) | `6e6056367c0bb9bfaffd7f235b9d8b96f1eb85892495cdafc70a8f5a5186d43b` |
| `policy-inputs.json` | `c4747d57dd93cb5f20ddae8984c167d5c8794bdc484dd3bc6bb2d58f3ca09859` |
| `policy-origin.json` | `006738bc95830af1e1cfb52f39ebab04604dd107d7bdd6034756ee4a31e25e90` |
| `received-final-integrity.json` | `8360e81821a2d12e24192d958393b5ca0c146abf8c29d56d5abc6064cac85ad5` |
| `policy-bundle-summary.json` | `250b66a39bd1d7234de88d7d2f9ebf4f99ee5f6682e43516e424144ff59ee61e` |
| `offline-restore-v2-01/SOURCE-RESTORE-RESULT.json` | `0e820ddd9da565904b238f71f3ceec4d8cc490db43ed204dbb7ad3aa3922685a` |
| `offline-restore-v2-01/CPM-source-inventory.json` | `273e2235288c05461985a8f7e3c32f60b4fd27e15a8b78cfbaf5b26b97e5aeee` |

The earlier successful draft TAR `ProsperoEden-integration-source-PENDING.tar`
(SHA-256 `cc1150b79abf7ad21b79a4e38a15937fddfb9270368c42703107d3537969c881`)
is retained as draft evidence, **superseded** by policy-v2 and its stricter
independent requirement/archive checks. Failed-attempt logs are also retained;
they were not converted into successful receipts. Do not publish the entire
volume, including scratch/object stores/trials, as a release artifact.

Two independent real creates produced the identical policy-v2 TAR: actual
`cmp` and independent SHA-256 calculations passed. Repeat used only canonical
private inputs, the private snapshot and `/repo:ro`, with networking disabled;
no received/coordinator/native mount was needed. The final 23-file index extends
(does not overwrite) the earlier 18-file evidence index and includes both TARs,
the real comparison log and reproducibility result. Exporter source SHA-256:
`9d8942ef3409a121827213e14d7c45061135457ce337190fbdc7155baf3c8920`.

Image identity: `sha256:e6137ec556fe8e63ebf25e7fabe36e573d089c469d0e9d391c3072f2ae04d551`.
These evidence identities bind this integration snapshot and exporter bytes,
not a future final coordinator HEAD. Regenerate and re-review at final HEAD.
