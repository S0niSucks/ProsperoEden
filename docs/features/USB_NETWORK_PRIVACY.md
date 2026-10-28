# USB network-credential staging privacy (2026-10-27)

Base: `62e99ba13563ac3d7f12947857c8e072c4afce74`.
Branch: `team/network-backup-privacy-1006000`, assigned exclusive USB worktree.
Only the USB backend, its filesystem test/runner and this feature-local note are changed.
No network feature commits were cherry-picked; no services, UI, OAuth, HTTP, locale,
aggregate documentation or network-owned helper edits are included.

## Policy and boundary

Reviewed immutable `6d47104dd812284a8e5f8ae1f0d7052bb5ba0920`:
`tools/network_privacy.py`, its two export-filter call sites, its focused policy check,
and `docs/NETWORK_SHARES.md`. Network configuration can contain a plaintext SMB password;
private file mode does not make a staged/editor copy safe to export.

For USB app-data paths, the protected second component is now either exactly
`network-shares.json` or that complete base followed by **`.` and any suffix**. Examples:
`config/network-shares.json.tmp`, `.json.bak`, `.json.staged.foo`. File nodes and the entire
subtree of any such directory node are excluded, including unknown/hidden/staging children.
Existing ASCII case folding and trailing-dot/space alias defense apply to both the `config`
component and the credential node. The root-relative scope is unchanged: this is not a
generic search for passwords or an exclusion of every staged JSON/config file.

The suffix boundary matters: `network-shares.json-safe`, `network-shares.jsonx`, harmless
children under `network-shares.json-safe-dir`, `network-shares-public.json` and unrelated
`config/unrelated.json.tmp` stay in backup and restore scope. A trailing dot alone is already
an exact-name alias and remains excluded. `config/nextendo/` and all its existing aliases
remain a whole-subtree exclusion, not a suffix rule.

`Scan` evaluates `Excluded` before opening a candidate, recursing into its directory or
adding it to the inventory used for byte/space estimates. Copy/progress/hash input therefore
does not contain these nodes. Enumerating the containing config directory to identify its
entry names is necessary; the protected directory's **contents** are never enumerated.
`Parse` applies the same policy to every restore manifest entry before payload inspection;
strict payload validation also rejects excluded files added outside the manifest. Dot segments,
backslashes and absolute/unsafe manifest paths are still rejected, not normalized into trust.

Completed older backups containing these staged credentials are now refused on restore.
This does not scrub a password already exported by an old build, encrypt backups, or revoke
credentials. No automatic deletion/mutation of an existing backup is attempted.

## Actual negative and positive evidence

All fixtures contain synthetic bearer/password equivalents only. Tests call the actual
`Inspect`, `Backup`, `InspectRestore`, `StageRestore` and cold-start `ApplyPending` filesystem
implementation; they do not invoke the private policy predicate as a substitute for behavior.

**Before the backend fix**, the new test produced a real, COMPLETE backup and verified that
six staged credential files in it contained the synthetic password body. It printed:

```text
USB privacy baseline: actual COMPLETE backup copied 6 synthetic credential files; inventory counted 13 files / 530 bytes
Assertion `!copied_credentials' failed.
```

That ASan/UBSan invocation failed (nonzero). The six files cover `.tmp`, `.bak`, `.staged.foo`, mixed
ASCII case and trailing-dot/space aliases. This proves a payload/estimate leak, not just a
missing string comparison in a unit test.

**After the fix**, the complete focused runner exits 0, with ASan/UBSan and leak detection:

- Control inventory stays at **7 harmless files / 236 bytes**. File count, byte count and
  required space stay identical after staged credential files and hostile directory nodes
  are added. Every reported copy/validation total/path is checked against harmless content.
- Actual output payload and manifest contain neither synthetic secret bodies nor their hashes.
  The guarded backup has exactly the same manifest as the clean backup.
- Independent fixture-path traps wrap actual `openat`, `fstatat` and `read` calls. A deliberate
  self-test proves all three traps fire and deny access; the counter is then reset. Backup,
  inspection and restore operations succeed/reject as intended with **zero forbidden probes**.
  The test runner adds only `--wrap=openat` to its existing I/O wrappers. Real calls go through
  the linker-resolved implementation; no raw-libc/syscall sanitizer bypass is used.
- Protected directory aliases contain unknown/hidden/staging files, an invalid component name,
  a FIFO and a broken symlink. A staged-sibling symlink is also excluded without opening it.
  No failure is disguised as a successful partial backup.
- Live credentials remain untouched after a harmless restore and do not enter recovery data.
  Both payload-only injection and self-consistent manifest/COMPLETE reinjection are rejected.
  File/directory manifests include staged aliases and descendant paths; a trap on the entire
  payload proves malformed manifests fail **before any payload open**. No pending restore is
  published. Existing Nextendo whole-tree/alias/injection tests remain in the same passing run.
- Existing corruption, hostile-file, cancellation, low-space and recovery/failure-boundary tests,
  synthetic service lifecycle and actual update-check drain checks also pass, plus the runner's
  native-directory/service-declaration/UI syntax checks. These are not new service integration
  or renderer/console qualification claims.

Runner: `tools/docker/run-usb-backup-tests.sh`, image `prosperoeden-host:features`, private
container/volume `prosperoeden-team-network-backup-privacy-20261027`. Limits: 2 CPUs, 2 GiB,
128 PIDs, network disabled. Assigned worktree mounted read-only at `/checkout`; outputs in
private `/work`; separate 128-MiB tmpfs mounts at `/usb` and `/mnt/usb0`.

Logs retained in that private volume:

- `/work/baseline-staged-network-privacy.log`: pre-fix leak and failing assertion.
- `/work/usb-network-privacy.log`: complete passing runner.
- `/work/immutable-network-rule-observations.json`: helper observations below.

No full build, WSL installation, real credentials, console run or additional agent was used.
PS5 filesystem primitives, USB removal, power-loss behavior and composed writer/service locks
remain coordinator/hardware qualification work.

## Coordinator handoff: other collector observations (not changed here)

The immutable Python helper was evaluated directly from `git show`, without importing a
network worktree or running a package builder. Results:

| Input | `private_network_path` at `6d47104` |
| --- | --- |
| `config/network-shares.json.tmp` | `true` |
| `CONFIG./NETWORK-SHARES.JSON.TMP` | **`false`** |
| `config/network-shares.json-safe` | **`true`** |

Its `parts[n] == 'config'` comparison lacks the USB policy's trailing-dot/space alias defense;
its unrestricted `startswith('network-shares.json')` also excludes non-dot suffixes. Thus the
new `make-dist.py` / `package-update.py` predicates do not filter the shown `CONFIG.` alias.
This is a confirmed rule mismatch and conditional export privacy gap, **not** a claim that a
real release/archive contained credentials. No archive was generated and those helper-owned
collectors were not modified. They also use `rglob` before the per-entry filter rather than
pruning credential directories up front; this review does not claim their traversal has the
USB backend's stronger no-open/no-enumeration guarantee. Python's arbitrary-depth component
search supports package path prefixes; USB keeps its existing data-root-relative scope.

The coordinator should reconcile helper boundaries/aliases with its owner and update the
incumbent `docs/USB_BACKUP.md` exact-only/harmless-`.bak` wording when integrating. That
document is intentionally untouched here. Other unknown sensitive config files are not
heuristically excluded; backups still need private storage as documented.

Coordinator imported this fix as `99f257c` and retrieved/hash-recorded the raw worker negative,
positive and immutable-helper-observation logs. The full runner then passed independently on
the clean frozen 841-file `99f257c` snapshot (`sh_17ffacb8e001wDaLmpbECNeXO6`): staged-sibling
copy/estimate/progress/hash exclusions, zero forbidden probes, seven harmless control files /
236 bytes, restore rejection, synthetic service lifecycle, actual update-check drain and syntax.
Snapshot, raw logs and exit-0 receipt are retained in
`.local/docker-release/usb-staged-privacy-integrated-20261027/`. This does not qualify the
separate Python collectors, later writer-admission changes or PS5 filesystem behavior.
