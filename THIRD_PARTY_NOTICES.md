# Third-party notices

ProsperoEden is licensed under GPL-3.0-or-later (see [LICENSE](LICENSE)). It
builds on the following projects, each under its own license.

## Emulator

- **[Eden](https://github.com/eden-emulator/mirror)**, GPL-3.0-or-later. The emulator core, built
  from the commit pinned in `UPSTREAM.json`, together with the dependencies Eden
  fetches and pins itself: Dynarmic, Boost, fmt, xbyak, zstd, lz4, Opus, SDL3,
  simpleini, sirit, SPIRV-Headers, Vulkan-Headers, Vulkan-Utility-Libraries,
  Vulkan Memory Allocator, ENet, frozen, cpp-httplib, nlohmann/json, and the time
  zone data. Each keeps its upstream license.
- **[FFmpeg](https://ffmpeg.org)**, LGPL-2.1-or-later. Built from Eden's pinned
  source with only the H.264, VP8 and VP9 decoders.
- **[OpenSSL](https://www.openssl.org)**, Apache-2.0, and
  **[zlib](https://zlib.net)**, zlib license. Taken from the
  [pacbrew](https://github.com/ps5-payload-dev/pacbrew-repo) PS5 packages.
- **[curl](https://curl.se)** (libcurl), curl license, and
  **[libpsl](https://github.com/rockdaboot/libpsl)**, MIT. From the same pacbrew
  packages; the update check uses them.
- **[PS5 Native App Boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)**,
  GPL-3.0-or-later. Its update check and self-update kit (`headless/update_check`),
  its self-update helper (`headless/self_update_helper`, built into `self-updater.elf`)
  and `tools/validate-loader-elf.py`.
- **[miniz](https://github.com/richgel999/miniz)** 3.0.2, MIT, unmodified in
  `third_party/miniz` with its `LICENSE`. The self-update helper reads release
  ZIPs with it; the app packs and unpacks save data for the save sync with it.
- **[QR Code generator library](https://github.com/nayuki/QR-Code-generator)** (C), MIT,
  unmodified in `third_party/qrcodegen` with its `LICENSE`. The launcher draws the QR code
  of a save sync's pairing with it.
- **LLVM compiler-rt** `emutls.c`, Apache-2.0 WITH LLVM-exception.

## Network client libraries

- **[libsmb2](https://github.com/sahlberg/libsmb2)**, pinned tag `libsmb2-6.3`,
  LGPL-2.1-or-later for the client library. Copyright Ronnie Sahlberg and
  contributors, including Primary Data Inc., Brian Dodge and André Guilherme.
  Used for read-only SMB access; the network build uses the static client library,
  not the upstream example applications. Exact upstream licensing and copyright
  notices are retained in [third_party/licenses/libsmb2](third_party/licenses/libsmb2).
  Its MD4 code is derived from the **RSA Data Security, Inc. MD4 Message-Digest
  Algorithm** and retains that implementation's attribution/license terms. The
  pinned source also contains public-domain notices for its tiny-AES128-C,
  Colin Plumb/Ian Jackson MD5 and portable-endian components; these are preserved
  in `libsmb2/SOURCE_NOTICES.txt`, not replaced by a generic LGPL notice.
- **[libnfs](https://github.com/sahlberg/libnfs)**, pinned tag `libnfs-8.0.0`,
  LGPL-2.1-or-later for the client library, with simplified BSD terms for protocol
  definitions and generated protocol code as described in upstream `COPYING`.
  Copyright Ronnie Sahlberg and other upstream rightsholders; NFSv4 protocol
  sources also retain The Internet Society's copyright notice. The application
  selects NFSv3, but the library build still includes NFSv4 code. Exact upstream
  texts and source notices are retained in
  [third_party/licenses/libnfs](third_party/licenses/libnfs). Upstream GPL example
  applications/utilities are not part of this client-only build.

Both source archive URLs and SHA-256 pins are in `tools/deps.json`; verification
and the additional file hashes are recorded in
[third_party/licenses/SOURCES.json](third_party/licenses/SOURCES.json).

## Adapted code and optional online data

- **[Citron Neo / Citron-Nextendo](https://github.com/NextendoNetwork/citron-nextendo)**,
  reference commit `8e76192472a3993a8f8fa6835e7ed061d443a9f7`. The account/API
  files inspected for the Nextendo adaptation say `Copyright 2026 citron Emulator
  Project`, GPL-2.0-or-later; the DNS/account-service reference files retain yuzu
  (2018) and citron (2025) notices under the same license. Their later-version
  option permits GPL-3.0-or-later adaptation in ProsperoEden; the reference README
  itself declares GPL-3.0-or-later. The actual pinned GPL texts and SPDX headers
  are retained in
  [third_party/licenses/citron-nextendo](third_party/licenses/citron-nextendo).
  This credits the bounded session/account/API and routing adaptation, not a
  wholesale import or license audit of that repository. See
  [Nextendo provenance](docs/features/NEXTENDO.md#source-and-license-provenance).
  No Ryujinx-Nextendo/PolyForm Shield implementation, signing keys, credentials or
  third-party server implementation is included by this feature.
- **[TitleDB](https://github.com/blawar/titledb)** by Blake Warner,
  MIT, reference commit `d424690446eec6f4484209b9a1c067c7f7511398`.
  Its format informs the cheat-catalog reader and its public catalog is an
  explicitly requested download source. The exact license, including
  `Copyright (c) 2019 Blake Warner`, is retained in
  [third_party/licenses/titledb/LICENSE](third_party/licenses/titledb/LICENSE).
  No TitleDB catalog or third-party cheat payload is bundled with ProsperoEden;
  tests use invented fixtures. A runtime download, selected-code installation or
  later export is distinct from distributing this application and must retain
  applicable source attribution and permissions. Other catalog URLs do not
  inherit TitleDB's MIT declaration. See [CHEAT_DATABASE.md](docs/CHEAT_DATABASE.md).
- **[AmiiboAPI](https://github.com/N3evin/AmiiboAPI)**, a public metadata service
  and optional user-selected photo source (`amiiboapi.org` and
  `raw.githubusercontent.com`). Its API software license is MIT, copyright
  Nevin Vu; no API server code or artwork is bundled here. That software license
  is not a blanket grant over third-party artwork, trademarks, game content or
  tag data. The feature downloads public names only after opt-in and photos only
  for explicitly selected models; it does not download tags or keys and preserves
  local names/custom art. Users and redistributors must assess rights for any
  separately obtained content. See [AMIIBO_LAUNCHER_SYNC.md](docs/AMIIBO_LAUNCHER_SYNC.md).

## Graphics

- **[PS5 Mesa](https://github.com/mihawk-99/PS5_Mesa)** and
  **[PS5 Vulkan](https://github.com/mihawk-99/PS5_Vulkan)** by Mihawk. The
  RADV Vulkan driver (Mesa, mainly MIT) and its PS5 link recipe and platform
  layer (GPL-3.0-or-later).
- **[ps5-vulkan](https://github.com/mpereiraesaa/ps5-vulkan)** by mpereiraesaa,
  an experimental Vulkan graphics and compute API for native PS5 homebrew.
- **[ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl)**,
  GPL-3.0-or-later, which includes Mesa components under their own licenses.
  `third_party/app_heap.c` comes from it.
- **[bcdec](https://github.com/iOrange/bcdec)** by Sergii "iOrange" Kudlai, MIT or public
  domain, unmodified in `third_party/bcdec` with its `LICENSE`. Decodes the BC7 blocks of a
  DDS wallpaper into pixels (docs/WALLPAPERS.md).

## Platform and interface

- **[PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk)** by John
  Törnblom (ps5-payload-dev).
- **[PS5 Native App Boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)**,
  GPL-3.0-or-later. The native app runtime and packaging tool. Its packaging
  tool translates parts of SvenGDK's
  [SharpProspero](https://github.com/SvenGDK/SharpProspero).
- **[PS5-Lapy-JB-Daemon](https://github.com/ArkSama/PS5-Lapy-JB-Daemon)** by
  ArkSama, with the cooperative owned-root design from
  **[mpereiraesaa's fork](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon)** and the pinned
  **[ProsperoEden compatibility fork](https://github.com/blackbearreloaded/PS5-Lapy-JB-Daemon)**.
  Lapy is MIT licensed; its shared protocol header is LGPL-2.1-or-later.
  ProsperoEden pins the fork, invokes its unmodified exact-title helper build,
  verifies its generated manifest and bundles the helper ELF with its license.
- **[ps5log](https://github.com/mpereiraesaa/ps5-agc-gears/tree/1ae1f9182abd2770c131b97419034fb85173c2dc/native/ps5log)**,
  GPL-3.0-or-later. The pinned single-header logging client used as an upstream
  Lapy helper build input.
- **[ProsperoPuzzles](https://github.com/blackbearreloaded/ProsperoPuzzles)**,
  GPL-3.0-or-later. The launcher's drawing, text, animation and sound code
  (`headless/prosperoeden/pe`) started there.
- **[ps5-homebrew-ui](https://github.com/blackbearreloaded/ps5-homebrew-ui)**,
  GPL-3.0-or-later. The launcher's themes start from its theme recipes: its
  theme table (`src/ui/theme.cpp`) is compiled into the launcher. Fetched at
  the commit pinned in `tools/deps.json`.
- **Montserrat** by Julieta Ulanovsky and contributors, SIL Open Font License
  1.1 (`third_party/fonts`). The launcher's font is baked from Montserrat Medium.
- **[HarfBuzz](https://github.com/harfbuzz/harfbuzz)**, "Old MIT" license (its
  `COPYING` file). Shapes the launcher's text in the scripts that need it, such
  as Thai and Arabic. Fetched at the release pinned in `tools/deps.json`.
- **[stb](https://github.com/nothings/stb)** by Sean Barrett, MIT or public
  domain (`tools/launcher/stb` and `third_party/stb`). `stb_truetype` bakes the launcher's font and,
  in the app, reads the console's system fonts for the scripts Montserrat does
  not have. `stb_image` (`third_party/stb`) reads the wallpaper images in the app.
  `stb_image_write` saves the PC previews and is not part of the app.
- The launcher's sound effects were generated with
  [ElevenLabs](https://elevenlabs.io) and edited for this project.
- `third_party/ps5_pad.hpp` and `third_party/native_audio.hpp` are this
  project's own platform declarations, GPL-3.0-or-later.

## Release packaging boundary

These notices and license copies do not by themselves satisfy source-delivery or
static-link relinking obligations. The release must retain applicable notices,
provide corresponding source/build materials through an appropriate license
compliance route, and preserve recipients' modification/relinking rights. The
new files still need to be included and checked in the final package; this notice
change does not modify packaging tools or certify compliance. See
[release license qualification](docs/features/RELEASE_LICENSE_NOTICES.md).

## Thanks

ProsperoEden exists thanks to the Eden maintainers and contributors, Mihawk,
mpereiraesaa, John Törnblom, SvenGDK, and the whole PS5 homebrew community.

## Lua 5.4.9

Embedded launcher scripting runtime. Copyright Lua.org, PUC-Rio. MIT license; see `third_party/lua/LICENSE.txt`. Source and checksum: `third_party/lua/SOURCE.json`.
