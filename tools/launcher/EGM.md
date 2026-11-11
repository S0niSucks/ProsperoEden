# Launcher host tools

## Overview

Offline asset generation, translation maintenance, bounded artwork indexing and host previews. Existing [tool documentation](../README.md) remains separate; third-party `stb` implementation and regression fixtures are outside this contract. `[assets.sh:HOST_CXX]` `[strings.py:main]`

## Interface

- [assets.sh](./assets.sh) builds/runs the [font baker](./bake_font.cpp) and regenerates art. `bake_font TTF OUTPUT [pixel_size=56] [sdf_range=8] [atlas_width=2048]` produces the font atlas; [bake-wordmark.py](./bake-wordmark.py) emits loading-wordmark GLSL. `[assets.sh:HOST_CXX]` `[bake_font.cpp:main]` `[bake-wordmark.py:main]`
- [strings.py](./strings.py) offers `extract`, `new`, `check`, `embed-fr`; checks include placeholders/fallback coverage and optional system-font checks via `PE_SYSTEM_FONTS`. `[strings.py:main]` `[strings.py:check]` `[strings.py:write_embedded_fr]`
- [generate-sfx.py](./generate-sfx.py) synthesizes 20 deterministic 48 kHz/16-bit cues; [process-sfx.py](./process-sfx.py) decodes supplied audio with ffmpeg, normalizes/fades/clamps and writes WAVs. [render-art.py](./render-art.py) generates launcher art; [render-demo-covers.py](./render-demo-covers.py) generates fictional-title covers. `[generate-sfx.py:render]` `[process-sfx.py:process]` `[render-art.py:main]` `[render-demo-covers.py:paint]`
- [build-box-art-index.py](./build-box-art-index.py) builds a bounded, deduplicated Switch case-art index, optionally downloading the source archive. [preview-botw-media.py](./preview-botw-media.py) fetches explicit host-preview images and writes source receipts. `[build-box-art-index.py:build]` `[build-box-art-index.py:download]` `[preview-botw-media.py:download]`
- [preview.sh](./preview.sh) builds/runs software-EGL `pe_preview` captures; `--tour` creates video through ffmpeg. `PE_PREVIEW_JOBS` must be a positive integer. `[preview.sh:--tour]` `[preview.sh:PE_PREVIEW_JOBS]`

## Invariants

- Generated demo covers represent fictional sample titles; previews and synthetic fixtures do not establish native rendering or game compatibility. `[render-demo-covers.py:main]` `[preview.sh:run]`
- Font checking is conditional on available font paths; successful catalog checks do not imply every system font was inspected. `[strings.py:check]`
- `preview.sh` invokes asset/dependency preparation before validating all preview arguments; even an invalid invocation is not guaranteed read-only. `[preview.sh:PE_PREVIEW_JOBS]`

## Side Effects

- Asset generation compiles host tools and overwrites generated repository assets. Translation write modes rewrite POT/PO files or the embedded French header; `check` is the checking mode. `[assets.sh:HOST_CXX]` `[render-art.py:backdrop]` `[strings.py:write_embedded_fr]`
- Index/media tools may make network requests and write outputs; previews build/run host binaries and may overwrite video via ffmpeg. No preview, asset regeneration or download was run for this documentation. `[build-box-art-index.py:download]` `[preview-botw-media.py:download]` `[preview.sh:--tour]`

## Verification

- From the repository root: `python3 tools/launcher/strings.py check` validates catalogs. Repository-root `make test` fetches missing dependencies and builds the host core. Both are documentary commands only, not executed here. `[strings.py:main]` `[Makefile:test]`
