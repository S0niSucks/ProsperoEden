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
3. Resolve the actual translation gate: `4a33dc9` and `2b9d869` found 764 source texts/30 catalogs
   with untranslated new entries. The catalog import was interrupted by a tool policy guard;
   it has not been retried, rerouted or waived. The older 659-text PASS is not current coverage.
4. Collect applied/generated sources, real objects/link argv and final binary identities;
   export/verify/restore the final source inputs offline and execute a real modified-library
   relink. Old source bundles and synthetic link fixtures do not satisfy this gate.
5. Repeat notices/privacy as UID/GID 65534 and retain exact upstream license bytes.
6. Perform physical PS5 retests: Galaxy cold/return sequences, #284 A/B, NFC/GPU/lifetime,
   firmware/rest mode, USB/filesystem/controller, real-server networking and long sessions.
   Live Nextendo also needs approved registration, provider contract, routes and trust.

Theme issues #173/#239 remain open. The independent IME review interrupted by a tool policy
guard remains unaccepted and is not replaced by a different review.

Snapshot-specific closure results: the 11 combined UI groups passed on `8090823`; 245
unprivileged notices/privacy/license CLI calls and source export/verify/offline restoration
passed on `2b9d869`. Native compilation reached pre-link capture refusal on `8090823`, not a
successful link/package. The host aggregate passed guest-idle with process-local no-ASLR then
stopped at an outdated recent-games fixture; its repair uses the production network predicate.
These results do not replace a complete run on the final freeze; see the team evidence ledger.

## Provenance and publication

[Changelog](CHANGELOG.md#v1006000), [integration matrix](UPSTREAM_INTEGRATION.md),
[team evidence](IMPLEMENTATION_TEAM.md) and [crash retests](CRASH_VALIDATION.md) preserve
the per-snapshot scopes. All existing original material/evidence is retained read-only;
new evidence uses fresh private destinations. Do not ship private negative-test archives.

Confirm the publication destination first: the user's requested Swordpdf repository and
the configured S0niSucks origin differ. ProsperoEden naming/identity and upstream licenses
remain unchanged; there is no TGS/Nightshade rebranding.
