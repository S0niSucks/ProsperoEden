# Launcher graphics primitives

## Overview

CPU drawing commands, text/image preparation, and OpenGL batch submission ([draw_list.hpp:DrawList](./draw_list.hpp), [gl_batch.hpp:GlBatch](./gl_batch.hpp)).

## Interface

- [draw_list.hpp:DrawList](./draw_list.hpp), `Instance`, and `Shape` define drawing commands; [draw_list.cpp:append](./draw_list.cpp), `push_clip`, and `text` produce CPU-side batches, not GPU submissions.
- [font.hpp:Font](./font.hpp) and [font.cpp:load](./font.cpp), `shaped`, `layout`, and `take_changed_rows` manage baked/dynamic text and atlas changes. [system_fonts.cpp:SystemFonts::set](./system_fonts.cpp), `ready`, `shape`, and `field` provide lazy system-font fallback.
- [bidi.cpp:resolve](./bidi.cpp) and `visual_order` implement a limited single-paragraph bidi subset, not full UAX #9.
- [gl_batch.cpp:GlBatch::draw](./gl_batch.cpp), `create_texture`, and `release` own GL submission/resources; [gl_program.cpp:build_program](./gl_program.cpp) builds shader programs.
- [gl_batch.hpp:GlBatch](./gl_batch.hpp) exposes `create_font_texture`, `sync_font_texture`, and `delete_texture` in addition to RGBA texture creation. Created texture names are caller-managed resources, separate from the batch's buffers/VAOs/program.
- [image.cpp:load_tga](./image.cpp), `load_avatar_dds`, and `halve` prepare bounded RGBA images.

## Invariants

- [draw_list.hpp:Instance](./draw_list.hpp) is a 96-byte shader-facing record; preserve its layout and `Shape` codes together.
- [font.cpp:load](./font.cpp) is not transactional. Font caches/atlas mutation are single-thread-owned, and callers must upload rows returned by `take_changed_rows`.
- [gl_program.cpp:set_glsl_prefix](./gl_program.cpp) borrows its prefix pointer; keep the storage alive. [gl_batch.cpp:GlBatch::draw](./gl_batch.cpp) does not restore general GL state for other renderers.
- [gl_batch.cpp:GlBatch::release](./gl_batch.cpp), including destruction, deletes only the batch buffers, VAOs, and program. Retire textures returned by `create_texture` or `create_font_texture` separately with [gl_batch.cpp:GlBatch::delete_texture](./gl_batch.cpp) while the owning GL context is valid.

## Side Effects

- [font.cpp:load](./font.cpp) consumes supplied font data and mutates font/atlas state; [system_fonts.cpp:SystemFonts::ready](./system_fonts.cpp) reads font files and creates shaping resources.
- [gl_batch.cpp:GlBatch::draw](./gl_batch.cpp), `create_texture`, and `release` change GL state, upload data, and allocate/delete GL objects. [gl_program.cpp:build_program](./gl_program.cpp) compiles/links shaders and emits failure logs.
- [image.cpp:load_tga](./image.cpp) and `load_avatar_dds` read image files and replace decoded buffers.

## Verification

- Repository host entry point, from the repository root: `make test` ([Makefile:test](../../../../Makefile)); it depends on `deps` and invokes `tools/build-headless-host.sh`.
- Not run here; external GL/HarfBuzz internals and PS5 rendering are not qualified by these source contracts.
