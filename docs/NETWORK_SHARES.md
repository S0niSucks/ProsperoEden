# Read-only network games (issue #111)

ProsperoEden can discover NSP, XCI and NRO files on explicitly configured SMB/NFS
shares and give Eden random-access reads without downloading a complete game.
Local ROM paths and the existing HTTP/download-source system are unchanged.

**Status:** focused host protocol tests and SDK compile/link checks passed on
2026-10-27. No full emulator/native-app build, console run, actual game launch or
PS5 interoperability/performance result is claimed.

## Configuration and Settings

Create `/data/prosperoeden/config/network-shares.json` privately (mode `0600`),
then open **Settings > Network shares > Reconnect**. The dialog shows the actual
configuration path, scan status, game count and sanitized errors. Cross reloads
settings and reconnects; Square disconnects/cancels; Circle returns. Configuration
is file-based; there is no in-UI server/credential editor in this implementation.

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
  Only configured servers are contacted, automatically when the launcher opens.
- `folder` is relative to the share/export; empty means its root. Absolute paths,
  `.`/`..`, empty components, backslashes, control bytes, URI escaping and unsafe
  delimiters are rejected. Relative paths are at most 2048 bytes. SMB `share` is
  one component; NFS `share` is an absolute export path.
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
- Games appear beside local games, labelled **Network** and the share name, with the file stem as
  their title. Discovery does not decrypt containers or download cover art. Title
  IDs are read through the network VFS at launch using the existing metadata path.
- Credential-free `pe-net://<id>/<relative-path>` identifiers are intercepted only
  by `RealVfsFilesystem::OpenFile` in read mode. Other paths retain local behavior.
  Saves, mods, user data and shader caches remain on the console.
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

### Deliberate gaps

Separately stored network updates/DLC are not registered. Home recents/history,
Library title/cover extraction and per-game settings before discovery knows a
title ID are not fully integrated for network entries. Automatic download-source
save sync and the cheat-catalog installer are disabled for network games because
their identities/folders assume local ROMs. Existing local add-ons, saves and mods
are not relocated. Authenticated/signed/encrypted SMB success, real NFS-server
interoperability and controller/visual behavior need dedicated acceptance runs.

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

Only `prosperoeden-host:features` was used, with the private container/volume
`prosperoeden-team-network-vfs-20261027` (2 CPUs, 3 GiB), this worktree mounted
read-only as `/src`, and private outputs under `/work`. Samba and synthetic RPC
services bind loopback. No real shares, accounts, credentials, games or keys are
used; no coordinator cache, full emulator/native rebuild or WSL install is needed.

With the verified client source trees under `/work/deps` and `smbd` installed in
the disposable container:

```sh
docker exec prosperoeden-team-network-vfs-20261027 python3 -B /src/tools/check-network-shares.py
docker exec -e NETWORK_SANITIZE=1 -e ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 -e UBSAN_OPTIONS=halt_on_error=1 prosperoeden-team-network-vfs-20261027 python3 -B /src/tools/check-network-shares.py
docker exec -e PS5_PAYLOAD_SDK=/work/sdk/ps5-payload-sdk prosperoeden-team-network-vfs-20261027 python3 -B /src/tools/check-network-integration.py
```

Results: real libsmb2 against loopback guest Samba and real libnfs against a
synthetic RFC1813 TCP/MOUNT service pass deterministic/random/concurrent reads,
5-GiB offsets, cross-block reads, EOF, cache hits/bounds, cancellation, stalled
timeouts, denied access, discovery, private config mode/symlink/FIFO/size rejection,
consumer lifetime, reconnect isolation, sanitized errors and SMB truncation/file
identity failures. NFS additionally cancels a deliberately stalled in-flight read.
The protocol run also passes ASan/UBSan with leak detection and halt-on-error.

Integration checks use the actual pinned Eden `67bada77f8a43a90da2e94e89b8e7da73c256989`
headers, verify adapter retention after replacing core sources, and exercise both
plain and incumbent-instrumented IStorage/IFile error overlays. These are focused
configure/header checks, not full HLE-handler or main-emulator compilation.
The optional SDK check cross-compiles both client libraries and network sources,
checks the adapter headers and links a separate smoke ELF with SDK libc++,
libkernel and libSceNet. The ELF is **not executed**. SDK v0.42 archive SHA-256:
`8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da`.
Transport/manager/check compile with `-Wall -Wextra -Werror`; changed UI sources
also pass host syntax checks. Full app/console acceptance remains outstanding.

## Translation handoff (no POT/locale edits)

New UI msgids (English fallback until catalog integration):

- `Network shares`
- `Play read-only SMB or NFS games without downloading them.`
- `SMB / NFS · read-only · on-demand reads`
- `Add your server, share, folder and private credentials to network-shares.json, then reconnect. See docs/NETWORK_SHARES.md. Only configured servers are contacted.`
- `Connecting and scanning…`
- `Connected · {0} games`
- `Disconnected`
- `Reconnect reloads settings and starts a new session. After a connection failure, start the game again. Saves and caches stay on the console.`
- `Reconnect`
- `Disconnect / cancel`
- `Network`
- `Read-only network game. Saves, mods and caches stay local.`

`Back` is reused. Fixed transport/configuration diagnostics currently remain
English (all are feature-local in `network.cpp`/`transport.cpp`); they never use
server/JSON error text. Aggregate documentation, POT and locale catalogs are
intentionally left for the integration owner.
