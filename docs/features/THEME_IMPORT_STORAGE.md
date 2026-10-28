# App-owned theme storage callback

Base: `fdc4a2d1836bf447ee5cba78909fa8d9589caad2`. This delivery implements the
ninth `RuntimeLauncher` argument, not the shared admission integration or a native app build.

## Implemented contract

- `Services::reserve_theme_import_storage(root, bytes)` defaults to **denial**. The production
  frontend always passes a nonempty lambda calling that virtual method. Neither missing
  implementation nor production denial selects the optional local fallback in `nx_storage.hpp`.
- `EdenServices` implements it in `theme_storage_services.inc`, included once at file scope.
  The accepted root is exactly `Eden::kDataDir`, including no trailing slash/alias/NUL suffix;
  theme paths/JSON never choose a storage domain. The root must exist and open as a directory
  without following a final symlink. No directories are created by the callback.
- Requests are bounded to **1–385 MiB**, covering the unchanged installer's
  `2 * total + largest + 1 MiB`, with total converted bytes bounded to 128 MiB. A new feature-local
  **process** ledger is shared across `EdenServices` instances. Total live credits are bounded
  to 385 MiB and each acquisition also leaves **64 MiB headroom** after known free space and
  already-outstanding credits. Subtraction-based tests avoid integer overflow.
- Acquisition takes the existing real `job_admission_mutex_`, then the feature ledger lock.
  It refuses unavailable filesystem access, active USB, launch commitment, network mutation,
  Nextendo, busy cheat/Amiibo workers, any unjoined save-sync/pairing worker, active/staged
  self-update, queued/active remote downloads and refreshing remote sources. Status snapshots
  are read-only; the callback does not join, cancel, wait for workers, access bridge, contact
  HTTP, translate, or call UI/GL. Small unrelated writes/FTP/other processes are not fenced.
- A successful `shared_ptr<void>` owns real byte credit, not a dummy success sentinel. Copies
  share one credit. Its final destruction releases only that credit through the ledger mutex;
  it captures no `this`, UI or admission/bridge reference. An allocation/status exception cannot
  strand credit. The token can safely outlive the service, while the **callback** still requires
  the service to outlive the launcher and joined workers (as the current frontend does).
- The unchanged `nx::install` shares one token through staging, resource publication, journal
  removal and the final directory sync. Cold recovery obtains a new live token from the same
  callback before resumed writes; no journal serializes a token. Denied/throwing callbacks do
  not fall back locally. Failure-safe installer behavior and prior packs are preserved.
  Releasing credit does **not** delete or subtract staged/published disk usage; each later
  request probes actual capacity again.

This is **capacity preflight + process reservation ledger + failure-safe installation**, not
a hard OS/filesystem reservation, a pre-existing global app budget, or cross-process admission.

### Known-space limit on the console

On host POSIX, the pinned root descriptor is queried using `fstatvfs`. Unknown capacity,
read-only storage, invalid blocks, multiplication overflow and insufficient headroom deny.
On `__PROSPERO__`, capacity remains **unknown and is denied**, with no `statfs`/raw syscall or
invented free-space value: `remote.cpp::FreeSpace` already records the unsafe/unavailable title
import problem, and the scene handoff does not qualify console `statvfs` or hardlinks.
The caller is wired, but console import/recovery writes must stay closed until a separately
qualified safe capacity source is supplied. Host fixtures do not prove console capacity,
hardlink support or native runtime acceptance. This task installs no toolchain or workaround.

## Required parent composition — not applied here

[`THEME_STORAGE_PARENT_GUARDS.patch`](THEME_STORAGE_PARENT_GUARDS.patch) is the **exact minimal
additive handoff against this base**. Apply/merge it only after the network owner's shared
composition. It inserts `theme_storage_->busy()` under the existing real admission mutex in:

- `can_launch_game`, `will_play`;
- `set_games_folder`, `set_files_folder`;
- `choose_profile`, `add_profile`, `rename_profile`, `remove_profile`;
- `begin_usb_backup`, **before** `usb_active_.exchange(true)` or any join/write;
- new `nextendo_action_with_consent` requests, before profile/bridge reads or busy publication;
- new `WithNetworkMutation` requests, before active publication, under the same admission mutex.

The baseline network seam lacks that common admission block. Its handoff adds a short block
inside the existing network mutation serialization, releases admission before `operation()`
and preserves the existing finish guard. If the network owner has already introduced this
block, add **only** the theme predicate inside it; preserve their USB/Nextendo/launch checks,
lock ordering and active-flag lifetime. Never wrap network HTTP/editor work in admission.
The Nextendo hunk reuses an already-marked safe generic error; no new catalog text is required.

These hunks are **not production changes in this branch**. Before parent composition, the
callback refuses already-active USB/network/Nextendo, but those unmodified shared entrypoints
do not yet reciprocally refuse a theme token. Do not claim the shared exclusion gate is closed.
The focused USB-after-token test explicitly composes just that parent hunk into a private fixture.

For the same conservative exclusion of other large writers, the parent should also insert
`if (theme_storage_->busy()) return false;` immediately after its existing admission lock in
`start_update`, `apply_update`, `check_cheat_database`, `install_cheat_database`, `start_amiibo_sync`,
`download`, and `start_pairing`; use `return;` in `refresh_sources` and `start_save_sync`.
Do not block cancellation/finish/release operations behind theme credit. These are admission
additions only, not permission to change any worker body, call Stop/Join, or hold bridge in I/O.

## Focused offline proof

Run from a read-only checkout, with a fresh private output directory and disposable `/data`:

```powershell
docker run --rm --cpus=2 --memory=3g --network=none --security-opt seccomp=unconfined `
  --tmpfs /data:rw,size=512m `
  --mount 'type=bind,src=<theme-storage-worktree>,dst=/repo,readonly' `
  --mount 'type=volume,src=prosperoeden-theme-storage-20261028,dst=/private' `
  --env CXX=clang++-18 --env PYTHONDONTWRITEBYTECODE=1 `
  --entrypoint python3 prosperoeden-host:features -u /repo/tools/check-theme-storage.py `
  --out /private/NEW-check
```

The checker compiles the actual service `.inc`, its verbatim theme member/declaration, the
real `Services` default and unchanged scene installer/recovery headers. Only other worker
statuses and capacity/error injection are synthetic. It checks normal, ASan/UBSan and TSan:
root/access/bounds; unknown/low/overflow/read-only space; final-symlink/missing root; cross-instance
concurrent credits; copied tokens, owner destruction and exceptions; active-worker denial;
USB before/after (the latter is the explicitly labeled parent-hunk fixture); default denial
without fallback; one token through actual installation; last-write/sync lifetime; interrupted
publication; denied then successful cold recovery; and existing pack preservation.

The exact changed frontend constructor expression is extracted into a host syntax TU using
the **real** `EdenServices` and `RuntimeLauncher` headers. No native headers are stubbed and no
SDK/cache writes are needed. This checks the ninth-argument C++ contract, **not** the full
platform `frontend.cpp`, SDK linking, GL, IME or app build. The native unknown-capacity branch
is separately exercised as a host conditional-compile test, not described as target execution.
The receipt records commands, outcomes and immutable source hashes. No sanitizer checks are
disabled; `seccomp=unconfined` is needed only for Docker TSan address-layout initialization.
Existing compact scene headers trigger two Clang misleading-indentation warnings; these remain
visible but nonfatal in the installer test TU only, without editing those library-owned headers.

Final offline qualification: **PASS**, 2026-10-28, private `/private/check-04` in volume
`prosperoeden-theme-storage-20261028`; normal + ASan/UBSan + TSan, host caller syntax and
conditional native-unknown denial. Test input hashes were rechecked after all runs. Image:
`sha256:e6137ec556fe8e63ebf25e7fabe36e573d089c469d0e9d391c3072f2ae04d551`.
The exact parent patch also passed `git apply --check` without changing production guards.

No scene/library/runtime/controller/rendering code, shared guard body, USB/network/Nextendo implementation,
catalog/POT/version/CMake/buildscript or parent checkout is edited by this delivery.
