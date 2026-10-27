// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/program_metadata.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/loader/loader.h"

namespace Eden {
// Both boot and the cheat manager use this exact update/LayeredExeFS selection. In particular,
// Reload preserves the original NPDM program ID, even when a replacement NPDM says otherwise.
inline Loader::ResultStatus ResolveProgramExeFS(
    FileSys::VirtualDir& dir, FileSys::ProgramMetadata& metadata, bool apply_updates,
    const Service::FileSystem::FileSystemController& filesystem,
    const FileSys::ContentProvider& contents) {
    const auto load = [&](bool reload) {
        const auto npdm = dir ? dir->GetFile("main.npdm") : nullptr;
        if (!npdm) return Loader::ResultStatus::ErrorMissingNPDM;
        return reload ? metadata.Reload(npdm) : metadata.Load(npdm);
    };
    if (const auto status = load(false); status != Loader::ResultStatus::Success) return status;
    if (apply_updates) {
        const FileSys::PatchManager manager(metadata.GetTitleID(), filesystem, contents);
        dir = manager.PatchExeFS(dir);
    }
    return load(true);
}
} // namespace Eden
