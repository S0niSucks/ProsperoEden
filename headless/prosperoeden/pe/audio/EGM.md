# Launcher audio

## Overview

Launcher cue/stream mixing and WAV decoding, centered on [mixer.hpp:Mixer](./mixer.hpp).

## Interface

- [mixer.hpp:Mixer](./mixer.hpp), `Clip`, `play_clip`, `swap_stream`, and `render` provide a 48 kHz stereo mixer with 32 voices and two stream slots.
- [mixer.hpp:Mixer](./mixer.hpp) exposes queued `set_bus_gain`, `set_master_gain`, `set_stream_gain`, and `stop_all` controls. `Bus` separates UI, SFX, and music gain; callers must check command-posting results.
- [stream_ring.hpp:StreamRing](./stream_ring.hpp) supplies the single-producer/single-consumer stream buffer.
- [sounds.cpp:SoundBank::load](./sounds.cpp) and `play` load cue variations with fallback tones; [wav.cpp:decode_wav](./wav.cpp) decodes supported RIFF PCM into clips.

## Invariants

- [mixer.hpp:Mixer](./mixer.hpp) separates game-thread command posting from audio-thread rendering. Its SPSC command queue has 255 usable entries; callers must handle failed posts ([mixer.cpp:post](./mixer.cpp)).
- [mixer.hpp:Clip](./mixer.hpp) and `swap_stream` borrow PCM/ring storage: keep that storage alive while the audio thread can reference it. Do not reload a live sound bank ([sounds.cpp:SoundBank::load](./sounds.cpp)).
- [mixer.cpp:Mixer::apply](./mixer.cpp) applies `stop_all` to voices only; it does not stop or detach stream slots. [mixer.cpp:Mixer::render_chunk](./mixer.cpp) consumes attached streams even at zero gain and fills short reads with silence. Muting is not pausing, and successful command posting does not acknowledge stream retirement.
- [wav.cpp:decode_wav](./wav.cpp) accepts 48 kHz mono/stereo 16- or 24-bit input; acceptance of extensible WAV does not include validating its subformat GUID.

## Side Effects

- [sounds.cpp:SoundBank::load](./sounds.cpp) reads cue files and owns decoded recordings; [sounds.cpp:SoundBank::play](./sounds.cpp) posts recording commands or synthesized fallback tones to the mixer.
- [mixer.cpp:render](./mixer.cpp) consumes commands/stream samples, updates voice state, and fills the caller's output buffer; device output belongs to [platform](../platform/EGM.md).

## Verification

- Repository host entry point, from the repository root: `make test` ([Makefile:test](../../../../Makefile)); it depends on `deps` and invokes `tools/build-headless-host.sh`.
- Not run here; this is not native/PS5 audio qualification.
