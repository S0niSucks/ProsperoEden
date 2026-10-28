# Release ZIP license integration

## Contract and boundary

Branch `team/release-license-packaging-1006000` starts exactly at
`f1a92c6c004b39891f35df75e558645f11ba7853`, including the license/notices import `f6c3f45`.
This change integrates the retained notices into **release ZIPs only**. It does not change
launcher-update licensing, source/license texts, native build scripts, versions or release inputs.

`tools/ci/make-dist.py` includes project `LICENSE`, `THIRD_PARTY_NOTICES.md`, and every file
in `third_party/licenses/` at the **same root-relative ZIP paths**, not under `PPSA99008/`.
Thus the notices' `third_party/licenses/...` links resolve to the retained files/directories.
The current manifest SHA-256 is
`fd79de67ad2fd83a6cd781b31d672f408a6c35dd15d060af823f48d1ab0588dc`:
it declares 11 license/notice files; with `SOURCES.json`, 12 license-tree files are packaged.
Three component directories contain `SOURCE_NOTICES.txt`; no new root-level notice file is
invented. Raw upstream bytes, including line endings and trailing whitespace, are preserved.

`tools/release_licenses.py` fails closed before opening the output ZIP if the directory,
manifest, required project notices or declared files are absent or malformed. It checks:

- `lstat` directory/regular-file types; no symlinks or special nodes are read or followed,
  including either directory ancestor and the manifest itself.
- Format version, nonempty file list, safe POSIX destinations, unique/case-distinct names,
  duplicate JSON keys, integer sizes and complete lowercase SHA-256 declarations.
- Exact inventory equality with `SOURCES.json` plus the manifest itself. Extra/unlisted
  files are rejected, **not silently dropped**, so successful packaging includes the full
  licensed file inventory. New retained files require corresponding manifest declarations.
- Each declared file's actual size and raw-byte hash. Checked notice bytes are snapshotted
  once and those same bytes are written to the ZIP; no text reserialization/normalization.

Only top-level `files[].destination/size/sha256` describes the retained payload. Nested excerpt
records, archive/source URLs and hashes remain documentary provenance, not full source files
to fetch or package. License excerpts are **not corresponding source**. This integration does
not choose a distribution route or promise corresponding-source/relink materials on anyone's
behalf. That material, final-binary verification and the remaining compliance review are still
pending separately, as detailed in `RELEASE_LICENSE_NOTICES.md`.

Packaging inputs must be quiescent: these are `lstat`/byte-integrity checks, not a filesystem
race sandbox. Existing release 0777 ZIP attributes, fixed timestamps, SHA256SUMS and optional
build-identity code remain in place. README remains optional but must be regular if present.

## Actual CLI qualification

`tools/check-release-licenses.py` stages only a synthetic non-executable app in a miniature
repository, plus the maintained verified notice bytes. It runs the actual release CLI twice,
checks exact ZIP membership and every extracted byte, verifies all 11 declared sizes/hashes,
checks relative license links, permissions/timestamps and SHA256SUMS, and requires identical
ZIP bytes even after changing notice-file mtimes. The app is not an actual release binary.

Adverse CLI fixtures cover missing directories/manifest/license/project notices, altered file
size and same-size altered bytes, malformed/duplicate-key JSON, unsupported format, invalid
records/hashes/sizes, duplicate/case-colliding/reserved destinations, traversal/absolute/
backslash/colon/trailing-alias paths, and undeclared extra files. Linux additionally covers
directory, component, file, manifest and project-license links, README links and FIFO nodes.
An independent IO guard permits `lstat` but records/refuses payload open and directory
enumeration on those synthetic hostile nodes: a passing refusal must make **zero attempted
forbidden IO operations**, rather than merely failing because the guard blocked a read.

`tools/check-export-privacy.py` now creates an explicit synthetic license directory/manifest
for miniature release fixtures and copies the new helper when present. Its share/Nextendo
privacy guard, blocked-node tests and launcher validations are unchanged. New release ZIPs
must include the expected license entries; missing entries cannot be waived to make a test
pass. Comparing an older collector explicitly checks the intentional added entries plus
unchanged bytes/metadata for **all old members**. Launcher clean ZIP byte equality remains
required. The historical `df7cfdf` hashes/claims in `EXPORT_PRIVACY.md` remain bound to their
old tested source; they are not claims about the intentionally changed release archive.

The license comparison uses unchanged `f1a92c6` collector code. The additional privacy run
retains the original `ac4a0b1` baseline to reproduce its leaks/pre-deny traversal controls.
Neither baseline archive contains or packages real user data. Every miniature repository
omits optional `build/` ELF/symbol inputs **by design**; no ELF is compiled or faked, and the
optional build-identity/symbol branch is not qualified by these fixtures.

## Native app / PKG placement review (not changed)

`Makefile:35-39` first builds `build/release/PPSA99008`, then passes it to `make-dist.py`.
`tools/package-headless-native.sh:50-51` copies only the Lapy MIT notice into the app's
`licenses/` folder; it does not copy this new root-level license tree/project notices.
`make install` uses `tools/install-ftp.py` on the **app folder**, not the release ZIP.

Therefore this ZIP fix does **not** qualify or populate a standalone app-folder/PKG
distribution. A native/PKG pipeline using that folder still needs a coordinated placement
decision and actual package inventory/extraction review, including valid internal relative
links. Native scripts were deliberately not edited or run. This task neither generated a
PKG nor made a source/relink delivery or legal-compliance certification.

## Reproduction and evidence

The ignored evidence root in the assigned worktree is
`.local/release-license-packaging-20261027/`. Source snapshots contain only the scoped Python
tools, unchanged dependencies needed by their fixtures, and the maintained notice tree/project
notices; they contain no application build, cache, real app-data folder or credentials.

```powershell
python -B tools/check-release-licenses.py --baseline .local/release-license-packaging-20261027/baseline --output .local/release-license-packaging-20261027/windows-licenses
python -B tools/check-network-privacy.py --baseline .local/export-privacy-20261027/baseline --output .local/release-license-packaging-20261027/windows-privacy
```

The retained `input/run.sh` extracts exact source/baseline tar files into a fresh owned private
volume directory, then runs both same Python checks on Linux. Existing image
`prosperoeden-host:features` is used with 2 CPUs / 2 GiB, no networking and all capabilities
dropped. No extra agent, dependency fetch, installation, native/full build or publication is
involved. Use fresh output directories when reproducing; retained evidence is not overwritten.

```powershell
$evidence = 'D:/leonardommello/PS5/ProsperoEden/.local/agent-worktrees/amiibo-sync/.local/release-license-packaging-20261027'
docker run --rm --name prosperoeden-team-release-license --cpus=2 --memory=2g --pids-limit=128 `
  --network=none --cap-drop=ALL `
  --mount "type=bind,source=$evidence/input,target=/input,readonly" `
  --mount "type=bind,source=$evidence/linux,target=/export" `
  --mount type=volume,source=prosperoeden-team-amiibo-sync-20261027,target=/work `
  --entrypoint bash prosperoeden-host:features /input/run.sh
```

The image identity was rechecked as
`sha256:e6137ec556fe8e63ebf25e7fabe36e573d089c469d0e9d391c3072f2ae04d551`.
Linux fixtures/results remain at `/work/release-license.u6Gbb6/` in the private volume.

## Results and hashes

**PASS** on both hosts (counts include expected refusal and baseline-control invocations):

| Actual CLI checks | Linux Python 3.12.3 | Windows Python 3.14.4 |
| --- | --- | --- |
| Release licenses, real retained notices plus synthetic app | 37 | 26 |
| Share/Nextendo packaging privacy and existing launcher validation | 121 | 35 |

All three component `SOURCE_NOTICES.txt` files, the eight whole license copies, and the raw
`SOURCES.json` are present at the matching ZIP paths. Extracted bytes/hashes, 0777 attributes,
fixed timestamps, checksums, relative license links and same-host reproducibility pass.
Every old member's bytes/metadata match the `f1a92c6` fixture archive; exactly the 12 expected
license-tree entries were added. No license-text byte was modified. License hostile-node
refusals show zero attempted forbidden IO; fixed privacy runs show zero attempted guarded
private-node operations. Launcher-update collector code and its validation rules were not
changed. Windows explicitly skips physical symlink/FIFO and POSIX-only filename fixtures;
those ran in Linux rather than being counted as Windows passes.

| Retained artifact | SHA-256 |
| --- | --- |
| Tested source snapshot tar | `ce447f0eed717aa0126448be456986a4b3cd210270d740986b5d8133f3470c63` |
| License baseline tar (`f1a92c6`) | `e07dcd2ab3dfcaac30228d32069d81208f57f5c3fb656f8a24aad896302820b3` |
| Privacy baseline tar (`ac4a0b1`) | `5006ca8d5549347e39e0b0b52e7c77e191e3a3b0fee94c4a983b22577a07576a` |
| Linux license report | `e5aded04361e477aefd18082a7310785f87697569c3b60881381b05850bf6275` |
| Windows license report | `1b0ef56252e29acb6a162187c33906500908c7f9a4c0eb7880d83e166eef116c` |
| Linux privacy report | `aeabfb0272f88fa142ddebd8ef9ea4ed70bd1a4696c2babb9b585b68bc9fbedc` |
| Windows privacy report | `187e8673a24052aa5702c68eb0bd0f54e1bfa26db0e4349de142d8bf4f1d466f` |
| New Linux synthetic-app / real-notices ZIP | `75012a491f2358d319cee9dd09063a1be1397a36157c940a1bd50ac99950269b` |
| New Windows synthetic-app / real-notices ZIP | `28f4fb3b148bce2548804ce88c4dd43bd2b8cfb2ad2fd4ec12525b55c7aedac3` |
| Verified Linux export index | `ae50c579a7af76f874580e3c9c061a7dcec383097668617f24dd037b1ed4a7b1` |

The two ZIP hashes differ because the Linux fixture deliberately includes extra POSIX-only
public alias-path controls; equal inputs on each host reproduce byte-for-byte. They are
qualification fixtures, **not actual release assets**.

Final exports are under `linux-verified/` (reports, per-invocation CLI/stdout/stderr/guard logs,
ZIPs/feeds/checksums, portable extracted notice files, full `extracted.tar` and hash index).
All **783** indexed exported artifact hashes were rechecked using local Python, and current
collector/test hashes match the tested snapshots on both hosts. `qualification.json` records
the same evidence plus both intentional baseline/new ZIP deltas and exact added member lists.

The original `linux/` export attempted to expose the full extracted POSIX fixture tree as
ordinary Windows paths. Post-copy verification caught Windows normalization of `config.`
alias names; it is retained but **not used as the verified export**. Only the exporter was
corrected: the existing Linux results were re-exported with the full extraction in tar and
the portable license tree separately. No test/policy/ZIP bytes were changed or waived, and
the original passing CLI reports retain their hashes. That read-only re-export used:

```powershell
docker run --rm --name prosperoeden-team-license-evidence --cpus=2 --memory=2g --pids-limit=128 `
  --network=none --cap-drop=ALL `
  --mount "type=bind,source=$evidence/input,target=/input,readonly" `
  --mount "type=bind,source=$evidence/linux-verified,target=/export" `
  --mount type=volume,source=prosperoeden-team-amiibo-sync-20261027,target=/work,readonly `
  --entrypoint python3 prosperoeden-host:features -B /input/export.py /work/release-license.u6Gbb6/results /export
```

This closes the bounded release-ZIP notice-inventory gap only. Native app-folder/PKG notice
placement, final-binary dependency inventory, and applicable corresponding-source/relink
materials/delivery remain pending separate coordinated qualification.
