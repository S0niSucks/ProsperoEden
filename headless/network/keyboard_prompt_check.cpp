// SPDX-License-Identifier: GPL-3.0-or-later
// Focused backend fixture: real keyboard adapter + Editor, synthetic AskSystemKeyboard only.
#include "keyboard_prompt.h"
#include "system_keyboard.h"
#include <cassert>
#include <filesystem>
#include <future>
#include <iostream>
#include <memory>
#include <thread>
#include <type_traits>

using namespace Eden::Network;
namespace {
Eden::TextRequest last;
Eden::TextAnswer next;
std::function<void()> during_ask;
unsigned asks = 0;
std::string Row(const Editor& editor, Field field) {
    for (const auto& row : editor.View().rows) if (row.field == field) return row.value;
    assert(false); return {};
}
void NoEcho(const Editor& editor, std::string_view secret) {
    const auto view = editor.View();
    assert(view.id.find(secret) == view.id.npos);
    for (const auto& row : view.rows) assert(row.value.find(secret) == row.value.npos);
    for (const auto& location : view.locations) assert(location.name.find(secret) == location.name.npos);
}
std::string Repeat(std::string_view text, unsigned count) {
    std::string value;
    while (count--) value += text;
    return value;
}
}
namespace Eden {
TextAnswer AskSystemKeyboard(const TextRequest& request, const std::atomic<bool>&) {
    ++asks; last = request;
    assert(request.title.size() <= 127 && request.placeholder.size() <= 127);
    assert(request.title.find(u'\0') == request.title.npos && request.placeholder.find(u'\0') == request.placeholder.npos);
    assert(request.initial.size() <= request.max_length && request.max_length <= 1023);
    if (request.password) assert(request.initial.empty());
    if (during_ask) during_ask();
    return next;
}
}
int main(int argc, char** argv) {
    assert(argc == 2);
    static_assert(!std::is_copy_assignable_v<KeyboardPrompts>);
    static_assert(!std::is_assignable_v<decltype(KeyboardPrompt::title)&, std::u16string>);
    static_assert(!std::is_assignable_v<decltype(KeyboardPrompt::field)&, Field>);
    static_assert(std::is_same_v<decltype(std::declval<Editor>().Enter(Field::name,
                  std::declval<const std::atomic<bool>&>())), EditorResult>);
    const auto ui_thread = std::this_thread::get_id();
    unsigned translations = 0;
    std::weak_ptr<int> released;
    std::string language = "A: ";
    PrivateKeyboard keyboard;
    {
        auto token = std::make_shared<int>(1); released = token;
        const auto prepared = PrepareKeyboardPrompts([token, &translations, &language, ui_thread](std::string_view key) {
            assert(std::this_thread::get_id() == ui_thread);
            ++translations; return language + std::string{key};
        });
        assert(prepared && translations == 16);
        for (const auto field : {Field::name, Field::server, Field::share, Field::folder, Field::updates,
             Field::user, Field::password, Field::domain, Field::uid, Field::gid, Field::mount_port,
             Field::port, Field::cache, Field::timeout}) {
            assert(prepared->Find(field) && prepared->Find(field)->field == field);
        }
        assert(!prepared->Find(Field::protocol) && !prepared->Find(Field::enabled) && !prepared->Find(Field::encrypt));
        keyboard = MakeNativeNetworkKeyboard(*prepared);
    }
    assert(released.expired()); // neither snapshot nor worker holds translator/captured state
    language = "B: "; // active language/catalog changes after factory and snapshot destruction
    namespace fs = std::filesystem;
    const auto root = fs::path{argv[1]}; fs::create_directories(root / "config");
    const auto file = (root / "config/network-shares.json").string();
    Editor editor; std::atomic<bool> stop{false};
    assert(editor.Open(file) == EditorResult::ok);
    assert(DefaultNetworkLocationName() == "Network location");
    assert(editor.Begin("") == EditorResult::ok && Row(editor, Field::name) == "Network location");
    const std::string created_name = "Localiza\xc3\xa7\xc3\xa3o \xf0\x9f\x8e\xae";
    assert(editor.Begin("", created_name) == EditorResult::ok && Row(editor, Field::name) == created_name);
    const auto enter = [&](Field field, std::u16string value) {
        next = {Eden::TextOutcome::accepted, std::move(value)};
        // The only future result is an enum. Enter alone consumes the private accepted answer.
        auto future = std::async(std::launch::async, [&] { return editor.Enter(field, stop, keyboard); });
        static_assert(std::is_same_v<decltype(future.get()), EditorResult>);
        return future.get();
    };
    assert(enter(Field::server, u"127.0.0.1") == EditorResult::ok);
    assert(last.title == u"A: Network server IPv4" && !last.password && !last.numbers && last.max_length == 15);
    assert(enter(Field::share, u"games") == EditorResult::ok);
    assert(enter(Field::port, u"445") == EditorResult::ok && last.numbers && !last.password && last.max_length == 10);
    assert(enter(Field::user, u"fixture-user") == EditorResult::ok);
    assert(enter(Field::password, u"Synthetic-private-only") == EditorResult::ok);
    assert(last.title == u"A: SMB password (private replacement)" && last.password && !last.numbers);
    assert(last.initial.empty() && last.max_length == 256);
    assert(last.placeholder == u"A: Enter a replacement; Cancel keeps the current password");
    NoEcho(editor, "Synthetic-private-only");
    assert(translations == 16 && editor.Save(stop) == EditorResult::ok);
    auto saved = ReadConfiguration(file);
    const auto id = saved.shares.at(0).id;
    assert(saved.shares[0].name == created_name && saved.shares[0].password == "Synthetic-private-only");
    // Existing and explicitly edited names are data, never refreshed from translated defaults.
    assert(editor.Begin(id, "B: localized default") == EditorResult::ok && Row(editor, Field::name) == created_name);
    assert(editor.Begin(id, std::string(257, 'x')) == EditorResult::ok && Row(editor, Field::name) == created_name);
    assert(enter(Field::name, u"User chosen \U0001f3ae") == EditorResult::ok);
    assert(last.initial == u"Localiza\u00e7\u00e3o \U0001f3ae");
    assert(last.placeholder == u"A: Confirm to use this value; Cancel keeps the previous value");
    assert(editor.Save(stop) == EditorResult::ok);
    assert(editor.Begin(id, "B: localized default") == EditorResult::ok && Row(editor, Field::name) == "User chosen \xf0\x9f\x8e\xae");
    assert(editor.Begin(id) == EditorResult::ok && Row(editor, Field::name) == "User chosen \xf0\x9f\x8e\xae");
    const auto old = ReadConfiguration(file).bytes;
    // Early and in-flight stop, user cancel and editor-generation cancellation discard answers.
    auto count = asks;
    stop = true; assert(editor.Enter(Field::password, stop, keyboard) == EditorResult::cancelled && asks == count); stop = false;
    next = {Eden::TextOutcome::cancelled, u"Never published private answer"};
    assert(editor.Enter(Field::password, stop, keyboard) == EditorResult::cancelled && last.initial.empty());
    next = {Eden::TextOutcome::accepted, u"Never published private answer"};
    during_ask = [&] { stop = true; };
    assert(editor.Enter(Field::password, stop, keyboard) == EditorResult::cancelled);
    stop = false; during_ask = [&] { editor.Cancel(); };
    assert(editor.Enter(Field::password, stop, keyboard) == EditorResult::cancelled);
    during_ask = {};
    assert(ReadConfiguration(file).bytes == old);
    assert(editor.Begin(id) == EditorResult::ok);
    // Native and UTF-8 byte limits are independent. Never accept truncated data or bad surrogates/NUL.
    for (const auto& invalid : {std::u16string(1, 0xd800), std::u16string(1, 0xdc00),
         std::u16string{0xd800, u'x'}, std::u16string{u'a', 0, u'b'}, std::u16string(257, u'x'),
         std::u16string(1024, u'x')}) {
        assert(enter(Field::name, invalid) == EditorResult::invalid);
        assert(Row(editor, Field::name) == "User chosen \xf0\x9f\x8e\xae");
    }
    std::u16string emoji;
    for (unsigned i = 0; i < 64; ++i) emoji += u"\U0001f3ae";
    assert(enter(Field::name, emoji) == EditorResult::ok); // exactly 256 UTF-8 bytes
    assert(enter(Field::name, emoji + u"\U0001f3ae") == EditorResult::invalid);
    assert(Row(editor, Field::name) == Repeat("\xf0\x9f\x8e\xae", 64));
    assert(enter(Field::password, std::u16string{u'x', 0, u'y'}) == EditorResult::invalid);
    NoEcho(editor, "Synthetic-private-only");
    const auto prefix = Repeat(std::string(250, 'x') + '/', 4);
    const auto edge = prefix + std::string(19, 'x');
    assert(edge.size() == 1023);
    assert(enter(Field::folder, std::u16string(edge.begin(), edge.end())) == EditorResult::ok);
    assert(last.max_length == 1023 && Row(editor, Field::folder) == edge);
    assert(enter(Field::folder, std::u16string(1024, u'x')) == EditorResult::invalid);
    assert(Row(editor, Field::folder) == edge);
    auto unicode_edge = std::u16string(prefix.begin(), prefix.end()) + std::u16string(17, u'x') + u"\U0001f3ae";
    assert(unicode_edge.size() == 1023);
    assert(enter(Field::folder, unicode_edge) == EditorResult::ok);
    assert(enter(Field::folder, unicode_edge) == EditorResult::ok && last.initial == unicode_edge);
    assert(Row(editor, Field::folder) == prefix + std::string(17, 'x') + "\xf0\x9f\x8e\xae");
    editor.Cancel();
    // The original two-argument provider remains usable and English, with no overload ambiguity.
    assert(editor.Begin(id) == EditorResult::ok);
    next = {Eden::TextOutcome::accepted, u"English fallback"};
    assert(editor.Enter(Field::name, stop) == EditorResult::ok && last.title == u"Network location name");
    during_ask = [] { throw std::runtime_error("Never propagate synthetic private details"); };
    assert(editor.Enter(Field::password, stop, keyboard) == EditorResult::invalid);
    during_ask = {};
    next = {Eden::TextOutcome::unavailable, {}};
    assert(editor.Enter(Field::name, stop, keyboard) == EditorResult::unavailable);
    editor.Cancel();
    // Bad new defaults do not replace an existing draft; exact bytes of a valid new label survive save.
    assert(editor.Begin("", created_name) == EditorResult::ok);
    const auto draft_id = editor.View().id;
    const std::vector<std::string> invalid_utf8 = {"\x80", "\xc0\x80", "\xc2", "\xe0\x80\x80", "\xed\xa0\x80",
        "\xf0\x80\x80\x80", "\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xe2\x28\xa1", std::string{"a\0b", 3}};
    auto invalid_names = invalid_utf8;
    invalid_names.insert(invalid_names.end(), {"", "bad\nname", std::string(257, 'x'), Repeat("\xf0\x9f\x8e\xae", 65)});
    for (const auto& invalid : invalid_names) {
        assert(editor.Begin("", invalid) == EditorResult::invalid);
        assert(editor.View().id == draft_id && Row(editor, Field::name) == created_name);
    }
    assert(editor.Begin("", Repeat("\xf0\x9f\x8e\xae", 64)) == EditorResult::ok);
    editor.Cancel();
    // Translation errors are refused during factory execution; not truncated or deferred to a worker.
    auto invalid_prompts = invalid_utf8;
    invalid_prompts.insert(invalid_prompts.end(), {"", std::string(128, 'x'), Repeat("\xf0\x9f\x8e\xae", 64)});
    for (const auto& invalid : invalid_prompts)
        assert(!PrepareKeyboardPrompts([&](std::string_view) { return invalid; }));
    assert(!PrepareKeyboardPrompts([](std::string_view) -> std::string { throw std::runtime_error("Private catalog error"); }));
    const auto at_limit = PrepareKeyboardPrompts([](std::string_view) { return Repeat("\xf0\x9f\x8e\xae", 63) + "x"; });
    assert(at_limit && at_limit->Find(Field::name)->title.size() == 127);
    assert(PrepareKeyboardPrompts([](std::string_view) { return std::string(127, 'x'); }));
    const auto english = PrepareKeyboardPrompts(); assert(english);
    // Validate raw private initial values before IME allocation; no invocation and no silent clipping.
    for (const auto& invalid : invalid_utf8) {
        count = asks;
        assert(NativeNetworkKeyboardWithPrompts(*english, {Field::name, invalid, 256, false, false}, stop).result == EditorResult::invalid);
        assert(asks == count);
    }
    count = asks;
    assert(NativeNetworkKeyboardWithPrompts(*english, {Field::folder, std::string(1024, 'x'), 2048, false, false}, stop).result == EditorResult::invalid);
    assert(asks == count);
    assert(NativeNetworkKeyboardWithPrompts(*english, {Field::protocol, {}, 256, false, false}, stop).result == EditorResult::invalid);
    assert(NativeNetworkKeyboardWithPrompts(*english, {Field::password, {}, 256, false, false}, stop).result == EditorResult::invalid);
    assert(NativeNetworkKeyboardWithPrompts(*english, {Field::name, {}, 0, false, false}, stop).result == EditorResult::invalid);
    // An imported safe 1254-byte relative path exceeds the native 1023-unit limit: preserve it.
    saved = ReadConfiguration(file);
    saved.shares[0].folder = Repeat(std::string(250, 'x') + '/', 4) + std::string(250, 'x');
    assert(WriteConfiguration(file, ReadConfiguration(file), saved.shares, stop) == ConfigError::none);
    assert(editor.Open(file) == EditorResult::ok && editor.Begin(id) == EditorResult::ok);
    count = asks;
    assert(editor.Enter(Field::folder, stop, keyboard) == EditorResult::invalid && asks == count);
    assert(Row(editor, Field::folder) == saved.shares[0].folder);
    assert(translations == 16 && ReadConfiguration(file).shares[0].password == "Synthetic-private-only");
    std::cout << "Native prompt snapshot/16-key factory-only translation; immutable ownership/language change; enum-only async Editor; exact creation-only names; cancellation/masking/no echo; UTF8/16/NUL/surrogate/native limits/no truncation PASS\n";
}
