# Campaign regression fixtures

Docker/Linux host-only tests of real application headers. These files retain the
exact candidate fixture bytes from A05/A02; historical NOT_RUN comments describe
their authors' initial delivery. A50 subsequently ran the original fixtures.
[Campaign integration](../../docs/feature-roadmap/campaign-20261107/INTEGRATION.md)
records the integrated run and remaining gates.

A50/integrated-04 executed these permanent fixtures against the live checkout:
image cancellation/gates passed and all40 mod cases passed. The separate A14-05
witness still observes the known defect. Existing regressions10/10 and syntax
755 Python files also passed. This is host evidence only; earlier raw failures
and refused preflights were not replaced.

The later live A10+A46 integration also passed these gates. The clean r2 mod
operation is now source-integrated with limited source/API-host acceptance;
fresh integration03 core gates passed. Its A10/global gate remains blocked by
an invalid UTF8 wrapper oracle, retained independently. The old two-call witness remains a
historical ordering control, not evidence that the new UI still uses two calls.

[Provider-owned witnesses](provider-owned/README.md) adds eight byte-identical
files from the reviewed v2 package. Its external run passed36+16 plumbing
controls, two actual enclosure probes, four FD/JPEG witnesses and two gates.
The promoted path/new live provider pin passed its own fresh A50 execution;
neither these host results nor the runner's acknowledgement flag qualify PS5.

| File | SHA-256 | Scope |
| --- | --- | --- |
| `read-image-cancel.cpp` | `43cfaec3b79b83f10ef3f3d888d9dbaeeca7e65e9a676ade83a7e145c7cf7e8a` | Actual I/O with wrapped cancellation/errors, outputs and closed FDs; 18/65537 bytes. Baseline exit10 means exactly four terminal-cancel regressions; exit20 is a separate control failure, not a valid baseline. Integrated header must return0. |
| `native-gate.cpp` | `ff3668a25444cec9c5dc1cbd34a558d85d51e0bdb8f7c50f14379e1a4cda75b8` | Host-compiled PS5_NATIVE path with qualifier absent or explicit0. Neither is native ABI/runtime qualification. |
| `mod_settings_check.cpp` | `b65dbda1c0abc4dde0421aa677eba312d2059e4a61adabf53e35ea94a3025698` | Two real settings writers, corrupt/non-object refusal, unrelated fields, aliases/sentinels and real write failure. |

## Reproduction payloads

Only inside the existing approved Docker sandbox: repo RO at `/repo`, pinned
nlohmann headers RO at `/json` (containing `nlohmann/json.hpp`), fresh writable
external `/out`, no network, bounded resources and container cleanup/timeout.
No whole-core staging or preparation script is needed. Build with the existing
C++20 compiler, e.g. `clang++-18`; keep all output outside the checkout.

```sh
clang++-18 -std=c++20 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -I/repo/headless /repo/tools/feature-roadmap-regressions/read-image-cancel.cpp -Wl,--wrap=read,--wrap=fstat,--wrap=fstatat -o /out/read-image-cancel
timeout -k 5s 30s /out/read-image-cancel
clang++-18 -std=c++20 -DPS5_NATIVE=1 -I/repo/headless /repo/tools/feature-roadmap-regressions/native-gate.cpp -o /out/native-gate-absent
timeout -k 5s 30s /out/native-gate-absent
clang++-18 -std=c++20 -DPS5_NATIVE=1 -DEDEN_NATIVE_USER_CONTENT_IO_QUALIFIED=0 -I/repo/headless /repo/tools/feature-roadmap-regressions/native-gate.cpp -o /out/native-gate-zero
timeout -k 5s 30s /out/native-gate-zero
clang++-18 -std=c++20 -O0 -g -Wall -Wextra -Werror -Wno-unused-parameter -fsanitize=address,undefined -fno-sanitize-recover=all -I/repo/headless -I/json /repo/tools/feature-roadmap-regressions/mod_settings_check.cpp -o /out/mod-settings
```

Run each settings case in a **fresh process and fresh empty scratch directory**:
the owned-staging sequence starts at zero per process. Invoke
`/out/mod-settings mod|all CASE 0|1 EMPTY_SCRATCH` for the cross-product of:

- Writers: `mod`, `all`.
- Enabled: `0`, `1`.
- Cases: `corrupt-open`, `corrupt-array`, `preserve`, `legacy-symlink`,
  `legacy-hardlink`, `owned-symlink`, `owned-hardlink`, `write-failure`, `missing`,
  `invalid-arguments`.

All 40 integrated cases must exit0, with no sanitizer diagnostics. Capture stdout,
stderr, command and exit **per case**, continue independent cases after a failure
and retain aggregate failure. Synthetic scratch is disposable, never user data.
The earlier aggregate runner's exit1 and four baseline diagnostic mismatches
remain preserved externally; a later run must not overwrite those receipts.

Run `mod known-gap-A14-05 1 EMPTY_SCRATCH` separately: exit0 plus
`KNOWN_GAP_A14_05` means the **known defect is still observed**, not repaired.
It witnesses two publications in UI order without claiming actual UI execution.
Do not add this to the 40 green security cases. Internal JSON-type, read/EIO,
close and general cancellation gaps remain outside this limited coverage.

The [forwarder include probe](../check-forwarded-dev-include-matrix.py) accepts
`--repo /repo --output /out/fresh-include-matrix` and optional `--baseline-main`
for the immutable original. It compiles only extracted preamble/consumers, using
a marker instead of SDK-dependent development input: no full-main/argv proof.
Supplemental A35 over-inclusion mutation controls remain separately NOT_RUN
unless a later receipt explicitly records them.
