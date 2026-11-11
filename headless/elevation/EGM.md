# Startup filesystem elevation

## Overview

Single-threaded startup elevation client and shared helper protocol ([elevation.hpp:request](./elevation.hpp), [protocol.hpp:Message](./protocol.hpp)).

## Interface

- [elevation.hpp:request](./elevation.hpp) accepts a capability request and returns a protocol `Status`.
- [protocol.hpp:Message](./protocol.hpp), `validate`, `matches`, and `transfer` define and validate the pointer-free 24-byte little-endian exchange, including partial transfers.
- [protocol.hpp:Message](./protocol.hpp) fields, in order, are `magic` (`uint32_t`, `0x31564c45`), `version` (`uint16_t`, `1`), `size` (`uint16_t`, `24`), `kind`, `capability`, `pid`, and `status` (each 32 bits). [protocol.hpp:Kind](./protocol.hpp) values are `request=1`, `prepare=2`, `prepared=3`, `response=4`; [protocol.hpp:Capability](./protocol.hpp) supports only `filesystem=1`.
- [protocol.hpp:Status](./protocol.hpp) values are `ok=0`, `invalid_request=1`, `unsupported_version=2`, `unsupported_capability=3`, `target_mismatch=4`, `unavailable=5`, `prepare_failed=6`, `apply_failed=7`, `rollback_failed=8`, `transport_error=9`, and `protocol_error=10`.
- [validate-helper.py:validate](./validate-helper.py) checks packaged ELF framing; it is a packaging guard, not an internal call from `request`.
- [Helper](./helper/EGM.md): target lookup, capability change/readback, rollback attempt, and helper build target.

## Invariants

- [elevation.hpp:request](./elevation.hpp) must be called during single-threaded startup, before workers or files that depend on the resulting filesystem scope. This is a caller contract, not internal thread serialization.
- [protocol.hpp:validate](./protocol.hpp) requires the expected magic/version/size, a declared kind/status, a PID in `1..INT32_MAX`, and the supported capability. `matches` additionally requires the expected kind and identical PID/capability.
- [protocol.hpp:io_timeout_us](./protocol.hpp) is shared by both endpoints and defaults to `5,000,000` µs. [elevation.cpp:submit](./elevation.cpp) applies it to send/receive/connect operations; [helper/main.cpp:main](./helper/main.cpp) configures receive/send timeouts. This is not a whole-request wall-clock deadline.
- [protocol.hpp:transfer](./protocol.hpp) continues after positive short operations but ends the attempt on interruption, timeout, EOF, failed operation, or an oversized count; it does not retry those failures.
- [helper/main.cpp:grant_filesystem](./helper/main.cpp) attempts rollback after failed application/readback; rollback failure can leave partial state.

## Side Effects

- [elevation.cpp:submit](./elevation.cpp) configures timeouts and uploads the helper through loopback port 9021; `exchange` performs the protocol exchange and calls `seteuid(geteuid())` for credential preparation.
- [helper/main.cpp:write_state](./helper/main.cpp) changes target process state. Successful elevation is not automatically restored by the helper.
- [validate-helper.py:__main__](./validate-helper.py) reads the ELF file and prints the validation result; `validate(data)` checks supplied bytes without file I/O. Neither establishes hardware qualification.

## Verification

- Documentary helper build, from the repository root: `make -C headless/elevation/helper all` ([helper/Makefile:all](./helper/Makefile)); requires the payload SDK.
- No build, helper upload, identity change, or native/PS5 execution was performed for this contract.
