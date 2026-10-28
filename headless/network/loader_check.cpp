// SPDX-License-Identifier: GPL-3.0-or-later
// Real pinned metadata/loader/provider/PatchManager objects, synthetic bytes, loopback NAS only.
#include "network-fixtures.h" // generated from the incumbent synthetic identity fixture helpers
#include "metadata.h"
#include "vfs.h"
#include "settings_store.h"
#include "load_failure.h"
#include "core/loader/loader.h"
#include "core/loader/nro.h"
#include <thread>
#include <chrono>
#include <iostream>
void NetworkLauncherCheck(const std::string&, const Eden::Network::Metadata&);
void NetworkLauncherFailureCheck(const std::string&, const std::string&);

namespace {
Bytes Dlc() {
    using namespace FileSys;
    const auto dlc = title + 0x1001;
    auto data = PlainNca(Package({{"main", Nso(7)}, {"main.npdm", Npdm(dlc)}}), dlc)->ReadAllBytes();
    NcaHeader nca{}; std::memcpy(&nca, data.data(), sizeof(nca));
    nca.content_type = NcaHeader::ContentType::Data;
    std::memcpy(data.data(), &nca, sizeof(nca));
    ContentRecord record{}; record.nca_id.fill(0x22); record.type = ContentRecordType::Data;
    CNMTHeader cnmt{}; cnmt.title_id = dlc; cnmt.type = TitleType::AOC;
    cnmt.number_content_entries = 1; cnmt.table_offset = sizeof(OptionalHeader);
    auto meta = PlainNca(Package({{"fixture.cnmt", CNMT(cnmt, OptionalHeader{}, {record}, {}).Serialize()}}), dlc, true)->ReadAllBytes();
    return Package({{"22222222222222222222222222222222.nca", data}, {"metadata.cnmt.nca", meta}});
}
void Wait() {
    for (unsigned n = 0; n < 500 && Eden::Network::Current().busy; ++n)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(!Eden::Network::Current().busy);
}
}
int main(int argc, char** argv) {
    assert(argc == 4);
    namespace fs = std::filesystem;
    using namespace Eden::Network;
    const fs::path root = argv[2], share = root / "share";
    // Incumbent NCA reader requires a key-area slot even for these plaintext test sections.
    Core::Crypto::Key128 invented_area{}; invented_area.fill(0x42);
    Core::Crypto::KeyManager::Instance().SetKey(Core::Crypto::S128KeyType::KeyArea, invented_area);
    Core::Crypto::KeyManager::Instance().SetKey(Core::Crypto::S256KeyType::Header, HeaderKey());
    if (std::string_view{argv[1]} == "fixtures") {
        Write(share / "Base.nsp", Container(1));
        Write(share / "addons/Old.nsp", Container(2, true, 65536));
        Write(share / "addons/New.nsp", Container(3, true, 131072));
        // Rebuild another title's CNMT/NCA, rather than trusting misleading file labels.
        auto code = PlainNca(Package({{"main", Nso(9)}, {"main.npdm", Npdm(title + 0x2000)}}), (title + 0x2000) | 0x800)->ReadAllBytes();
        FileSys::ContentRecord record{}; record.type = FileSys::ContentRecordType::Program; record.nca_id.fill(0x11);
        FileSys::CNMTHeader cnmt{}; cnmt.title_id = (title + 0x2000) | 0x800; cnmt.type = FileSys::TitleType::Update;
        cnmt.title_version = 999999; cnmt.number_content_entries = 1; cnmt.table_offset = sizeof(FileSys::OptionalHeader);
        auto meta = PlainNca(Package({{"fixture.cnmt", FileSys::CNMT(cnmt, {}, {record}, {}).Serialize()}}), cnmt.title_id, true)->ReadAllBytes();
        Write(share / "addons/Misleading same-game newest.nsp", Package({{"11111111111111111111111111111111.nca", code}, {"metadata.cnmt.nca", meta}}));
        Write(share / "addons/Dlc.nsp", Dlc());
        Write(share / "addons/child/Child.nsp", Container(4, true, 196608));
        Write(share / "addons/child/too-deep/Ignored.nsp", Container(8, true, 524288));
        Write(share / "Broken.nsp", Bytes{1, 2, 3});
        Bytes nro(0x80 + 0x38 + 0x4000);
        std::memcpy(nro.data() + 0x10, "NRO0", 4); Put(nro, 0x18, 0x80);
        std::memcpy(nro.data() + 0x80, "ASET", 4);
        Put(nro, 0x98, 0x38, 8); Put(nro, 0xa0, 0x4000, 8);
        FileSys::RawNACP nacp{}; std::strcpy(nacp.language_entries.language_entries[0].application_name.data(), "Network homebrew fixture");
        nacp.save_data_owner_id = title; nacp.supported_languages = static_cast<FileSys::SupportedLanguage>(1);
        std::memcpy(nro.data() + 0xb8, &nacp, sizeof(nacp));
        std::ifstream icon(argv[3], std::ios::binary); Bytes png{std::istreambuf_iterator<char>(icon), {}};
        assert(!png.empty()); Put(nro, 0x88, nro.size() - 0x80, 8); Put(nro, 0x90, png.size(), 8);
        nro.insert(nro.end(), png.begin(), png.end()); Write(share / "Demo.NRO", nro);
        // A valid multi-GiB NRO: metadata must not read the sparse game payload.
        fs::resize_file(share / "Demo.NRO", 5ull << 30);
        return 0;
    }
    for (const auto [key, name] : {std::pair{Common::FS::EdenPath::NANDDir, "nand"},
            {Common::FS::EdenPath::SDMCDir, "sdmc"}, {Common::FS::EdenPath::LoadDir, "mods"},
            {Common::FS::EdenPath::DumpDir, "dump"}, {Common::FS::EdenPath::KeysDir, "keys"},
            {Common::FS::EdenPath::ConfigDir, "config"}, {Common::FS::EdenPath::LogDir, "log"}}) {
        fs::create_directories(root / name); Common::FS::SetEdenPath(key, root / name);
    }
    Common::Log::Initialize(); Common::Log::Start();
    Configure(argv[3]); Wait();
    assert(Known("pe-net://test/Demo.NRO"));
    assert(!Known("pe-net://test/addons/Old.nsp"));
    auto opened = Open("pe-net://test/Demo.NRO"); assert(opened.session);
    const auto before = opened.session->TransferredBytes();
    const auto info = InspectGame("pe-net://test/Demo.NRO", (root / "keys").string(), (root / "cover.tga").string());
    assert(info.error.empty()); assert(info.title == "Network homebrew fixture" && info.title_id == title);
    assert(!info.cover.empty() && fs::file_size(info.cover) > 18);
    assert(opened.session->TransferredBytes() - before < (2u << 20));
    assert(opened.session->CachedBytes() <= (1u << 20));
    const auto base = InspectGame("pe-net://test/Base.nsp", (root / "keys").string(), (root / "base.tga").string());
    assert(base.title_id == title); // no filename/title-ID guessing; real NSP/CNMT/NCA reader
    assert(base.title.empty() && base.cover.empty()); // base fixture intentionally has no control archive
    const auto settings = (root / "history.json").string();
    assert(Eden::SaveLastGame("pe-net://test/Demo.NRO", settings));
    assert(Eden::SaveRecentGame("pe-net://test/Demo.NRO", settings));
    assert(Eden::LoadLastGame(settings) == "pe-net://test/Demo.NRO");
    assert(Eden::LoadRecentGames(settings).at(0) == "pe-net://test/Demo.NRO");
    assert(Eden::SaveGameDocked(info.title_id, true, settings));
    assert(Eden::LoadGameDocked(info.title_id, settings)); // title available before launch
    assert(!Eden::SaveLastGame("pe-net://user:secret@nas/Demo.NRO", settings));
    assert(!Eden::SaveLastGame("pe-net://test/../Demo.NRO", settings));
    assert(Eden::SaveLastGame("Local/Game.nsp", settings));
    NetworkLauncherCheck(root.string(), info);
    BeginGame();
    FileSys::ManualContentProvider packed;
    Core::System system;
    auto vfs = std::make_shared<FileSys::RealVfsFilesystem>(); system.SetFilesystem(vfs);
    auto local = vfs->OpenFile((share / "Base.nsp").string(), FileSys::OpenMode::Read);
    auto local_loader = Loader::GetLoader(system, local); assert(local_loader);
    u64 local_title{};
    assert(local_loader->ReadProgramId(local_title) == Loader::ResultStatus::Success && local_title == title);
    assert(vfs->OpenDirectory((share / "addons").string(), FileSys::OpenMode::Read));
    assert(Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir) == root / "nand");
    assert(Common::FS::GetEdenPath(Common::FS::EdenPath::LoadDir) == root / "mods");
    system.SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
    Settings::values.external_content_dirs = AddonRoots("pe-net://test/Base.nsp");
    assert(Settings::values.external_content_dirs.size() == 1);
    {
        ReadBudget scan;
        system.GetFileSystemController().CreateFactories(*vfs);
        assert(!scan.Exhausted() && GameError().empty());
    }
    auto& provider = system.GetContentProvider();
    assert(provider.GetEntryVersion(title | 0x800).value_or(0) == 196608);
    assert(provider.HasEntry(title + 0x1001, FileSys::ContentRecordType::Data));
    assert(!provider.HasEntry(title + 0x1002, FileSys::ContentRecordType::Data));
    auto f = vfs->OpenFile("pe-net://test/Base.nsp", FileSys::OpenMode::Read);
    auto loader = Loader::GetLoader(system, f); assert(loader);
    u64 program{}; assert(loader->ReadProgramId(program) == Loader::ResultStatus::Success && program == title);
    assert(!vfs->OpenFile("pe-net://test/Base.nsp", FileSys::OpenMode::ReadWrite));
    auto network_dir = vfs->OpenDirectory(Settings::values.external_content_dirs.front(), FileSys::OpenMode::Read);
    assert(network_dir && !network_dir->IsWritable() && !network_dir->CreateFile("forbidden"));
    assert(!vfs->OpenDirectory("pe-net://test/unconfigured", FileSys::OpenMode::Read));
    // The launcher destructor uses StopDiscovery. Neither it nor a released RealVfs owner can
    // invalidate loader/provider OffsetVfsFile chains or their exact transport identity.
    StopDiscovery(); vfs.reset();
    FileSys::NSP container(f); assert(container.GetStatus() == Loader::ResultStatus::Success);
    const auto exefs = container.GetNCA(title, FileSys::ContentRecordType::Program)->GetExeFS();
    const auto selected = Eden::ReadCheatIdentity(exefs, true, title, system.GetFileSystemController(), provider);
    assert(selected && selected.program == title && selected.build[0] == 4); // not foreign 9, old 2 or too-deep 8
    assert(!system.IsPoweredOn());
    {
        ReadBudget tiny(1);
        auto remote = OpenVfs("pe-net://test/Demo.NRO");
        assert(remote->ReadAllBytes().empty() && tiny.Exhausted());
    }
    assert(GameError().empty()); // metadata refusal is not a made-up successful read or disconnection
    CancelGame();
    assert(!GameError().empty());
    NetworkLauncherFailureCheck(root.string(), GameError());
    std::array<u8, 32> untouched{}; untouched.fill(0xab);
    assert(f->Read(untouched.data(), untouched.size(), 0) == 0);
    for (auto c : untouched) assert(c == 0xab);
    EndGame(); Refresh(); Wait(); BeginGame();
    assert(GameError().empty());
    auto fresh = OpenVfs("pe-net://test/Demo.NRO"); assert(fresh);
    Loader::AppLoader_NRO nro(fresh);
    assert(nro.ReadProgramId(program) == Loader::ResultStatus::Success && program == title);
    assert(f->Read(untouched.data(), untouched.size(), 0) == 0); // old consumers never revive
    auto broken = Loader::GetLoader(system, OpenVfs("pe-net://test/Broken.nsp"));
    assert(broken);
    const auto failure = broken->ReadProgramId(program);
    assert(failure != Loader::ResultStatus::Success);
    const auto error_base = static_cast<unsigned>(Core::SystemResultStatus::ErrorLoader);
    NetworkLauncherFailureCheck(root.string(), Eden::LoadFailure::Text(
        error_base + static_cast<unsigned>(failure), error_base, Loader::GetResultStatusString(failure), false, -1));
    EndGame(); Disconnect();
    std::cout << "real network NRO/NSP metadata, bounded sparse reads, history, actual Launcher selection/settings/error re-entry, read-only provider/PatchManager update+DLC selection, loader lifetime, failure/reconnect PASS\n";
}
