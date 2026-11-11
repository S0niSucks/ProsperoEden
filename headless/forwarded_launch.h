// SPDX-License-Identifier: GPL-3.0-or-later
// Forwarder argument/admission boundary. No settings, loader or process side effects.
#pragma once
#include "game_files.h"
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace Eden {

inline constexpr int kForwardedMaxArgs = 64;
inline constexpr std::size_t kForwardedMaxPathBytes = 1024;

enum class ForwardedArgsError {
    None, InvalidArgv, TooManyArgs, ArgumentTooLong, ControlCharacter,
    MissingRom, DuplicateRom, DuplicateExit, MalformedExit
};

struct ForwardedArgs {
    std::string rom;
    bool exit_after_game = false;
    ForwardedArgsError error = ForwardedArgsError::None;
    bool valid() const { return error == ForwardedArgsError::None; }
};

namespace ForwardedDetail {
inline bool HasUnsafeByte(std::string_view text) {
    for (unsigned char c : text)
        if (c < 32 || c == 127 || c == '\\' || c == ':') return true;
    return false;
}

// Validate BEFORE constructing/normalizing filesystem paths: normalization must
// not hide traversal, repeated separators, a scheme or an embedded NUL.
inline bool SafeSpelling(std::string_view text, bool absolute) {
    if (text.empty() || text.size() > kForwardedMaxPathBytes ||
        HasUnsafeByte(text) || (text.front() == '/') != absolute) return false;
    for (std::size_t start = absolute ? 1 : 0; start <= text.size();) {
        const auto end = text.find('/', start);
        const auto part = text.substr(start, end == text.npos ? text.npos : end - start);
        if (part.empty() || part == "." || part == "..") return false;
        if (end == text.npos) break;
        start = end + 1;
    }
    return true;
}

// A stricter subset of settings_store.h:ValidRomFilename, deliberately kept
// independent of its JSON dependency. The fixture checks against that actual
// validator. Do not expand this to arbitrary nested ROMs or change persistence.
inline bool AdmissibleName(std::string_view name) {
    if (name.size() < 5 || name.size() > 255 || !SafeSpelling(name, false)) return false;
    return IsDeconstructedGameSelection(name) ||
           (name.find('/') == name.npos && IsGameFile(name));
}

// Check every ancestor, including those above the selected ROM directory.
// canonical() alone would follow an outside-root symlink before validating it.
inline bool RealDirectoryChain(const std::filesystem::path& directory) {
    namespace fs = std::filesystem;
    fs::path current = directory.root_path();
    std::error_code error;
    const auto root_status = fs::symlink_status(current, error);
    if (error || !fs::is_directory(root_status) || fs::is_symlink(root_status)) return false;
    for (const auto& part : directory.relative_path()) {
        current /= part;
        error.clear();
        const auto status = fs::symlink_status(current, error);
        if (error || !fs::is_directory(status) || fs::is_symlink(status)) return false;
    }
    return true;
}

inline bool RealFile(const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    std::error_code error;
    const auto status = fs::symlink_status(path, error);
    if (error || !fs::is_regular_file(status) || fs::is_symlink(status)) return false;
    // A regular hardlink can also alias data outside the selected tree.
    const auto links = fs::hard_link_count(path, error);
    return !error && links == 1;
}
} // namespace ForwardedDetail

// Scan from index zero, like the source forwarder helper: an executable-name
// token is ignored, but a raw native launch vector can begin with a flag. argv
// pointers must obey the usual readable C-string/argc contract. A null entry
// INSIDE argc, any duplicate known flag, or malformed known value invalidates
// the entire request (no partially accepted ROM or exit modifier survives).
inline ForwardedArgs ParseForwardedArgs(int argc, const char* const* argv) {
    const auto fail = [](ForwardedArgsError error) { return ForwardedArgs{{}, false, error}; };
    if (argc < 0 || (argc > 0 && argv == nullptr)) return fail(ForwardedArgsError::InvalidArgv);
    if (argc > kForwardedMaxArgs) return fail(ForwardedArgsError::TooManyArgs);
    ForwardedArgs result;
    bool have_rom = false;
    bool need_rom = false;
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == nullptr) return fail(ForwardedArgsError::InvalidArgv);
        std::size_t size = 0;
        while (size <= kForwardedMaxPathBytes && argv[i][size] != '\0') ++size;
        if (size > kForwardedMaxPathBytes) return fail(ForwardedArgsError::ArgumentTooLong);
        const std::string_view arg{argv[i], size};
        for (unsigned char c : arg)
            if (c < 32 || c == 127) return fail(ForwardedArgsError::ControlCharacter);
        if (need_rom) {
            if (arg.empty() || arg.starts_with("--")) return fail(ForwardedArgsError::MissingRom);
            result.rom = arg;
            need_rom = false;
        } else if (arg == "--rom" || arg.starts_with("--rom=")) {
            if (have_rom) return fail(ForwardedArgsError::DuplicateRom);
            have_rom = true;
            if (arg == "--rom") {
                need_rom = true;
            } else {
                const auto value = arg.substr(6);
                if (value.empty() || value.starts_with("--")) return fail(ForwardedArgsError::MissingRom);
                result.rom = value;
            }
        } else if (arg == "--exit-after-game") {
            if (result.exit_after_game) return fail(ForwardedArgsError::DuplicateExit);
            result.exit_after_game = true;
        } else if (arg.starts_with("--exit-after-game=")) {
            return fail(ForwardedArgsError::MalformedExit);
        }
        // Unknown bounded, control-free tokens do not affect this interface.
    }
    if (need_rom) return fail(ForwardedArgsError::MissingRom);
    return result;
}

// Return an EXISTING admitted loader path, or empty on any invalid spelling,
// unsafe node, missing file or filesystem error. roms_dir is the already
// selected ROM root (normally AssetsPath("roms")), not a caller-chosen fallback.
// Both absolute and relative requests must retain a valid persisted identity.
// No moved-file search: <folder>/Game.nsp is not an Encore library/history name,
// and guessing another folder's "main" would select an unrelated ExeFS title.
// This is a point-in-time metadata check, not an open handle or a TOCTOU guard;
// callers must keep the storage tree quiescent until the normal loader opens it.
inline std::string ResolveForwardedRom(std::string_view rom, std::string_view roms_dir) {
    namespace fs = std::filesystem;
    using namespace ForwardedDetail;
    if (!SafeSpelling(roms_dir, true) || rom.empty()) return {};
    const bool absolute = rom.front() == '/';
    if (!SafeSpelling(rom, absolute)) return {};
    const std::string prefix = std::string{roms_dir} + '/';
    std::string_view name = rom;
    if (absolute) {
        if (!rom.starts_with(prefix)) return {};
        name.remove_prefix(prefix.size());
    }
    if (!AdmissibleName(name) || prefix.size() + name.size() > kForwardedMaxPathBytes) return {};
    const fs::path root{std::string{roms_dir}};
    if (!RealDirectoryChain(root)) return {};
    const fs::path selected = root / std::string{name};
    if (IsDeconstructedGameSelection(name)) {
        const auto folder = selected.parent_path();
        if (!IsDeconstructedGameDirectory(folder) || !RealDirectoryChain(folder) ||
            !RealFile(folder / "main.npdm")) return {};
    }
    return RealFile(selected) ? selected.generic_string() : std::string{};
}
} // namespace Eden
