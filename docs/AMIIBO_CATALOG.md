# Amiibo identification and pictures — implementation study

Research date: 27 October 2026. Folder discovery, offline catalog identification and
selected-item PNG previews now have implementations and Docker host checks. An opt-in
PC helper prepares metadata and selected photos. OpenGL and Vulkan previews have been
rendered offscreen on Mesa, not on the PS5. **Updating directly from the PS5 launcher
and console qualification remain pending.**

## What other applications actually do

| Application and inspected source | Observed behavior | Applicable here |
|---|---|---|
| Eden, pinned `67bada77`, `src/yuzu/main_window.cpp`, `OnLoadAmiibo` / `LoadAmiibo` | Opens a `.bin` chooser when the guest is waiting; explicitly removes a tag already present | Keep the existing real NFC driver and explicit scan/remove actions |
| Ryujinx-derived mirror, `fd88d4ce`, `AmiiboWindowViewModel` / `AmiiboApi` | Separate JSON catalog, name/series filtering, preview image, ID composed of `head + tail`; local JSON cache and remote refresh | Separate display metadata from NFC data; show exact variants rather than infer from filenames |
| TagMo, `d4e8246c`, `Amiibo`, `AmiiboManager` | Resolves tag ID through a JSON catalog; image paths use `icon_<head>-<tail>.png`; includes local/default database fallbacks | Identify a user's dump by its model ID and cache metadata/images independently |
| AmiiboAPI, `8bitDream` revision `743bdbc7` | Compact database keyed by 16-digit IDs, with separate series/type/character tables; API returns names, series and image addresses | A metadata source, not a dump source |

TagMo is a tag manager, not an emulator. The inspected Ryujinx source is a downstream
mirror, not evidence that every Ryujinx version behaves identically. Its code also
has virtual-tag functionality; that is **not** authorization to generate or distribute
tag data here.

The user-suggested `AmiiboDB/Amiibo`, tree `58cf4558`, contains `.bin`/`.nfc` collections
organized in folders. Its complete, non-truncated 2,051-entry tree had **no PNG/JPEG/WebP
files or JSON/CSV catalog**, and the repository did not expose a license. It is useful
as an organizational reference, not as a source of code, dumps, keys or artwork to bundle.
No dumps or keys were fetched during this study.

## Implemented local foundation

- `headless/amiibo_catalog.cpp` uses the pinned core's actual layout validation before
  reading eight model-ID bytes. Its compile-time offset checks distinguish the raw
  layout (`0x54`) from the packed/internal layout (`0x1dc`). Neither/neither-unique
  matches leave the ID unknown rather than choose by catalog or filename.
- The command strip reads optional `amiibo/amiibo.json` when opened and resolves only
  its selected dump, not every tag in the collection. It accepts the compact upstream
  database and API list/single-object responses, rejects ambiguous IDs/duplicate JSON
  keys, and bounds file bytes, entries, parser events, nesting and label lengths.
- Existing loose files and subfolders remain in place; discovery has a global 4,096-entry
  budget and eight-level folder limit. Matching model IDs never merge distinct dumps.
- The image candidate priority is paired `<dump stem>.png`, then `cover.png` in a
  one-dump folder, then `images/icon_<head>-<tail>.png`. Candidates have bounded regular
  files and PNG signatures before decoding. No URL from a catalog is fetched or used
  as a filesystem path.
- Actual NFC/catalog/image file opens reject symlinks in every path component using
  descriptor-relative `openat`. Directory discovery also reads its verified descriptor;
  `lstat` permission is not required for the NFC file accesses.

Dedicated Docker host checks passed the initial catalog implementation normally,
with ASan/UBSan and with TSan (`sh_17f1e6240001ypDJxmlbccQyCn`), using synthetic tag bytes
and the existing project icon as the PNG fixture. This included both real core layouts,
variants, malformed/oversized catalogs, duplicate files, bounded folder discovery,
offline labels and image candidate priority. The later descriptor-directory refactor
requires the next integrated run; its separate I/O/record fault checks already passed
normally and under ASan/UBSan. The later full host/native runs passed too:
`sh_17f220842001lSzaqYxEqiVm0R` and `sh_17f2208640013Nx1l2Lv0usy9V`. Those runs predate
the photo decoder/renderers below; PS5 qualification remains pending.

## Photo decoding and rendering

The user confirmed preserving ProsperoEden's existing visual identity. The picture uses
the current HUD panel, below its text, without changing shortcuts or pausing the guest.
No picture means no photo panel. Only `QuickMenu::DescribeSelection` decodes an image;
the renderers receive immutable pixels with the matching text in one locked snapshot.

- `quick_picture.cpp`: private PNG-only stb decoder, 4 MiB file cap, 1,024-pixel dimension
  cap and **12 MiB aggregate decoder-allocation budget**, including compressed-stream
  expansion. It reads the same no-follow descriptor it checked, alpha-weight filters
  into a 256x256 aspect-fit thumbnail, and returns an empty result on decode/allocation
  failure. It neither edits nor uploads tags and does not affect Eden's other decoders.
- OpenGL retains one texture in its dedicated HUD context, uploads only on picture
  changes and deletes it with that context. The image has transparent letterboxing.
- Vulkan uses a separate sampled-image descriptor for the photo. On a changed image,
  `scheduler.Finish()` completes earlier uses **before** replacing its image/view or
  updating its descriptor; ordinary frames do not wait or decode. A hidden/cheat menu
  does not draw the retained photo. The render pass retains at most one thumbnail.
- Both paths preserve the 128-byte Vulkan push-constant limit and existing text/loading
  paths. Output scales with the frame height; the photo is not stretched or cropped.

Evidence for the initial photo implementation:

- `tools/check-quick-picture.py`, `sh_17f2ff2a70011prEFKz4yHEWXK`: normal, ASan/UBSan,
  TSan; compressed bomb, truncated/mutated images, size/dimension limits, decoder
  allocation balance, symlinks/FIFOs, aspect/alpha/orientation, concurrent decoding and
  10,000 atomic text/image selections. Fixtures are generated pixels, not tag/art dumps.
- `tools/check-quick-overlay.py`, `sh_17f2dbb02001mp8njmv25lnR6k`: executes the actual
  OpenGL `DrawHud` method on Mesa with its real shader/font; photo orientation, replacement,
  no stale photo on fallback, visible text at 720p and 1080p.
- `tools/check-quick-vulkan.py`, `sh_17f2fd49b001Fxyev3qq7Ju8vJ`: executes the actual
  Vulkan draw include and compiled/validated SPIR-V on Mesa lavapipe; photo/text pixels,
  replacement/fallback at 720p and 1080p, zero Vulkan validation errors. The fixture's
  initial mixed-case text bypassed the producer; its follow-up uses the same normalization
  as the menu. This is not execution of the PS5 memory allocator or scheduler.

## Optional PC cache synchronization

`tools/sync-amiibo.py <folder>` explicitly downloads a bounded public-name catalog.
Photos are fetched only for repeated `--id <16-digit model ID>` arguments (max 64),
never by scanning the user's tags. The configured image template supplies request
addresses; **URLs returned in catalog data are ignored**. The default sources are the
maintained `amiiboapi.org` endpoint and the upstream image repository, not the obsolete
`.com` service. This helper is separate from the console's NFC and render threads.

Both sources can be changed to an HTTPS mirror. Credentials/fragments, protocol
downgrades and cross-origin redirects are rejected. Byte, timeout, catalog nesting,
item/label and image dimension/decompression limits apply. Before any cache replacement,
all requested downloads are validated, including PNG chunk CRCs and bounded scanline
streams. Existing valid photos are reused by default. Each replacement uses a flushed
temporary file and atomic rename; a disk failure between files is **not** an all-or-nothing
transaction. No tags, keys or third-party images are included in the source repository.

The PC output folder must be user-controlled and not renamed concurrently. Preflight
rejects symlinks, Windows junctions/reparse points and nonregular cache files; this is
not the console's stronger descriptor-relative ancestry guarantee. Downloaded photos
are revalidated/decoded by the console reader before rendering. Cached names/photos
continue working when the console has no network.

`tools/check-sync-amiibo.py` tests synthetic catalogs/PNG streams, duplicate IDs/JSON,
source/redirect restrictions, response size/deadline checks, cache reuse, unknown IDs,
failed-batch preservation and symlink/FIFO refusal, without contacting public services.

The optional graphics Docker image adds only Linux Mesa/Vulkan test dependencies;
nothing is installed in Windows/WSL. The normalized-text Vulkan follow-up passed
`sh_17f313d43001mHu3W5p0geaRQ4`; the later PNG allocation-refusal/fallback test passed
normal/ASan/UBSan/TSan in `sh_17f38f241001cEbhVy0oYtjEgN`.

Native photo build `sh_17f2d250d001D0Jn63FK1OmHh3` passed linking, import/package checks,
ELF identity and source provenance. Its ZIP/symbols were retained in
`.local/docker-release/amiibo-photos-20261027/` with ZIP SHA-256
`c1b53d6d3abd81e56d40f6a277cd3fa16d6f2caf31d4ddc81a375a41c7c5c48f` and symbol ELF
`b351ea34b704ef5e179d808f52005a496bc501328399c32cf67aa3c5bd31d1c9`. The actual ZIP
CRC/0777 modes, identity/ELF/eboot/source-manifest correspondence were checked before
preservation. It predates the later C++ allocation-exception fallback and PC helper.
This build uses the old version/release notes and is not the final requested release.
The corresponding complete host suite `sh_17f2d250d002SEdc5zUn7mQ4SY` passed catalog,
actual PNG menu selection, NFC and live-cheat regressions with sanitizers; device/NRO
evidence is `devices-host-20261027-211827.JXusIb` / `nro-host-20261027-211831.fqfGQv`.

## Direction for ProsperoEden

**Model ID identifies; folders organize; pictures illustrate.** The filename and folder
name must not override the identity read from a dump. A display lookup is not NFC
authentication or proof that a game will accept the file.

- Read the public model identifier using the pinned NFC layout, not a guessed filename
  or the tag's individual UUID. Match all eight identifier bytes, preserving character,
  series, type and variant. Check the supported dump layout before using its offsets;
  Eden's packed/internal and raw tag structures are not interchangeable.
- Keep each user's file distinct even when two dumps share the same model ID: they can
  hold different saved progress. Never deduplicate, rename or relocate the user's dumps.
- Support existing loose `.bin` files and bounded subfolders. A friendly organization
  can be `amiibo/Mario - Smash/tag.bin` with an optional `cover.png`; it is not necessary
  to force everyone to rearrange their existing collection.
- A local PNG is optional. Prefer a specifically paired image when a folder has multiple
  dumps; do not arbitrarily take the first image. A catalog image is keyed by model ID,
  never by a fuzzy match on "Mario". Show the series/variant to distinguish similar names.
- Unknown IDs, unavailable metadata and missing/broken images fall back to the user's
  label and an explicit no-image state; they do not prevent presenting a supported dump.
- Metadata and image caches remain separate from files that the guest can write. PNGs
  are never passed to the NFC driver. Opening the browser or changing the selected item
  does not scan a tag or modify its dump.
- Keep online lookup optional, bounded and off the guest GPU/timing threads. A previous
  valid local cache must continue working offline. Never upload a dump, UUID, saved tag
  data or keys. Network requests need only the public model ID.
- Fetching metadata or pictures is not fetching usable tags. No automatic `.bin`, `.nfc`
  or key download is part of this feature.

The current AmiiboAPI service was verified at **`amiiboapi.org`** with a single public
model-ID request. Its maintained README explicitly says to replace the historical
`.com` domain with `.org`. The historical domain returned unrelated content during
this investigation and must not be copied from old emulator examples.

## License and delivery boundaries

AmiiboAPI's repository has an MIT software license. Its README credits third-party
image sources; this is not a blanket grant of rights to Nintendo's artwork. Do not
bundle a collection of images on the strength of the code license. Preserve source
attribution for any reused licensed material; support user-provided PNGs independently.
No upstream code or image was copied into the app during this study.

Accepting a PNG path alone is not a photo implementation. Decode/upload/draw on **both
OpenGL and Vulkan** now have host evidence above; PS5 resource lifetime and gameplay
remain separate qualification steps. Loading every collection image at once is not acceptable.

Before calling the extension complete, test bounded traversal, global entry/depth
limits, symlink rejection, duplicate IDs with distinct files, unknown IDs, corrupt and
oversized metadata/PNGs, offline cached operation, and unchanged scan/remove/write
semantics. Use synthetic tag/image fixtures, the real driver/factories and sanitizers;
retain separate PS5 gameplay and rendering qualification requirements.

## Sources

- [Eden main window at the pinned core revision](https://github.com/eden-emulator/mirror/blob/67bada77f8a43a90da2e94e89b8e7da73c256989/src/yuzu/main_window.cpp)
- [Ryujinx-derived view model, inspected mirror revision](https://github.com/Stella-sea/ryujinx-admin/blob/fd88d4ce7d049c2f6536439f0750b14f306d24b7/src/Ryujinx/UI/ViewModels/AmiiboWindowViewModel.cs)
- [Ryujinx-derived catalog ID model](https://github.com/Stella-sea/ryujinx-admin/blob/fd88d4ce7d049c2f6536439f0750b14f306d24b7/src/Ryujinx/Common/Models/Amiibo/AmiiboApi.cs)
- [TagMo ID/image model](https://github.com/HiddenRamblings/TagMo/blob/d4e8246c9f087db70f2ad159a831f851127f033c/app/src/main/java/com/hiddenramblings/tagmo/amiibo/Amiibo.kt)
- [TagMo database parsing and fallbacks](https://github.com/HiddenRamblings/TagMo/blob/d4e8246c9f087db70f2ad159a831f851127f033c/app/src/main/java/com/hiddenramblings/tagmo/amiibo/AmiiboManager.kt)
- [Maintained AmiiboAPI README](https://github.com/8bitDream/AmiiboAPI/blob/743bdbc7e38408fb0a42571f0f2a55c48e6c2acb/README.md)
- [Compact metadata schema](https://github.com/8bitDream/AmiiboAPI/blob/743bdbc7e38408fb0a42571f0f2a55c48e6c2acb/database/amiibo.json)
- [User-suggested repository, inspected tree](https://github.com/AmiiboDB/Amiibo/tree/58cf4558de56863575421844bc27695ef1ba56ad)
