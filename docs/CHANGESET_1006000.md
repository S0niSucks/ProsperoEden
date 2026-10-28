# ProsperoEden 1.006.000 integration changeset

**Unreleased candidate.** The metadata version identifies the intended build, not a PASS,
complete issue closure, successful source/relink delivery or PS5 qualification. No tag,
push or publication is authorized. The final source snapshot/ELF/ZIP hashes belong in the
generated build-identity receipt, not invented in this changeset before the build.

## Included code

- The 17 open upstream PR heads and selected follow-ups, retaining commit ancestry/credits.
- Independent game/update storage, bounded NRO metadata and setup distinctions.
- File-backed NFC, local Amiibo names/photos and explicit offline-first catalog/photo sync.
- Session/build-bound live cheats and separately confirmed default-off database installation.
- Opt-in Cover Flow; Grid remains the default and perspective image modes 9/10 are retained.
- Experimental wired Pro Controller support; Bluetooth is not implemented.
- USB app-data backup, exclusion of named credentials and cold-start restore/recovery.
- Read-only SMB/NFS VFS and a native private Settings editor, not a JSON-only workflow.
- Complete current network/Nextendo catalog additions across the existing 30 locales and a
  regenerated 764-text template, with named-token and native prompt-length regressions.
- Native keyboard buffers retained through failed system termination, safe cleanup/reopen,
  buffer clearing after release and cancellation preserved across late completion.
- Default-off Nextendo with request-bound explicit consent, PKCE/state and separate game
  credentials; no web/refresh token persistence or unregistered live sign-in claim.
- Retained scenes, bounded theme import and app-owned process storage credits. Full NXTheme
  parity is not implemented; unknown PS5 capacity denies import/recovery writes.
- Paged 4×4 Settings with the original first-page placement and reachable network category.
- Crash/build identity, privacy/notices checks, pinned offline source restoration and opt-in
  producer/link evidence capture. These mechanisms alone do not establish source closure.

## Acceptance still required

1. Freeze one clean Git HEAD/tree after reciprocal network/USB/theme admission composition and
   its fixture integration. Guards and `3a399d7` fixtures are integrated with focused sanitizer
   proof; complete application qualification is still pending.
2. Run the complete host suite, all-feature UI and native build on that same immutable snapshot,
   invalidating launcher-dependent cache objects. No running or partial test counts as PASS.
3. Confirm the translation gate on the new final freeze. Earlier native runs found 764 source
   texts/30 failing catalogs; the older 659-text PASS was not current coverage. The current
   corrections add 106 translations per locale, remove one obsolete entry per locale and pass
   the original checker (764 texts, 30 catalogs) without reducing its coverage. Focused checks
   preserve existing translations, named/numeric tokens and all 16 native prompt limits.
   This working-source result is not a complete host/native aggregate or console font check.
4. Collect applied/generated sources, real objects/link argv and final binary identities;
   export/verify/restore the final source inputs offline and execute a real modified-library
   relink. Old source bundles and synthetic link fixtures do not satisfy this gate.
5. Repeat notices/privacy as UID/GID 65534 and retain exact upstream license bytes.
6. Perform physical PS5 retests: Galaxy cold/return sequences, #284 A/B, NFC/GPU/lifetime,
   firmware/rest mode, USB/filesystem/controller, real-server networking and long sessions.
   Live Nextendo also needs approved registration, provider contract, routes and trust.

Theme issues #173/#239 remain open. Historical interrupted tasks have no accepted result.
The new native keyboard lifetime correction received a separate review of its actual diff:
that review found a cancellation regression, which was repaired and reproduced as passing
with the reviewer's fixture in normal and ASan/UBSan builds. A negative control reproduces
the old adapter's heap-use-after-free when system termination times out. Native/full-app and
physical-console acceptance must still use the new frozen snapshot; no old review is relabelled.

Snapshot-specific closure results on `2bdedef`: 11 combined UI groups and 245 unprivileged
notices/privacy/license CLI calls passed, as did source export/verify/offline restoration.
The actual captured native link succeeded, but the native aggregate failed the mandatory
catalog gate (764 texts, 30 catalogs); no package was produced. The host aggregate passed
guest-idle with process-local no-ASLR and Amiibo/cheat regressions, then stopped at a Nextendo
service fixture missing the composed theme/network members. Runtime guard policies were not
weakened to obtain a result. The test-only repair `862c78b` uses the real ledger and passed
the complete Nextendo checker plus normal/ASan/UBSan/TSan fixture runs. These snapshot-specific
results do not replace a complete run
on the final freeze or binary-bound source/relink evidence; see the team evidence ledger.

Later test-only fixes complete the network configure/link fixtures, container service
dependencies and Ganesha MEM cleanup. Their focused proofs do not qualify physical network
storage or remove the catalog/IME-review blocks. No final package has been accepted, and
the earlier input-source export is not a final binary-bound corresponding-source delivery.

## Provenance and publication

[Changelog](CHANGELOG.md#v1006000), [integration matrix](UPSTREAM_INTEGRATION.md),
[team evidence](IMPLEMENTATION_TEAM.md) and [crash retests](CRASH_VALIDATION.md) preserve
the per-snapshot scopes. All existing original material/evidence is retained read-only;
new evidence uses fresh private destinations. Do not ship private negative-test archives.

Confirm the publication destination first: the user's requested Swordpdf repository and
the configured S0niSucks origin differ. ProsperoEden naming/identity and upstream licenses
remain unchanged; there is no TGS/Nightshade rebranding.
