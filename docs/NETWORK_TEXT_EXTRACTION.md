# Network text extraction follow-up to 251c732

Branch: `team/network-text-extraction-1006000`, based directly on `251c732`.
The initial `1ec3910` follow-up is extraction metadata and fixture coverage only. It does not compose or change USB/launch
admission, `EdenServices`, native UI, keyboard behavior, CMake, catalog files or the POT.
The separate continuation implements the private native prompt seam and creation-only name
option; see `NETWORK_KEYBOARD_PROMPTS.md`. Actual UI/service/admission composition is **PENDING**.

## Exact coverage and counts

The read-only translation handoff is the parent checkout's
`.local/agent-worktrees/usb-backup/.local/handoffs/network-editor-translations-251c732/`.
`input.json` SHA-256:
`304bc230c8291da0912832a32a07a4f139911f264d8b7e61bf63fa4dcbdc5905`.

- `NETWORK_EDITOR_MSGIDS.txt`: **89 physical lines, 83 unique msgids**.
- Candidate union: **86 keys** (83 handoff strings + three Library/Settings strings).
- Reused existing keys: `Back`, `Back / cancel`, `Choose`, `Off`, `On`.
- Translation input: **81 new keys × 30 locales = 2430 cells**; existing translations are not replaced.
- `coverage.json::feature_needs_extraction_marking`: **19 exact-match unique keys**,
  comprising **13 messages + six states**. There are 20 inline marker occurrences because the
  cancellation message is returned by both config and editor. No message bytes changed.

`config.cpp` and `editor.cpp` include the lightweight `pe/core/strings.hpp` declaration header
and wrap these existing literals with its identity macro `TR(...)`. The backend still returns
English keys. It does not call `pe::tr`, read a catalog, translate on a worker or add a UI link
dependency. The unchanged native view translates those keys where displayed. The existing
handoff, not a second backend-message table, supplies the regression's expected set.

The extractor retains the current scanner/adjacent-literal/comment handling and all old source
names, including the `.inc` support from parent commit `42a20b4` absent in the 251 baseline.
Only marked text from these two additional source paths is admitted:

- `headless/network/config.cpp`, provenance `network/config.cpp`.
- `headless/network/editor.cpp`, provenance `network/editor.cpp`.

It does not recursively scan other headless/network sources, keyboard prompts or diagnostics.
Missing optional backend source files are ignored, as absent launcher source matches are.

## Frozen 1ec3910 audit: 17 literals missing from the unchanged 81-key input

All **16** native keyboard literals below are absent from the 81 new keys and from the complete
86-key candidate union. The table/line numbers describe `1ec3910`, and are retained as the
independent exact-match regression receipt. The continuation now marks these literals in
`keyboard.cpp`; it does not merge translations or modify the original 81-key input.

| Source in `headless/network/keyboard.cpp` | Exact visible English literal |
| --- | --- |
| line 45, title | `Network location name` |
| line 46, title | `Network server IPv4` |
| line 47, title | `Network share / export` |
| line 48, title | `Network games folder (relative)` |
| line 49, title | `Network updates / DLC folder (relative)` |
| line 50, title | `SMB user (empty = guest)` |
| line 51, title | `SMB password (private replacement)` |
| line 52, title | `SMB domain (optional)` |
| line 53, title | `NFS AUTH_SYS UID` |
| line 54, title | `NFS AUTH_SYS GID` |
| line 55, title | `NFS mount port (0 = portmapper)` |
| line 56, title | `Network server TCP port` |
| line 57, title | `Network cache (1-32 MiB)` |
| line 58, title | `Network timeout (250-15000 ms)` |
| line 61, password hint | `Enter a replacement; Cancel keeps the current password` |
| line 61, other hint | `Confirm to use this value; Cancel keeps the previous value` |

One additional visible literal is outside those 16 prompts: `Network location`, the persisted
default name at `editor.cpp:130` in `1ec3910`. It is absent from the same candidate union. This makes **17
additional visible literals** found in this narrow audit. Its continuation marks the key in
`DefaultNetworkLocationName()` and accepts a copied creation-only label via `Begin(id, initial_name)`;
this is still not 17 deployed UI translations.
User-entered names/paths/credentials and internal protocol/JSON/error tokens are not msgids.
This is not an audit of every transport/launcher diagnostic.

**Parent composition still pending:** resolve prompt title/hint keys synchronously on
the UI thread using the active catalog, convert/copy the resolved UTF-16 strings, then pass an
immutable, credential-free prompt snapshot into the private keyboard future. Do not read the
mutable global catalog from the asynchronous backend or add a `pe::tr` stub/link workaround.
Preserve masking, empty password initial text, private answer consumption and enum-only futures.
The continuation implements the creation-only display-versus-storage contract: never retranslate
stored/user-entered names. No service/interface/admission change is made here while the USB audit
owns that area. The exact future parent API/caller snippets are documentary only in
`NETWORK_KEYBOARD_PROMPTS.md`.

Current scoped extraction is **20 config/editor keys + 16 keyboard prompt keys = 36**, with the
original **19** independently verified against the original handoff/coverage set. The 17-key
continuation does not imply complete UI, native IME or 30-locale acceptance. Only
`network/keyboard.cpp` is added to the explicit allowlist; no broad headless scan is introduced.

## Initial 1ec3910 fixture validation and parent composition

All tests ran Docker-only with read-only `/src` and `/parent`, private `/work`, 2 CPUs / 3 GiB.
The Python reference container used `--network none` and disabled bytecode writes. No source
catalog, POT, parent file, image or release artifact was written.

- `test-strings.py`: the existing C++/header/`.inc`/settings/adjacent-literal/comment cases remain;
  additional fixtures enforce the exact two-file allowlist and provenance. Real extraction adds
  **exactly the 19 keys** listed in the independent coverage receipt. Composing only the new
  backend sources over the parent's current scanner preserved all **659 existing msgids and
  their provenance**, including `.inc` entries, with exactly 19 additions.
- `test-import-translations.py`: retains the parent's additive/escaping/no-op/negative cases,
  then imports the real extracted 19 backend keys into fixtures. The SHA-bound 81×30 input is
  admitted using the actual parent importer, preserves existing fixture bytes, is idempotent,
  and refuses a mutated real feature placeholder before any fixture write.
- `g++ -std=c++20 -Wall -Wextra -Werror -fsyntax-only`: both changed backend sources passed.
- The normal `eden-network-editor-check` target passed without any UI objects or a `pe::tr` stub.
  `nm -C -u libeden-network.a` found no `pe::tr` / `pe::Catalog` references. No new CMake link
  dependency was introduced.

The 251 baseline has neither the parent's importer nor its two test files. This follow-up does
not backport the production importer: its test accepts `LAUNCHER_TRANSLATION_IMPORTER` pointing
to the read-only parent file. After composition, the normal neighboring importer is the default.
The new test files can therefore be an add/add resolution on the parent; retain its existing
cases and append the regressions rather than replacing newer parent work. The extractor diff
also explicitly retains `42a20b4`'s `.inc` addition.

Reproduce the Python tests with these environment variables inside the bounded reference
container (every temporary catalog stays under private `TMPDIR`):

```sh
export TMPDIR=/work/network-text-extraction/tmp
export PYTHONDONTWRITEBYTECODE=1
export NETWORK_STRINGS_REFERENCE_ROOT=/parent
export LAUNCHER_TRANSLATION_IMPORTER=/parent/tools/launcher/import-translations.py
export NETWORK_TRANSLATION_HANDOFF=/parent/.local/agent-worktrees/usb-backup/.local/handoffs/network-editor-translations-251c732/input.json
export NETWORK_TRANSLATION_COVERAGE=/parent/.local/agent-worktrees/usb-backup/.local/handoffs/network-editor-translations-251c732/coverage.json
python3 -B /src/tools/launcher/test-strings.py
python3 -B /src/tools/launcher/test-import-translations.py
```

Private logs/audit JSON and a minimal test/extractor resolution patch against the read-only
parent reference are in `/work/network-text-extraction/`, copied into this worktree's ignored
`.local/network-text-extraction/`. These are fixture/extraction results, not native prompt,
console font/layout, hardware, full native/emulator build or actual catalog-publication evidence.
