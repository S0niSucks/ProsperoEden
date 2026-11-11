# Launcher component boundaries

## Overview

Reusable launcher components beneath the service-driven [UI](./ui/EGM.md), separating CPU drawing/mixing from device output ([ui/launcher.hpp:Launcher](./ui/launcher.hpp), [gfx/draw_list.hpp:DrawList](./gfx/draw_list.hpp), [audio/mixer.hpp:Mixer](./audio/mixer.hpp)).

## Interface

- [Audio](./audio/EGM.md): cue/WAV decoding, mixer commands, and stream rings (`audio/mixer.hpp:Mixer`).
- [Core](./core/EGM.md): bounded file reads, PO catalog lookup, and local timing (`core/file.hpp:read_file`, `core/strings.hpp:Catalog`).
- [Graphics](./gfx/EGM.md): draw lists, text/image preparation, and GL batching (`gfx/draw_list.hpp:DrawList`, `gfx/gl_batch.hpp:GlBatch`).
- [Platform](./platform/EGM.md): EGL display and mixer-to-device output (`platform/display_egl.hpp:Display`, `platform/audio_out.hpp:AudioOut`).
- [UI](./ui/EGM.md): launcher state, provider interface, widgets, and asynchronous textures (`ui/launcher.hpp:Launcher`, `ui/services.hpp:Services`, `ui/textures.hpp:Textures`).

## Invariants

- [audio/mixer.hpp:Mixer](./audio/mixer.hpp) separates command producer and audio consumer; [platform/audio_out.cpp:AudioOut::start](./platform/audio_out.cpp) borrows that mixer until the worker stops.
- [gfx/draw_list.cpp:append](./gfx/draw_list.cpp) builds CPU commands; [gfx/gl_batch.cpp:GlBatch::draw](./gfx/gl_batch.cpp) performs GL submission. Do not treat draw-list creation as presentation evidence.
- [ui/services.hpp:Services](./ui/services.hpp) is the provider boundary; [ui/launcher.cpp:~Launcher](./ui/launcher.cpp) cancels and waits for outstanding futures before destruction.

## Side Effects

- [core/file.hpp:read_file](./core/file.hpp) and [audio/sounds.cpp:SoundBank::load](./audio/sounds.cpp) read files; [ui/settings.cpp:press_settings](./ui/settings.cpp) delegates persistence to services.
- [platform/display_egl.cpp:Display::swap](./platform/display_egl.cpp) presents display buffers; [platform/audio_out.cpp:AudioOut::run](./platform/audio_out.cpp) submits audio. [gfx/gl_batch.cpp:GlBatch::release](./gfx/gl_batch.cpp) and [ui/textures.cpp:Textures::release](./ui/textures.cpp) delete GL resources.

## Verification

- Repository host entry point, from the repository root: `make test` ([Makefile:test](../../../Makefile)); it depends on `deps` and invokes `tools/build-headless-host.sh`.
- Not run here; component source contracts do not qualify native/PS5 output or external library internals.
