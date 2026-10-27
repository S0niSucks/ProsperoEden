# Cheat database manager — implementation record

Target: the complete **1.006.000** integration, issue #74. Ingestion, effective-main lookup,
cancellable transport, cache, safe installation and launcher jobs/actions are implemented.
The new launcher wiring still requires its preview/native regression runs and PS5 qualification.
No third-party cheat codes are shipped or installed by the tests; their on-disk codes are invented fixtures.

## Inspected source and matching contract

`blawar/titledb` at `d424690446eec6f4484209b9a1c067c7f7511398` declares MIT licensing.
The complete `cheats.json` was inspected earlier (12,012,925 bytes, Git blob
`f716e198710a3f0c3ef0b985e9f94ca8bd900fef`), alongside its README and license. No cheats
from that database are included in this repository. `HamletDuFromage/switch-cheats-db`
did not expose a license in the consulted path; absence is not permission to copy it.

The TitleDB format is a JSON object keyed by **16-digit program ID**, then **16-digit
build-ID prefix**, then 64-digit entry identifiers. Each entry has a bracketed `title`
and `source` opcode text. A build's `version` value is informational, never a matching
substitute. The format publishes the first eight main-NSO build-ID bytes, not all 32:
do not describe this as full 256-bit build authentication.

`headless/cheat_catalog.{h,cpp}` accepts the caller's resolved program ID and 32-byte
effective main build ID. It matches the exact program and that standard eight-byte
prefix; it never falls back to another title, an update label, a nearby build or the
first result. A zero/unknown identity is rejected. The **caller still must obtain the
actual updated/patched main identity**; the parser cannot establish it from a filename.

## Bounded, inert parsing

- SAX parsing keeps only one selected build, not the entire database DOM. Input is
  limited to 16 MiB, depth to 16, events to one million, selected entries to 512 and
  generated text to 1 MiB.
- Duplicate matching title/build keys and entry-ID case aliases are rejected. Repeated
  fields, names, aliases of the launcher's trimmed 60-byte switch key and multiple master
  blocks are rejected rather than selected arbitrarily.
- Header names fit the core's 63-byte name capacity. Source accepts whitespace-separated
  eight-hex-digit words only, up to the core's 256-word per-entry limit; embedded headers,
  comments or malformed code cannot inject additional entries into the output.
- Masters precede optional entries in the generated text. Syntax is not proof of safe
  semantics or compatibility: the real cheat parser/VM still decides what it can execute.
- A missing match can retain up to 32 **other build-prefix/version-integer labels for this
  exact program only**, plus a truncation flag. Their code is never stored, installed or used
  as fallback; the version integer is catalog metadata, not a claimed human release version.
- This module performs no HTTP, file writes, setting changes or VM updates. Parsing or
  previewing a source never turns on a cheat. Installation must not overwrite user cheats
  or implicitly enable newly downloaded code.

`tools/check-cheat-catalog.py` uses synthetic fixtures for program/build mismatch, unknown
identity, malformed/oversized inputs, duplicate keys, code/header/word limits and large
unrelated data. The real-core `eden-live-cheats-check` also feeds generated catalog text
to Eden's actual parser/VM before its existing bounded-memory write and mailbox tests.
Standalone parsing passed normal/ASan/UBSan, including a compatibility read of the full
pinned database (15 entries selected for one exact program/build; none installed). The
alias/zero-prefix cases and real-core parser/VM ingestion passed normal/ASan/UBSan/TSan
in `sh_17f4f5370001x4mnyQisFf6M3c` (the core library itself is not wholly instrumented).

## Effective main identity

`effective_exefs.h` is called by both the generated **actual boot loader** and
`ReadCheatIdentity`. Both load the base NPDM, ask the actual `PatchManager` to choose the
enabled update and layered ExeFS, then reload NPDM with the original program ID retained.
Only the selected `main` NSO header supplies the build ID; missing/truncated/unknown main
data is refused, not silently replaced by the base or last-played build. IPS/pchtxt code
changes can retain that ID: an ID match is not proof that cheats are safe with arbitrary mods.

The derived actual `ProgramMetadata::Load` bounds NPDM capability-array sizes/extent/alignment
before allocating or reading them. This covers early NSP title discovery as well as base and
replacement NPDMs selected by the shared resolver. It does not
claim to verify a whole executable's hashes, code validity or bootability.

`ProbeCheatIdentity` opens the current NSP/NSZ/XCI/XCZ and constructs the same filesystem
factories, manual-container content and external-update folders used for boot. It temporarily
sets the game's mod switches/local folder, disables ExeFS dumping and restores those settings
on exit. It never calls `Core::System::Initialize` or boots a guest. The launcher service
serializes it with metadata through its bridge mutex and stops it before launch.
Only the container's default program is currently addressed; NRO and unknown identities are
not matched to commercial cheats. Exact builds are reported to the diagnostic log, without
printing cheat opcode text or catalog credentials.

The real-core identity test uses invented keys and synthetic plaintext Program NCAs (not
commercial files or BKTR data) for six update insertion orders, plus replacement/disabled
main, retained program identity and malformed NPDM/main inputs. New cases cover complete
synthetic NSP/CNMT containers, external updates, extracted NSPs and restoration of the
temporary settings/paths, passed `sh_17f7317db00114Vy96zy3tBrm5`, including the 2 MiB
NPDM allocation-limit negative control. Full encrypted-container,
game-folder native-VFS and PS5 qualification are separate requirements.
The shared generated-loader check and real identity test passed in
`sh_17f4f5370001x4mnyQisFf6M3c`; the whole host suite must still be repeated after the
boot-resolver change. This synthetic test does not qualify complete encrypted-container probes.

## Explicit transport, cache and installation

- `DownloadCheatCatalog` is a blocking worker operation, never a startup task. The launcher
  explicitly requests it, owns/cancels/joins its job and passes its current resolved identity.
  The default source is TitleDB's HTTPS JSON URL; a caller can choose another HTTPS endpoint.
  Embedded credentials, control bytes, fragments and alternate protocols are refused.
- It reuses console HTTP/libcurl, with HTTPS-only redirects, normal certificate/host
  verification, a 60-second overall transfer deadline, a 16 MiB streamed-body limit and
  byte progress. Only a whole HTTP 200 body is eligible for parsing; cancellation, bad JSON,
  missing matches, truncated streams and network failures remain distinct from a ready catalog.
  Progress callbacks cannot unwind through C/libcurl. No disk writes or VM changes happen here.
- The optional app-owned cache stores one bounded (2 MiB) normalized record per exact source,
  program and **full** build identity. Reads recheck provenance, content checksum and the
  bounded catalog parser; disk JSON depth, events, duplicate fields and strings are limited
  before DOM construction. Offline use is explicit, never misrepresented as a fresh download.
  Failed writes preserve the previous record. The cache has no automatic eviction yet.
- Installation publishes only the compatible normalized text in a separate
  `mods/<program>/Downloaded cheats - <prefix> - <content-key>/cheats/<prefix>.txt` mod.
  Source, full resolved build and text SHA-256 remain in `provenance.json`. Source/code/build
  changes yield a different mod identity, not an overwrite of existing code or user selections.
  Master-only downloads are not installable through this API.
- Path traversal holds no-follow directory descriptors; cache/install reads refuse symlinks,
  FIFOs and oversized files. Temporary file writes are fsynced before publication. Final cheat
  publication uses an exclusive hard link, not a replacing rename: existing user files,
  symlinks or edited managed cheats are left intact. Unsupported filesystem operations fail
  closed; there is no unsafe copy/path fallback. Directory-entry/power-loss durability and
  those syscalls on PS5/USB still need console qualification.
- Immediately before publishing the `.txt`, the caller's serialized identity recheck must
  succeed and cancellation is checked again. Incomplete/cancelled jobs can leave bounded
  provenance/folders, but no new executable `.txt`. Operations are not a multi-file transaction.
  The caller must cancel/join before allowing a game to launch, and serialize UI settings with
  probe reads. Same-user concurrent edits of the app-owned cache are not a supported workflow.
- The reserved managed-mod namespace has a core launch invariant in `Mods::Switches`:
  **no selected optional entry means skip the entire mod, including masters**. Single-code
  downloads use the same explicit selection as multi-code downloads. New code is therefore
  off even if settings are absent, and a changed download cannot inherit a previous opt-in.
  Hand-copied mods retain their existing behavior. Enabling code is still a deliberate user
  action and can affect memory irreversibly; syntax/build matches do not prove safety.

`tools/check-cheat-download.py` passed normal/ASan/UBSan plus actual loopback libcurl/TLS
(`sh_17f54e811001iet0Dz6g6ayVGT`): trusted/untrusted/wrong-host certificates, redirected,
chunked, truncated, partial/error and oversized responses, downgrade/loop refusal and cancellation.
Its synthetic transport also checks no unsolicited request and callback-exception containment.
The complete loopback transport -> parsing -> bound offline cache -> managed installation
path also passed `sh_17f61016b001wH2i0jsEKRO9JW`; the resulting single-code mod was verified
off through `Mods::Switches`. Its identity and code are local fixtures, not a live game's cheats.
`tools/check-cheat-store.py` exercises real I/O, failure injection, preservation, path attacks,
identity/cancellation refusal, concurrent installers and default-off masters/single codes
normal/ASan/UBSan/TSan (`sh_17f580819001epOwoS3KVz6aqD`). Subsequent cache-depth/checksum
hardening, stricter managed-name recognition and tests also passed the subsequent standalone
normal/ASan/UBSan/TSan run `sh_17f5bf08d001XGdLYq3FE74We0`. These are not PS5 filesystem tests.

## Launcher workflow and lifetime

**Library > Game settings > Cheats** has explicit actions to check online, use an exact-build
offline cache, and install the checked catalog **off**. Installation requires a second Cross
confirmation, reopens the current container and rechecks the full effective build before final
publication. Installed optional codes then appear in the existing selection list; neither the
download nor installation changes the user's enabled selections or a running VM.
Before installation, the checked optional names are shown as read-only rows. On a miss,
read-only rows instead name the other catalog builds; they never offer an install action.
The complete name/other-build vectors and label truncation flag are also in the Lua status.

`CheatManager` owns one cancellable worker. Its status snapshot exposes phase, source, full
build, compatible optional count, byte progress, cache/offline provenance and errors. Requests
are bound to the selected relative file, absolute path, program, game folder, source and cache/
mods destinations. The actual service callback rechecks those paths/source and game existence
under the metadata mutex, also immediately before publication, not only at the button press.
A different request cannot install an old preview; failed/cancelled/missing
matches clear eligibility rather than reusing a previous result. Only an explicit cache action
loads offline data. A cache-save failure warns but can retain the checked session preview.

The service's actual `bridge_` mutex is shared with all metadata users. Probing holds it; HTTP
does not. Installation holds it through revalidation/publication; launcher setters that affect
settings/paths/profiles/mod selection use the same mutex. The launcher refuses launch while busy;
`will_play` and service destruction also cancel/join defensively. A synchronous core probe
cannot be interrupted mid-parser, so cancellation stays busy until that probe returns. Back
cancels an active request without leaving the page, making that wait visible. Background
library scanning can queue the job behind another bridge user.

The optional `cheat_catalog_url` string in **config/prosperoeden.json** selects the HTTPS
source. Omit it for the TitleDB default. Invalid explicit values fail, never silently fall back
to a different host. No automatic catalog request happens on app startup or entering Cheats.
Lua launchers expose `host.query("cheat_database")` and `cheat_database.check`, `.cache`,
`.install` (requires `confirmed=true`) and `.cancel` actions. Game arguments use a current
library handle, not arbitrary file paths, and new requests are refused during a pending launch.

Job regression `sh_17f704d5c001Up4anXAOKPhLTJ` passed normal/ASan/UBSan/TSan with real
cache/installer I/O and invented transport/identity. It covers stale full-build/source refusal,
no stale result reuse, offline requests with zero HTTP, progress, busy rejection, cancellation
while queued and during a probe, bridge exclusion, destructor join and concurrent snapshots.
This is not evidence of the console's libcurl/filesystem or a working commercial-game cheat.

UI snapshot `sh_17f7a0a7d001qjYujv5XPvxPKQ` passed 24 built-in/Lua controller checks
at 720p/1080p, with actual Mesa draw calls and ASan/UBSan (Mesa leak scanning excluded),
and the existing 23 workflow regressions. Screenshots/report/source snapshot are preserved in
`.local/docker-release/cheat-ui-actions-20261027/`. That run preceded the extra named-preview
and alternate-build rows. `sh_17f7ba158001446LHYgucJfgG3` also passed actual worker/libcurl
TLS, cache and default-off installer together; it precedes those status/parser extensions.

The named-preview/other-build snapshot subsequently passed **32** controller/API assertions
at both resolutions and the same 23 workflow regressions (`sh_17f828cf6001LjtWI29yRw9r5U`).
Captures/logs/report/source snapshot are preserved in
`.local/docker-release/cheat-ui-named-builds-20261027/`; 720p and 1080p captures were inspected.
The extended catalog checks passed normal/ASan/UBSan (`sh_17f828c05001rDl24ojpeBw9g2`),
and extended manager checks passed normal/ASan/UBSan/TSan (`sh_17f828c94001i4IcFpoXclPHJg`).

Native package `sh_17f76fd44001d1gWDxi93hcd4V` completed its translation/import/inventory
checks and was preserved in `.local/docker-release/cheat-launcher-native-translated-20261027/`.
It is an intermediate v1.005.000 package, not a final release or PS5 qualification. The host
run `sh_17f76fd180017gleRpy2dMvTFd` stopped at a probe-overload link error; inspection found
the new copied source paired with an old `cheat_probe.cpp.o`. Do not assign that native
incremental build coverage to newer probe/service edits. Concurrent legacy jobs sharing the
same build volume are a possible cause, not a confirmed compiler defect. Docker runners now
refuse a busy volume, verified against deliberately held locks for host/native/preview.
Port-dependent objects were explicitly invalidated for host/native recovery runs
`sh_17f8ee1c4001e29O13pI2US2mN` / `sh_17f8ee1ec001Zf9nzTqqubihCm`. The native run
subsequently **passed** all 738 build steps, source/link/import/inventory checks and packaging.
Its source snapshot precedes the independent encrypted-container fixtures and team work.
The host recovery compiled/linked all 648 steps (including the new probe overload), then stopped
at the existing guest-idle timing assertion `IdlePollBudget(2) == 0`; it is **not a full-suite pass**.
The same policy/wakeup/cost/lock check passed isolated with ASan/UBSan/TSan in
`sh_17fadd14e001azk7cH1JRTYcNZ`; scheduler interference is a possibility, not an established cause.
No runtime idle policy was changed to suppress that failure. Host binaries, compile database,
source snapshot, invalidation list and preceding test evidence are preserved in
`.local/docker-release/cheat-fresh-objects-host-20261027/`. The remaining saved-snapshot tests
**passed separately** without a core rebuild (`sh_17fb07c1d001nMrrr0D3Aw4b7y`), including
actual NFC/live-cheat/catalog factories under sanitizers, memory/JIT/device checks and three
NRO launch/exit cycles. RomM real-server tests were explicitly skipped. Logs and the final
device/NRO evidence are preserved in that same folder. This does not erase the original timing
failure or constitute a single uninterrupted full-suite pass. Native evidence, fixed runner, source
snapshot, package and matching symbols are preserved in
`.local/docker-release/cheat-fresh-objects-native-20261027/`. ZIP SHA-256:
`dac5e83ec81289a129d148530d28f863b79ff5ce6dd2fc3fc3b7cec7cbcd63d7`;
symbol ELF SHA-256: `5df516231a42f8688b824988fbf7db61945d1910314085880b655575c5e6cb62`.
This remains an intermediate v1.005.000 package, not the completed v1.006.000 or PS5 evidence.

The first per-copy local-mod probe failed because the host core was still using the upstream
BIS factory while the console uses `GameFolderMods::Root`. CMake now derives that identical
mod-root resolver for **both** builds; only the firmware-root constructor extension stays
native-only. The full host recovery run includes the per-copy check. Further encrypted
header/section/CNMT/update fixtures, written with independent OpenSSL and invented keys, were
added after its source snapshot and were checked separately. The first focused run
`sh_17fadd135001Y1d88z63azCGZO` reached those cases (including both per-copy mod checks), but
the wrong-header-key negative failed: this core's `KeyManager::SetKey` deliberately does not
replace an existing key. The fixture now uses real `prod.keys` reload and asserts the effective
key bytes before refusal checks. The corrected focused run
`sh_17fb0095b001poPxgTAYSXCTyP` **passed**: independent encrypted NSP/CNMT/base/update bytes,
actual wrong-header/area-key refusal, both per-copy native-VFS mod/update paths and the real
NPDM reader, plus the original-reader allocation-limit negative control. The real core archive
was mounted read-only and its hash verified unchanged. Input hashes/log are preserved in the
host evidence folder as `encrypted-probe-reload.{json,log}`. This was a test setup defect, not
evidence that the reader accepted a wrong key. No commercial keys were used.

The independent BKTR extension also completed successfully. Run
`sh_17fb6212f0018RCSZ6B5yYyp8P` exceeded the shell caller's 240-second deadline, but the
container completed and saved `bktr-probe.json` with exit code **0**, its source hash and
`bktr-probe.log`; these were read after the timed-out container exited, not inferred from
a partial console log. The tested fixture SHA-256 is
`24a3acd5a9d876cc1c35d78aecc074d3e2b60b82eba669756fd7f6241d5aa667`.
Integrated as `2d6b596`, it writes invented-key encrypted CTREx/indirect tables independently,
asserts missing-base status, reads a real base/patch merge byte-for-byte, then probes the
encrypted NSP/CNMT update's effective ExeFS. The payload is synthetic RomFS storage, not a
commercial filesystem or a gameplay result. This establishes these exercised BKTR paths,
not every malformed table or every real game's update layout.

## Remaining qualification

1. Encrypted/BKTR and game-folder native-VFS synthetic fixtures pass the focused runs above.
   Complete the combined real-container/real-TLS/manager cross-layer test and final integrated
   regression. Plaintext full
   NSP/CNMT/external-update/extracted-NSP fixtures passed isolated actual-core linking in
   `sh_17f6746d1001IAphtUmQ2rtAJ2`; the negative control subsequently passed in
   `sh_17f7317db00114Vy96zy3tBrm5`.
2. The idle test's late-entry race is now demonstrated and the test-only entry handshake
   fixed (`baefe49`); the integrated default ASan/UBSan/TSan/lock invocation passed
   `sh_17fc6ec0d001Uze9q247hdC5K2`. Exact original scheduling history remains unknown.
   Run the final uninterrupted combined suite after all features land; post-idle tests passed separately,
   fresh-object native packaging passed. UI built-in/Lua regressions have passed as recorded
   above; that does not qualify the actual service callback on PS5.
3. Verify the install filesystem operations and default-off invariant on the console. Apply
   no network results directly to a running cheat VM.
4. The 19 new messages are translated in all 30 catalogs (576 texts; no checker problems).
   Complete final release documentation only after the entire integration is ready. A real
   game/update/cheat combination and unchecked console-font glyphs still need PS5 tests.

Sources: [TitleDB source revision](https://github.com/blawar/titledb/tree/d424690446eec6f4484209b9a1c067c7f7511398),
[inspected JSON](https://github.com/blawar/titledb/blob/d424690446eec6f4484209b9a1c067c7f7511398/cheats.json),
[license](https://github.com/blawar/titledb/blob/d424690446eec6f4484209b9a1c067c7f7511398/LICENSE).
