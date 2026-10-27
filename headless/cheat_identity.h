// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/file_sys/vfs/vfs_types.h"
#include <array>
#include <cstdint>
#include <string>

namespace FileSys { class ContentProvider; }
namespace Service::FileSystem { class FileSystemController; }

namespace Eden {
struct CheatIdentity {
    std::uint64_t program{};
    std::array<std::uint8_t, 32> build{};
    explicit operator bool() const { return program != 0; }
};
struct CheatProbePaths {
    std::string keys, mods, cheats, updates, game_folder, settings;
};
// Only the effective main NSO's header, never a container version or a previous session's ID.
// Filesystem/provider and disabled-add-on settings must be the same as those used at boot.
CheatIdentity ReadCheatIdentity(FileSys::VirtualDir exefs, bool apply_updates,
    std::uint64_t expected_program, const Service::FileSystem::FileSystemController& filesystem,
    const FileSys::ContentProvider& contents);
// Launcher-only, serialized with the metadata bridge and never during a game. Reopens the
// container and current providers for each request; temporary session/settings are restored.
// Empty on unsupported/unknown input. No Core::System::Initialize(), guest memory or guest boot.
CheatIdentity ProbeCheatIdentity(const std::string& rom_path, const std::string& game_folder,
                                std::uint64_t expected_program);
CheatIdentity ProbeCheatIdentity(const std::string& rom_path, std::uint64_t expected_program,
                                const CheatProbePaths& paths);
} // namespace Eden
