# Nextendo account and guest-network adapter (#89)

## Scope and current boundary

This is **experimental opt-in plumbing**, not a claim that a game can play online on PS5.
Local Wireless rooms remain the existing, separate multiplayer implementation. Nothing contacts
Nextendo merely because the launcher opens, a profile is chosen, or a game starts with routing off.

Implemented:

- Import a **user-owned existing Citron-Nextendo session**, link it to the selected emulated player,
  explicitly enable/disable service routing, explicitly check the account, and unlink locally.
  Import/replacement always switches routing **off**. Replacement and unlink require confirmation.
- Per-profile private storage; no PID, username, friend code or bearer appears in launcher status,
  logs or errors. A PID is bearer-equivalent, not harmless diagnostic metadata.
- Frozen launch-time identity/routing, refusal to mix a local room with Nextendo, native rather
  than room-proxy guest sockets, and an exact-host DNS redirect adapter. No official-service
  account or password is requested, generated or shipped.
- Explicit PS5 resolver/free adapter using the already-qualified HTTPS object's console resolver;
  no process-wide libc replacement. An absent adapter fails closed, not a false successful lookup.
- Guest TLS uses the mapped DNS name for SNI and certificate hostname verification. Chain and
  hostname verification remain required even if the guest asks to skip them. The native adapter
  loads the console CA list used by HTTPS; missing/unreadable trust fails closed. TLS debug key
  logging is suppressed for an active Nextendo guest. Certificate-pinned games may still refuse it.
- Guest account IDs use the linked profile's PID only for the selected user; another user cannot
  inherit that identity. The pinned core's successful zero-filled BAAS token stubs are **rejected**
  when Nextendo is active instead of being misrepresented as authentication.

### Exact blockers, not implemented success paths

1. Public Citron source builds its browser OAuth `client_id` from **unshipped
   `web_service/nextendo_secrets.h`** (`AppClientId` otherwise empty). ProsperoEden has no registered
   client ID/approved redirect contract. No client ID was guessed or borrowed.
2. The inspected login is **browser authorization code + S256 PKCE + a 127.0.0.1 loopback callback**,
   not a phone device-code API. A phone's loopback points at the phone, not the PS5. No device-code
   routes, passwords, account registration, outreach, or live credential sessions were attempted.
3. The fork's guest BAAS wrapper locally signs an `id_token` carrying the signed NEX proof (`nnex`).
   It also contains explicit references to Ryujinx-Nextendo-derived claim-shape behavior. We have
   **not copied** that claim construction, signing keys, or the incompatible Ryujinx implementation.
   A reviewed, GPL-compatible BAAS contract (or an authoritative server-issued id_token API),
   including title/version/audience rules, is still needed. A `nex_token` is not a BAAS id_token.
   Consequently game login intentionally fails at this boundary, rather than issuing fake tokens.
4. The public DNS fork defaults its server IP configuration to loopback and obtains actual server
   addresses externally. It cannot provide an audited, certificate-valid endpoint list. This
   implementation ships **no fabricated/default server map**. The fork's global TLS verification
   bypass was inspected and deliberately rejected. Operators must supply verified DNS routes with
   valid matching certificates; if servers only support the original SNI/self-signed certificates,
   interoperability remains blocked until an explicit reviewed trust contract exists.
5. Guest sockets, interface discovery/NIFM, game authentication, NAT traversal, friends/presence,
   title-specific patches and actual matchmaking still require PS5/server evidence. Host synthetic
   tests do not qualify them. No full emulator or native application build was performed here.

## Setup and recovery (explicit user actions only)

The launcher entry is **Multiplayer > Nextendo (experimental)**, alongside (not inside) the room
choices. It displays local linkage/routing state and explicitly says game sign-in is unverified.

1. Choose the intended player profile. Select **Import my session** once to create the private
   folder if it does not exist. Obtain your own existing session from your own Citron-Nextendo
   configuration; do not use somebody else's account or a bare test PID.
2. Place it at `/data/prosperoeden/config/nextendo/import.txt`. This accepts the existing four-line
   `pid=…`, `username=…`, `friend_code=…`, `token=…` format, or JSON with `version:1`, an unsigned
   nonzero `pid`, and a `token`. **No real values belong in this repository, screenshots or logs.**
   The directory must have mode **0700**, the import mode **0600**, and both must belong to the
   application user. Filesystem privilege support on PS5 needs hardware confirmation. Failure
   is reported; the code does not silently fall back to publicly readable storage.
3. Select **Import my session** again. Validated data is atomically stored as
   `nextendo/<emulated-profile-key>.account`; the import is deleted after success. Only PID/token
   are retained; username/friend code are discarded. Existing linkage requires a second Cross to
   confirm replacement. Import/replacement never consents to online traffic.
4. **Check account** explicitly makes an HTTPS GET to `https://nextendo.network/api/profile`, with
   a 15-second deadline, bounded response, cancellation and **no redirects**. A valid profile
   response means only the session was accepted by that API, not that a game connected. A 401
   removes local linkage and disables routing. Server response/error bodies never reach the UI.
5. Supply `nextendo/routes.json` from an independently verified operator contract: a JSON object
   mapping each **exact official-service DNS name** to its **exact certificate-valid Nextendo DNS
   name**. Keys must be under `nintendo.net`, `nintendo.com`, or `nintendowifi.net`; destinations must
   be `nextendo.network` or its subdomains. No wildcard, IP, URL, suffix substring, environment
   override, or arbitrary account API origin is accepted. No pretend example server is included.
6. Select **Turn routing on** explicitly. There must be at least one valid route and no local
   room. The next guest launch snapshots this profile's account/routes; settings cannot switch
   identity under an already-running game. A conflicting local room blocks launch with recovery
   guidance. Unmapped official DNS names fail instead of falling through to official DNS. This is
   a resolver policy, **not a firewall for hard-coded IPs or alternate in-game DNS implementations**.
7. Select **Turn routing off** to disable. This remains possible with damaged routes; a disabled
   profile's damaged routing file does not block offline launch. **Unlink account** requires two
   Cross presses and removes the account and stale staging file. Moving focus/Back cancels
   confirmations. Removing a player profile also removes its local session, while preserving saves.

Unlink is **local deletion**, not remote session revocation or guaranteed flash secure erase.
Revoke the session in the provider's supported account UI if needed. Private files are **not
encrypted with a hardware keystore**; no suitable PS5 keystore contract was found in this project.
Mode/owner checks protect against ordinary filesystem users and link substitution, not elevated
homebrew, a compromised application, unprotected FTP transport, or forensic storage recovery.
Use a trusted transfer path. Keep this directory, `import.txt`, staging files and any upstream
session exports out of packages, source snapshots, diagnostics, screenshots and shared backups.
Backup integration must explicitly exclude or separately protect `config/nextendo/`.

### Lifetimes and failures

UI actions run in one `std::async` future; Back sets cancellation and launcher teardown waits for
that future before services/textures disappear. The network operation has a bounded total timeout.
No worker captures a destroyed service. The UI binds the local profile key before scheduling its
worker; the service rejects a changed selection before I/O. Profile switching and launch are refused
while its action is running. Storage uses bounded reads,
owner/mode/type/link-count checks and ancestor-by-ancestor `O_NOFOLLOW` opens. Reads, rename and
deletion stay relative to a held directory descriptor, not a path that can be substituted midway.
Exclusive 0600 staging + fsync + rename preserves the old account on a failed replacement. A stale
`.new` is an error, not a reason to truncate it.
Cancellation observed before the import write prevents it; cancellation during or after a storage
commit does not roll it back. Files are under the app-owned config root, not SDMC/game/USB paths.

The launch session outlives the emulator service objects and is cleared after their teardown.
Native resolver allocations/freeing use the same captured allocator even if shutdown is requested.
There is no PID/token bridge file in the guest SDMC.

## Focused evidence

Use `prosperoeden-host:features` and **only** the private volume
`prosperoeden-team-nextendo-20261027`. Mount this worktree read-only. Do not reuse coordinator build
volumes. Tests use synthetic sessions, a stub account server, loopback HTTP redirects, and generated
certificates; none contacts a public account/game server.

```
TMPDIR=/work CXX=clang++-18 CC=clang-18 python3 /repo/tools/check-nextendo.py --source /pinned
```

`/pinned` is Eden `67bada77f8a43a90da2e94e89b8e7da73c256989` (the three service files and
`src/core/internal_network/network.cpp` suffice for derivative guards). This checks:

- Checked patch counts against the actual pinned acc, DNS, TLS and native network source.
- ASan/UBSan account parsing, bounds, profile isolation, default-off behavior, atomic replacement,
  stale staging protection, ancestor/file symlink, hardlink and permissive-mode refusal, unlink, 401, cancellation,
  bounded/malformed replies, no response-body credential disclosure and launch snapshots.
- Real in-memory OpenSSL handshakes: trusted matching certificate succeeds; wrong hostname,
  unknown CA, expiry, missing/stale SNI, numeric-only TLS name and guest verification bypass fail.
- Real curl loopback redirect refusal, full-length (8192-byte) synthetic bearer transport and
  preservation of existing non-Nextendo redirect behavior.
- Native resolver/free bridge contract and console CA import, including missing adapters/CA files.
- Actual service adapter with synthetic platform dependencies: a changed profile before worker
  execution is rejected without touching storage; busy/storage/room guards and profile isolation.

`tools/check-nextendo-ui.py <private-preview-binary> <output>` drives the actual built-in launcher
at 720p and 1080p with a **synthetic** service (not a credential/network simulation presented as
console evidence). It covers opening without work, import-off, explicit enable, replacement/unlink
confirmation, cancellation, account-check wording and GL-error-free captures. The changed launcher
translation units also pass focused `-Wall -Wextra -Werror -fsyntax-only` checks. Actual native core
compilation/linking and PS5/socket/server tests remain coordinator qualification work.

## Source and license provenance

- Destination identity/licensing remains **ProsperoEden**, GPL-3.0-or-later, pinned Eden 67bada77.
  New feature files are GPL-3.0-or-later. Existing Eden/yuzu notices remain in derived files.
- Reference: `NextendoNetwork/citron-nextendo` at exact commit
  **8e76192472a3993a8f8fa6835e7ed061d443a9f7**. The coordinator inspected README GPL-3.0-or-later
  and `common/nextendo_account.{h,cpp}`, `web_service/nextendo_api.h` as GPL-2.0-or-later. This worker
  read the account implementation and relevant browser OAuth/profile/API-origin sections of
  `web_service/nextendo_api.cpp`, routing sections of `sockets/sfdnsres.cpp`, account/token sections
  of `acc/acc.cpp`, and the OpenSSL backend's verification/SNI behavior. Inspected source headers
  carry GPL-2.0-or-later; compatible logic is adapted under GPL-3.0-or-later here.
- Adaptations: account session field names, explicit linked PID rather than the user UUID hash,
  `/api/profile` bearer contract, opt-in DNS identity redirection. New stricter storage, endpoint
  validation, lifecycle, CA handling and tests are local implementation. The reference's logging
  of PID/token-bearing bodies, unprotected session files, bare test-PID fallback, SDMC credential
  bridge and blanket TLS bypass are deliberately **not** imported.
- The rest of the reference tree has **not** been comprehensively reviewed. No wholesale core,
  title patches, SDK/firmware/account data, server code without a reviewed license, signing keys,
  or Ryujinx-Nextendo/PolyForm Shield code is copied or shipped.

## Integration / shared-file touchpoints

Small shared edits: `headless/CMakeLists.txt` includes the feature overlays and one launcher
transport source, composing the native network derivative after existing socketpair changes;
`main.cpp` freezes/clears the launch session; `remote/http.{h,c}` adds default-off `no_redirect`
and avoids silent authorization-header truncation;
launcher service interfaces, profile removal and multiplayer choice/lifetime hooks connect it.
`host/runtime_preview.cpp` has feature-local synthetic fixture hooks. No aggregate release docs,
versions, generated POT or locale catalogs were changed.

Conflict-prone existing files for coordinator integration (no merge attempted here):

- `headless/CMakeLists.txt`, `headless/main.cpp`, `headless/remote/http.{h,c}`.
- `headless/prosperoeden/eden_services.{h,cpp}`.
- `headless/prosperoeden/pe/ui/launcher.{cpp,hpp}`, `pe/ui/multiplayer.cpp`, `pe/ui/services.hpp`.
- `headless/prosperoeden/host/runtime_preview.cpp`.

The backup workstream must exclude/protect the whole `config/nextendo/` directory. Do not expose
account files via general-purpose backup, diagnostics or source-snapshot collection.

### New msgids for coordinator catalog integration

Source: `headless/prosperoeden/pe/ui/multiplayer.cpp` and `pe/ui/nextendo.inc` (the current strings
extractor scans cpp/hpp/h, so **include the feature's .inc strings** when consolidating catalogs).
All existing strings retain their msgids. English service errors follow the existing backend-error
convention and are sanitized; catalog work is intentionally not performed independently here.

- `Nextendo (experimental)`
- `Link a community account. Separate from local rooms; off by default.`
- `Service routing on`
- `Service routing off`
- `Account linked to this profile`
- `No account linked`
- `Community services, not local rooms. Game sign-in and PS5 online play are not yet verified.`
- `Confirm replace`
- `Replace linked session`
- `Import my session`
- `Turn routing off`
- `Turn routing on`
- `Check account`
- `Confirm unlink`
- `Unlink account`
- `Import your existing Citron-Nextendo session as config/nextendo/import.txt (private mode 0600). Import keeps routing off. Configure verified endpoints in nextendo/routes.json before enabling. See docs/features/NEXTENDO.md.`
- `Working... Circle cancels.`
- `Account accepted. This is not a game connection test.`
- `Native browser linking needs a registered Nextendo client. No phone-code API is assumed.`
- `Back / cancel`
