# Selected ProsperoEden improvements — 30 October 2026

Source: local `../ProsperoEden`, snapshot `5211718fb77f687a027dc5f675209bc64af4b982`.
Destination baseline: Encore `main`, `21fdbc1ca7f5a74e4f4a67c0b721349cf1353ffb`.
This is an **initial, partial port**, not completion of the original feature list,
a release/version bump or PS5 qualification. The old 1.006 candidate was reported
unable to open; its host/build results are not inherited by Encore.

## Application-owned adaptations

| Improvement | Encore adaptation | Origin |
| --- | --- | --- |
| Controller recovery | Reopen after 250 consecutive failed reads; then retry after 500/1000/2000/4000/8000 polls. Successful reads reset recovery. Failed reopens reserve the player/user slot without retaining the closed handle. Preserve mapping, rumble handoff and uninitialized read scratch. | Oliv3rmoon: `d9240dc`, `9ec8ab0`, `f19f8d1` |
| ExeFS recent/last game | Retain `Title/main`, not the ambiguous basename `main`. Existing strict persisted-name and symlink admission checks remain; arbitrary nested ROM paths are not enabled. | Oliv3rmoon: PR #286 |
| Loader diagnostics | Show bounded loader status text and highest present master-key **revision name**, not key material. Preserve the separate video-backend error. Formatter/wiring tested; selected core API compatibility remains uncompiled. | Oliv3rmoon: `dae49cf` |
| NRO metadata | Optional ASET title/title-ID/language/icon extraction; 1 MiB encoded icon, fixed `0x4000` NACP and overflow/overlap/truncation rejection. Preserve package/card, glyph-version and add-on paths. | Leonardo Medeiros de Mello Jr: `20ef10e` |
| Image preflight | Limit encoded reads to 64 MiB and dimensions to `1..4096` before decoding. Preserve ordinary cover resolution; NRO icons downsample to at most 512 pixels/side. | Leonardo Medeiros de Mello Jr: `20ef10e` |
| Library interchange | Publish complete local `library.json` snapshots before network enrichment, with bounded comparison and exclusive staging. Canceled/failed scans do not replace a prior snapshot. | Oliv3rmoon: PR #292; Encore-specific writer adaptation |
| Private release inputs | Name-first fail-closed preflight in `make-dist.py`, before ZIP/FTP/image generation, including `--image-only` and before deleting `dist`. Reject private network/Nextendo paths, links/reparse points and special files without reading credentials. | Leonardo Medeiros de Mello Jr: `f1a92c6` |

Schema: [LIBRARY_INTERCHANGE.md](LIBRARY_INTERCHANGE.md). Release protection
requires **clean, quiescent staging**; it is not a concurrent-mutation sandbox
or general secret-content scanner. Direct invocation of `package-image.sh`
does not gain this preflight.

## Preserved boundaries and remaining work

- Encore already admits NSZ/XCZ, direct formats and ExeFS and owns its current
  title/profile, glyph, graphics and input work. No wholesale CMake, renderer,
  JIT, heap, firmware/elevation or source-tree replacement.
- **Still pending:** Amiibo/NFC with folders, names and PNGs; per-code/live cheats
  and explicit database installation; SMB/NFS read-only VFS **with Settings
  editor**; independent games/wallpaper storage; Nextendo; native in-game
  keyboard; experimental USB Pro Controller; USB backup/restore; missing
  theme/layout parity and launch-argument forwarders.
- Partial drivers, JSON-only network configuration and automatic startup restore
  are not presented as completed features. Saves/settings are not reset,
  migrated or replaced. No new external service is contacted.
- The vendor gitlink remains `f7561e518e9b3646b9a3f45bece1286d044b45af`.
  Its checkout was initialized; `vendor/sources/eden` is excluded from further
  AI inspection/modification under that subtree's instructions. Core-internal
  parity remains unverified.

## Validation

Focused tests use real application helpers or exact extracted functions with
observable mock boundaries. C++ fixtures run in existing Docker images only,
with a read-only checkout, executable `/tmp` scratch, no network, `--rm`, no named
volumes and no host toolchain installation. Example:

```powershell
$root = (Get-Location).Path
docker run --rm --pull never --network none --read-only --cap-drop ALL --security-opt no-new-privileges --memory 2g --user 65534:65534 --mount "type=bind,src=$root,dst=/repo,readonly" --tmpfs /tmp:rw,exec,nosuid,size=600m --workdir /repo --env CXX=clang++-18 --entrypoint python3 prosperoeden-host:network-features -B tools/check-nro-assets.py
```

The host-only preflight invokes:

| Witness | Scope |
| --- | --- |
| `tools/check-pad-recovery-host.py` | Shipping/dev, O0/O2 ASan/UBSan, failed/oversized reads, neutralization, recovery/backoff, handle reuse, reserved slots/logout/cleanup and defective-code controls. |
| `tools/check-recent-games.py` | Actual naming helper plus persisted-name validator. |
| `tools/check-loader-message.py` | Actual formatter and bounded call-site wiring, **not** selected core enum/API compilation. |
| `tools/check-nro-assets.py` | Real parser/bridge readers, O0/O2 ASan/UBSan. VFS/NACP stand-ins are **not** native core compatibility proof. |
| `tools/check-metadata-image-bounds-host.py` | Actual image functions, O0/O2 ASan/UBSan, observable pre-allocation boundaries. Real stb decoding is a separately reported optional fixture using an explicitly permitted header. |
| `tools/check-library-file.py` | Actual serializer/writer, Unicode/invalid UTF-8, unchanged-file inode, links/hard links, FIFO, limits and failure cleanup. |
| `tools/check-export-privacy.py` | Actual packager CLI in synthetic temporary repos. Image tool is a stub, **not** PFS qualification. Windows/Linux exercise different applicable path/link cases. |

Existing ExeFS, F06 pad-transition, rumble, no-zero scratch and launcher case-index
witnesses remain applicable; no whole-suite PASS is inferred from these fixtures.
Local receipts, when present, are under `.local/ports-20261030/`.

### Results on this application tree

- All seven new focused witnesses passed in Docker/Linux. Image checks also
  passed with the explicitly permitted stb header from
  `vendor/sources/opengl-nanovg/src/stb_image.h` (not the selected Eden decoder
  build); export privacy passed 97 synthetic CLI invocations.
- Existing ExeFS, F06 pad transitions, no-zero scratch/edge diagnostics,
  rumble handoff, release case-index dedup, Python syntax and FTP-layout tests
  passed. Named credits and legal/license/SFX preflights also passed.
- Linux source inventory and workflow-reference checks passed using only a
  **prospective index view** that adds the new working-tree files to root
  `git ls-files` output. Actual hashes, file/proof checks and submodule queries
  were unchanged; the real Git index was not staged or modified. The current
  inventory has 514 reviewed application/maintained source owners; that count
  is triage, not runtime acceptance.
- These inventory scripts are Linux-oriented: the stock Windows run hits a
  pre-existing slash/backslash path comparison. Windows Python text checks
  need UTF-8 mode for this checkout. Neither issue was silently bypassed in a
  claimed Windows full-suite result.

**Not run:** full vendor-linked/native build, PS5 boot/gameplay or network-share
sessions. No release acceptance, commit, push or tag is claimed.
