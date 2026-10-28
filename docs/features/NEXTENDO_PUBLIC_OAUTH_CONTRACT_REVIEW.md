# Nextendo public OAuth contract review

Review date: 2026-10-27 local (snapshot receipt: 2026-10-28 01:18 UTC).
Reviewed source: `de3db59ea7b1b38407471b14ddacd95b384cbad4`, including integrated
`62e99ba` security changes. **Investigation only; no runtime changes.**

## Conclusions

- **No proven authorization-endpoint bug:** official documentation uses both
  `/oauth/authorize` and our current `/api/oauth/authorize`. It does not establish whether
  they are aliases, different entry points or stale examples. Neither endpoint was requested.
- **`app` is mandatory in our registration schema**, but is not listed as a required
  provider query parameter in the public endpoint reference. Do not invent its registered value.
- **`profil` is a documented, literal scope**, not evidence of a truncated `profile`.
  It grants profile editing, which is unnecessary for identity-only linking.
- **A separate, reproducible response-contract mismatch exists:** current code expects
  `account.pid` and `nex_token` directly from the OAuth exchange. Public docs describe a web
  access token, then a separate `/api/nex-token` request with `game.matchmaking`. The local
  response-conversion fragment rejects the documented shapes offline. This does not prove
  that the live server lacks an undocumented legacy response.
- A public client ID, a bot credential, an OAuth access token and a game NEX token have
  different purposes. None substitutes for the others. A supplied link proves neither
  registration approval nor consent, a successful exchange, or PS5 online operation.

## Official public sources

Only anonymous static documentation was fetched, with redirects disabled and without
executing scripts. No authorization, token, userinfo, NEX, session or sign-in route was opened.

1. [Nextendo Developers](https://nextendo.network/developers.html): HTML lines **31, 79–80**
   link to the official wiki; line **127** describes public desktop/mobile applications with
   PKCE and no client secret. Merely reading this page does not access the developer account.
2. [NexWiki developers](https://wiki.nextendo.network/developers), whose route renders
   [the public documentation module](https://wiki.nextendo.network/assets/js/pages/developers-docs.js).
   All **D:** line references below refer to that module's 2,783-line captured text, including
   its English dictionary and rendered examples. Unused translation strings alone were not
   treated as supported scope names.

The documentation is mutable and has no reviewed version/commit identifier. A UTF-8 snapshot
restored from the tool-delivered line-numbered response is retained outside git; its SHA-256 is
`9f15d6860dcd9cddafd64345a11920b6b7563b3f88eb20f181d04dcd4bac782c`.
This hashes the retained text, not an independently captured HTTP wire body.

## Contract comparison and source locations

| Topic | Official documentation | Versioned implementation / conclusion |
| --- | --- | --- |
| Authorization path | **D:1828–1831, 2453** use `/api/oauth/authorize`; **D:2257, 2340** use `/oauth/authorize`. | `headless/nextendo_oauth.cpp:269–271` uses the documented `/api/` form. A path-only correction is not justified by these conflicting examples. |
| Exchange path | **D:1830–1831, 2474** use `/api/oauth/token`; **D:2268, 2350** use `/oauth/token`. | `headless/nextendo_oauth.cpp:314–321` uses `/api/oauth/token`. Alias/canonical-path behavior remains untested for both routes. |
| Public client / PKCE | **D:199, 595, 1243–1255**: public apps have no client secret; S256 challenge required. | `headless/nextendo_oauth.cpp:248–250, 269–271, 314–315` generates private verifier/state and sends the matching challenge/exchange verifier. A literal `YOUR_CODE_CHALLENGE` cannot stand in for this per-attempt proof. |
| Registration schema | Provider authorization parameters **D:2460–2465** list client ID, response type, redirect, challenge/method and scope, but no `app`. | `headless/nextendo_oauth.cpp:208–221` requires version 1 and string `client_id`, `redirect_uri`, `app`; **:34–41** rejects an absent/empty/invalid app. `headless/nextendo_oauth.h:7–10` defines the fields. Existing local instructions require the registered application name: `docs/features/NEXTENDO.md:79–86`. This is a local/legacy contract, not proof of an official mandatory `app` query parameter. |
| Fixed loopback | **D:727** documents registering `http://127.0.0.1/callback` and accepting dynamic ports; **D:1231, 1339** require the approved callback and same redirect at exchange. | `headless/nextendo_oauth.cpp:38–58, 258–268` accepts the exact `http://127.0.0.1:47823/callback` spelling structurally. Explicit fixed ports are not silently changed if occupied. Our `{port}` placeholder is a local template expanded before the request, not evidence that the provider accepts a literal `{port}` registration. |
| OAuth response | **D:1291** describes a Bearer access token; **D:2276–2280** uses `access_token` for `/api/oauth/userinfo`; **D:2351** names `access_token`, `refresh_token`, `expires_in`. | `headless/nextendo_oauth.cpp:335–340` requires nested `account.pid` plus `nex_token`, otherwise reports an incomplete/invalid session. Existing synthetic provider success fixtures return precisely that legacy shape: `tools/check-nextendo-oauth.py:125–126`; their pass is not provider-schema evidence. |
| Game credentials | **D:1581, 1593, 1605** separates web access from game NEX: request `game.matchmaking`, GET `/api/nex-token` using web access, receive flat `pid`/`nex_token` plus metadata/expiry; game token lasts 24 hours. | Current browser flow requests only `identity` (**:270**) and proceeds straight from exchange to parsing (**:314–340**). No separate NEX acquisition occurs in that flow. Additional compatibility concern: `headless/nextendo_http.cpp:28–30` sends the stored game-session value as Bearer to `/api/profile`, whereas **D:1581, 1605** say game credentials do not open account APIs. This latter concern was not exercised. |

The source header attributes the original wire contract to pinned Citron-Nextendo
(`headless/nextendo_oauth.cpp:2`). That attribution is not a fresh inspection of the reference
repository or proof of its live compatibility. No extra upstream code was fetched.

## Minimum scopes and credential classes

The rendered scope table **D:2377–2435** lists exactly `identity`, `friends`, `presence`,
`game.matchmaking`, `mods.favorites`, and **`profil`**. Its English descriptions are at
**D:907–943, 979, 1051, 1087, 367**; identity is marked required (**D:931**), the others optional.

- **Account identity only:** explicitly request `identity`. The profile endpoint description
  requires identity/consent (**D:1351**). The scope *parameter* is listed optional at
  **D:2465**, but no default sufficient for our use was proven; keep the minimum explicit.
- **Documented game/NEX acquisition:** `identity game.matchmaking` is the narrow documented
  candidate, subject to app approval. `identity` alone does not establish entitlement to a
  game credential. This is not a recommendation to broaden scopes silently in runtime.
- **Friends/presence:** request only if those features are implemented and intentionally
  approved. Neither is required for the account identity-only request our code currently makes.
- **`profil`:** literal table entry **D:2430**, described as reading/editing display name,
  avatar and country (**D:367**). Do not auto-correct it to `profile`, and do not request it
  merely to read base identity. `mods.favorites` is likewise unrelated to this linking task.
- An unspecified broader list cannot be audited exhaustively from its summary. The docs also
  reserve cloud saves/account management/console linking to official Nextendo applications
  (**D:259**); marketing text or unused translation entries do not grant third-party access.

The client ID is a **public application identifier** (**D:1195**), not an authorization header
or a secret. Dedicated bot credentials are for backend/bot automation (**D:1447, 1495**),
remain subject to player consent (**D:1471**), and can be reset to disable the prior credential
(**D:271**). They are not a client ID, a public-client PKCE secret, an OAuth user grant or a
game session. No private credential was read, searched for, requested, used or recorded here.
The earlier recommendation to revoke an exposed credential remains appropriate.

## Offline reproduction and limits

Final private probe: `/private/nextendo-public-oauth-review-20261027/probe-02/`.
It compiled actual `ParseRegistration`, actual `ParseAccount` and the **exact extracted
statement at `nextendo_oauth.cpp:337`**, with the actual `nextendo_json.h` parser. It included
the unmodified OAuth source and discarded unused network functions at link time; no HTTP
producer or API/type stub was introduced. Only invented, local test data was used.

Clang 18, host, ASan/UBSan results:

1. Fixed `127.0.0.1:47823/callback` + synthetic nonempty app: registration parser accepts.
2. Missing or empty app: registration parser rejects.
3. An OAuth response shaped as the documented access/refresh response lacks `account`:
   extracted conversion raises `nlohmann::json::out_of_range.403`.
4. A separate flat NEX-shaped response also lacks `account`: same result. Simply switching
   the exchange URL to the NEX endpoint would not repair the flow or parser.
5. Legacy nested-account synthetic control: accepted by the same conversion/ParseAccount.

This is a **response-fragment/schema proof**, not an end-to-end `BrowserLink::Finish` test.
No `BrowserLink` was opened, no listener created, no request sent and no login performed.
Thus it reproduces incompatibility **with the documented shapes**, not a live HTTP error,
provider rejection, successful registration, working callback/browser on PS5, BAAS acceptance,
or online gameplay. No shared runtime fix is included or authorized by this report.

The first scratch probe tried linking the unused HTTP implementation and stopped at its
unprovided `console_curl_setup` contract. Final probe-02 instead excludes those unused
sections; no replacement producer was added. Both attempts remained offline.

Retained evidence directory on Windows:
`C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/nextendo-public-oauth-review-20261027/`.
It contains the public documentation snapshot/receipt, `probe-offline.py`, generated source,
exact compiler command and sanitized logs. No executable or private credential is exported.

| Binding | SHA-256 |
| --- | --- |
| Reviewed `headless/nextendo_oauth.cpp` | `bf1667db59563df483ac4c3740b37d98a02348ac36d73721fb09d589dc0ac35b` |
| Reviewed `headless/nextendo_json.h` | `692b22b5cbe8192543c397a204d86a71a10a1341396ba725cce8e1fb4eb38a23` |
| Scratch reproduction script | `2e496a958080ab3ec16c2e9af7c97619589299c8d5ff69254fb9247d700324d8` |
| Generated fragment probe | `b3b9a9908c38acfb1fe5c2037eeba76f070960027c83f54ed50d42f669f2d776` |
| Final probe receipt | `cc34dc0c666d36a0d4e20582a6cde3662723b1559873c8c4c609f340d6db6826` |

Exact invocation (a fresh private output directory is required for another run):

```powershell
docker run --rm --cpus=2 --memory=3g --network=none `
  --mount 'type=bind,src=D:/leonardommello/PS5/ProsperoEden/.local/agent-worktrees/pro-controller,dst=/repo,readonly' `
  --mount 'type=bind,src=C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/nextendo-public-oauth-review-20261027,dst=/review,readonly' `
  --mount 'type=volume,src=prosperoeden-team-pro-controller-20261027,dst=/private' `
  --entrypoint python3 prosperoeden-host:features -u /review/probe-offline.py
```

Image ID: `sha256:e6137ec556fe8e63ebf25e7fabe36e573d089c469d0e9d391c3072f2ae04d551`.
No coordinator/archive volume was mounted for the probe; no toolchain was installed.

## Still needed before a usable registration/flow can be claimed

1. Owner confirmation of the public app registration corresponding to the supplied identifier:
   public-client type, active/published or authorized tester status, allowed scopes and approved
   exact redirect (or documented dynamic-port policy). A public ID's spelling is not that proof.
   The public portal documentation itself describes restricted team access/tester roles
   (**D:235**); no account/portal permissions were checked here.
2. The nonempty registered `app` value required by our current schema, and clarification of its
   mapping/legacy requirement because the public endpoint reference omits it. Do not borrow an
   upstream application's name/ID or hard-code the supplied identifier.
3. Per-attempt `response_type=code`, S256 challenge derived from a fresh retained verifier,
   verified random `state`, and the identical redirect at exchange. Our code generates the
   dynamic proof/state; they should not be supplied as static config, sent for review, or replaced
   with a placeholder. Callback loopback is the browser's own machine, not a phone-to-PS5 link.
4. Agreement by the OAuth/HTTP owners on the canonical endpoints and current response/NEX
   contract, then owner-scoped implementation/tests if needed. An extra `game.matchmaking`
   scope alone cannot fix the missing acquisition step or the response-shape mismatch.

No real `oauth.json` was written. No missing credential is requested. Only this new report is
committed; EdenServices, USB, HTTP, OAuth, store, overlays and other owners' work remain untouched.
