# Launcher core utilities

## Overview

Bounded file reads, catalog lookup, and local animation state ([file.hpp:read_file](./file.hpp), [strings.hpp:Catalog](./strings.hpp), [tween.hpp:Spring](./tween.hpp)).

## Interface

- [file.hpp:read_file](./file.hpp) reads a file with a default 64 MiB limit.
- [strings.cpp:Catalog::load](./strings.cpp), `Catalog::find`, `catalog_candidates`, and `fill` implement catalog loading, lookup, candidate locale tags, and substitutions. `catalog_candidates` does not construct filesystem paths; callers add the catalog directory and extension.
- [strings.cpp:catalog](./strings.cpp) exposes the shared mutable catalog. `tr(const char*)` returns borrowed text; `tr(const std::string&)` returns an owned string.
- [log.hpp:pe::sys::log](./log.hpp) is the printf-style launcher logging entrypoint.
- [tween.hpp:Spring](./tween.hpp) and `Timer` provide local interpolation/timing state.

## Invariants

- [file.hpp:read_file](./file.hpp) can return false after leaving a partial prefix in the destination; success must be checked before using the data.
- [strings.cpp:Catalog::find](./strings.cpp) and `tr(const char*)` borrow catalog storage when translated, or the caller's English input on fallback. Preserve the relevant storage lifetime; catalog mutation can invalidate translated references. The PO parser and RTL-mark heuristic are limited, not a full bidi implementation.
- [tween.hpp:Spring](./tween.hpp) and `Timer` do not validate negative/nonfinite frame deltas; callers own that input constraint.

## Side Effects

- [file.hpp:read_file](./file.hpp) reads files and changes the destination buffer.
- [log.hpp:pe::sys::log](./log.hpp) writes the `[ProsperoEden] launcher: ` prefix, formatted arguments, and a newline to stderr.
- [strings.cpp:Catalog::load](./strings.cpp) parses supplied PO text and replaces mutable catalog storage; [tween.hpp:Spring](./tween.hpp) and `Timer` mutate their local state.

## Verification

- Repository host entry point, from the repository root: `make test` ([Makefile:test](../../../../Makefile)); it depends on `deps` and invokes `tools/build-headless-host.sh`.
- Not run here; catalog text is not proof of implemented features.
