// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor.h"
#include "keyboard_prompt.h"
#include "system_keyboard.h"
#include "prosperoeden/pe/core/strings.hpp" // Identity markers only; no pe::tr/catalog on this worker.
#include <algorithm>
#include <stdexcept>

namespace Eden::Network {
namespace {
constexpr std::size_t kPromptUnits = 127, kInputUnits = 1023;
// One source for the 14 native titles and two hints. These are English catalog keys, not
// pretranslated backend errors. Only PrepareKeyboardPrompts invokes the caller's translator.
constexpr struct { Field field; const char* title; } kTitles[] = {
    {Field::name, TR("Network location name")},
    {Field::server, TR("Network server IPv4")},
    {Field::share, TR("Network share / export")},
    {Field::folder, TR("Network games folder (relative)")},
    {Field::updates, TR("Network updates / DLC folder (relative)")},
    {Field::user, TR("SMB user (empty = guest)")},
    {Field::password, TR("SMB password (private replacement)")},
    {Field::domain, TR("SMB domain (optional)")},
    {Field::uid, TR("NFS AUTH_SYS UID")},
    {Field::gid, TR("NFS AUTH_SYS GID")},
    {Field::mount_port, TR("NFS mount port (0 = portmapper)")},
    {Field::port, TR("Network server TCP port")},
    {Field::cache, TR("Network cache (1-32 MiB)")},
    {Field::timeout, TR("Network timeout (250-15000 ms)")},
};
constexpr auto kPasswordHint = TR("Enter a replacement; Cancel keeps the current password");
constexpr auto kValueHint = TR("Confirm to use this value; Cancel keeps the previous value");
std::u16string To16(std::string_view value, std::size_t max_units) {
    if (value.size() > max_units * 4) throw std::runtime_error("encoding");
    std::u16string out;
    for (std::size_t i = 0; i < value.size();) {
        auto c = static_cast<unsigned char>(value[i++]); unsigned code = c, count = 0, minimum = 0;
        if (c >= 0xc2 && c <= 0xdf) { code = c & 31; count = 1; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { code = c & 15; count = 2; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { code = c & 7; count = 3; minimum = 0x10000; }
        else if (c >= 0x80) throw std::runtime_error("encoding");
        while (count > 0) { --count; if (i >= value.size() || (static_cast<unsigned char>(value[i]) & 0xc0) != 0x80) throw std::runtime_error("encoding");
            code = (code << 6) | (static_cast<unsigned char>(value[i++]) & 63); }
        if (!code || code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) throw std::runtime_error("encoding");
        if (code >= 0x10000) { code -= 0x10000; out += static_cast<char16_t>(0xd800 + (code >> 10)); out += static_cast<char16_t>(0xdc00 + (code & 1023)); }
        else out += static_cast<char16_t>(code);
        if (out.size() > max_units) throw std::runtime_error("encoding");
    }
    return out;
}
std::string To8(const std::u16string& value) {
    std::string out;
    for (std::size_t i = 0; i < value.size(); ++i) {
        unsigned code = value[i];
        if (!code) throw std::runtime_error("encoding");
        if (code >= 0xd800 && code <= 0xdbff) {
            if (++i == value.size() || value[i] < 0xdc00 || value[i] > 0xdfff) throw std::runtime_error("encoding");
            code = 0x10000 + ((code - 0xd800) << 10) + value[i] - 0xdc00;
        } else if (code >= 0xdc00 && code <= 0xdfff) throw std::runtime_error("encoding");
        if (code < 0x80) out += static_cast<char>(code);
        else if (code < 0x800) { out += static_cast<char>(0xc0 | (code >> 6)); out += static_cast<char>(0x80 | (code & 63)); }
        else if (code < 0x10000) { out += static_cast<char>(0xe0 | (code >> 12)); out += static_cast<char>(0x80 | ((code >> 6) & 63)); out += static_cast<char>(0x80 | (code & 63)); }
        else { out += static_cast<char>(0xf0 | (code >> 18)); out += static_cast<char>(0x80 | ((code >> 12) & 63)); out += static_cast<char>(0x80 | ((code >> 6) & 63)); out += static_cast<char>(0x80 | (code & 63)); }
    }
    return out;
}
}
const KeyboardPrompt* KeyboardPrompts::Find(Field field) const {
    for (const auto& prompt : prompts) if (prompt.field == field) return &prompt;
    return nullptr;
}
std::optional<KeyboardPrompts> PrepareKeyboardPrompts(PromptTranslator translate) {
    try {
        const auto resolve = [&](std::string_view key) {
            return To16(translate ? translate(key) : std::string{key}, kPromptUnits);
        };
        const auto password_hint = resolve(kPasswordHint), value_hint = resolve(kValueHint);
        std::vector<KeyboardPrompt> prompts;
        prompts.reserve(std::size(kTitles));
        for (const auto& [field, title] : kTitles) {
            auto resolved = resolve(title);
            if (resolved.empty()) return std::nullopt;
            prompts.push_back({field, std::move(resolved), field == Field::password ? password_hint : value_hint});
        }
        if (password_hint.empty() || value_hint.empty()) return std::nullopt;
        return KeyboardPrompts{std::move(prompts)};
    } catch (...) { return std::nullopt; } // no catalog/exception details cross the private boundary
}
PrivateKeyboard MakeNativeNetworkKeyboard(KeyboardPrompts prompts) {
    return [prompts = std::move(prompts)](const PrivateText& text, const std::atomic<bool>& stop) {
        return NativeNetworkKeyboardWithPrompts(prompts, text, stop);
    };
}
PrivateAnswer NativeNetworkKeyboard(const PrivateText& text, const std::atomic<bool>& stop) {
    if (stop) return {EditorResult::cancelled, {}};
    // Backward-compatible English fallback. No catalog/translator is used, even on first call.
    static const auto prompts = PrepareKeyboardPrompts();
    if (!prompts) return {EditorResult::invalid, {}};
    return NativeNetworkKeyboardWithPrompts(*prompts, text, stop);
}
PrivateAnswer NativeNetworkKeyboardWithPrompts(const KeyboardPrompts& prompts, const PrivateText& text,
                                               const std::atomic<bool>& stop) {
    if (stop) return {EditorResult::cancelled, {}};
    const auto* prompt = prompts.Find(text.field);
    if (!prompt || !text.limit || text.password != (text.field == Field::password)) return {EditorResult::invalid, {}};
    Eden::TextRequest request;
    request.title = prompt->title; request.placeholder = prompt->hint;
    request.password = text.password; request.numbers = text.numbers; request.max_length = text.limit;
    try {
        request.max_length = std::min(text.limit, kInputUnits); // native IME's hard limit
        if (!text.password) {
            if (text.initial.size() > text.limit) return {EditorResult::invalid, {}};
            request.initial = To16(text.initial, request.max_length); // reject; never truncate imported data
        }
        const auto answer = Eden::AskSystemKeyboard(request, stop);
        if (stop || answer.outcome == Eden::TextOutcome::cancelled) return {EditorResult::cancelled, {}};
        if (answer.outcome != Eden::TextOutcome::accepted) return {};
        if (answer.text.size() > request.max_length) return {EditorResult::invalid, {}};
        auto value = To8(answer.text);
        if (value.size() > text.limit) return {EditorResult::invalid, {}};
        return {EditorResult::ok, std::move(value)};
    } catch (...) { return {EditorResult::invalid, {}}; }
}
} // namespace Eden::Network
