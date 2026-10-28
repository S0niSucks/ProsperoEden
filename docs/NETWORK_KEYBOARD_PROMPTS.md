# Localized private native keyboard seam — UI composition PENDING

Continuation of `1ec3910` on `team/network-text-extraction-1006000`. This implements a backend
seam, **not** active UI localization. No changes to `EdenServices`, `network/services.cpp`,
`pe/ui`/host, CMake, catalogs/POT, USB or admission are included. Do not integrate across the
parent's reserved service area before its network/USB audit is released.

## Implemented contract

`headless/network/keyboard_prompt.h` exposes:

- `PrepareKeyboardPrompts(PromptTranslator translate = {}) -> optional<KeyboardPrompts>`.
  The translator is passed by value, receives only fixed English keys and is invoked **only
  synchronously inside this factory**. It is never saved in the snapshot/provider. The caller
  must run the factory on the catalog-owning/UI thread; the API does not magically dispatch it.
  An empty callback selects English. Callback exceptions, empty/invalid/oversized output return
  `nullopt`, without raw errors, partial publication or worker translation.
- An owned, immutable snapshot of 14 `KeyboardPrompt` values: `field`, UTF-16 `title`, UTF-16
  `hint`. Const members/private const storage and read-only `Find(Field)` expose no mutation.
  There is no entered/initial value, username, server, password, service or translator reference.
- `NativeNetworkKeyboardWithPrompts(snapshot, private_text, stop)` consumes that snapshot;
  `MakeNativeNetworkKeyboard(snapshot)` owns a copy and returns the existing private provider.
  Neither accesses `pe::tr` or a catalog. The old two-argument `NativeNetworkKeyboard` remains
  unambiguous and uses an immutable English fallback with no translator.

`keyboard.cpp` is the single runtime source of the **14 titles + two hints**, with inline
identity `TR` markers. The extractor adds only `network/keyboard.cpp` to its explicit allowlist.
Its original config/editor group is separate: **20 keys**, including the new default-name key,
with the original **19** still proved independently. The new prompt group is the **16 exact**
literals in the frozen audit table of `NETWORK_TEXT_EXTRACTION.md`, not just a whitelist count.
Together the two groups expose **36 unique keys**, an increase of **17** over `1ec3910`.

### Creation-only names

`DefaultNetworkLocationName()` returns the one marked English key `Network location`.
`Editor::Begin(id, initial_name)` accepts a copied label **only when `id` is empty**, before draft
replacement. It must be nonempty scalar UTF-8, at most 256 bytes, and satisfy the original
no-control/NUL constraint. Invalid new labels return `invalid` without destroying the draft.
The exact bytes are stored, with no normalization, implicit translation, truncation or language
refresh. Existing IDs ignore this argument entirely, even if it is invalid. Subsequent private
name entry remains user data and persists verbatim. `Begin(id)` keeps the English behavior.

### Encoding, native bounds and privacy

Titles and hints are validated **before publication**: scalar UTF-8, no embedded NUL, nonempty,
at most **127 UTF-16 units** (the existing native adapter's title/hint cap). Oversized translations
must be shortened/reviewed by the parent; the factory does not silently clip them or read another
catalog on a worker. Valid surrogate pairs count as two units. Overlong UTF-8, surrogate scalars,
out-of-range codepoints, malformed continuations and partial sequences are rejected.

Private initial input is independently bounded by its byte limit and at most **1023 UTF-16
units**. Accepted answers are checked against both the request's UTF-16 cap and the field's
UTF-8 byte cap. NUL/unpaired surrogates are rejected. Imported long paths are refused, not
truncated before the native call. No credential flag/answer API is added: password initial text
stays empty, the password flag stays enabled, and only `Editor::Enter` consumes accepted text.
The existing safe view exposes password state only. Cancellation/generation checks and enum-only
UI futures remain in place. This is private plaintext persistence, not secure-memory storage.

## Exact parent composition proposal — DOCUMENTARY ONLY, NOT APPLIED

These additions are deliberately **not** present in this branch's service interfaces. The parent
must compose the declarations, definitions and caller atomically after the audit, retaining its
own `WithNetworkMutation` reservation/admission lifetime and avoiding a UI mutex held over IME.
The old APIs can remain for English/host callers. Add `#include "network/keyboard_prompt.h"` where
these proposed types are declared/used; no UI library link dependency is needed by the backend.

Proposed additions in `pe/ui/services.hpp` (inside the incumbent services class):

```cpp
virtual Eden::Network::EditorResult network_editor_begin_named(
    const std::string&, std::string) { return Eden::Network::EditorResult::blocked; }
virtual Eden::Network::EditorResult network_editor_enter_prepared(
    Eden::Network::Field, const std::atomic<bool>&, Eden::Network::KeyboardPrompts) {
    return Eden::Network::EditorResult::blocked;
}
```

Matching additions in `EdenServices`' public declaration:

```cpp
Eden::Network::EditorResult network_editor_begin_named(const std::string&, std::string) override;
Eden::Network::EditorResult network_editor_enter_prepared(
    Eden::Network::Field, const std::atomic<bool>&, Eden::Network::KeyboardPrompts) override;
```

Matching definitions in `network/services.cpp`, **using the parent's audited admission seam**:

```cpp
Eden::Network::EditorResult EdenServices::network_editor_begin_named(
    const std::string& id, std::string initial_name) {
    return WithNetworkMutation([&] { return network_editor_.Begin(id, initial_name); });
}
Eden::Network::EditorResult EdenServices::network_editor_enter_prepared(
    Eden::Network::Field field, const std::atomic<bool>& stop,
    Eden::Network::KeyboardPrompts prompts) {
    const auto keyboard = Eden::Network::MakeNativeNetworkKeyboard(std::move(prompts));
    return WithNetworkMutation([&] { return network_editor_.Enter(field, stop, keyboard); });
}
```

In `Launcher::press_network_shares`, replace the creation/edit selection call on the UI thread:

```cpp
const std::string id = network_row_ < locations ? network_view_.locations[network_row_].id : "";
const std::string initial_name = id.empty()
    ? tr(std::string{Eden::Network::DefaultNetworkLocationName()}) : std::string{};
const auto result = services_.network_editor_begin_named(id, initial_name);
```

The incumbent selection/Begin is synchronous. If the parent makes it async later, capture
`id` and `initial_name` **by value before** `std::async`; never call `tr` in that worker. Existing
name rows/location names must continue to be drawn as data, not passed to `tr`.

Replace only the text-entry branch's job construction with this UI-thread preparation:

```cpp
auto prepared = Eden::Network::PrepareKeyboardPrompts([](std::string_view key) {
    return tr(std::string{key}); // synchronous UI thread; owned copy via incumbent tr(string) overload
});
if (!prepared) { network_result(EditorResult::invalid); return; }
network_stop_ = false;
network_commit_job_ = false;
network_job_ = std::async(std::launch::async,
    [this, field, prompts = *prepared] {
        return services_.network_editor_enter_prepared(field, network_stop_, prompts);
    });
```

The future still returns only `EditorResult`. The snapshot owns text across a language change or
catalog destruction; the provider does not capture that translator, `this` or catalog references.
The existing launcher lifetime/cancel/join path must remain authoritative. Do not replace it with
a detached task or bypass parent USB/launch admission.

## Focused acceptance

Docker-only runner: `python3 -B /src/tools/check-network-keyboard-prompts.py` in the bounded private
container; optional `PS5_PAYLOAD_SDK=/work/sdk/ps5-payload-sdk` adds syntax checks. It compiles the
real network backend into a **new private** output directory, reuses existing pinned client
archives, and links no UI objects and no `pe::tr` stub. It never runs CMake, rebuilds emulator/
clients, probes a LAN endpoint, edits a parent snapshot or publishes artifacts.

The new `keyboard_prompt_check.cpp` uses only a synthetic `AskSystemKeyboard` answer seam and
the real `keyboard.cpp`, `Editor`, validation and private writer. It covers 16 factory calls on
the factory thread, translator-capture release, immutable lifetime across language changes,
enum-only async entry, creation-only byte-exact names, unchanged existing/custom names,
cancellation before/during IME and generation changes, masking/empty initial/no echo,
UTF-8/UTF-16 supplementary characters, NUL/invalid sequences, 127/1023 native caps and 256-byte
name boundaries, refused imported long paths, unavailable/exception refusal and English fallback.
The previous `editor_check.cpp` is linked/run as well, without changing that test or CMake.
The five documentary C++ blocks above are syntax-checked together with the real header and
incumbent `tr` declarations, in an isolated facade only; this is **not** compiling/composing
the actual service/caller or linking a `pe::tr` stub.

`tools/launcher/test-strings.py` separately asserts original **19**, backend **20**, prompt **16**
and `.inc`/provenance preservation; it does not infer completeness from allowlist growth.
`test-import-translations.py` retains the SHA-bound **81×30** input unchanged and admits the new
**17×30** using **synthetic fixture labels only**. Real translations remain translator/parent work.

Final local results: **PASS** for the normal and ASan/UBSan runs (leak detection and
halt-on-error enabled), previous private editor/config regressions, no `pe::tr`/`pe::Catalog`
undefined references in the backend objects, documentary proposal syntax, SDK changed
backend/header/native adapter syntax, exact extraction groups and additive fixture imports.
The parent reference's **659 pre-existing msgids/provenances**, including `.inc`, were preserved.
Only this feature's C++ objects were rebuilt/instrumented; pinned SMB/NFS client archives were
reused unchanged. Native IME itself was not executed. Sources were read-only, build/fixtures
stayed on the private `/work` volume, with 2 CPUs/3 GiB; parent snapshots/evidence were untouched.
Final private logs are `/work/network-keyboard-prompts-final.log` and
`/work/network-keyboard-prompts-strings-final.log`, with the 19/20/16/17 receipt under
`/work/network-keyboard-prompts/extraction-receipt.json`. Copies/hashes are kept in this
worktree's ignored `.local/network-keyboard-prompts/`; these are not published evidence.

**Still PENDING:** applying/reviewing the service/caller proposal above, parent admission and
teardown qualification, actual 17×30 catalog publication/font/layout review, executing PS5 IME,
hardware testing and full-app acceptance. Host/sanitizer/SDK syntax evidence cannot qualify these.
