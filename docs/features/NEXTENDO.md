# Nextendo account and guest-network adapter (#89)

## Scope and current boundary

This is **experimental opt-in plumbing**, not a claim that a game can play online on PS5.
Local Wireless rooms remain the existing, separate multiplayer implementation. Nothing contacts
Nextendo merely because the launcher opens, a profile is chosen, or a game starts with routing off.

Implemented:

- Actual browser authorization-code + S256 PKCE linking, with a user/operator-provided registered
  client/app/redirect. A bounded, one-use 127.0.0.1 receiver validates state/Host/query/expiry, then
  exchanges the code over verified HTTPS and atomically links only the captured local profile.
  The launcher shows a short **same-PS5** loopback entry URL, never a token, verifier or callback code.
- Import a **user-owned existing Citron-Nextendo session**, link it to the selected emulated player,
  explicitly enable/disable service routing, explain game-only check limitations, and unlink locally.
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
   client ID/approved redirect contract bundled. The OAuth implementation is runnable and tested
   with a synthetic provider; live use requires the owner's registration in `oauth.json`. Missing
   registration is a precise configuration refusal, not an omitted implementation. No client ID
   was guessed or borrowed.
2. The inspected login is **browser authorization code + S256 PKCE + a 127.0.0.1 loopback callback**,
   not a phone device-code API. A phone's loopback points at the phone, not the PS5. No device-code
   routes, passwords, account registration, outreach, or live credential sessions were attempted.
   Automatic PS5 browser launch is not asserted: the explicit URL workflow requires a browser on
   that PS5 while ProsperoEden continues processing its callback. Foreground/background behavior
   and access to that browser require PS5 qualification; manually opening it on a phone is not valid.
3. The fork's guest BAAS wrapper locally signs an `id_token` carrying the signed NEX proof (`nnex`).
   It also contains explicit references to Ryujinx-Nextendo-derived claim-shape behavior. We have
   **not copied** that claim construction, signing keys, or the incompatible Ryujinx implementation.
   A reviewed, GPL-compatible BAAS contract (or an authoritative server-issued id_token API),
   including title/version/audience rules, is still needed. A `nex_token` is not a BAAS id_token.
   Consequently game login intentionally fails at this boundary, rather than issuing fake tokens.
   The exact GPL reference location is `src/core/hle/service/acc/acc.cpp:278–330` (`BuildIdToken`),
   whose comments at 303–307 explicitly tie claim-shape choices to Ryujinx-Nextendo. This is not
   treated as a separately authoritative, cleanly licensed server acceptance specification.
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

### Browser linking

The current public-contract implementation and parent consent integration are described in
[NEXTENDO_OAUTH_NEX_CONTRACT.md](NEXTENDO_OAUTH_NEX_CONTRACT.md). The default high-level
entrypoint refuses linking until the frontend explicitly confirms identity/game matchmaking
permissions; an older frontend does not silently gain these scopes.

1. Using a trusted transfer path, create the app-owned `config/nextendo/` directory with mode 0700
   (the existing import action can also create it). Put **your registered** configuration in
   `config/nextendo/oauth.json`, owner-only mode 0600. Schema, with placeholders **not credentials
   and not usable registrations**:

   ```json
   {"version":1,"client_id":"<YOUR_REGISTERED_CLIENT_ID>","redirect_uri":"http://127.0.0.1:{port}/callback"}
   ```

   The registered redirect must explicitly permit this loopback template/ephemeral port, as in the
   GPL reference, or configure an exact registered port (1024–65535). Only `http://127.0.0.1:` plus
   a port or `{port}` and a simple absolute callback path is accepted. No LAN IP, localhost DNS,
   phone callback, arbitrary origin, redirect query, client secret or borrowed Citron app ID.
   The legacy `app` field is optional, omitted from requests when absent, and strictly
   validated when present. No borrowed application name or client ID is supplied.
2. Select **Link in this PS5's browser**. Replacing an existing link requires confirmation. Missing,
   malformed or non-private registration fails **before binding a listener or making any request**.
   The listener binds only 127.0.0.1; an occupied configured port fails, never silently picks another.
3. On **this PS5**, open the displayed `http://127.0.0.1:<port>/start` while keeping ProsperoEden
   running. This is a short local entry point, not a phone code or a LAN service. It redirects the
   browser to `https://nextendo.network/api/oauth/authorize` with the configured registration,
   `response_type=code`, scopes `identity game.matchmaking`, random state and S256 challenge,
   after explicit frontend confirmation and then provider consent. No automatic account
   creation, form filling or credential collection is performed by ProsperoEden.
4. Complete the provider's browser consent. Its registered callback must return to the same
   loopback listener. The callback page says only that authorization was received; final linkage
   can still fail at token validation, cancellation or storage. Return to the launcher for its result.
5. The code is consumed once via form POST to `/api/oauth/token` with `grant_type`, `code`,
   `client_id`, the **same actual redirect URI**, and the private `code_verifier`.
   `X-Nextendo-Client-Id` is included, as required by the GPL reference. Both server chain and hostname
   are verified; redirects are refused. Web access is used in memory for `/api/oauth/userinfo`
   and then `/api/nex-token`; their nonzero unsigned PIDs must match. Only the game credential,
   PID and bounded expiry metadata are retained in private version-2 storage; web/refresh
   credentials, scopes and username/friend code are discarded. Success saves private credentials
   directly, without an import/guest bridge file, and switches routing **off**.
   Refresh is not implemented: link again after game-session expiry. Remote account checks
   of a saved game-only session explicitly fail without network or deletion. Legacy imports
   remain supported with unknown expiry; their game tokens are never sent to `/api/profile`.
6. Circle cancels. The overall lifetime is five minutes including the bounded token exchange
   (at most 15 seconds and never past the overall deadline). A client gets at most two seconds to
   supply at most 8192 bytes of HTTP headers; at most 64 local requests are serviced. Invalid
   Host/Origin/state/duplicate or malformed callback fields cannot exchange a token. A valid provider
   denial ends this attempt. The listener and published URL are removed on completion/cancel/error;
   a failed replacement or cancellation observed before its storage commit preserves the previous
   session and routing consent.

The earlier legacy wire contract came from `web_service/nextendo_api.cpp:277–284,357–485` at
the pinned reference. The current public web-to-NEX contract and its limits are cited in
`NEXTENDO_OAUTH_NEX_CONTRACT.md`. Registration must authorize the two requested scopes and
redirect. No device-code grant is inferred, and refresh is not retained or used. Game-only
checks do not make requests, regardless of whether a client registration is present.

### Existing-session import, checking and routing

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
4. **Check account** reports that remote validation is unavailable for a game-only session,
   without sending its NEX value to an account API or deleting it. There is no documented
   NEX introspection route. Link again to acquire a fresh game session. The unchanged service
   wrapper may still ask for registration before surfacing this result; parent integration is
   tracked in `NEXTENDO_OAUTH_NEX_CONTRACT.md`. No check claims game connectivity.
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
Cancellation observed before an import/browser-link write prevents it; cancellation during or after a storage
commit does not roll it back. Files are under the app-owned config root, not SDMC/game/USB paths.

The launch session outlives the emulator service objects and is cleared after their teardown.
Native resolver allocations/freeing use the same captured allocator even if shutdown is requested.
There is no PID/token bridge file in the guest SDMC.

Browser linking never holds the store mutex while waiting for a browser or HTTPS. The existing
worker busy/profile guards apply, and the short local URL is separately bridge-mutex-protected for
UI polling without I/O. The verifier, form, code and raw response have best-effort in-memory wiping;
this is **not** a guarantee that JSON/curl/allocator copies or process memory have been securely erased.
The receiver uses the existing native fcntl socket wrapper (SO_NBIO fallback); it refuses failure to
set nonblocking mode rather than accepting an unbounded blocking client. Do not enable TLS debug,
proxy tracing or credential-bearing browser history exports during account operations.

## Focused evidence

Use `prosperoeden-host:features` and the assigned private volume
`prosperoeden-team-pro-controller-20261027`. Mount this worktree and any coordinator cache inputs
read-only; never use a coordinator volume for outputs. Tests use synthetic sessions, a stub account server, loopback HTTP redirects, and generated
certificates; none contacts a public account/game server.

```
TMPDIR=/private CXX=clang++-18 CC=clang-18 python3 /repo/tools/check-nextendo.py --source /pinned
```

`/pinned` is Eden `67bada77f8a43a90da2e94e89b8e7da73c256989` (the three service files and
`src/core/internal_network/network.cpp` suffice for derivative guards). This checks:

- Checked patch counts against the actual pinned acc, DNS, TLS and native network source.
- ASan/UBSan account parsing, bounds, profile isolation, default-off behavior, atomic replacement,
  stale staging protection, ancestor/file symlink, hardlink and permissive-mode refusal, unlink,
  cancelled/unavailable game checks, version-2 expiry, no credential-bearing requests and launch snapshots.
- Real in-memory OpenSSL handshakes: trusted matching certificate succeeds; wrong hostname,
  unknown CA, expiry, missing/stale SNI, numeric-only TLS name and guest verification bypass fail.
- Real curl loopback redirect refusal, full-length (8192-byte) synthetic bearer transport and
  preservation of existing non-Nextendo redirect behavior.
- Native resolver/free bridge contract and console CA import, including missing adapters/CA files.
- Actual service adapter with synthetic platform dependencies: a changed profile before worker
  execution is rejected without touching storage; busy/storage/room guards and profile isolation.

`TMPDIR=/private CXX=clang++-18 CC=clang-18 python3 /repo/tools/check-nextendo-oauth.py` compiles the actual
receiver, token exchange, curl transport and store with ASan/UBSan. A synthetic browser follows the
real local entry redirect through a generated-certificate **HTTPS authorization endpoint**, then the
actual callback; a loopback HTTPS token endpoint checks the RFC 7636 proof and registered header.
The test-only platform adapter changes the destination socket/CA, **not** the production URLs,
TLS peer hostname, verification, request parser or token logic. No production origin override exists.
Cases include missing registration/no activity, fixed-port collision, random state/PKCE, invalid and
duplicate callback fields, Host/Origin refusal, slow/oversized clients, request flood, expiry, cancellation
while waiting/exchanging/before save, provider denial/errors, token redirect/body limits, unknown CA,
wrong hostname, one-use consumption, storage failure, previous-session preservation and default-off save.

`tools/check-nextendo-ui.py <private-preview-binary> <output>` drives the actual built-in launcher
at 720p and 1080p with a **synthetic** service (not a credential/network simulation presented as
console evidence). It covers opening without work, import-off, explicit enable, replacement/unlink
confirmation, missing-client refusal, same-device URL, cancellation, account-check wording and
GL-error-free captures. The changed launcher
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
- OAuth additions adapt only the pinned GPL account authorization wire contract. No upstream
  logging of authorization URLs, state, PID, tokens or response bodies is imported. The receiver
  uses existing system sockets and OpenSSL; no new third-party server or OAuth dependency is added.
- The rest of the reference tree has **not** been comprehensively reviewed. No wholesale core,
  title patches, SDK/firmware/account data, server code without a reviewed license, signing keys,
  or Ryujinx-Nextendo/PolyForm Shield code is copied or shipped.

### Public BAAS / endpoint investigation (2026-10-27)

Read-only Nextendo-owned repository metadata, license files and the following documentation were
inspected. These are **documentary contract facts**, not a source license grant or evidence of a
working public deployment. No server implementation from these repositories was read/adapted.

| Nextendo-owned source (immutable revision) | Evidence and limit |
|---|---|
| [nextendo-docs](https://github.com/NextendoNetwork/nextendo-docs/tree/d34c759420ec6cb5924f43097b21aba5b64eaea4) | `LICENSE.md` declares PolyForm Shield 1.0.0. `ARCHITECTURE.md:63–66,89–113` describes SNI passthrough, an `nx2.` proof and runtime-supplied addresses/keys/TLS. It is not a normative BAAS claim schema. |
| Same revision, `DEPLOYMENT.md:14–20,63–80` | Operator-controlled server/NAT addresses and client-trusted certificates for redirected hostnames; `NEXTENDO_BAAS_SIGNING_KEY` is supplied externally. Addresses/ports in examples are explicitly examples, not an audited service map. Original-host SNI/private CA deployment is not interchangeable with our mapped certificate-valid public DNS policy. |
| [baas-jwks](https://github.com/NextendoNetwork/baas-jwks/tree/fe141462a5b622be49bfe28a01174c653af3e23f), `README.md:12–20` | Describes RS256 verification, `jku`/`kid` and a public JWK derived from an externally supplied signing key. No private key, fixed infrastructure address or complete claims/audience/title/version contract is published there. `LICENSE.md` declares PolyForm Shield. No signing keys were downloaded. |
| [sni-router](https://github.com/NextendoNetwork/sni-router/tree/ea2976c97ebd042a0c82928e5851bc8f234a2a42), `README.md:3–7` | Describes TLS SNI passthrough; license badge and `LICENSE.md` declare PolyForm Shield. Does not establish which mapped Nextendo DNS names/certificates a public operator serves. |
| [nextendo-account](https://github.com/NextendoNetwork/nextendo-account/tree/61845fa2742f2053d2aa75bbc5d6b3a888eb2d1a), [nx-dauth](https://github.com/NextendoNetwork/nx-dauth/tree/5ce9901d077065a0f718d9f748fa621164e6c6c8), [nextendo-nncs](https://github.com/NextendoNetwork/nextendo-nncs/tree/86936ca0fcfdc7508cbcc63f7be3ce7e55a9b451) | Each inspected `LICENSE.md` declares PolyForm Shield. Repository presence is not permission to transplant its implementation or proof of console compatibility. |
| Pinned GPL Citron `src/core/hle/service/acc/nextendo_baas_nsa.{h,cpp}` | Fully inspected in this follow-up. Describes a title-specific NSA/`baasUserID` identity lookup through `/api/me` and friends, not a server-issued BAAS id_token. It also references Ryujinx behavior; none was copied. An NSA identifier is not authentication proof or a substitute for the missing token contract. |

The pinned repository's complete GitHub tree was also searched for Nextendo/BAAS/OAuth paths;
the public Nextendo homepage was inspected for documentation/API links. This is a bounded source
review, not a proof that no further private/undiscovered specification exists.

Exact outstanding inputs, rather than a generic TODO:

1. **OAuth live configuration/qualification:** owner-issued public client ID, explicit identity/game
   matchmaking consent and approved loopback redirect/ephemeral-port rule (legacy app value optional);
   parent frontend confirmation handoff and PS5 evidence that the chosen
   same-device browser can load the local entry/callback while this title's receiver continues running.
   The implementation and synthetic real-transport flow do not depend on receiving these inputs.
2. **BAAS implementation contract:** a redistributable GPL-compatible normative claim/header schema
   (issuer, audience, title/application/version identifiers, subject/PID, `nnex`, timing/nonce rules,
   algorithm, `kid`, `jku` and verification/rotation rules), with synthetic accepted/rejected vectors;
   plus an authorized signer/private-key provisioning contract **or** an authoritative server-issued
   id_token endpoint and its authentication/response contract. A JWKS README or a NEX bearer alone
   does not supply these. No server-issued id_token API was established in the GPL material reviewed.
3. **Network/trust contract:** exact per-service DNS destination/port map and NAT vantage points,
   certificate names/chain and expected SNI for those destinations. If only original-host/private-CA
   SNI routing is supported, a separate explicitly provisioned/reviewed trust design is required;
   neither `verify=false` nor pretending an arbitrary map is compatible is acceptable.
4. **Qualification:** title/version-specific guest account/TLS/socket/NIFM/NAT/friends/presence/server
   traces and successful/negative test cases on the target PS5 build. None is inferred from host tests.

## Integration / shared-file touchpoints

The following shared-file integration was delivered earlier, not edited by the current OAuth/NEX
contract task. Its separate parent UI/service handoff is in `NEXTENDO_OAUTH_NEX_CONTRACT.md`.

Small shared edits: `headless/CMakeLists.txt` includes the feature overlays, account HTTP and OAuth
sources, composing the native network derivative after existing socketpair changes;
`main.cpp` freezes/clears the launch session; `remote/http.{h,c}` adds default-off `no_redirect`
and optional registered-client header fields, and avoids silent authorization-header truncation;
launcher service interfaces, profile removal and multiplayer choice/lifetime hooks connect it.
`host/runtime_preview.cpp` has feature-local synthetic fixture hooks. No aggregate release docs,
versions, generated POT or locale catalogs were changed.

The follow-up appends `Action::link_browser` (preserving existing enum values), retains the original
`AccountCheck`/`Store` constructor signatures, and adds a separate callable constructor for the
configured client header. The original unconfigured HTTP-check entry point now refuses before I/O.
`Services::nextendo_link_url()` defaults to empty, so other existing service implementations need not
override it. `remote_http_request.nextendo_client_id` is appended, NULL by default for existing users.
Integrate these small hunks around any intervening scene/CoverFlow/Amiibo work; this branch does not
assume those coordinator changes are absent and has not attempted to merge them.

Conflict-prone existing files for coordinator integration (no merge attempted here):

- `headless/CMakeLists.txt`, `headless/main.cpp`, `headless/remote/http.{h,c}`.
- `headless/prosperoeden/eden_services.{h,cpp}`.
- `headless/prosperoeden/pe/ui/launcher.{cpp,hpp}`, `pe/ui/multiplayer.cpp`, `pe/ui/services.hpp`.
- `headless/prosperoeden/host/runtime_preview.cpp`.

The backup workstream must exclude/protect the whole `config/nextendo/` directory. Do not expose
account files via general-purpose backup, diagnostics or source-snapshot collection.

### New msgids for coordinator catalog integration

Source: `headless/prosperoeden/pe/ui/multiplayer.cpp` and `pe/ui/nextendo.inc`. The extractor now
includes `.inc` sources, covered by `tools/launcher/test-strings.py`.
All existing strings retain their msgids. English service errors follow the existing backend-error
convention and are sanitized. The coordinator additively imported all 24 feature msgids across
all 30 catalogs (720 translations), preserving existing bytes. The checker passes at 659 total
source texts, zero problems; eight new heuristic length advisories are not measured clipping.
Console system-font coverage and localized layout remain unqualified. Reviewed batch SHA-256:
`130349ccb695c5ae99ad7957c2741bb66b5d9be896a84b0eb78ff841d728a4d3`.
Validation log: `.local/upstream-audit/strings-usb-amiibo-nextendo-complete.log`.

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
- `Confirm browser replacement`
- `Link in this PS5's browser`
- `Browser linking needs your registered client in nextendo/oauth.json (0600). Or import your own session as nextendo/import.txt. Linking keeps routing off. Verified routes are required before enabling. See docs/features/NEXTENDO.md.`
- `Working... Circle cancels.`
- `Account accepted. This is not a game connection test.`
- `Use a browser on this PS5 while ProsperoEden keeps running. A phone or another device cannot reach this loopback callback.`
- `Open on THIS PS5 (expires in five minutes; Circle cancels):`
- `Keep ProsperoEden running. Do not open this on a phone or another device.`
- `Back / cancel`
