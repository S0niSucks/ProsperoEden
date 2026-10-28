# Nextendo OAuth security review (2026-10-27)

Base: immutable `30724082bcca4824d64bcebfc84e2891493a1686`.
Branch: `team/nextendo-oauth-security-1006000`, in the assigned exclusive worktree.
Scope: `nextendo_oauth.{cpp,h}`, `nextendo.{cpp,h}`, `nextendo_http.cpp`, OAuth-related
`remote/http.c`, their focused tests and one private JSON helper. No service, UI, CMake,
translation or aggregate-document changes. Service admission/lifetime/USB integration belongs
to the coordinator; the existing synthetic adapter check is not integrated lifetime evidence.

## Wire-contract evidence and trust boundary

Reviewed the feature's recorded public evidence in `NEXTENDO.md:35-64,71-116,233-292`:
GPL Citron-Nextendo `8e76192472a3993a8f8fa6835e7ed061d443a9f7`,
`web_service/nextendo_api.cpp:277-284,357-485`. The recorded contract is browser authorization
code + S256 PKCE, loopback redirect, registered client/app, identity scope, form token POST,
`X-Nextendo-Client-Id`, and `account.pid`/`nex_token` output. No guessed device-code/refresh
endpoint, borrowed client identity, provider password, BAAS claim schema or TLS bypass is added.
This review did not independently qualify a public deployment or its operator registration.
No operator was contacted and no real credentials were used.

All actual network tests ran with Docker **`--network none`**, using loopback HTTP and private
generated-certificate TLS. The test platform adapter changes only curl's destination socket/CA;
production HTTPS URL and TLS peer hostname remain `nextendo.network`. Browser authorization,
token POST and profile GET otherwise use the actual parsers/transport/store.

## Established defects and minimal fixes

### 1. Header allocation failure could erase a good session

`headers = curl_slist_append(headers, ...)` at the Accept/Content-Type construction sites
replaced an already populated list with NULL on allocation failure, leaking it and discarding
Authorization/client headers. A subsequent successful client-header append allowed the request
to proceed with an incomplete header set. For a profile check this meant an unauthenticated
GET could receive 401 and cause `Store::Apply` to delete the existing private account.

The pre-fix `profile-header-oom` regression deterministically failed its previous-account
preservation assertion: the synthetic TLS provider returned 401 for the missing bearer.
This is a real resource-failure/session-loss bug, **not** evidence that a network attacker can
arbitrarily forge a verified provider response or read the dropped token.

`remote/http.c` now preserves the existing list on failed append, frees it and fails before
`curl_easy_perform`. Accept, API-version, Content-Type and multipart Expect construction use
the checked helper; the already checked Authorization/client paths remain checked. Existing
default redirect behavior for non-Nextendo users is unchanged. Tests inject failures at the
actual Authorization, Accept, registered-client and token Content-Type appends and assert that
**no request** reaches the provider and no previous account is changed.

### 2. Duplicate JSON keys silently selected identity/consent fields

Default DOM parsing used last-key-wins behavior for registration, imported/saved sessions,
routes, token replies and profile replies. Conflicting repeated `account`/`pid`/`nex_token`
fields could therefore be accepted as an unambiguous new session; repeated `enabled` fields
could choose routing consent when loading a saved account. Existing text-session and callback
parsers already rejected duplicate fields, but JSON parsing discarded this evidence first.

The pre-fix `duplicate-token` real-TLS regression accepted the malformed reply and failed its
failed-replacement/previous-session preservation assertion. This establishes a fail-closed
validation gap at the credential boundary. It does **not** establish account takeover: token
responses still require the trusted TLS provider and local inputs still require private storage.

The private `nextendo_json.h` callback records decoded keys per object and rejects the whole
document on any duplicate, including escaped aliases and nested account fields, before field
selection. Repeated names in separate objects/array elements remain valid. The five parsing
sites use this helper; field validation, input limits and generic secret-free errors are retained.
Regression coverage includes token/account/PID, profile, registration, routes, saved consent
and import duplicates, untouched previous accounts and refusal to activate a malformed session.

## Reviewed behavior not misreported as a vulnerability

- **Receiver:** loopback-only bind; nonblocking accepted sockets; 8192-byte headers, two-second
  per-client deadline and 64-request cap; exact Host, restricted optional Origin, GET/HTTP 1.1,
  no request body/transfer encoding, unique decoded query fields, constant-time state comparison,
  one-use listener and code exchange. Added hostile-method/framing/duplicate-Host/escaped-query
  tests all pass. A same-user malicious native process is not isolated by loopback/state alone.
- **PKCE/transport:** independent random 256-bit verifier/state, S256 (RFC 7636 vector), exact
  registered redirect, fixed HTTPS token/profile origins and no redirect following. Unknown CA
  and wrong-host certificates fail. Registration parsing disallows header-control characters;
  account parsing rejects nonprintable/header-injection tokens. No demonstrated wire/log leak.
- **Cancel/save linearization:** `Store::Link` checks cancellation under the store mutex before
  entering storage preparation; import checks again before Save. A cancellation already present
  at that admission point prevents replacement. The documented contract explicitly allows a
  save already in progress to finish; it does not promise atomic global cancellation/rollback.
  `cancel-before-save` preserves the old enabled session; `cancel-in-save` sets cancellation at
  the real staging-file fsync and verifies a complete, readable, routing-off new account and no
  leftover staging file. The wrapper calls the real fsync/interceptor, not a libc-bypassing syscall.
  No new cancellation API, save rollback or service-lifetime change was justified here.
- **Expiry:** listener and token transfer share a steady-clock deadline; delayed token replies
  fail without saving. Bounded parsing/storage is not a hard real-time scheduler or interruptible
  filesystem operation. The store is not given a browser deadline. No hard filesystem completion
  deadline or forced rollback after an admitted save is claimed.
- **Storage:** root must be app-owned; descriptor-relative no-follow ancestor/file opens,
  owner/type/link/mode/size checks and exclusive 0600 staging protect the intended boundary.
  Rename is atomic, but Save syncs the file, **not the containing directory**: this review does
  not qualify power-loss durability. Privileged/same-UID compromise and secure flash erasure are
  outside the mode-check guarantee.
- **Memory/privacy:** raw reply, verifier, code and form have best-effort wiping; DOM construction,
  the existing token-to-session serialize/reparse step, curl and allocator copies are **not**
  guaranteed erased. This was already explicitly disclosed, not a demonstrated diagnostic leak.
  Errors do not include provider bodies, identifiers, tokens, codes, verifier or full auth URL.

## Actual checks and reproducibility

Private image `prosperoeden-host:features`; container/volume
`prosperoeden-team-nextendo-oauth-security-20261027`; `--cpus=2 --memory=2g --pids-limit=128
--network none`; assigned worktree mounted read-only at `/repo`, private volume at `/work`.
No coordinator cache, full emulator/native build, WSL install, live service or second agent.

Inside that container:

```sh
TMPDIR=/work CXX=clang++-18 CC=clang-18 ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 python3 -B /repo/tools/check-nextendo-oauth.py
TMPDIR=/work CXX=clang++-18 CC=clang-18 ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 python3 -B /repo/tools/check-nextendo.py
```

- OAuth runner: **29 cases, exit 0**, actual loopback browser/HTTPS/token/profile/store under
  ASan/UBSan with leak detection, compiled `-Wall -Wextra -Werror`. Includes both fixed negative
  regressions, four header-allocation fault sites, TLS refusal, expiry during token exchange,
  cancellation before/during save, one-use consumption and previous-session preservation.
- Existing focused runner: **exit 0**. ASan/UBSan storage/parsing and synthetic service adapter,
  in-memory trusted/wrong-host/unknown/expired TLS checks, actual curl full-length bearer and
  redirect compatibility, synthetic native resolver/CA-adapter checks. This runner's transport
  and adapter-only binaries are not sanitizer builds; its primary storage/service binaries are.
  No `--source` was provided: upstream derivative guards and native core compilation were not run.
- Baseline negatives: exit 1 on the two preservation assertions, before the production fixes;
  no sanitizer bookkeeping bypass was used in either injector.
- Logs in the private volume: `/work/baseline-duplicate-token.log`,
  `/work/baseline-profile-header-oom.log`, `/work/oauth-security.log`,
  `/work/nextendo-security-storage.log`.

PS5 socket/nonblocking/permission behavior, same-device browser suspension, native curl/CA
integration, rest/power-loss/removal handling and the coordinator's composed service admission,
teardown/profile/USB writers remain unqualified. No public operator compatibility, BAAS/game
login, full build or hardware success is inferred from these host tests.

## Coordinator integration repeat

Imported as `d96e8cc`. A synchronously frozen clean 824-file snapshot passed all 29 actual
loopback/HTTPS OAuth cases and the existing checked-derivative/storage/TLS/service-adapter
runner, plus the shared real cheat-download/TLS and Amiibo worker/reader/TLS regressions.
Docker run `sh_17fe4bfea001szCLUFwW9EwKgG`, evidence
`.local/docker-release/nextendo-security-integrated-20261027/` (all four step exit codes 0).
The run used no external network, genuine libcurl/OpenSSL loopback TLS and ASan/UBSan leak
checks; Amiibo's normal/TSan/ASan/UBSan variants passed with the documented test-only seccomp
exception. It predates the later storage-gate and SDK snapshot compatibility commits and is
not a native full-build or PS5/operator-interoperability result.
