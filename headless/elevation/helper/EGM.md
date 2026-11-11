# Sandbox elevation helper

## Overview

Payload-side filesystem-capability helper for the Encore title, implemented by [main.cpp:handle_request](./main.cpp).

## Interface

- [main.cpp:handle_request](./main.cpp), `find_target`, `grant_filesystem`, and `write_state` implement the request/state exchange using [../protocol.hpp:Message](../protocol.hpp).
- [Makefile:all](./Makefile) builds `OUTPUT`, defaulting to `../../../build/sandbox-elevation/sandbox-elevator.elf`; `PS5_PAYLOAD_SDK` selects the SDK include location.

## Invariants

- [main.cpp:find_target](./main.cpp) restricts the target to `PPSA99008`; `handle_request` revalidates the prepared target before applying the filesystem-capability change.
- [main.cpp:handle_request](./main.cpp) requires the process and filedesc pointers to remain unchanged, the prepared `ucred` pointer to differ from the original, and the cloned credential/filesystem snapshot to equal the original state. Never mutate the preexisting shared credential, copy `cr_prison`, or swap credential pointers in this helper.
- [main.cpp:grant_filesystem](./main.cpp) checks readback and attempts rollback on failure. A `rollback_failed` result can leave partial state; successful elevation does not restore the previous state.

## Side Effects

- [main.cpp:write_state](./main.cpp) changes target process state; `handle_request` exchanges protocol messages.
- [Makefile:all](./Makefile) creates the output directory and executable when invoked; `clean` removes `OUTPUT`.

## Verification

- Documentary build command from the repository root: `make -C headless/elevation/helper all` ([Makefile:all](./Makefile)); requires the payload SDK. No helper test target is declared.
- No build or native/PS5 execution was performed for this contract.
