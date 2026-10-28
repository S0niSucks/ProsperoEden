# Nextendo public OAuth → game-session contract

Implementation based on integrated `9cfebff`, on
`team/nextendo-oauth-nex-contract-1006000`. No real registration, credential, provider login,
session endpoint or console was used. All authenticated traffic in tests terminates at a
synthetic HTTPS fixture in a network-disabled Docker container.

## Implemented flow

1. The high-level `LinkBrowser` entrypoint requires **explicit game-permission confirmation**.
   The default is false: existing frontend callers compile but fail before binding/listening,
   publishing a URL or requesting anything. This deliberately prevents an old identity-only
   UI from silently acquiring a broader grant. Parent UI/service integration is still required.
2. An approved public-client registration supplies its own `client_id` and loopback redirect.
   `app` is optional for compatibility with existing local registrations; absent means omitted
   from the provider query. If present, it must be a nonempty string of at most 64 printable
   non-space ASCII characters. No name/ID/secret is bundled. Existing duplicate-key, private
   file and exact callback spelling checks remain. No client secret is used.
3. The system-browser URL requests exactly `identity game.matchmaking`, PKCE S256 and a
   fresh state. Callback state/host/origin/request bounds and one-use consumption remain.
   `/api/oauth/authorize` and `/api/oauth/token` are retained: the official guide contains
   both prefixed and unprefixed examples, not evidence that our current routes are invalid.
4. Exchange the code/verifier for a **web access credential** via `/api/oauth/token`.
   Require a Bearer `token_type` (case-insensitive), bounded non-game/non-bot access value,
   and integer `expires_in` in 1–3600 seconds. Access fields are never saved as game fields.
5. Using **only that web credential**, GET `/api/oauth/userinfo`. Require a nonzero unsigned
   64-bit root `pid`. Then GET `/api/nex-token` with the same web credential. Require the same
   unsigned `pid`, a bounded `nx2.` game credential and integer `expires_in` in 1–86400 seconds.
   A PID string, float, negative/overflow/zero value, a different PID or another credential
   class is not silently converted. No nested legacy account response is assumed.
6. Persist **only the game session**, private and default-off. Discard web access, refresh,
   scopes and unused profile/username/friend-code fields. No token/PID/server body is included
   in status/error/callback-page/published start URL/log/diagnostics. OAuth's protocol-required
   one-time code reaches the loopback callback, but is not displayed or logged; no access,
   refresh or game credential is put in any browser URL.

Explicit returned scope must contain exactly the two requested scopes (either order), with
no narrowing, extra permission, duplicates or non-string representation. If `scope` is omitted,
RFC 6749 §5.1 defines it as unchanged from the request; both protected identity and NEX reads
must still succeed. We do not claim to inspect actual provider consent UI from a fixture.
`friends`, `presence`, `mods.favorites` and the valid but editing-capable `profil` scope are
not requested. Any denied/malformed exchange or protected read preserves the previous session.

### Transport and lifetime boundaries

- Three sequential requests use the same existing `remote_http_run` transport. It was not
  changed. HTTPS peer/hostname trust, no redirects, safe request-header allocation, 32 KiB
  body limit and duplicate-key rejection apply to **every stage**.
- All stages share the deadline established when opening the flow (maximum five minutes).
  Each HTTP request also has a maximum 15-second timeout and is bounded by remaining time.
  Identity/NEX reads additionally share the earlier web-expiry deadline, measured conservatively
  from before the exchange. No per-stage restart grants more overall time.
- The game lifetime is measured from before its request, not after a delayed response. Expired
  credentials in transit are rejected. Positive TTL maxima follow the guide's one-hour web /
  24-hour game lifetimes; the implementation fails closed on larger/missing/mistyped values.
- `Store::Link` accepts the original flow deadline, checks stop/deadline before committing, and
  rechecks after fsync before atomic rename. Cancellation/expiry before rename removes only
  its staging file and preserves the old account. Game expiry is rechecked there too, so a
  credential expiring inside fsync cannot be committed as a fresh link. Rename is the commit point: a later cancel
  cannot undo an already committed session. Other existing storage/admission guards stay intact.
  Cancellation/deadlines are cooperative: a blocking OS filesystem syscall is not forcibly
  interrupted; the deadline is checked again before publication once that syscall returns.

## Storage, checks and backwards compatibility

Version 1 JSON and the existing owner-provided `pid=...` / `token=...` import remain supported.
Their expiry is **unknown**, not validated or inferred from token text. Imports still default
off. New browser links save version 2 with these fields only:

`version`, `credential_kind` (`nex`), `pid`, `token`, `enabled`, `issued_at`, `expires_at`.

No usable example credential/config is shipped. Version 2 requires positive unsigned times,
expiry after issue and at most 86400 seconds apart, and a game credential rather than a web
value. Existing 0700 directory / 0600 file, held-directory/no-follow/hardlink/size restrictions,
exclusive `.new` and atomic replacement remain. Disabling/unlinking an expired session is
allowed. Enable, explicit check and launch refuse known expiry or a clock earlier than issue;
they do not erase the stored session. System wall-clock correctness remains an external
requirement; unknown legacy expiry cannot be reconstructed securely.

Guest snapshots remain immutable, profile-bound and game-only. A launcher relink does not
switch identity under a running guest. Expiry is checked before launch, not by switching an
active guest back to official routing mid-game; the server still governs game acceptance.

`CheckAccountHttp` and `CheckAccountHttpWithClient` now return `unavailable` (or `cancelled`)
**without network**. There is no public documented NEX introspection route, and we deliberately
retain no web credential. The stored NEX value is never used as Bearer for `/api/profile` or
any other account API. A 401 from an OAuth linking attempt cannot delete the previously saved
game session. `Store` preserves its generic explicitly-authoritative callback contract, but
the production HTTP adapter never fabricates an authoritative rejection for a game token.

Refresh is documented by the provider but **not implemented** here: its unused credential is
discarded rather than persisting a web/refresh session and adding a second lifecycle. Link again
when the game session expires. Unlink removes local files only; no undocumented revocation route
is invented and local deletion is not advertised as revoking provider authorization. App-owner
consent management/revocation remains at the provider.

## Parent UI/service handoff — mandatory before enabling browser linking

No shared UI, `EdenServices`, admission code or `*services.inc` was modified. Four stable message
IDs are exact string constants in `headless/nextendo_status.h`; two additional changed/new
error literals in `nextendo_oauth.cpp` also need parent localization:

| Constant | English msgid / frontend contract |
| --- | --- |
| `GameLinkConsentMessage` | `Link a Nextendo game session? Share your identity and allow game matchmaking (identity, game.matchmaking). No friends, presence or profile editing permission is requested. Game routing stays off until you enable it separately.` |
| `GameLinkConsentRequired` | `Confirm identity and game matchmaking permission before linking a Nextendo game session.` |
| `GameCheckUnavailable` | `Remote checks are unavailable for game-only Nextendo sessions. Link again to obtain a fresh game session; the saved session has not been removed.` |
| `GameSessionExpired` | `The Nextendo game session has expired or the clock is incorrect. Link again before enabling or launching.` |
| New linking error literal | `Nextendo game linking failed. Check registration, consent, connectivity and trusted certificates.` |
| Updated registration error literal | `Invalid oauth.json. Supply your registered client_id and HTTP 127.0.0.1 redirect_uri (port >= 1024 or {port}); app is optional.` |

Display/localize the consent description **before** accepting the action. Pass the fifth
`LinkBrowser(..., publish_url, true)` argument only for that confirmed request, using the same
captured player, cancellation and existing launch/storage/job guards. Cancel/focus/profile
changes must not leave a reusable confirmation. Browser consent is additional, not a replacement
for the frontend description. Do not tie confirmation to routing enable; that remains separate.

Exact coordinator-owned composition points (not modified by this delivery):

| Caller/surface | Required change |
| --- | --- |
| `headless/prosperoeden/pe/ui/nextendo.inc`, browser-link action | Present `GameLinkConsentMessage`; dispatch only after affirmative confirmation. Cancel dispatches nothing. Keep routing enable separate. |
| UI → `nextendo_action` service request | Carry that one-shot confirmation with the captured player/request through the existing guarded job. Do not infer confirmation merely from `Action::link_browser`, a stored preference or an old browser URL. |
| `headless/prosperoeden/nextendo_services.inc`, browser-link branch | Supply the confirmed request's value as the fifth `LinkBrowser(store, bound_profile, stop, publish_url, confirmed_game_permissions)` argument. Existing four-argument callers intentionally return `GameLinkConsentRequired` without network/listener. Keep the existing busy/launch/storage/profile/cancellation guards. |
| Same adapter, account-check branch | Call the game-only Store check without first loading/parsing OAuth registration; report `GameCheckUnavailable` without network/deletion. Preserve the Store's expired/cancelled outcomes. |
| UI localization/catalog composition | Add exactly the six msgids in the table above (four constants plus two literals); do not add secrets or server response text to translation/status payloads. |
| Coordinator composition fixtures (`headless/launcher_admission_check.cpp`) | Carry explicit confirmation in positive browser-link requests and use code→web→userinfo→NEX fixtures with matching PIDs. Keep unconfirmed/cancelled/busy requests inert and game-only checks non-networking; do not restore legacy nested-account success as the public OAuth contract. |

The unchanged service adapter currently loads `oauth.json` before its check callback. Parent
may surface the game-only unavailable result directly, without requiring a registration merely
to explain that no remote check exists. That wrapper's present missing-registration error is
not a network check and is not fixed by this worker editing another owner's integration file.

## Evidence and reproduction

- **Real negative baseline before edits:** saved under `baseline/` in private evidence. The
  unchanged `9cfebff` `BrowserLink::Finish` processed a synthetic HTTPS exchange returning the
  documented web fields, failed linking, and left `Store`'s prior account intact. This is not
  the earlier extracted-statement-only proof. Baseline OAuth source SHA-256:
  `bf1667db59563df483ac4c3740b37d98a02348ac36d73721fb09d589dc0ac35b`.
- The maintained `tools/check-nextendo-oauth.py` compiles the actual OAuth/core/store and
  shared HTTP implementation with ASan/UBSan. Its fixture verifies each request's class,
  order, exact two scopes, PKCE, identity/PID linkage, and no `/api/profile`/other API calls.
  Positive flows exercise `BrowserLink::Finish`, high-level consent, fixed/optional-app
  registration, v2 private storage, default-off and immutable snapshots. Negative cases cover
  replay/invalid callbacks, scope narrowing/expansion/duplicates, token/PID/type/TTL failures,
  malformed/duplicate/oversized bodies, wrong trust/hostname, redirects, header allocation,
  stage cancellation/expiry, combined deadlines, cancelled/expired fsync and original-file
  preservation. Former `profile-*` case names remain as compatibility aliases for zero-network
  game-only refusal; actual header-OOM coverage now lives at token/identity/NEX stages.
- Standard Nextendo (including actual service adapter guards and transport), shared HTTP,
  Amiibo and cheat regressions are run independently of provider availability. Amiibo TSan
  initially could not initialize under Docker seccomp; the repeat uses
  `--security-opt seccomp=unconfined` only in the network-disabled, capped test container.
  No sanitizer assertion is converted to a pass or removed. No shared source is weakened.
- Focused genuine SDK/PacBrew compilation uses the existing unmodified core qualifier and
  additionally compiles the actual OAuth, game-only check and unchanged shared HTTP C bodies.
  Partial linking checks the real resolver/CA producer, OpenSSL/curl and SDK libc++ symbols,
  not a mock API/header. Exact dependency hashes/commands and remaining imports are retained.

All test containers use `prosperoeden-host:features`, image
`sha256:e6137ec556fe8e63ebf25e7fabe36e573d089c469d0e9d391c3072f2ae04d551`,
`--cpus=2 --memory=3g --network=none`. The checkout is `/repo,readonly`, integration cache
volume is `/work` and `/root/.cache,readonly`, release resources are `/native,readonly` when
needed, and only `prosperoeden-team-pro-controller-20261027:/private` is writable scratch.
No install, configure/full emulator/native build or coordinator/archive write is performed.

To repeat the maintained functional tests in that container (with `CC=clang-18`,
`CXX=clang++-18`, and corresponding `HOST_CC/HOST_CXX` for shared HTTP):

```text
python3 /repo/tools/check-nextendo-oauth.py
python3 /repo/tools/check-nextendo.py --source /work/ps5-eden-headless.2YUJmY/source
python3 /repo/tools/check-remote.py
python3 /repo/tools/check-amiibo-sync.py
python3 /repo/tools/check-amiibo-file.py
python3 /repo/tools/check-cheat-download.py
python3 /repo/tools/check-cheat-store.py
python3 /repo/tools/check-cheat-manager.py
python3 /repo/tools/check-cheat-flow.py --build /work/ps5-eden-headless.2YUJmY/build --out <NEW_PRIVATE_OUTPUT>
```

### Final result and hash bindings

Final retained directories, under `/private/nextendo-oauth-nex-20261027/`:

| Evidence | Result |
| --- | --- |
| `baseline/` | Actual pre-fix Finish/Store negative with documented web response, expected refusal |
| `regressions-04/` | **9/9 checks PASS**, **111 OAuth/NEX cases**, no skipped checks; 417 source hashes reverified |
| `regressions-04/cheat-flow/` | 29 actual real-core probes, cross-layer TLS/binding/cancel/install checks PASS |
| `core-02/` | Six actual host + six SDK-target compilations and genuine native bridge/TLS partial links PASS |
| `oauth-sdk-02/` | Actual OAuth C++, check C++ and unchanged shared HTTP C target objects + native subset partial link PASS |

The final SDK subset has **343 unresolved core/platform imports**, exported as
`oauth-sdk-02/undefined.json`. The actual `remote_http_run`, isolated resolver/CA producer,
Nextendo, curl and selected OpenSSL imports resolve; checked Linux system-header and ABI
leak markers are absent. SDK clock/shared-pointer runtime is supplied by genuine libc++.
Neither relocatable partial link is an executable-link or module/runtime acceptance test.

`core-02` records 3,520 input hashes; the extra SDK proof records 1,043, verifies the core
manifest before reuse, and records exact compiler/link commands and compiler dependencies.
It reuses the qualifier's actual target commands, not host OpenSSL/curl headers. The genuine
read-only prefix is `/native/cache/ps5-eden-headless.6d6053a486f3/{sdk,cdeps}` (OpenSSL 3.5.2,
curl 8.18.0), with native saved nlohmann 3.12.0 and SDK 0.42.

| Binding | SHA-256 |
| --- | --- |
| Final regression receipt | `b94f1fa2cc264d64a4d0a79f656a28caf621090f4f07b4c214a0d2acafd098a1` |
| Final core input manifest | `a70f7fded0826c0d858b653de776922664f130030cb779272ffe3cbdcc298c75` |
| Final core provenance JSON | `2e3edbd59dd38ff75313ff43ae8f048491b107cf6944e2618f63d3832a4b2465` |
| Extra SDK provenance JSON | `f82d07152d0190e3dce5d5837e1cc7a4426984e5a124dcb6360f0f4a58b3e731` |
| Final cheat-flow input manifest | `2d010c9e3535d8335edd0d94e052aa10fc1ba13a95b8c59ecd1336811a609622` |
| Tested `nextendo.cpp` | `15a70e9904ef064b4f8d60ae638ce198c55d1a6d81c885043ca403ee5c65db11` |
| Tested `nextendo_oauth.cpp` | `c90e5d03a2cfa02fe2c3e2a1a37de8347b28ee3da20901da667f440b605cac59` |
| Tested `nextendo_http.cpp` | `b5018323546e7a6b4315247aca030b685c70a4c8f611c20ff9b7aadee150edf1` |
| Maintained OAuth fixture runner | `528e7b6031948018c18436035260d14fdf5875006013b667287fcc0bc581a6d1` |
| Actual target OAuth object | `bed2975d90299584ac51cdf716ab1847d739e6ad4bd050e8daa577fdacc828d2` |
| Extra native partial-link object | `08efc8a8fc579a74eb74d4dc3301dcc00744be03aa1d3b25658e8ba4bdbcdf47` |

The evidence bundle supplies the exact scratch baseline/regression/extra-SDK scripts, logs,
dependencies, source snapshots and manifest. Baseline requires the original `9cfebff` source;
its expected negative is not a regression command for the fixed source. No compiled objects,
archives, generated TLS private keys, account files or real credentials are exported. Earlier
iteration logs retain the original TSan initialization block. One foreground Docker client
timed out after all test commands passed; the completed `regressions-04/receipt.json` confirms
all return codes and final hash verification, rather than treating that client timeout as success.

Exact maintained core invocation inside the stated read-only mounts (fresh output required):

```text
python3 /repo/tools/check-nextendo-core.py --build /work/ps5-eden-headless.2YUJmY/build --out /private/nextendo-oauth-nex-20261027/core-02 --sdk /native/cache/ps5-eden-headless.6d6053a486f3/sdk --boost /private/pro-controller/boost/boost-1.92.0 --fmt /native/ProsperoEden/.deps/fmt-12.1.0 --json-include /native/cache/ps5-eden-headless.6d6053a486f3/source/.cache/cpm/nlohmann_json/v3.12.0/include --cdeps /native/cache/ps5-eden-headless.6d6053a486f3/cdeps
python3 /scripts/nextendo-nex-sdk.py
```

The second script, supplied in evidence, reads the first run's command/provenance and writes
`oauth-sdk-02`; change both scratch names when reproducing rather than overwriting evidence.

## Closed delivery — 2026-10-28

The scoped implementation and offline contract proof are complete. Apply these commits in order
on `9cfebff` (no push/publication was performed):

1. `0699208fabf270024a287d8231d164af98c79f5b` — production OAuth→web→identity→NEX and game-only lifecycle.
2. `6943832d3512287ded297dbcac0c02864a59cead` — 111-case fixture, regression coverage and consent/msgid contract.

The final component was rebuilt in `closure-build-01/` with the already-qualified genuine SDK
commands after checking the original core/SDK input hashes. All four generated objects are
byte-identical to the qualified outputs: OAuth C++, game-only check C++, unchanged shared HTTP C,
and their native relocatable partial link. The two ZIPs below are local deliverables under
`C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/`:

| Deliverable | SHA-256 |
| --- | --- |
| Evidence: `nextendo-oauth-nex-contract-20261027.zip` | `318f623cdf5a1b2e17c9e18bcf778ad7a0dfe4cb5c24452ac68c7bf4bb3f3d6c` |
| Build: `nextendo-oauth-nex-sdk-component-20261028.zip` | `0a02b489eae075387497ce13ae715ce0c3ee649539051cf2959ee59c7d26f9fe` |
| Build ZIP's `build-receipt.json` | `555401e6504b57d1c7217e17e30283763a190fa219d64ba544950844ed8551ef` |

The evidence ZIP contains 138 hash-verified payload files plus its manifest; the separate build
ZIP contains the four objects, exact rebuild script, commands/dependencies/logs and receipt.
The build ZIP is a **focused SDK component**, not a runnable app/package: the previously listed
343 core/platform imports remain. Full application/UI composition belongs to the coordinator;
neither it nor console/live qualification is required to close this worker's offline proof.
No further audit, real credential, provider contact, toolchain install or shared-file edit is
pending in this workstream. The caller/UI changes above are an explicit integration handoff,
not a claim that the unchanged shared frontend is already wired for the new confirmation.

## Public sources and remaining limits

[Official developer landing](https://nextendo.network/developers.html) links to
[official developers documentation](https://wiki.nextendo.network/developers). Its static
[renderer module](https://wiki.nextendo.network/assets/js/pages/developers-docs.js) was retained
in the preceding review (UTF-8 text hash
`9f15d6860dcd9cddafd64345a11920b6b7563b3f88eb20f181d04dcd4bac782c`). Module line references:
public clients/PKCE **199, 595**; loopback **727**; web exchange/userinfo **2268–2280,
2350–2351**; scope table **2377–2435**; prefixed endpoints **1828–1831, 2453, 2474**;
web-vs-game separation and NEX acquisition **1581, 1593, 1605**; refresh rotation **1641**.
RFC 6749 **§5.1, §7.1** defines omitted scope and case-insensitive token types.

The public guide is mutable, not a complete versioned OpenAPI contract. In particular this
implementation deliberately requires a root unsigned `pid` from userinfo, Bearer token type,
the documented lifetime limits and `nx2.` game format; alternate shapes or legacy extra fields
are not live-qualified by these tests. A real registered public app, approved callback/scopes,
same-device browser and provider consent remain prerequisites. No fixture proves that the
live server accepts this registration, that PS5 has browser/certificate/file access, that
BAAS/NEX game authentication works, or that the whole app links or runs on hardware.
