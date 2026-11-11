#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile/test Encore's actual forwarder helper in Docker; no SDK/core/vendor.

This is synthetic host admission evidence, not native argv/lifecycle validation.
The current persistence validator is extracted only for compatibility assertions.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile


def main():
    if sys.platform != "linux" or not Path("/.dockerenv").is_file():
        raise SystemExit("Run this compiler fixture in the approved Docker sandbox, not on the host.")
    root = Path(__file__).resolve().parents[1]
    settings = (root / "headless/settings_store.h").read_text(encoding="utf-8")
    start = settings.index("inline bool ValidRomFilename(std::string_view name) {")
    validator = settings[start:settings.index("\n}", start) + 2]
    startup = (root / "headless/main.cpp").read_text(encoding="utf-8")
    assert 'forwarded_pending = forwarded_pending && boot.AllowsForwarded(!last_crash.report.empty());' in startup
    code = r'''
#include "forwarded_launch.h"
#include "dev_launch_policy.h"
#include <cassert>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>
#include <sys/stat.h>
namespace Eden {
VALIDATOR
}
namespace fs = std::filesystem;
using Eden::ForwardedArgsError;
using Eden::ParseForwardedArgs;
using Eden::ResolveForwardedRom;

static Eden::ForwardedArgs Parse(std::initializer_list<const char*> args) {
    const std::vector<const char*> values{args};
    return ParseForwardedArgs(static_cast<int>(values.size()), values.data());
}

static void Invalid(const Eden::ForwardedArgs& result, ForwardedArgsError error) {
    assert(!result.valid() && result.error == error);
    assert(result.rom.empty() && !result.exit_after_game);
}

static void File(const fs::path& path) {
    std::ofstream file(path);
    file << "synthetic fixture, not a playable ROM";
    assert(file.good());
}

static void ExeFS(const fs::path& path) {
    assert(fs::create_directory(path));
    File(path / "main");
    File(path / "main.npdm");
}

int main() {
    Eden::DevLaunch::BootIntent boot;
    assert(boot.AllowsForwarded(false) && !boot.AllowsForwarded(true));
    boot.Observe("autoboot=on");
    assert(boot.ShouldAutoboot(false));
    boot.Observe("launcher=first");
    assert(!boot.ShouldAutoboot(false) && !boot.AllowsForwarded(false));
    Eden::DevLaunch::BootIntent disabled;
    disabled.Observe("autoboot=off"); disabled.Observe("autoboot=on");
    assert(!disabled.ShouldAutoboot(false) && !disabled.AllowsForwarded(false));
    assert(ParseForwardedArgs(0, nullptr).valid());
    assert(ParseForwardedArgs(0, nullptr).rom.empty());
    assert(!ParseForwardedArgs(0, nullptr).exit_after_game);
    Invalid(ParseForwardedArgs(-1, nullptr), ForwardedArgsError::InvalidArgv);
    Invalid(ParseForwardedArgs(1, nullptr), ForwardedArgsError::InvalidArgv);
    Invalid(Parse({nullptr}), ForwardedArgsError::InvalidArgv);
    Invalid(Parse({"app", "--rom", "Game.nsp", nullptr}), ForwardedArgsError::InvalidArgv);
    Invalid(Parse({"--exit-after-game", nullptr, "--rom=Game.nsp"}), ForwardedArgsError::InvalidArgv);
    assert(Parse({"app", "--unknown", "unknown-value", ""}).rom.empty());
    const auto separated = Parse({"app", "--rom", "My Game.NSP", "--exit-after-game"});
    assert(separated.valid() && separated.rom == "My Game.NSP" && separated.exit_after_game);
    char program[] = "app", flag[] = "--rom", value[] = "Game.nsp";
    char* native_argv[] = {program, flag, value, nullptr};
    assert(ParseForwardedArgs(3, native_argv).rom == "Game.nsp");
    const auto equals = Parse({"--exit-after-game", "--rom=My Game.NSP", "--unknown"});
    assert(equals.valid() && equals.rom == separated.rom && equals.exit_after_game);
    const auto modifier = Parse({"app", "--exit-after-game"});
    assert(modifier.valid() && modifier.rom.empty() && modifier.exit_after_game);
    assert(Parse({"--rom", "-single-dash.nro"}).valid());
    for (const auto args : {
        std::initializer_list<const char*>{"--rom"},
        {"--rom", ""}, {"--rom="}, {"--rom", "--exit-after-game"},
        {"--rom=--unknown"}, {"--rom", "--rom=Game.nsp"},
        {"--exit-after-game", "--rom"}})
        Invalid(Parse(args), ForwardedArgsError::MissingRom);
    Invalid(Parse({"--rom=Game.nsp", "--rom=Game.nsp"}), ForwardedArgsError::DuplicateRom);
    Invalid(Parse({"--rom", "Game.nsp", "--exit-after-game", "--rom", "Other.xci"}),
            ForwardedArgsError::DuplicateRom);
    Invalid(Parse({"--exit-after-game", "--exit-after-game"}), ForwardedArgsError::DuplicateExit);
    Invalid(Parse({"--rom=Game.nsp", "--exit-after-game=yes"}), ForwardedArgsError::MalformedExit);
    std::vector<const char*> args(Eden::kForwardedMaxArgs, "unknown");
    assert(ParseForwardedArgs(static_cast<int>(args.size()), args.data()).valid());
    // Must reject the declared count BEFORE looking at any argument pointer.
    Invalid(ParseForwardedArgs(Eden::kForwardedMaxArgs + 1, args.data()), ForwardedArgsError::TooManyArgs);
    const std::string boundary(Eden::kForwardedMaxPathBytes, 'a');
    assert(Parse({boundary.c_str()}).valid());
    assert(Parse({"--rom", boundary.c_str()}).rom == boundary);
    const std::string long_token(Eden::kForwardedMaxPathBytes + 1, 'a');
    Invalid(Parse({long_token.c_str()}), ForwardedArgsError::ArgumentTooLong);
    Invalid(Parse({"--exit-after-game", "--rom", long_token.c_str()}), ForwardedArgsError::ArgumentTooLong);
    const std::string equals_boundary = "--rom=" + std::string(Eden::kForwardedMaxPathBytes - 6, 'a');
    assert(Parse({equals_boundary.c_str()}).valid());
    const std::string equals_long = equals_boundary + 'a';
    Invalid(Parse({equals_long.c_str()}), ForwardedArgsError::ArgumentTooLong);
    for (int c = 1; c <= 127; ++c) {
        if (c >= 32 && c != 127) continue;
        const std::string bad = std::string("Game") + static_cast<char>(c) + ".nsp";
        Invalid(Parse({"--rom", bad.c_str()}), ForwardedArgsError::ControlCharacter);
        const std::string equal_bad = "--rom=" + bad;
        Invalid(Parse({equal_bad.c_str()}), ForwardedArgsError::ControlCharacter);
        Invalid(Parse({"--rom=Game.nsp", bad.c_str()}), ForwardedArgsError::ControlCharacter);
    }
    // Parsing does not trim or try to validate filenames/containment itself.
    assert(Parse({"--rom=Game.nsp "}).rom == "Game.nsp ");
    assert(Parse({"--rom=../Game.nsp"}).valid());
    std::cout << "FORWARDED_ARGS_PASS\n";

    const auto base = fs::current_path();
    const auto roms = base / "roms";
    const std::string root = roms.generic_string();
    assert(fs::create_directory(roms));
    assert(fs::create_directory(base / "outside"));
    const auto rejected = [&](std::string_view name) {
        assert(ResolveForwardedRom(name, root).empty());
    };
    const auto admitted = [&](const std::string& name) {
        const std::string expected = (roms / name).generic_string();
        assert(Eden::ValidRomFilename(name));
        assert(ResolveForwardedRom(name, root) == expected);
        assert(ResolveForwardedRom(expected, root) == expected);
        assert(Eden::RememberedRomName(expected, roms) == name);
        assert(Eden::ValidRomFilename(Eden::RememberedRomName(expected, roms)));
    };
    for (const char* extension : {"nsp", "XcI", "NsZ", "xCZ", "NRO", "nso", "NcA", "NAX", "kip"}) {
        const std::string name = std::string("Game.") + extension;
        File(roms / name);
        admitted(name);
    }
    for (const std::string& name : {std::string("My Game [id].nsp"), std::string("日本語.nro"),
                                    std::string(".hidden.nsp"), std::string("-single-dash.nro"),
                                    std::string(251, 'a') + ".nsp"}) {
        File(roms / name);
        admitted(name);
    }
    ExeFS(roms / "Title");
    admitted("Title/main");
    ExeFS(roms / "日本語");
    admitted("日本語/main");
    // 250-byte folder + /main is the exact persisted-name ceiling.
    const std::string long_folder(250, 'e');
    ExeFS(roms / long_folder);
    admitted(long_folder + "/main");
    ExeFS(roms / std::string(251, 'e'));
    rejected(std::string(251, 'e') + "/main");
    for (const char* invalid : {"", "Game.nsp ", "Game.nsp\r", "Game.nsp\n", "Game.nsp\t",
        "Game.ncz", "Game.lz4", "Game.zstd", "Game.txt", "Game.nsp/", "main", "Title",
        "Title/main.npdm", "Title/main/", "Title//main", "Title/./main", "Title/../main",
        "../Game.nsp", "./Game.nsp", "x/../Game.nsp", "Title/../../Game.nsp",
        "Title\\main", "Game\\.nsp", "pe-net://share/Game.nsp", "C:/Game.nsp", "a:Game.nsp",
        "/", "//Game.nsp", "/outside/Game.nsp", "Folder/Game.nsp", "A/B/main"})
        rejected(invalid);
    rejected(boundary);
    rejected(long_token);
    rejected(std::string("Game\0.nsp", 9));
    for (int c = 1; c <= 127; ++c) {
        if (c >= 32 && c != 127) continue;
        rejected(std::string("Game") + static_cast<char>(c) + ".nsp");
    }
    const std::string game = (roms / "Game.nsp").generic_string();
    for (const auto& bad_root : {std::string{}, std::string("/"), std::string("roms"), root + '/',
                                root + "/.", root + "/..", root + "//", root + "\\",
                                root + "\n", root + std::string("\0", 1), std::string(1025, 'a')})
        assert(ResolveForwardedRom(game, bad_root).empty());
    assert(ResolveForwardedRom(game, (base / "absent").generic_string()).empty());
    assert(ResolveForwardedRom(game, (roms / "Game.nsp").generic_string()).empty());
    rejected(root);
    rejected(root + "/../roms/Game.nsp");
    rejected(root + "//Game.nsp");
    rejected(root + "/./Game.nsp");
    rejected(root + "-other/Game.nsp");
    const auto other_root = base / "roms-other";
    assert(fs::create_directory(other_root));
    File(other_root / "Game.nsp");
    rejected((other_root / "Game.nsp").generic_string());
    assert(ResolveForwardedRom(game, other_root.generic_string()).empty());
    std::cout << "FORWARDED_NAMES_AND_CONTAINMENT_PASS\n";

    // Wrong-type, incomplete, inaccessible and linked selections are not ROMs.
    assert(fs::create_directory(roms / "Directory.nsp"));
    rejected("Directory.nsp");
    File(roms / "PlainFolder");
    rejected("PlainFolder/main");
    assert(fs::create_directory(roms / "Incomplete"));
    File(roms / "Incomplete/main");
    rejected("Incomplete/main");
    assert(fs::create_directory(roms / "Incomplete/main.npdm"));
    rejected("Incomplete/main");
    assert(mkfifo((roms / "Pipe.nsp").c_str(), 0600) == 0);
    rejected("Pipe.nsp");
    assert(fs::create_directory(roms / "PipeExeFS"));
    File(roms / "PipeExeFS/main.npdm");
    assert(mkfifo((roms / "PipeExeFS/main").c_str(), 0600) == 0);
    rejected("PipeExeFS/main");
    ExeFS(roms / "Locked");
    fs::permissions(roms / "Locked", fs::perms::none);
    rejected("Locked/main");
    fs::permissions(roms / "Locked", fs::perms::owner_all);
    admitted("Locked/main");
    fs::permissions(roms, fs::perms::none);
    rejected("Game.nsp");
    fs::permissions(roms, fs::perms::owner_all);
    admitted("Game.nsp");
    File(base / "outside/External.nsp");
    fs::create_symlink(base / "outside/External.nsp", roms / "Linked.nsp");
    rejected("Linked.nsp");
    fs::create_symlink(roms / "Game.nsp", roms / "InternalLink.nsp");
    rejected("InternalLink.nsp");
    fs::create_symlink(roms / "missing", roms / "Dangling.nsp");
    rejected("Dangling.nsp");
    ExeFS(base / "outside/External");
    fs::create_directory_symlink(base / "outside/External", roms / "LinkedFolder");
    rejected("LinkedFolder/main");
    fs::create_directory_symlink(roms / "Title", roms / "InternalFolder");
    rejected("InternalFolder/main");
    assert(fs::create_directory(roms / "LinkedMain"));
    File(roms / "LinkedMain/main.npdm");
    fs::create_symlink(roms / "Title/main", roms / "LinkedMain/main");
    rejected("LinkedMain/main");
    assert(fs::create_directory(roms / "LinkedNpdm"));
    File(roms / "LinkedNpdm/main");
    fs::create_symlink(roms / "Title/main.npdm", roms / "LinkedNpdm/main.npdm");
    rejected("LinkedNpdm/main");
    fs::create_directory_symlink(roms, base / "alias-roms");
    assert(ResolveForwardedRom("Game.nsp", (base / "alias-roms").generic_string()).empty());
    // Reject a linked ancestor even when the ROM directory itself is real.
    fs::create_directory_symlink(base, base / "alias-parent");
    const auto alias_root = (base / "alias-parent/roms").generic_string();
    assert(ResolveForwardedRom("Game.nsp", alias_root).empty());
    rejected((base / "alias-parent/roms/Game.nsp").generic_string());
    fs::create_hard_link(base / "outside/External.nsp", roms / "Hard.nsp");
    rejected("Hard.nsp");
    assert(fs::create_directory(roms / "HardMain"));
    File(roms / "HardMain/main.npdm");
    fs::create_hard_link(base / "outside/External.nsp", roms / "HardMain/main");
    rejected("HardMain/main");
    assert(fs::create_directory(roms / "HardNpdm"));
    File(roms / "HardNpdm/main");
    fs::create_hard_link(base / "outside/External.nsp", roms / "HardNpdm/main.npdm");
    rejected("HardNpdm/main");
    std::cout << "FORWARDED_NODE_ADMISSION_PASS\n";

    // No upstream moved-file behavior can bypass Encore's persisted-name rule.
    assert(fs::create_directory(roms / "Game"));
    File(roms / "Game/Game.nsp");
    admitted("Game.nsp"); // existing file wins without consulting nested copies
    assert(fs::remove(roms / "Game.nsp"));
    rejected("Game.nsp"); // even a unique matching <stem>/<name> is inadmissible
    rejected(game);
    rejected("Game/Game.nsp");
    assert(fs::create_directory(roms / "Other"));
    File(roms / "Other/Game.nsp");
    rejected("Game.nsp"); // ambiguous nested matches are also never selected
    assert(fs::remove(roms / "Game/Game.nsp"));
    rejected("Game.nsp"); // sole non-stem folder match still cannot be persisted
    rejected("Other/Game.nsp");
    File(roms / "Game.nsp");
    admitted("Game.nsp");
    ExeFS(roms / "AnotherTitle");
    admitted("Title/main");
    assert(fs::remove(roms / "Title/main"));
    rejected("Title/main"); // another title's main is not an identity fallback
    admitted("AnotherTitle/main");
    std::cout << "FORWARDED_EXISTING_AND_NO_UNSAFE_MOVED_FALLBACK_PASS\n";
    std::cout << "FORWARDED_ROM_HOST_PASS (synthetic admission only; native argv unverified)\n";
}
'''.replace("VALIDATOR", validator)
    with tempfile.TemporaryDirectory(prefix="encore-forwarder-") as temporary:
        workspace = Path(temporary)
        executable = workspace / "forwarded-rom"
        command = [os.environ.get("CXX", "c++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                   "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                   "-I", str(root / "headless"), "-x", "c++", "-", "-o", str(executable)]
        print("Compiling actual forwarded_launch.h with ASan/UBSan", flush=True)
        subprocess.run(command, input=code, text=True, check=True, cwd=workspace, timeout=120)
        subprocess.run([str(executable)], cwd=workspace, check=True, timeout=30)


if __name__ == "__main__":
    main()
