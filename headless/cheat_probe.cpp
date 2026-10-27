// SPDX-License-Identifier: GPL-3.0-or-later
#include "cheat_identity.h"
#include "assets_dir.h"
#include "game_files.h"
#include "mods.h"
#include "common/fs/path_util.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/submission_package.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/loader.h"
#include <exception>

namespace Eden {
CheatIdentity ProbeCheatIdentity(const std::string& rom_path, std::uint64_t expected_program,
                                const CheatProbePaths& paths) try {
    if (!expected_program || rom_path.empty() || rom_path.find('\0') != rom_path.npos ||
        (!IsPackageFile(rom_path) && !IsCardImageFile(rom_path))) return {};
    const auto old_keys = Common::FS::GetEdenPath(Common::FS::EdenPath::KeysDir);
    const auto old_load = Common::FS::GetEdenPath(Common::FS::EdenPath::LoadDir);
    SCOPE_EXIT {
        Common::FS::SetEdenPath(Common::FS::EdenPath::KeysDir, old_keys);
        Common::FS::SetEdenPath(Common::FS::EdenPath::LoadDir, old_load);
    };
    Common::FS::SetEdenPath(Common::FS::EdenPath::KeysDir, paths.keys);
    Common::FS::SetEdenPath(Common::FS::EdenPath::LoadDir, paths.mods);
    auto vfs = std::make_shared<FileSys::RealVfsFilesystem>();
    const auto file = vfs->OpenFile(rom_path, FileSys::OpenMode::Read);
    if (!file) return {};
    std::shared_ptr<FileSys::NSP> package;
    if (IsPackageFile(rom_path)) package = std::make_shared<FileSys::NSP>(file);
    else {
        FileSys::XCI card(file);
        if (card.GetStatus() != Loader::ResultStatus::Success) return {};
        package = card.GetSecurePartitionNSP();
    }
    if (!package || package->GetStatus() != Loader::ResultStatus::Success ||
        (!package->IsExtractedType() && package->GetProgramTitleID() != expected_program)) return {};

    // These globals are also consumed by the derived BIS factory. Keep precisely the boot
    // values for this title, then restore even when a provider/parser throws. The service's
    // bridge mutex serializes this operation with all other metadata work.
    auto old_dirs = ::Settings::values.external_content_dirs;
    auto old_disabled = ::Settings::values.disabled_addons;
    auto old_folder = Mods::SessionGameFolder();
    const auto old_title = Mods::SessionTitle();
    const auto old_dump = ::Settings::values.dump_exefs.GetValue();
    SCOPE_EXIT {
        ::Settings::values.external_content_dirs.swap(old_dirs);
        ::Settings::values.disabled_addons.swap(old_disabled);
        Mods::SessionGameFolder().swap(old_folder);
        Mods::SessionTitle() = old_title;
        ::Settings::values.dump_exefs.SetValue(old_dump);
    };
    Mods::SessionGameFolder() = paths.game_folder;
    Mods::SessionTitle() = expected_program;
    const auto all_mods = Mods::List({paths.mods, paths.cheats, paths.game_folder}, expected_program);
    const auto switches = Mods::Switches(all_mods, LoadDisabledMods(expected_program, paths.settings),
        LoadChosenCheats(expected_program, paths.settings), LoadCheatsOff(expected_program, paths.settings),
        LoadModsEnabled(expected_program, paths.settings), LoadCheatsEnabled(expected_program, paths.settings));
    ::Settings::values.disabled_addons[expected_program] = switches.mods_off;
    ::Settings::values.external_content_dirs = {paths.updates};
    if (!paths.game_folder.empty()) ::Settings::values.external_content_dirs.push_back(paths.game_folder);
    ::Settings::values.dump_exefs.SetValue(false); // inspection must not dump the game's code

    FileSys::ManualContentProvider contents; // outlives the system referring to it
    Core::System system;                    // no Initialize: no guest RAM, timing or threads
    system.SetFilesystem(vfs);
    system.SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
    system.GetFileSystemController().CreateFactories(*vfs);
    if (!package->IsExtractedType() && !contents.AddEntriesFromContainer(file)) return {};
    system.RegisterContentProvider(FileSys::ContentProviderUnionSlot::FrontendManual, &contents);
    FileSys::VirtualDir exefs;
    if (package->IsExtractedType()) exefs = package->GetExeFS();
    else {
        const auto nca = package->GetNCA(expected_program, FileSys::ContentRecordType::Program);
        if (!nca || nca->GetStatus() != Loader::ResultStatus::Success) return {};
        exefs = nca->GetExeFS();
        if (!exefs) {
            const auto update = system.GetContentProvider().GetEntry(
                FileSys::GetUpdateTitleID(nca->GetTitleId()), FileSys::ContentRecordType::Program);
            if (update) exefs = update->GetExeFS();
        }
    }
    return ReadCheatIdentity(std::move(exefs), !package->IsExtractedType(), expected_program,
        system.GetFileSystemController(), system.GetContentProvider());
} catch (const std::exception&) { return {}; }

CheatIdentity ProbeCheatIdentity(const std::string& rom_path, const std::string& game_folder,
                                std::uint64_t expected_program) {
    return ProbeCheatIdentity(rom_path, expected_program, {AssetsPath("keys"), AssetsPath("mods"),
        AssetsPath("cheats"), GamesPath("updates"), game_folder, SettingsFile()});
}
} // namespace Eden
