# Launcher platform output

## Overview

EGL display ownership and the launcher mixer-to-device audio worker ([display_egl.hpp:Display](./display_egl.hpp), [audio_out.hpp:AudioOut](./audio_out.hpp)).

## Interface

- [display_egl.cpp:Display::open](./display_egl.cpp), `swap`, and `close` manage the GL 4.6 display/configuration, dimensions, and swap interval.
- [audio_out.cpp:AudioOut::start](./audio_out.cpp), `run`, and `stop` connect a borrowed [Mixer](../audio/mixer.hpp) to 256-frame output batches on a pthread.

## Invariants

- [display_egl.cpp:Display::open](./display_egl.cpp) owns context setup; dynamic mode switching is compile-time gated and may fall back or warn. A requested mode is not a 120 Hz guarantee.
- [audio_out.hpp:AudioOut](./audio_out.hpp) requires the caller to initialize the platform AudioOut subsystem first. [audio_out.cpp:AudioOut::start](./audio_out.cpp) opens the port and starts its worker; it does not initialize that subsystem.
- [audio_out.cpp:AudioOut::start](./audio_out.cpp) has no already-running guard. Start once per stopped instance and keep the borrowed mixer alive until `stop` joins the worker.

## Side Effects

- [display_egl.cpp:Display::open](./display_egl.cpp), `swap`, and `close` create/make current/present/destroy display resources.
- [audio_out.cpp:AudioOut::run](./audio_out.cpp) submits device output and sleeps on output errors; `stop` joins, drains, and closes output.

## Verification

- Repository host entry point, from the repository root: `make test` ([Makefile:test](../../../../Makefile)); it depends on `deps` and invokes `tools/build-headless-host.sh`.
- Not run here; no native audio, display-mode, or scanout qualification was performed.
