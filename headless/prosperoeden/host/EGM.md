# Host launcher preview

## Overview

Host-only launcher rendering with synthetic service data ([preview_main.cpp:main](./preview_main.cpp), [fake_services.hpp:FakeServices](./fake_services.hpp)).

## Interface

- [preview_main.cpp:main](./preview_main.cpp) accepts `pe_preview assets output [--tour] [width height]`, initializes host EGL/OpenGL 4.5, and writes PNG output or raw RGBA to stdout.
- [preview_main.cpp:Stage](./preview_main.cpp) supports preview staging; `PE_LANG`, `PE_LOOK`, `PE_SPECIMEN`, and `PE_PREVIEW_SWITCH_THEME` select language, accessibility settings, specimen text, and alternate presentation.
- [fake_services.hpp:FakeServices](./fake_services.hpp) implements the UI service boundary with in-memory settings/mods and synthetic games. [fake_services.cpp:FakeServices::FakeServices](./fake_services.cpp) prepares twelve sample games/artwork.

## Invariants

- [fake_services.cpp:FakeServices::home](./fake_services.cpp) and save-transfer methods provide sample behavior, not real save copying or PS5 service validation.
- [preview_main.cpp:main](./preview_main.cpp) is a host GL preview; its output is not evidence of native PS5 rendering or guest execution.

## Side Effects

- [fake_services.cpp:FakeServices::FakeServices](./fake_services.cpp) generates fixture TGA artwork; service mutations change the fake's in-memory state.
- [preview_main.cpp:main](./preview_main.cpp) creates GL resources and writes preview images or stdout pixels.

## Verification

- Repository host entry point, from the repository root: `make test` ([Makefile:test](../../../Makefile)); it depends on `deps` and invokes `tools/build-headless-host.sh`. This does not assert preview-specific test coverage.
- No build, preview execution, or native/PS5 validation was performed for this contract.
