#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded TitleDB matching; fixtures are local synthetic instructions, never downloaded cheats."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
code = r'''
#include "cheat_catalog.h"
#include <cassert>
#include <cstdio>
#include <nlohmann/json.hpp>
using namespace Eden;
using Json = nlohmann::json;
int main() {
    constexpr uint64_t title = 0x0100000000001234;
    std::array<uint8_t, 32> build{}; build[0] = 0xab; build[7] = 0xcd; build[31] = 0xef;
    const std::string key(64, 'a'), other(64, 'b');
    Json source = {{"0100000000001234", {{"AB000000000000CD", {
        {key, {{"title", "[Write test value]"}, {"source", "04000000 00000000 00000001"}}},
        {other, {{"title", "{Master test}"}, {"source", "04000000 00000004 00000002"}}}, {"version", 42}}}}}};
    const auto read = [&](const Json& data) { return ReadCheatCatalog(data.dump(), title, build); };
    const auto valid = read(source); assert(valid.status == CheatCatalogStatus::Ready && valid.entries.size() == 2);
    assert(valid.Text().starts_with("{Master test}\n04000000 00000004 00000002\n"));
    assert(valid.Text().find("[Write test value]\n04000000 00000000 00000001\n") != std::string::npos);
    assert(ReadCheatCatalog(source.dump(), title + 1, build).status == CheatCatalogStatus::NoMatch);
    auto changed = build; changed[0] ^= 1;
    assert(ReadCheatCatalog(source.dump(), title, changed).status == CheatCatalogStatus::NoMatch);
    const auto mismatch = ReadCheatCatalog(source.dump(), title, changed);
    assert(mismatch.entries.empty() && mismatch.other_builds == std::vector<std::string>{"ab000000000000cd / v42"});
    assert(ReadCheatCatalog(source.dump(), title + 1, build).other_builds.empty());
    auto alternate = source;
    alternate["0100000000001234"]["123456789ABCDEF0"] = {{"version", 65536}, {"invalid unselected cheat", "ignored"}};
    assert(read(alternate).Text() == valid.Text()); // labels never become code or a fallback
    assert(read(alternate).other_builds == std::vector<std::string>{"123456789abcdef0 / v65536"});
    auto nested = alternate;
    nested["0100000000001234"]["123456789ABCDEF0"]["other"] = {{"version", 999}};
    assert(read(nested).other_builds == read(alternate).other_builds);
    alternate["0100000000001234"]["123456789abcdef0"] = {{"version", 1}};
    assert(read(alternate).status == CheatCatalogStatus::Invalid); // no ambiguous visible aliases
    alternate = source;
    for (unsigned n = 0; n < 100; ++n) {
        char prefix[17]{}; std::snprintf(prefix, sizeof(prefix), "%016x", n);
        alternate["0100000000001234"][prefix] = {{"version", n}};
    }
    const auto bounded_labels = read(alternate);
    assert(bounded_labels.status == CheatCatalogStatus::Ready && bounded_labels.Text() == valid.Text());
    assert(bounded_labels.other_builds.size() == 32 && bounded_labels.other_builds_truncated);
    assert(ReadCheatCatalog(source.dump(), 0, build).status == CheatCatalogStatus::Invalid);
    assert(ReadCheatCatalog(source.dump(), title, {}).status == CheatCatalogStatus::Invalid);
    auto unknown_prefix = std::array<uint8_t, 32>{}; unknown_prefix[31] = 1;
    assert(ReadCheatCatalog(source.dump(), title, unknown_prefix).status == CheatCatalogStatus::Invalid);
    for (const char* bad : {"{", "[]", "null", "{} trailing"})
        assert(ReadCheatCatalog(bad, title, build).status == CheatCatalogStatus::Invalid);
    assert(ReadCheatCatalog(std::string(16 * 1024 * 1024 + 1, ' '), title, build).status == CheatCatalogStatus::Invalid);
    for (const char* bad : {"", "label", "[bad\nname]", "[bad] [extra]", "[bad{nested}]"}) {
        auto data = source; data["0100000000001234"]["AB000000000000CD"][key]["title"] = bad;
        assert(read(data).status == CheatCatalogStatus::Invalid);
    }
    for (const char* bad : {"", "[Injected]\n04000000", "0400000", "zzzzzzzz", "04000000 ;comment", "040000000"}) {
        auto data = source; data["0100000000001234"]["AB000000000000CD"][key]["source"] = bad;
        assert(read(data).status == CheatCatalogStatus::Invalid);
    }
    auto duplicate = source;
    duplicate["0100000000001234"]["ab000000000000cd"] = source["0100000000001234"]["AB000000000000CD"];
    assert(read(duplicate).status == CheatCatalogStatus::Invalid);
    duplicate = source;
    duplicate["0100000000001234"]["AB000000000000CD"][std::string(64, 'A')] = source["0100000000001234"]["AB000000000000CD"][key];
    assert(read(duplicate).status == CheatCatalogStatus::Invalid);
    duplicate = source;
    duplicate["0100000000001234"]["AB000000000000CD"][other]["title"] = "[ Write test value ]";
    assert(read(duplicate).status == CheatCatalogStatus::Invalid);
    duplicate = source;
    duplicate["0100000000001234"]["AB000000000000CD"][key]["title"] = "[" + std::string(60, 'a') + "one]";
    duplicate["0100000000001234"]["AB000000000000CD"][other]["title"] = "[" + std::string(60, 'a') + "two]";
    assert(read(duplicate).status == CheatCatalogStatus::Invalid); // the launcher's 60-byte switch keys alias
    duplicate["0100000000001234"]["AB000000000000CD"][other]["title"] = "[Different]";
    assert(read(duplicate).status == CheatCatalogStatus::Ready); // full 63-byte name still works
    duplicate["0100000000001234"]["AB000000000000CD"][key]["title"] = "[" + std::string(64, 'a') + "]";
    assert(read(duplicate).status == CheatCatalogStatus::Invalid);
    auto oversized = source;
    auto& table = oversized["0100000000001234"]["AB000000000000CD"];
    table.clear();
    for (unsigned n = 0; n < 513; ++n) {
        char checksum[65]{}; std::snprintf(checksum, sizeof(checksum), "%064x", n);
        table[checksum] = {{"title", "[Entry " + std::to_string(n) + "]"}, {"source", "04000000 00000000 00000001"}};
    }
    assert(read(oversized).status == CheatCatalogStatus::Invalid);
    oversized = source; std::string words; for (unsigned n = 0; n < 257; ++n) words += "00000000 ";
    oversized["0100000000001234"]["AB000000000000CD"][key]["source"] = words;
    assert(read(oversized).status == CheatCatalogStatus::Invalid);
    words.resize(words.size() - 9); // exactly 256 words is accepted, not an accidental lower limit
    oversized["0100000000001234"]["AB000000000000CD"][key]["source"] = words;
    assert(read(oversized).status == CheatCatalogStatus::Ready);
    assert(ReadCheatCatalog("{\"other\":" + std::string(20, '[') + "0" + std::string(20, ']') + "}", title, build).status == CheatCatalogStatus::Invalid);
    auto invalid_type = source;
    invalid_type["0100000000001234"]["AB000000000000CD"][key] = Json::array();
    assert(read(invalid_type).status == CheatCatalogStatus::Invalid);
    invalid_type = source;
    invalid_type["0100000000001234"]["AB000000000000CD"][key]["source"] = 7;
    assert(read(invalid_type).status == CheatCatalogStatus::Invalid);
    // Unrelated titles/builds do not become matches and are never retained in the result.
    auto large = source;
    for (unsigned n = 0; n < 1000; ++n) large["Unrelated " + std::to_string(n)] = Json::array({std::string(2000, 'x'), 42});
    assert(read(large).Text() == valid.Text());
    const auto raw = source.dump(); const auto at = raw.find("\"source\":");
    auto repeated = raw; repeated.insert(at, "\"source\":\"00000000\",");
    assert(ReadCheatCatalog(repeated, title, build).status == CheatCatalogStatus::Invalid);
    std::puts("Cheat catalog PASS: bounded SAX parsing, exact program/build-prefix matching, bounded other-build/version labels without code/fallback, duplicate refusal, header/code/VM-word limits and master-first output");
}
'''
with tempfile.TemporaryDirectory(prefix='eden-cheat-catalog-') as tmp:
    folder = Path(tmp)
    cpp, binary = folder / 'check.cpp', folder / 'check'
    cpp.write_text(code)
    for sanitizer in ('', 'address,undefined'):
        flags = ['-fsanitize=' + sanitizer, '-fno-omit-frame-pointer'] if sanitizer else []
        subprocess.run([os.environ.get('CXX', 'clang++-18'), '-std=c++20', '-O1', '-g', '-UNDEBUG',
                        *flags, '-I', str(root / 'headless'), str(cpp),
                        str(root / 'headless/cheat_catalog.cpp'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=60)
