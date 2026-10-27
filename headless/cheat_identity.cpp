// SPDX-License-Identifier: GPL-3.0-or-later
#include "cheat_identity.h"
#include "effective_exefs.h"
#include "core/loader/nso.h"
#include <algorithm>
#include <exception>

namespace Eden {
CheatIdentity ReadCheatIdentity(FileSys::VirtualDir exefs, bool apply_updates,
    std::uint64_t expected_program, const Service::FileSystem::FileSystemController& filesystem,
    const FileSys::ContentProvider& contents) try {
    if (!expected_program) return {};
    FileSys::ProgramMetadata metadata;
    if (ResolveProgramExeFS(exefs, metadata, apply_updates, filesystem, contents) !=
        Loader::ResultStatus::Success || metadata.GetTitleID() != expected_program) return {};
    const auto main = exefs->GetFile("main");
    Loader::NSOHeader header{};
    if (!main || main->ReadObject(&header) != sizeof(header) ||
        header.magic != 0x304f534e || // NSO0
        std::all_of(header.build_id.begin(), header.build_id.begin() + 8,
                    [](auto byte) { return byte == 0; })) return {};
    return {metadata.GetTitleID(), header.build_id};
} catch (const std::exception&) { return {}; }
} // namespace Eden
