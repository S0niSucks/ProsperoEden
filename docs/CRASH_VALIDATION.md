# Crash validation for the integration build

Build/test results are not proof that every game crash is fixed. This checklist keeps
the supplied crash evidence, the relevant changes and console retests separate.

## Supplied archive, 27 October 2026

The local audit read all 23 entries (520,424 uncompressed bytes). It found four actual
crashes with **one distinct signature**: Galaxy 2, `nvservices`, SIGSEGV at `0x10000`,
immediately after `common/multi_level_page_table.inc:36: assert base`.

The crashes happened about 2 seconds into Galaxy initialization. In the longest
process, Mario Kart ran beforehand; the process uptime was not Galaxy playtime.
The logs identify a **17 October diagnostic executable, version 01.004.000**, not
the subsequent integration executable. Its code size differs from the available ELF.
No function names were assigned using those mismatched symbols.

The exact report, extracted input, signatures, source hashes and chronology stay local
in `.local/upstream-audit/user-crashes-20261027/`. Raw user logs are not shipped.

## GPU address-space table

- Existing upstream **#237 / `1390bf5`** already replaces the large anonymous mapping
  with the port's sparse/direct-memory allocation and commits chunks before writing.
  It is directly relevant to this signature, not a general fix for every GPU crash.
- The integration adds a constructor regression on the actual generated table:
  40-bit space, zero reads, 2 MiB slot boundaries, multiple tables, five destruction/
  recreation cycles, dense fallback and injected allocation refusal. The old anonymous
  allocation and a removed null guard are negative controls.
- Reservation failure logs the address/page/level bits, entry width, byte count and
  errno, then throws before exposing a null table. The allocator logs sparse-refusal
  stage and dense-fallback failure. This is fail-closed diagnostics, **not a guarantee
  that out-of-memory cannot end a game**.
- Status of the supplied signature: **candidate fix, console retest required**.

### Console retest

Keep the package checksum, `build-identity.json` and matching unstripped ELF. Verify
the boot trace names this build identity rather than an old eboot left in `/app0`.

Use Galaxy 2 `0100FD8022DAA000`, effective update `0.5.0`, guest build
`7FF9F6FE1D509C76153E927E2A6CF8CCC9C4F6C2`, no mods/cheats, Vulkan 1x/Bilinear,
1080p/60 Hz, matching the supplied failure as closely as possible.

1. Cold boot, start Galaxy, reach a playable scene.
2. Start Mario Kart, return by shortcut, start Galaxy.
3. Repeat at least five complete launch/return cycles and one prolonged session.
4. Retain the boot trace, stderr, Eden log and any crash reports for each build.

Pass only if the game progresses beyond initialization without this assertion or
equivalent SIGSEGV, including the repeated table release/recreation path.

## Other reports need their own reproduction

The archive contains no primary crash evidence for guest write promotion (#246/#296/
#301), UltraCam/heap exhaustion, ExecuteProgram (#298) or suspend/controller recovery
(#97/#269). Zero pipelines in a game that crashes during initialization do not establish
shader-cache loss (#297). Do not mark those issues fixed from this archive or from a
generic successful launch.

## Symbol identity

Packaging stamps an identity into a readable ELF field: SHA-256 of the entire
unstripped ELF with only that field normalized to zero. The boot trace and crash
report copy it without reading executable-only code or hashing inside a signal handler.
`tools/symbolize-crash.py` checks that identity against the supplied ELF's actual bytes,
as well as code size, **before** invoking the symbolizer. Missing/mismatched/modified
builds are refused; their offsets remain raw. This is identity checking, not signing.

`make release` also writes `dist/build-identity.json` with the stamped identity, full
ELF/eboot/ZIP hashes and, for the Docker runner, the verified source snapshot (HEAD,
dirty flag, diff hash and per-file hashes). Preserve it with the symbols and checksum
file. Legacy reports without an identity cannot be automatically trusted by this tool.
