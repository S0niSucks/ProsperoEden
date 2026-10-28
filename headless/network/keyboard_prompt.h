// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "editor.h"
#include <optional>
#include <string_view>
#include <utility>

namespace Eden::Network {
// Public prompt data only: no initial text, entered answers, credentials or catalog references.
struct KeyboardPrompt {
    const Field field;
    const std::u16string title, hint;
};
using PromptTranslator = std::function<std::string(std::string_view)>;
class KeyboardPrompts {
public:
    const KeyboardPrompt* Find(Field field) const;
private:
    explicit KeyboardPrompts(std::vector<KeyboardPrompt> prompts) : prompts(std::move(prompts)) {}
    const std::vector<KeyboardPrompt> prompts;
    friend std::optional<KeyboardPrompts> PrepareKeyboardPrompts(PromptTranslator);
};
// Call synchronously on the catalog-owning/UI thread. The copied callback is used ONLY here,
// never retained. Empty callback selects English. Invalid/oversized translations fail closed.
std::optional<KeyboardPrompts> PrepareKeyboardPrompts(PromptTranslator translate = {});
// Distinct name preserves the existing two-argument function/default provider unambiguously.
PrivateAnswer NativeNetworkKeyboardWithPrompts(const KeyboardPrompts&, const PrivateText&, const std::atomic<bool>&);
// Captures only an owned immutable prompt snapshot, not the translator or its catalog.
PrivateKeyboard MakeNativeNetworkKeyboard(KeyboardPrompts prompts);
} // namespace Eden::Network
