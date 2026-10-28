# Read-only network games (issue #111)

ProsperoEden can discover NSP, XCI and NRO files on explicitly configured SMB/NFS
shares and give Eden random-access reads without downloading a complete game.
Local ROM paths and the existing HTTP/download-source system are unchanged.

**Status:** host implementation includes Library metadata, credential-free history,
pre-launch title settings and read-only external updates/DLC. Focused real-loader,
controller/lifetime, protocol and SDK checks are described below. No full emulator/
native-app build, console run, actual game launch or PS5 performance result is claimed.

## Configuration and Settings

Open built-in **Settings > Network shares**, beside **ROMs and updates**, to add,
edit or remove up to four locations. Enter server/share/folder and optional add-on
roots, SMB sign-in/encryption or NFS UID/GID/ports with the native private-keyboard
path. Password text never enters a UI future, runtime/Lua keyboard status or render
view. Save persists without connecting; **Save and reconnect** explicitly opts in
to network requests. Discard and removal require confirmation. See
[NETWORK_EDITOR.md](NETWORK_EDITOR.md) for the complete flow, persistence safety,
parent USB/launch admission contract and focused host/SDK evidence boundaries.

Private JSON import at `/data/prosperoeden/config/network-shares.json` (mode `0600`)
continues to work. The editor never overwrites an invalid import with an empty
configuration. The runtime-launcher's generic keyboard bridge is not reused or
given any network credential action/query binding. PS5 IME execution still needs
console qualification; a host preview is not that evidence.

Example using documentation-only addresses and placeholder credentials:

```json
{
  "shares": [
    {
      "id": "nas-smb",
      "name": "SMB games",
      "protocol": "smb",
      "server": "192.0.2.10",
      "share": "games",
       "folder": "Switch",
       "updates_dir": "addons",
      "user": "read-only-user",
      "password": "<private read-only password>",
      "domain": "",
      "encrypt": true,
      "timeout_ms": 5000,
      "cache_mib": 16,
      "enabled": true
    },
    {
      "id": "nas-nfs",
      "name": "NFS games",
      "protocol": "nfs",
      "server": "192.0.2.11",
      "share": "/exports/games",
      "folder": "Switch",
      "port": 2049,
      "mount_port": 20048,
      "uid": 65534,
      "gid": 65534,
      "timeout_ms": 5000,
      "cache_mib": 16
    }
  ]
}
```

- At most four shares in a regular, non-symlink configuration file of at most
  64 KiB. Missing configuration means no shares. The reader sets `0600` before
  reading; an inaccessible/non-private file fails closed. Create it privately
  initially: changing its mode cannot undo earlier exposure.
- `id` is unique, stable and at most 40 bytes; `name` defaults to it. Numeric IPv4
  only: no DNS/mDNS, IPv6, URLs, broadcasts, LAN scans or share enumeration.
  Only configured servers are contacted, automatically when the launcher opens or
  on explicit reconnect; draft edits and Save without reconnect do not probe servers.
- `folder` is relative to the share/export; empty means its root. Absolute paths,
  `.`/`..`, empty components, backslashes, control bytes, URI escaping and unsafe
  delimiters are rejected. Relative paths are at most 2048 bytes. SMB `share` is
  one component; NFS `share` is an absolute export path.
- Optional `updates_dir` is relative to `folder` (e.g. `folder: "Switch"` and
  `updates_dir: "addons"` reads `Switch/addons`). Empty/absent disables separate
  network add-ons. Only that root and one subfolder level are exposed read-only to
  Eden's actual `ExternalContentProvider`; the tree is excluded from boot-game discovery.
  Other directories and deeper descendants are not exposed by `OpenDirectory`.
- `port` defaults to 445 (SMB) or 2049 (NFS). NFS `mount_port: 0` uses the server's
  portmapper; set it explicitly for a fixed-port/firewalled service.
- `timeout_ms`: 250–15000, default 5000, per operation/entire VFS read, not per
  packet. `cache_mib`: 1–32, default 16, **per share session**. `enabled` defaults
  to true. `uid`/`gid` default to 65534 and must be nonnegative integers.

### Authentication, privacy and server policy

SMB uses SMB2/3 and NTLMSSP. A nonempty `user` requires signing; `encrypt: true`
requests SMB3 encryption. Authentication failure does not retry with another
account, guest credentials or SMB1. Empty `user` explicitly selects unsigned
SMB2.x guest access and requires empty password/domain and no encryption. This
SMB2.x restriction avoids libsmb2 6.3's SMB3.1.1 guest TREE_CONNECT signing issue.
The explicitly configured password/domain override ambient credentials; connect
does not reload `NTLM_USER_FILE` after that override.

NFS is **NFSv3 AUTH_SYS over TCP, plaintext**. It uses the explicit UID/GID, not
passwords, Kerberos or TLS. NFS username/password/domain or `encrypt: true` are
rejected, never silently ignored. Use only on a trusted LAN with narrowly scoped,
read-only exports and appropriate server-side access controls.

Credentials remain plaintext in the private JSON and process memory; this is not
an encrypted credential vault or secure-memory implementation. The UI, errors
and library identifiers never include passwords or credential URLs. JSON/library
error strings are not forwarded. Do not put credentials in `id`, `name` or file
names, and do not include the private JSON in diagnostics or source control.
`tools/network_privacy.py` supplies a case-insensitive config/staged-sibling deny
rule for shared diagnostics/backups/exports. Distribution and launcher-pack collectors use it,
and Git ignores `**/config/network-shares.json*`. Crash diagnostics in this checkout
record selected lifecycle fields, not configuration-file contents. A separate shared
backup/diagnostics collector must apply this rule too; that collector is not present
in this worktree and its coordinator-owned integration must not be assumed complete.

Configure the server share/export read-only too. Client operations never request
writes, truncation, renames or creation. Discovery skips reported links/special
files and access validates path prefixes; NFS uses lstat and O_NOFOLLOW. These
checks are not an atomic sandbox against a hostile NAS or concurrently swapped
path prefixes. Use stable, link-free game folders and enforce containment on the
server. Do not modify game files while a session is active.

## Discovery, launch and lifetime

- Discovery reads the configured root plus one game-folder level, not a recursive
  NAS crawl. Lists exceeding 4096 accepted entries fail explicitly; at most 4096
  games per share are published. The protocol clients materialize directory
  listings internally: the entry limit is not a hard cap on all client-buffer
  allocation. Use small dedicated folders, not entire NAS roots.
- Solid NSZ/XCZ, split/deconstructed games and remote sibling traversal are not
  supported: they could require full unpacking or different directory semantics.
  Loader directory probes receive an isolated, empty read-only directory instead
  of a null pointer, so invalid container bytes are not a null-directory crash.
- Games appear beside local games, labelled **Network** and the share name. The
  asynchronous Library pass uses the existing real metadata bridge through network
  VFS for container title IDs/control data and NRO ASET/NACP/icons. Covers/names are
  cached locally using the complete credential-free identity; missing/invalid data
  falls back to the saved name/cover and then file stem, never a guessed title ID.
  Once known, title-specific settings are available before launch. Home consumes
  cached Library metadata without synchronous network metadata reads.
- Metadata scopes allow at most 32 MiB of transport payload per game/add-on probe,
  4 MiB per direct VFS byte-vector request, and five seconds between requests. The
  whole network Library pass also has a 128 MiB/five-second scope. An in-flight
  request retains its configured operation timeout. These are not hard limits on
  every allocation inside the inherited container parsers/protocol libraries.
  Metadata refusal uses a filename fallback; an incomplete launch-time add-on
  scan aborts launch rather than silently starting a different/base version.
- Credential-free `pe-net://<id>/<relative-path>` identifiers are intercepted only
  by `RealVfsFilesystem::OpenFile` in read mode, with narrowly scoped add-on
  `OpenDirectory` routing. Other paths retain local behavior.
  Saves, mods, user data and shader caches remain on the console.
- History/recents retain the full `pe-net` identity, validated separately from local
  paths. Library selection follows that identity through rescans/reordering. Offline
  shares do not turn into local paths; history remains stored for explicit reconnect.
- Add-on registration uses the selected share's configured root, not another
  share's or local `updates/` directory. The existing provider/CNMT/NCA title IDs
  and PatchManager select matching content and newest versions, not file labels.
  The launch-time language/add-on summary is refreshed for the selected share.
  Packed updates still follow the incumbent container/manual-provider path.
- Each serialized protocol session owns at most 16 handles and a 256-KiB block
  LRU. On-demand read-ahead is one block, not a whole-file copy. Cached payload
  bytes never exceed the configured bound; one scratch block and client/metadata
  buffers are additional. Handles/cache entries bind to file identity; size,
  inode and modification timestamps are checked when a handle is opened/reopened.
- VFS files retain shared session ownership after discovery/the launcher ends.
  Disconnect, cancel, timeout or transport failure poisons that session, including
  cached reads. Cancellation wakes polling within 25 ms (scheduling aside).
  Reconnect makes **new** sessions; old consumers remain failed. No automatic
  reopen/retry can switch an existing consumer to a different file.
- Failed reads return zero bytes, never a fabricated successful zero-filled
  buffer. Earlier valid bytes may already have been copied on a multi-block
  failure; callers must honor the return value. IStorage/IFile IPC overlays reject
  a latched game-session error before success reaches the guest. Loading/runtime
  monitoring returns to the launcher with a sanitized error. Reconnect there and
  start the game again; there is no transparent resume.

### Qualification boundaries

Automatic download-source save sync and the cheat-catalog installer remain disabled
for network games because their identities/folders assume local ROMs. Saves, mods
and caches remain local; network add-on folders are never mod/save/cache roots.
Synthetic update tests exercise real NCA/CNMT registration/version selection and
PatchManager ExeFS selection, not commercial BKTR delta patches, signatures, DLC
guest mounting, real games or firmware. The controller check has inert graphics and
unrelated console services; it is not visual/native-app/console acceptance.

## Dependencies and licensing

`tools/deps.json` pins the source archives; no system SMB/NFS fallback is used.
`headless/network/CMakeLists.txt` builds static client libraries only, disabling
examples, tests, utilities, shared libraries, Kerberos/GSSAPI, TLS and NFSv4.2.
NFSv4 code remains in libnfs's library but the application selects v3 only.

| Client | Pin | Archive SHA-256 | License |
| --- | --- | --- | --- |
| libsmb2 | `libsmb2-6.3` security release | `5011003fd091513f897e1ebbb8e1a40af634386df7199716348b7f77569a2790` | LGPL-2.1-or-later |
| libnfs | `libnfs-8.0.0` | `bc91216e927a85142b5de611c7f711558e02119a7daad67620ea631634038350` | LGPL-2.1-or-later client; BSD protocol files |

These are compatible with the GPL-licensed application; redistribution must
retain upstream notices and corresponding-source/relinking rights. This work
does not generate or release a package. libnfs's tag's CMake output says
`libnfs 18.0.0`; the source/archive pin is still `libnfs-8.0.0`.

## Focused acceptance evidence

Only `prosperoeden-host:features` was used, with private volume
`prosperoeden-team-network-vfs-20261027` and continuation container
`prosperoeden-team-network-vfs-loader-20261027` (2 CPUs, 3 GiB), this worktree
mounted read-only as `/src`, and private outputs under `/work`. The coordinator
volume `prosperoeden-integration-20261027` is mounted **read-only** as `/coordinator`;
only its saved host archives/compile/link recipes are consumed. No coordinator
build/write, full emulator/native rebuild, WSL install or LAN probing is performed.
Samba/Ganesha/synthetic RPC endpoints use loopback. Authentication uses a disposable
container-only account and generated password, never host/NAS accounts or secrets.
No commercial games/keys are used.

With the verified client source trees under `/work/deps` and `smbd` installed in
the disposable container:

```sh
docker exec prosperoeden-team-network-vfs-loader-20261027 python3 -B /src/tools/check-network-shares.py
docker exec -e NETWORK_SANITIZE=1 -e ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 -e UBSAN_OPTIONS=halt_on_error=1 prosperoeden-team-network-vfs-loader-20261027 python3 -B /src/tools/check-network-shares.py
docker exec -e PS5_PAYLOAD_SDK=/work/sdk/ps5-payload-sdk prosperoeden-team-network-vfs-loader-20261027 python3 -B /src/tools/check-network-integration.py
docker exec prosperoeden-team-network-vfs-loader-20261027 python3 -B /src/tools/check-network-loader.py
docker exec -e NETWORK_SANITIZE=1 -e ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 -e UBSAN_OPTIONS=halt_on_error=1 prosperoeden-team-network-vfs-loader-20261027 python3 -B /src/tools/check-network-loader.py
docker exec prosperoeden-team-network-vfs-loader-20261027 python3 -B /src/tools/check-network-security.py
docker exec prosperoeden-team-network-vfs-loader-20261027 python3 -B /src/tools/check-network-nfs-server.py
docker exec prosperoeden-team-network-vfs-loader-20261027 python3 -B /src/tools/check-network-privacy.py
docker exec prosperoeden-team-network-vfs-loader-20261027 python3 -B /src/tools/check-network-frontend.py
```

Results: real libsmb2 against loopback guest Samba and real libnfs against a
synthetic RFC1813 TCP/MOUNT service pass deterministic/random/concurrent reads,
5-GiB offsets, cross-block reads, EOF, cache hits/bounds, cancellation, stalled
timeouts, denied access, discovery, private config mode/symlink/FIFO/size rejection,
consumer lifetime, reconnect isolation, sanitized errors and SMB truncation/file
identity failures. NFS additionally cancels a deliberately stalled in-flight read.
The protocol run also passes ASan/UBSan with leak detection and halt-on-error.
Final sequential host logs are private at `/work/check-network-*-final.log`;
the protocol/loader sanitizer logs use `*-sanitized-final.log`. Test Samba processes
explicitly use null stdin: inheriting a piped check driver's exhausted stdin makes
foreground Samba exit with `EOF on stdin`, which is a harness failure, not SMB evidence.
The optional loader sanitizer run instruments the newly compiled feature/controller
objects and network clients; the coordinator's saved core/metadata archives are
not rebuilt or sanitizer-instrumented. It must not be described as a sanitized
full emulator or metadata-parser build.

Continuation checks:

- The loader runner compiles only feature/metadata-VFS/controller check objects and
  relinks the saved `libeden-metadata.a`/core archives in the private volume. Real
  network NRO ASET title/icon/title-ID extraction, NSP/CNMT title-ID extraction,
  missing-control fallback results, pre-launch title settings and URI history round-trips
  are exercised. A 5-GiB sparse NRO transfers less than 2 MiB for metadata.
  Local NSP/directory access and local NAND/mod paths are checked alongside them.
- The actual external provider built through `FileSystemController::CreateFactories`
  exposes configured root+one child only, refuses writes, selects the newest matching
  synthetic update/DLC, ignores a misleading foreign-title update and a newer but
  too-deep update. Real loader/PatchManager consumers retain their original VFS
  sessions across menu/discovery teardown and cannot revive after reconnect.
- Native `Launcher` list/selection/settings/history consumers, destructor/async-scan
  joining and error re-entry are exercised with real network-derived entries and
  a service seam that supplies real network-derived metadata and inert graphics/
  console services (not an instantiated `EdenServices`). This is not the full `main`
  loop or an executing guest/renderer. Real transport cancellation and invalid-container
  loader failure are tested separately from the error-screen consumer.
  An invalid NSP loader result is also formatted with the incumbent load-failure
  helper and passed to a new actual Launcher instance to verify failure re-entry.
- Samba 4.19.5 with mandatory signing passes authenticated SMB2/SMB3 reads; required
  encryption passes with SMB3. Wrong password, nonexistent-user-to-guest mapping,
  unsigned guest against a restricted share and SMB2-only encryption requests fail
  without fallback. Generated passwords/configs are never printed or passed in argv.
- Real NFS-Ganesha 4.3 with **MEM FSAL** passes AUTH_SYS/TCP mount/list/stat/random
  reads/EOF/denied-export checks. Its test-only writer seeds a bounded 1-MiB memory
  inode; production sessions still expose reads only. This is a real protocol server,
  distinct from the RFC1813 mock, but not a disk-backed NAS qualification. Ganesha's
  VFS FSAL was attempted and blocked by `open_by_handle_at: Operation not permitted`
  under ordinary Docker capabilities; no privileged container/host mount was used.

Integration checks use the actual pinned Eden `67bada77f8a43a90da2e94e89b8e7da73c256989`
headers, verify adapter retention after replacing core sources, and exercise both
plain and incumbent-instrumented IStorage/IFile error overlays. These are focused
configure/header checks, not full HLE-handler or main-emulator compilation.
The optional SDK check cross-compiles both client libraries, network sources and
the private editor/config writer, checks adapter headers and links a separate smoke
ELF with the actual native `AskSystemKeyboard`, SDK libc++, libkernel/libSceNet,
IME/UserService/Sysmodule imports and the project's CommonDialog import facade.
The ELF is **not executed**. SDK v0.42 archive SHA-256:
`8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da`.
Transport/manager/check compile with `-Wall -Wextra -Werror`; changed UI sources
also pass host syntax checks. Full app/console acceptance remains outstanding.

## Translation handoff (no POT/locale edits)

Network identity/Library UI msgids (English fallback until catalog integration;
the editor's complete additional handoff is `NETWORK_EDITOR_MSGIDS.txt`):

- `Network shares`
- `Play read-only SMB or NFS games without downloading them.`
- `Connecting and scanning…`
- `Connected · {0} games`
- `Disconnected`
- `Disconnect / cancel`
- `Network`
- `Read-only network game. Saves, mods and caches stay local.`

`Back` is reused. Transport diagnostics currently remain English (feature-local
in `network.cpp`/`transport.cpp`); the native editor also translates safe fixed
result/state strings listed in its handoff. None use raw server/JSON error text.
Aggregate documentation, POT and locale catalogs are
intentionally left for the integration owner.
