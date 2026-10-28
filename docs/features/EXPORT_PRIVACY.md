# Export privacy qualification

## Policy and scope

Release (`tools/ci/make-dist.py`) and launcher-update (`tools/launcher/package-update.py`)
exports must exclude these paths anywhere below their input root, including nested app-data:

- `config/network-shares.json*`: the existing **broad prefix** is intentional. It covers the
  main file, staging/editor/backup siblings and any descendants if such a name is a directory.
  It is not narrowed to just `.tmp` or `.new`, even when the file is empty or disabled.
- The **entire** `config/nextendo` node/subtree: account, OAuth, routes, tokens and any other
  children. This applies independently of the USB backup policy.

Name matching is case-insensitive, treats `/` and `\` as separators, and removes trailing
Windows dots/spaces from each component **for the deny decision only**. Matching any adjacent
`config` / private-name pair protects nested app data too. Archive names are never rewritten.
This is deliberately not an all-config blacklist: settings, public assets, `nextendo.json`,
`nextendo-public/` and nonmatching names remain eligible under each collector's normal rules.
It does not promise content-based secret detection or identify arbitrary renamed credentials.

`public_export_paths` lists public parent names, checks a child's lexical name first, then
uses `lstat` and only descends into admitted directories. A denied node is never stat'ed,
opened, readlinked or enumerated, regardless of whether it is a file, directory, symlink or
FIFO. Discovering its name in its public parent's listing is necessary and not descent.
Nonprivate symlinks/special files fail closed. Launcher path, manifest, resource, version,
HTTPS, size/count, feed, SHA-256 and publication validations are not disabled or bypassed.
The release collector now also refuses nonprivate links/special nodes instead of following
file links. Packaging inputs must be quiescent; this is not a concurrent-mutation sandbox.

Only the Python privacy helper/collectors/checks and focused qualification documentation are
changed. No C++ USB/backend/service/UI policy, license/notices, version, actual release input,
coordinator-root file, real user configuration or credentials are changed or read.

## Actual CLI fixture tests

`tools/check-network-privacy.py` now runs lexical checks **and**
`tools/check-export-privacy.py`; it no longer substitutes AST/source-filter presence for
packaging evidence. The focused driver copies unchanged collector/helper code into disposable
miniature repositories. Every payload is synthetic, including `eboot.bin`, font, manifest,
config and privacy canaries. It invokes both CLIs as subprocesses and opens their resulting
ZIPs to check exact membership, public bytes, checksums, feed identity and release permissions/
fixed timestamps. Clean fixture ZIPs must remain byte-identical to the baseline on each host.
Input/public bytes and copied source-code hashes must remain unchanged.

An independently configured child-process guard (not the helper under test) records and
refuses access to exact synthetic private nodes/descendants. Python audit hooks cover open,
scandir/listdir; wrappers cover stat/lstat/readlink. Each guarded CLI must show its active
guard record and **zero attempted denied operations**, not merely missing ZIP members.
Directory canaries contain unreadable children and, on Linux, a FIFO and external-canary link.
Direct private symlinks, dangling links and FIFOs are also exercised, including dot/space and
backslash aliases. Baseline directory probes prove the guard detects pre-deny access even
when Python's glob/stat code swallows PermissionError.

Public link/FIFO refusal and existing launcher validation refusal cases cover manifest ID/
schema/version/contract/API, HTTPS feed/asset URL, missing/private entry/font, output inside
input, notes/minimum-version, path length/depth/backslash/colon/control characters, per-file/
aggregate size, file count, previous-feed identity and symbolic pack root. Large-file cases
are sparse metadata fixtures, not large content reads. A backslash-bearing public ancestor
is still rejected by the launcher before private-child access; a directly matching private
backslash name is pruned. Neither behavior relaxes existing resource-path checks.

The optional miniature-repository `build/` symbol inputs are **absent**. No ELF is compiled or
fabricated to bypass identity checks. Consequently this qualification checks ZIP/feed/source
byte identity but does not exercise optional ELF/symbol build-identity manifest generation,
console loading, runtime parsers, real USB storage or concurrent filesystem mutation.

## Reproduction and retained evidence

Branch `team/export-privacy-qualification-1006000` starts exactly at
`ac4a0b18659a931056c50eb9744cbb50d2c7ccf7` (including network privacy `6d47104`). Baseline
collectors are extracted unchanged from that commit. All work/evidence remains in the assigned
exclusive `amiibo-sync` worktree; nothing is packaged from the coordinator checkout.

```powershell
git archive --format=tar -o .local/export-privacy-20261027/input/baseline.tar ac4a0b1 tools/network_privacy.py tools/build_identity.py tools/ci/make-dist.py tools/launcher/package-update.py
tar -xf .local/export-privacy-20261027/input/baseline.tar -C .local/export-privacy-20261027/baseline
python -B tools/check-network-privacy.py --baseline .local/export-privacy-20261027/baseline --output .local/export-privacy-20261027/windows-qualified
```

The Linux run uses only existing `prosperoeden-host:features` image
`sha256:e6137ec556fe8e63ebf25e7fabe36e573d089c469d0e9d391c3072f2ae04d551`, 2 CPUs / 2 GiB,
no network, no capabilities, and private volume `prosperoeden-team-amiibo-sync-20261027`.
The small input tar contains just the four collector/helper files plus the two Python checks;
the baseline tar contains the four unchanged baseline files. The retained `input/run.sh`
extracts each into a fresh private directory and runs:

```sh
python3 -B "$run/source/tools/check-network-privacy.py" --baseline "$run/baseline" --output "$run/results"
```

The full Docker command, snapshot/report hashes and final observations accompany the results
below. No host installation, extra agent, native/full build, public network request, actual
release packaging/version bump or publication is involved. Linux-only physical alias, link
and FIFO cases are explicitly reported as skipped on Windows, not passed there.

```powershell
tar -cf .local/export-privacy-20261027/input/source.tar tools/network_privacy.py tools/check-network-privacy.py tools/check-export-privacy.py tools/build_identity.py tools/ci/make-dist.py tools/launcher/package-update.py
$evidence = 'D:/leonardommello/PS5/ProsperoEden/.local/agent-worktrees/amiibo-sync/.local/export-privacy-20261027'
docker run --rm --name prosperoeden-team-export-privacy --cpus=2 --memory=2g --pids-limit=128 `
  --network=none --cap-drop=ALL `
  --mount "type=bind,source=$evidence/input,target=/input,readonly" `
  --mount "type=bind,source=$evidence/linux-final,target=/export" `
  --mount type=volume,source=prosperoeden-team-amiibo-sync-20261027,target=/work `
  --entrypoint bash prosperoeden-host:features /input/run.sh
```

## Results

**PASS**: Windows Python 3.14.4 ran **35 actual CLI invocations**; Docker Linux Python 3.12.3
ran **121** (87 fixed-source invocations and 34 baseline controls). These counts include
expected refusals and intentional baseline leak demonstrations, not 121 accepted packages.
The fixed runs reported **zero attempted guarded private-node operations**. Both hosts kept
the clean baseline/fixed ZIPs byte-identical; generated release SHA256SUMS and launcher
feed SHA-256/size/identity matched the actual ZIP bytes. Public bytes and collector source
hashes were unchanged. All **635** indexed Linux export artifact hashes were rechecked after
copying out of the container; reported test/collector hashes match the committed file content.

Baseline leaks were reproduced in actual ZIPs, with only the synthetic canary payload:

| Baseline collector | Leaking fixture cases | Examples of leaked members |
| --- | --- | --- |
| Release | 8 | `PPSA99008/config./network-shares.json`, `PPSA99008/config /network-shares.json.new`, `PPSA99008/config/nextendo/oauth/refresh.json` |
| Launcher | 6 | `privacy-fixture/nested/CONFIG. /NeTwOrK-ShArEs.JsOn.pending`, `privacy-fixture/config/nextendo/account.json`, `privacy-fixture/config. /NEXTENDO. /oauth/refresh.json` |

Eight separate baseline hostile-directory controls recorded attempted `os.scandir` and/or
`os.stat` on private nodes. Those guard probes refused access before canary contents could
be read. Fixed hostile directory/link/dangling-link/FIFO fixtures, including alias forms,
produced public-only ZIPs without even attempted protected metadata access. Canonical,
mixed-case and broad-prefix staged siblings remained excluded too.

The first Linux attempt stopped on an incorrect fixture expectation: a public ancestor
containing a backslash is already invalid in a launcher resource path. The test was corrected
to require that existing safe refusal, with no private-child access or ZIP publication, rather
than weakening the validator. That failed report/source snapshot remains under `linux/` and
`input/source-path-validation.tar`; it is not counted as final qualification.

Retained evidence root: `.local/export-privacy-20261027/` (ignored, assigned worktree only):

- `input/`: exact source/baseline snapshots and local-only Docker runner/export helper.
- `linux-final/`: final report, per-invocation commands/stdout/stderr/access-guard logs,
  generated synthetic ZIPs/feeds/checksums and `export-sha256.json`.
- `windows-qualified/`: final Windows report plus synthetic fixtures/archives/command logs.
- `qualification.json`: source/snapshot/report hashes and platform/count summary.
- Private volume `/work/export-privacy.hkuOMq/`: full Linux source and synthetic fixture trees.

| Exact retained artifact | SHA-256 |
| --- | --- |
| Baseline source tar (`ac4a0b1`) | `5006ca8d5549347e39e0b0b52e7c77e191e3a3b0fee94c4a983b22577a07576a` |
| Tested source tar | `a2aad58d3f78ae6f42e602c33ce8b4fda728182f2ec92a1d78287e59c58fc585` |
| Final Linux report | `15ec5e0df5cdc0ea1204905cc697fc083b4c67de3353ba71dd85ba46b16e1973` |
| Final Windows report | `7f7c6fb657dc56d4120fa2d8fe47af09f49f4e4b68f5ed7f9bb7e012febfb3ff` |
| Linux export index | `c7836e483a5052b53e7ebb752f516ccc3dfca7f5d0f84c519f99a873ea09199b` |
| Clean Linux release fixture ZIP (baseline = fixed) | `fc3c5c07e2b5ef61530a6310b3d9fa4ba76fa532257a10b7bd55a30fd7b15c15` |
| Clean Linux launcher fixture ZIP (baseline = fixed) | `14b3e8f5075b97ce498e9e0189d7386d92cbb4fafc74ed6e0e623374b7077cec` |

These artifacts contain only synthetic fixture material; leaking baseline ZIPs deliberately
retain that synthetic evidence and are not release packages to publish. This is host packaging
qualification with Python-level access instrumentation, not kernel tracing, a race-hardening
claim, hardware qualification or real-credential testing.

## Coordinator import and bounded repeat

Imported as `f1a92c6`. The coordinator independently verified all 635 Linux export-artifact
hashes against the recorded index and preserved them under
`.local/docker-release/export-privacy-worker-20261027/`, including the intentional synthetic
baseline-leak controls. The integrated Windows fixed-source repeat also passed **19 actual
CLI invocations** (no baseline requested), report SHA-256
`1400cf42f34f89ff217ef645a4ad65ba79f90acd66bcd84b23d472d32a4746b8`, retained in
`.local/docker-release/export-privacy-integrated-windows-20261027/`.
The full host path now invokes this maintained CLI/ZIP suite. Subsequent license-package
integration remains separate and must preserve the private-node guard and normal validations.
None of these fixtures exercises optional symbol/ELF packaging or publishes a release.
