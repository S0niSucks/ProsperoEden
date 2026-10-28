# Built-in network-location editor (#111)

This continues `6d47104` / `20ea7c6`; it does not replace or reset those commits.
Settings → **Network shares**, immediately after **ROMs and updates**, opens the native editor.
The existing JSON import remains supported. No runtime/Lua editor or credential API is added.

## User flow

- Add up to four locations, or select a saved location to edit it. IDs are generated for new
  locations and preserved on edit, keeping credential-free `pe-net://` history identities stable.
- Set name, SMB/NFS protocol, numeric IPv4, share/absolute NFS export, relative games folder,
  optional relative updates/DLC directory, enabled state, port, timeout and cache allowance.
- SMB adds user, masked password replacement, domain and required SMB3 encryption. An empty user
  explicitly selects SMB2 guest and requires empty password/domain and encryption off.
- Password entry always starts empty. Cancel retains the draft password. **Square / Clear** removes
  it; **Triangle / Keep saved** restores the originally saved password (or no password for a new
  location). Neither the view nor a UI future contains password text.
- NFS explicitly exposes UID, GID and mountd port (0 = server portmapper); NFSv3 AUTH_SYS/TCP is
  fixed, not a misleading editable set of unsupported NFS flags. SMB credential/encryption fields
  are absent. Protocol changes clear share/SMB credentials **in the draft only**.
- **Save (no connection)** writes settings without probing a server. **Save and reconnect** is an
  explicit opt-in to reload/connect enabled locations. Reconnect and Disconnect are also list rows.
  No endpoint is contacted for a keystroke, draft change or keyboard cancellation.
- Back/Cancel on a dirty draft asks before discarding. Square on a saved location asks before
  removing its configuration; no NAS files, local saves, mods or caches are deleted.
- L1/R1 pages long forms. Fields have contextual help and bounds. IME unavailable/cancelled and
  rejected values keep the previous draft value; no parser/keyboard exception excerpt is surfaced.

The system IME caps native entry at 1023 UTF-16 units; the feature also enforces its UTF-8 byte
limits (256 for credentials/names, 15 for IPv4, ten for numeric fields). Imported relative paths
may retain the transport's 2048-byte allowance. Opening a longer-than-native initial value refuses
entry rather than silently truncating it; such paths can still be changed through JSON import.

## Private persistence

`headless/network/config.cpp` is shared by import and the editor. It rejects unknown fields,
duplicate JSON keys/IDs/locations, wrong scalar types, unsupported credentials/options, unsafe
paths, >4 shares and >64 KiB input. An invalid import is never treated as an empty configuration
for saving; it must be fixed externally and reopened. Concurrent changes produce a conflict,
not a full-file reset. Imported configs are tightened to `0600` on safe regular-file reads.

Every ancestor is walked through directory descriptors with `O_DIRECTORY | O_NOFOLLOW`;
the leaf is opened nonblocking/no-follow, must be regular and have one link. A save exclusively
creates its own `0700` staging directory named `config/network-shares.json.tmp-<pid>-<sequence>-<tick>`
and its own `0600` file inside. It never reuses/truncates someone else's staging file. It writes,
fsyncs, checks the expected bytes/inode and parent identity, then atomically renames through held
descriptors. Cancellation/failure before rename leaves the old file unchanged and removes only
the writer's own stage. Rename is the commit point; a subsequent directory-fsync problem is a
**saved / durability warning**, not a false claim that no change occurred.

This is private **plaintext**, not encryption or secure-memory erasure. A same-account attacker or
administrator can inspect credentials; abandoned crash stages must remain private/excluded. As
with the network directory policy, this is not an atomic sandbox against a hostile same-identity
process repeatedly renaming ancestors at the final syscall boundary. It does not follow symlinks
to write their targets. Require trusted, stable application configuration directories.

`tools/network_privacy.py::private_network_path` excludes the config **and every staged descendant**
case-insensitively. Distribution and launcher-pack collectors already use it. The USB/shared-backup
collector is not present on this exclusive branch: its owner must apply the same predicate before
traversal/copy (including private staging directories), never a filename-only exact match. The
parent reports its root shared-backup rule protects `config`; this branch cannot verify that code.

## Exact parent admission composition contract

`EdenServices::WithNetworkMutation(const std::function<EditorResult()>&)` is the single admission
seam for editor Open/Begin/Change/Password/Enter/Save/Remove and reconnect/disconnect. Cancel only
discards a private draft. The branch baseline has no USB admission subsystem; its local mutex
serializes network mutations against `will_play`, checks filesystem access / `launch_committed_`,
and its active flag makes `can_launch_game` refuse while a private keyboard/save is running.

**Required when the parent composes the USB worker:** replace this seam's local admission with
the parent's actual `job_admission_mutex_`, checking `usb_active_` and `launch_committed_` under
that same mutex before accepting an operation. Keep a network-job reservation through the entire
operation, including IME future lifetime and Save's final Configure/Disconnect, and release on all
exception/cancel paths. USB-writer and launch admission must check that reservation under the same
mutex (or hold their existing exclusive admission for its lifetime). Do not leave a check-then-run
window, do not hold a UI-thread-blocking mutex while waiting for an IME, and do not merely check
`usb_active_` once before scheduling a future. The parent should adapt this **defined seam**, not
invent calls to methods absent from either branch. No other worker's guard implementation is
cherry-picked or edited here. Replace the local `will_play` critical section at the same time;
do not retain an opposite nested local/shared-mutex order. Console acceptance remains conditional
on this parent composition.

## Focused validation

Run in the already-approved capped Docker/private volume, never in the coordinator build tree:

```sh
python3 -B /src/tools/check-network-editor.py
python3 -B /src/tools/check-network-frontend.py
python3 -B /src/tools/check-network-privacy.py
PS5_PAYLOAD_SDK=/work/sdk/ps5-payload-sdk python3 -B /src/tools/check-network-integration.py
```

`editor_check.cpp` tests the real private native-keyboard adapter with synthetic IME answers,
strict validation, keep/replace/clear, four locations, duplicate refusal, `0600` atomic writes,
each prepublication cancellation checkpoint, concurrent import, old-file preservation and
FIFO/hardlink/symlink/ancestor refusal. `editor_ui_check.cpp` drives an actual Settings/Launcher
against that same editor with a synthetic service admission/server counter and produces native
draw-list/Mesa previews. It does not instantiate the full `EdenServices` or execute the PS5 IME.
Preview fallback system fonts/kit themes are disabled; the real bundled Latin font/default theme,
layout and shaders render the synthetic screens. No synthetic password is permitted in fit-report
text, UI views or screenshots. The native service glue separately receives native-gated syntax
checks. SDK ELF linkage is not execution. No emulator/full native rebuild is involved.

### Acceptance evidence (2026-10-27)

The incremental backend commit is `ad79973`. It remains on the exclusive branch atop the two
accepted network commits; no reset, cherry-pick, push, release or version change was performed.

The sequential acceptance pass completed successfully for the editor, native-gated frontend,
privacy/exports, real SMB fixtures/security, real NFS-Ganesha MEM FSAL and saved-archive real
loader/provider/Launcher tests. The private editor also passed ASan/UBSan with halt-on-error;
pure editor/config tests retained leak detection. The Mesa controller/preview executable disables
driver leak reporting only, not invalid-access/undefined-behavior checks. No full emulator or
native-app target was built. Existing saved core/metadata archives remain uninstrumented.

The SDK cross-build compiled the clients, editor, atomic writer and native keyboard adapter, then
linked the actual PS5 `AskSystemKeyboard` object with SDK IME/UserService/Sysmodule imports and the
existing project CommonDialog facade into `/work/network-check/integration/sdk-smoke.elf`.
That ELF was **not executed**. It verifies symbol/type/link compatibility, not the PS5 filesystem,
IME behavior or console firmware interoperability.

Private evidence stays on `prosperoeden-team-network-vfs-20261027`'s writable volume, in the
continuation container `prosperoeden-team-network-vfs-loader-20261027` (2 CPUs / 3 GiB). Source and
coordinator mounts stayed read-only. The logs contain only synthetic fixture results:

- `/work/check-network-editor-final.log`
- `/work/check-network-editor-sanitized-final.log`
- `/work/check-network-editor-sdk-final.log`
- `/work/check-network-editor-frontend-final.log`
- `/work/check-network-editor-privacy-final.log`
- `/work/check-network-editor-shares-final.log`
- `/work/check-network-editor-security-final.log`
- `/work/check-network-editor-nfs-final.log`
- `/work/check-network-editor-loader-final.log`

Nine synthetic 1920×1080 Mesa previews are in `/work/network-check/editor/previews/`, also copied
to this worktree's ignored `.local/network-editor-evidence/` (not committed or exported):

1. `00-settings-network-entry.png` — Settings placement beside ROMs and updates.
2. `01-add-smb.png` — configured SMB server/share/folders.
3. `02-private-password-state.png` — password presence only; replacement text is not drawn.
4. `03-discard-confirm.png` — dirty-draft discard confirmation.
5. `04-nfs-options.png` — explicit AUTH_SYS UID/GID/mountd settings.
6. `05-save-choice.png` — save without connecting; adjacent reconnect action is reachable below.
7. `06-delete-confirm.png` — named location removal confirmation.
8. `07-admission-blocked.png` — synthetic protected-job refusal, no write/connect.
9. `08-invalid-import.png` — invalid JSON preservation with larger text enabled.

The visual inspection was bounded: one batched inspection and one confirmation of the layout fix.
Controller assertions additionally prevent old save notices carrying into discard/remove prompts.
There is no image of an actual PS5 system keyboard and no real password or NAS address was used.
`NETWORK_EDITOR_MSGIDS.txt` is the feature-local translation handoff; its coverage and the absence
of runtime/Lua editor bindings are checked by `check-network-privacy.py`.

**Still unqualified:** the parent-composed USB/launch admission described above, the absent
USB/shared-backup collector, PS5 IME execution, a full app/render/guest launch and real disk-backed
NAS interoperability. The earlier Ganesha disk-FSAL capability limitation remains unchanged.
This host workflow completes the missing built-in Add/Edit/Remove implementation; it is not a
console-acceptance claim or a claim that parent-owned integration has already happened.
