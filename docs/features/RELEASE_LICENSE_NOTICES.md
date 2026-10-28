# Release license notices: bounded source verification

## Scope and status

Prepared from ProsperoEden `931522e43045d812a42527e157b191e18395c57b` on
2026-10-27. Changes are limited to `THIRD_PARTY_NOTICES.md`,
`third_party/licenses/` and this record. ProsperoEden remains GPL-3.0-or-later.
This is source/notice evidence, **not legal certification, an exhaustive dependency
audit, or a claim that a release package meets every license obligation**.
No application build, package build or final binary inventory was performed.

## Verified inputs and license boundaries

### Static network clients

The existing archives in the network worker volume
`prosperoeden-team-network-vfs-20261027`, mounted read-only, were hashed before
reading individual members. Both exactly match `tools/deps.json` at the baseline:

| Component | Source archive | SHA-256 |
|---|---|---|
| libsmb2 | `https://codeload.github.com/sahlberg/libsmb2/tar.gz/refs/tags/libsmb2-6.3` | `5011003fd091513f897e1ebbb8e1a40af634386df7199716348b7f77569a2790` |
| libnfs | `https://codeload.github.com/sahlberg/libnfs/tar.gz/refs/tags/libnfs-8.0.0` | `bc91216e927a85142b5de611c7f711558e02119a7daad67620ea631634038350` |

No dependency archive, toolchain or package was downloaded. The retained
`COPYING` and `LICENCE-*.txt` files are **byte-for-byte archive members**, including
upstream spelling/formatting. They are not reconstructed license templates.

- libsmb2's `COPYING` licenses its library under LGPL-2.1-or-later, separately
  from its BSD example programs. The library also contains the **RSA Data
  Security, Inc. MD4 Message-Digest Algorithm** and public-domain components.
  Its actual MD4 identification/derivative-attribution conditions, both original
  copyright notices, and the other source notices are preserved verbatim.
- libnfs's `COPYING` separates LGPL-2.1-or-later library code from simplified BSD
  `.x`/generated protocol files. The standalone BSD text has no copyright line;
  `SOURCE_NOTICES.txt` therefore also retains the real protocol copyright headers,
  conditions and disclaimer. The Internet Society's NFSv4 copyright is retained
  too; it is not silently replaced with Ronnie Sahlberg's BSD attribution.
- `headless/network/CMakeLists.txt` builds static `smb2`/`nfs` libraries, disables
  examples/tests/utilities/shared libraries, Kerberos/GSSAPI, TLS and NFSv4.2.
  NFSv4 code still belongs to the library even though the app selects v3.
  The source tag remains `libnfs-8.0.0`; a different version printed by its CMake
  metadata is not a different dependency pin.
- The new notice directory does not include libnfs's example-only
  `LICENCE-GPL-3.txt`. Complete upstream source archives must still retain it;
  this is not permission to strip notices when supplying corresponding source.

Network documentation was consulted with `git show 20ea7c6:docs/NETWORK_SHARES.md`:
that file is absent at the requested baseline, and was not imported or edited.
The exact document revision/blob/hash is recorded in `SOURCES.json`.

### Adapted Citron-Nextendo code

Read-only public HTTPS queries addressed
`NextendoNetwork/citron-nextendo` at
`8e76192472a3993a8f8fa6835e7ed061d443a9f7`, not a moving branch. Git blob IDs were
verified against the returned file bytes. Retained files are its actual root
`LICENSE` (GPLv3) and `LICENSES/GPL-2.0-or-later.txt`, plus verbatim SPDX headers.
The latter license file is reformatted upstream; it was not substituted with a
different generic GPLv2 copy.

The account/API reference headers name `Copyright 2026 citron Emulator Project`
and GPL-2.0-or-later. DNS/account-service references name yuzu (2018) and citron
(2025) under GPL-2.0-or-later. The later-version option allows the compatible
GPL-3.0-or-later adaptation; the root README declares GPL-3.0-or-later. The exact
scope of adaptation/rejected behavior remains in
[NEXTENDO.md](NEXTENDO.md#source-and-license-provenance), not a claim that the
whole reference tree has been reviewed or redistributed.

The preserved account-service header credits inspected context; it does **not**
mean that the rejected BAAS signing/claim implementation was copied. No
Ryujinx-Nextendo/PolyForm Shield implementation, proprietary credentials, signing
keys or third-party server implementation is added by this notices work.

### Optional catalogs and artwork are not linked libraries

- `blawar/titledb` reference
  `d424690446eec6f4484209b9a1c067c7f7511398` has an MIT `LICENSE`, copyright
  2019 Blake Warner, Git blob `94adbef386321cb78c2d526c19b17678491aa350`.
  That exact license is retained for attribution/provenance. No `cheats.json`,
  real cheat entry or catalog payload was fetched or added here. The reference
  revision is **not a claim that future runtime catalog downloads are pinned to
  it**. User-selected downloads/installations/exports must preserve applicable
  notices; alternative sources require their own permission review. See
  [CHEAT_DATABASE.md](../CHEAT_DATABASE.md).
- AmiiboAPI's software `LICENSE` was inspected at
  `N3evin/AmiiboAPI@6115745c56ff762ce212ced6fbad9ea195d659b3`: MIT,
  copyright 2017 Nevin Vu. Its reference hash is in `SOURCES.json`, but no API
  software license directory is added because no API server code is copied.
  This documentary revision is not a runtime metadata/artwork pin. Public API
  accessibility and MIT software licensing are **not blanket artwork/data rights**.
  The app's explicit opt-in downloads public names and explicitly selected photos,
  not tags or keys; local names/custom art are preserved. No third-party artwork
  is bundled by the feature. See
  [AMIIBO_LAUNCHER_SYNC.md](../AMIIBO_LAUNCHER_SYNC.md#user-flow-and-privacy).

## Retained file hashes

All paths below are relative to `third_party/licenses/`.
`SOURCES.json` records archive members or immutable public source URLs/Git blobs,
sizes and SHA-256s; for source-comment excerpts it also records byte offsets,
lengths and excerpt hashes. File/path headings in `SOURCE_NOTICES.txt` are local
annotations; the quoted comment bytes are unchanged. Identical comment blocks
are deduplicated, retaining their source path lists. This is bounded notice-header
extraction, not an audit of every line or conditional platform in each project.

| File | SHA-256 |
|---|---|
| `libsmb2/COPYING` | `749c710c2b3d5c95906c785f28af9abbe4ce909311e72eb72eee5c6873e2fa6f` |
| `libsmb2/LICENCE-LGPL-2.1.txt` | `dc626520dcd53a22f727af3ee42c770e56c97a64fe3adb063799d8ab032fe551` |
| `libsmb2/SOURCE_NOTICES.txt` | `4edef72a5dfb1322cc5a4add64c56bd8ddd22224ebe3c9e81e9ecdccc52b20a3` |
| `libnfs/COPYING` | `edd960c0142b8ada98b43b6396b78f4e557b0bc70ac601a51e397ad04070e2c5` |
| `libnfs/LICENCE-BSD.txt` | `d9406ced95457941032aa11d04623b8ab71f2827a3395ebef137aec475be35b1` |
| `libnfs/LICENCE-LGPL-2.1.txt` | `dc626520dcd53a22f727af3ee42c770e56c97a64fe3adb063799d8ab032fe551` |
| `libnfs/SOURCE_NOTICES.txt` | `e562151f160ee018134016b1fbb87ebf62aad8bd8ca31f67e5e4a51896e9f396` |
| `citron-nextendo/LICENSE` | `3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986` |
| `citron-nextendo/GPL-2.0-or-later.txt` | `aaf135472f81c5b4a0dca9367e5bb5e9750032b5bebe5442b36e4c0a47430df3` |
| `citron-nextendo/SOURCE_NOTICES.txt` | `148dd4f1e17a801f5eb4d7323ddc2f38f8856a1ad2f62cb5eef7d1975a997ce0` |
| `titledb/LICENSE` | `55995f3a0fca47c7410c70764c5d99d237b8765551b2fd2f09d94736b87b6e45` |

## Validation and retained upstream whitespace

- All 11 retained license/notice file sizes and SHA-256s match the source manifest;
  the eight whole-file copies match their archive members or public Git blobs.
- All 114 recorded network comment locations were compared with the verified
  archives, including each byte range/hash and its presence in the retained notice.
  All six public-source SPDX excerpts match their recorded hashes. Git's staged
  blobs were compared byte-for-byte with the working files, checking that text
  conversion did not alter the copied material.
- Only the 14 scoped notice/license/qualification files are staged. Local Markdown
  links and authored Markdown/manifest whitespace checks pass. No packaging tool,
  application source or aggregate release document is changed.
- The full staged `git diff --check` reports four intentional inherited-format
  diagnostics, not a clean result: `libnfs/COPYING:39` has an upstream blank line at
  EOF; `libnfs/SOURCE_NOTICES.txt:105,122` and
  `libsmb2/SOURCE_NOTICES.txt:318` retain trailing spaces inside original copyright/
  license comments. Paths here are relative to `third_party/licenses/`. These
  bytes are deliberately not normalized or presented as locally authored defects.

The private validation logs are
`.local/release-license-archive-validation.log` and
`.local/release-license-notices-validation.log` in the exclusive worker tree.
Their scripts use only Python's standard library and the existing Docker image;
archive validation has networking disabled and the dependency volume read-only.

## Required package integration (coordinator-owned, not performed here)

1. Include `THIRD_PARTY_NOTICES.md`, the project `LICENSE`, and **all** of
   `third_party/licenses/`, including `SOURCE_NOTICES.txt` and `SOURCES.json`,
   alongside the already-required notices for other shipped components. Check
   the actual final archive/package file inventory and extracted file hashes.
   Repository presence is not evidence that a collector includes these files.
2. Determine/record the applicable distribution route for the final statically
   linked binary. Under an LGPL-2.1 section 6 route, provide the required library
   source and modifications plus application source and/or linkable objects, as
   applicable, sufficient to modify the library and relink the executable. Retain
   the recipient's modification/reverse-engineering-for-debugging rights required
   by that route; a dynamic-replacement mechanism must not be claimed for this
   static build. If relying on a later-version/GPL route instead, review and meet
   that route's requirements rather than treating a license label as compliance.
3. Supply/identify the complete corresponding source for GPL-covered code and
   adapted Nextendo portions through a valid license-compliance mechanism. Include
   exact upstream pins, local modifications, generated-source inputs, configure/
   compile/link recipes and relevant build/install material. License excerpts,
   archive checksums or links to moving upstream branches alone are insufficient.
   Verify the delivered source/relink materials against the **actual final binary**;
   this change supplies neither an object bundle nor that verification.
4. Build any source bundle from a reviewed allowlist. Exclude live credentials,
   `config/nextendo/`, network-share configurations and staged siblings, user data,
   tags/keys, downloaded catalogs/artwork and working/build caches. Do not collect
   the entire `.local`, `.deps`, another worker tree or private runtime directory.
   The parent-owned privacy integration `6d47104` is **not part of this baseline**;
   its presence/effect and final collectors require separate coordinator review.
5. If catalog data or artwork is later redistributed rather than obtained by a
   user's explicit request, review its own source/rights/notice requirements.
   Include TitleDB's copyright/permission notice where applicable to its copied
   material; do not infer that MIT API code licenses artwork or all other sources.

No `make-dist.py`, package-update/collector tool, aggregate changelog,
`UPSTREAM` record or implementation-team document is changed here. These notes
do not establish that earlier packages or any other dependencies satisfy all
distribution obligations.
