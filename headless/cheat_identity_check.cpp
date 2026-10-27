// SPDX-License-Identifier: GPL-3.0-or-later
// Actual metadata/PatchManager/VFS/NCA code, synthetic code/header bytes and invented keys only.
#include "cheat_identity.h"
#include "effective_exefs.h"
#include "mods.h"
#include "settings_store.h"
#include "common/fs/path_util.h"
#include "common/logging.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/fssystem/fssystem_nca_header.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/submission_package.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/file_sys/vfs/vfs_vector.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/nso.h"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace {
constexpr u64 title = 0x0100000000001000;
using Bytes = std::vector<u8>;
void Put(Bytes& data, size_t offset, u64 value, unsigned size = 4) {
    for (unsigned n = 0; n < size; ++n) data.at(offset + n) = static_cast<u8>(value >> (8 * n));
}
Bytes Npdm(u64 program = title) {
    Bytes data(0x400);
    std::memcpy(data.data(), "META", 4);
    Put(data, 0x70, 0x80); Put(data, 0x74, 0x40);
    Put(data, 0x78, 0xc0); Put(data, 0x7c, 0x240);
    std::memcpy(data.data() + 0x80, "ACI0", 4);
    Put(data, 0x90, program, 8); Put(data, 0xa0, 0x340); Put(data, 0xb0, 0x40);
    std::memcpy(data.data() + 0x2c0, "ACID", 4);
    Put(data, 0x2e0, 0x300);
    return data;
}
Bytes Nso(u8 id) {
    Loader::NSOHeader header{};
    header.magic = 0x304f534e;
    header.build_id[0] = id; header.build_id[7] = 0x77; header.build_id[31] = 0x99;
    Bytes bytes(sizeof(header)); std::memcpy(bytes.data(), &header, sizeof(header)); return bytes;
}
FileSys::VirtualFile File(Bytes bytes, std::string name) {
    return std::make_shared<FileSys::VectorVfsFile>(std::move(bytes), std::move(name));
}
FileSys::VirtualDir Directory(Bytes main, Bytes npdm = Npdm()) {
    return std::make_shared<FileSys::VectorVfsDirectory>(
        std::vector<FileSys::VirtualFile>{File(std::move(main), "main"), File(std::move(npdm), "main.npdm")});
}
void Write(const std::filesystem::path& path, const Bytes& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size()); assert(out.good());
}
// Minimal plaintext Program NCA with a hash-structured ExeFS. Not a commercial image or a BKTR
// fixture: it exercises the real NCA reader and provider version selection without secrets.
Bytes Package(const std::vector<std::pair<std::string, Bytes>>& files) {
    std::string names;
    size_t total{};
    for (const auto& [name, data] : files) { names += name + '\0'; total += data.size(); }
    const auto start = 0x10 + files.size() * 0x18 + names.size();
    Bytes pfs(start + total);
    std::memcpy(pfs.data(), "PFS0", 4); Put(pfs, 4, files.size()); Put(pfs, 8, names.size());
    std::memcpy(pfs.data() + 0x10 + files.size() * 0x18, names.data(), names.size());
    size_t offset{}, name_offset{}, index{};
    for (const auto& [name, data] : files) {
        const auto entry = 0x10 + index++ * 0x18;
        Put(pfs, entry, offset, 8); Put(pfs, entry + 8, data.size(), 8); Put(pfs, entry + 16, name_offset);
        std::memcpy(pfs.data() + start + offset, data.data(), data.size());
        offset += data.size(); name_offset += name.size() + 1;
    }
    return pfs;
}
FileSys::VirtualFile PlainNca(Bytes pfs, u64 nca_title, bool meta = false) {
    using namespace FileSys;
    assert(pfs.size() <= 0x1000); pfs.resize(0x1000);
    NcaFsHeader fs{};
    fs.version = 2; fs.fs_type = NcaFsHeader::FsType::PartitionFs;
    fs.encryption_type = NcaFsHeader::EncryptionType::None;
    fs.hash_type = NcaFsHeader::HashType::HierarchicalSha256Hash;
    auto& hash = fs.hash_data.hierarchical_sha256_data;
    hash.hash_block_size = 0x1000; hash.hash_layer_count = 2;
    // Int64 is itself an aggregate of low/high words: assign fields, not flattened braces.
    hash.hash_layer_region[0].offset = 0; hash.hash_layer_region[0].size = 32;
    hash.hash_layer_region[1].offset = 0x200; hash.hash_layer_region[1].size = 0x1000;
    std::array<u8, 32> block{}; SHA256(pfs.data(), pfs.size(), block.data());
    SHA256(block.data(), block.size(), hash.fs_data_master_hash.value.data());
    NcaHeader header{};
    header.magic = NcaHeader::Magic3; header.program_id = nca_title;
    header.content_type = meta ? NcaHeader::ContentType::Meta : NcaHeader::ContentType::Program;
    header.sdk_addon_version = 0xffffffff; header.content_size = 0x1e00;
    header.fs_info[0] = {6, 15, 0, 0};
    SHA256(reinterpret_cast<const u8*>(&fs), sizeof(fs), header.fs_header_hash[0].value.data());
    Bytes archive(header.content_size);
    std::memcpy(archive.data(), &header, sizeof(header));
    std::memcpy(archive.data() + 0x400, &fs, sizeof(fs));
    std::memcpy(archive.data() + 0xc00, block.data(), block.size());
    std::memcpy(archive.data() + 0xe00, pfs.data(), pfs.size());
    auto file = File(std::move(archive), "fixture.nca");
    const NCA check(file);
    if (check.GetStatus() != Loader::ResultStatus::Success || (!meta && !check.GetExeFS()))
        std::fprintf(stderr, "Synthetic update NCA: status=%u exefs=%d\n",
            static_cast<unsigned>(check.GetStatus()), check.GetExeFS() != nullptr);
    assert(check.GetStatus() == Loader::ResultStatus::Success && (meta || check.GetExeFS()));
    return file;
}
FileSys::VirtualFile Update(u8 id) {
    return PlainNca(Package({{"main", Nso(id)}, {"main.npdm", Npdm()}}), title | 0x800);
}
Core::Crypto::Key256 HeaderKey() {
    Core::Crypto::Key256 key{};
    for (unsigned n = 0; n < key.size(); ++n) key[n] = static_cast<u8>(0x20 + n);
    return key;
}
void Encrypt(const EVP_CIPHER* algorithm, const u8* key, const u8* iv, u8* data, size_t size) {
    assert(size <= 0x10000);
    const auto context = EVP_CIPHER_CTX_new(); assert(context);
    assert(EVP_EncryptInit_ex(context, algorithm, nullptr, key, iv) == 1);
    assert(EVP_CIPHER_CTX_set_padding(context, 0) == 1);
    Bytes encrypted(size + EVP_MAX_BLOCK_LENGTH);
    int written{}, final{};
    assert(EVP_EncryptUpdate(context, encrypted.data(), &written, data, static_cast<int>(size)) == 1);
    assert(EVP_EncryptFinal_ex(context, encrypted.data() + written, &final) == 1);
    assert(static_cast<size_t>(written + final) == size);
    std::memcpy(data, encrypted.data(), size);
    EVP_CIPHER_CTX_free(context);
}
// Independent OpenSSL writer: invented keys only, encrypted header AND hashed ExeFS section.
// The pinned NCA reader must decrypt these bytes; plaintext fallback cannot read their magic.
Bytes EncryptedNca(Bytes archive) {
    using namespace FileSys;
    NcaHeader header{}; NcaFsHeader fs{};
    std::memcpy(&header, archive.data(), sizeof(header));
    std::memcpy(&fs, archive.data() + 0x400, sizeof(fs));
    Core::Crypto::Key128 data_key{}, area_key{};
    for (unsigned n = 0; n < data_key.size(); ++n) data_key[n] = static_cast<u8>(0x70 + n);
    area_key.fill(0x42);
    auto* encrypted_key = header.encrypted_key_area.data() + NcaHeader::DecryptionKey_AesCtr * data_key.size();
    std::memcpy(encrypted_key, data_key.data(), data_key.size());
    Encrypt(EVP_aes_128_ecb(), area_key.data(), nullptr, encrypted_key, data_key.size());
    fs.encryption_type = NcaFsHeader::EncryptionType::AesCtr;
    fs.aes_ctr_upper_iv.value = 0x0102030405060708;
    SHA256(reinterpret_cast<const u8*>(&fs), sizeof(fs), header.fs_header_hash[0].value.data());
    std::memcpy(archive.data(), &header, sizeof(header));
    std::memcpy(archive.data() + 0x400, &fs, sizeof(fs));
    std::array<u8, 16> iv{};
    for (unsigned n = 0; n < 8; ++n) {
        iv[7 - n] = static_cast<u8>(fs.aes_ctr_upper_iv.value >> (n * 8));
        iv[15 - n] = static_cast<u8>(u64{0xc00 / 16} >> (n * 8));
    }
    Encrypt(EVP_aes_128_ctr(), data_key.data(), iv.data(), archive.data() + 0xc00, archive.size() - 0xc00);
    const auto key = HeaderKey();
    for (size_t offset = 0; offset < 0xc00; offset += 0x200) {
        iv.fill(0);
        const auto sector = offset / 0x200;
        // NCA uses a big-endian sector number in the low half of its XTS tweak.
        for (unsigned n = 0; n < 8; ++n) iv[15 - n] = static_cast<u8>(sector >> (8 * n));
        Encrypt(EVP_aes_128_xts(), key.data(), iv.data(), archive.data() + offset, 0x200);
    }
    assert(std::memcmp(archive.data() + 0x200, "NCA3", 4) != 0);
    assert(std::memcmp(archive.data() + 0xe00, "PFS0", 4) != 0);
    return archive;
}
Bytes Container(u8 id, bool update = false, u32 version = 0, bool encrypted = false) {
    using namespace FileSys;
    const auto nca_title = update ? title | 0x800 : title;
    auto code = PlainNca(Package({{"main", Nso(id)}, {"main.npdm", Npdm()}}), nca_title)->ReadAllBytes();
    if (encrypted) code = EncryptedNca(std::move(code));
    ContentRecord content{};
    content.nca_id.fill(0x11); content.type = ContentRecordType::Program;
    for (unsigned n = 0; n < content.size.size(); ++n) content.size[n] = static_cast<u8>(code.size() >> (8 * n));
    SHA256(code.data(), code.size(), content.hash.data());
    CNMTHeader header{}; header.title_id = nca_title; header.title_version = version;
    header.type = update ? TitleType::Update : TitleType::Application;
    header.number_content_entries = 1; header.table_offset = sizeof(OptionalHeader);
    const auto cnmt = CNMT(header, OptionalHeader{}, {content}, {}).Serialize();
    auto meta = PlainNca(Package({{"fixture.cnmt", cnmt}}), nca_title, true)->ReadAllBytes();
    if (encrypted) meta = EncryptedNca(std::move(meta));
    return Package({{"11111111111111111111111111111111.nca", code}, {"metadata.cnmt.nca", meta}});
}
} // namespace

int main() {
    namespace fs = std::filesystem;
    using Common::FS::EdenPath;
    char folder[] = "/tmp/eden-cheat-identity-XXXXXX"; assert(mkdtemp(folder));
    const fs::path root = folder;
    for (const auto [key, name] : {std::pair{EdenPath::NANDDir, "nand"},
            {EdenPath::SDMCDir, "sdmc"}, {EdenPath::LoadDir, "mods"},
            {EdenPath::DumpDir, "dump"}, {EdenPath::KeysDir, "keys"}, {EdenPath::ConfigDir, "config"},
            {EdenPath::LogDir, "log"}}) {
        fs::create_directories(root / name); Common::FS::SetEdenPath(key, root / name);
    }
    Common::Log::Initialize(); Common::Log::Start();
    auto& keys = Core::Crypto::KeyManager::Instance();
    Core::Crypto::Key128 area{}; area.fill(0x42);
    keys.SetKey(Core::Crypto::S128KeyType::KeyArea, area);
    const auto header_key = HeaderKey();
    keys.SetKey(Core::Crypto::S256KeyType::Header, header_key);
    // SetKey intentionally never replaces an existing key in this pinned core.
    // Use its real file-reload path, and assert the effective bytes, so wrong-key
    // controls cannot accidentally keep testing the already-loaded correct key.
    const auto load_keys = [&](const Core::Crypto::Key256& header, const Core::Crypto::Key128& area_key) {
        const auto hex = [](const auto& value) {
            constexpr char digits[] = "0123456789abcdef";
            std::string text;
            for (const auto byte : value) { text += digits[byte >> 4]; text += digits[byte & 15]; }
            return text;
        };
        {
            std::ofstream file(root / "keys/prod.keys", std::ios::binary | std::ios::trunc);
            file << "header_key = " << hex(header) << '\n'
                 << "key_area_key_application_00 = " << hex(area_key) << '\n';
            assert(file.good());
        }
        keys.ReloadKeys();
        assert(keys.GetKey(Core::Crypto::S256KeyType::Header) == header);
        assert(keys.GetKey(Core::Crypto::S128KeyType::KeyArea) == area_key);
    };
    FileSys::ManualContentProvider updates;
    Core::System system;
    system.SetFilesystem(std::make_shared<FileSys::RealVfsFilesystem>());
    system.SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
    Settings::values.external_content_dirs.clear();
    Settings::values.dump_exefs.SetValue(false);
    Settings::values.disabled_addons.clear();
    system.GetFileSystemController().CreateFactories(*system.GetFilesystem());
    system.RegisterContentProvider(FileSys::ContentProviderUnionSlot::FrontendManual, &updates);
    const auto read = [&](FileSys::VirtualDir dir, bool patch = true, u64 expected = title) {
        return Eden::ReadCheatIdentity(std::move(dir), patch, expected,
            system.GetFileSystemController(), system.GetContentProvider());
    };
    assert(read(Directory(Nso(1))).build[0] == 1);
    assert(!read(nullptr) && !read(Directory(Nso(1)), true, 0));
    assert(!read(Directory(Nso(1)), true, title + 1));
    assert(!read(Directory(Bytes(255))) && !read(Directory(Bytes(256))));
    assert(!read(Directory(Nso(1), Bytes(127))));
    auto corrupt = Npdm(); Put(corrupt, 0xb4, 0xfffffffc);
    assert(!read(Directory(Nso(1), corrupt))); // refuses before a multi-GiB vector allocation
    corrupt = Npdm(); Put(corrupt, 0xb4, 1);
    assert(!read(Directory(Nso(1), corrupt))); // non-word count, no parser buffer overrun
    FileSys::ProgramMetadata direct;
    assert(direct.Load(File(corrupt, "main.npdm")) == Loader::ResultStatus::ErrorBadKernelCapabilityDescriptors);
    auto zero_prefix = Nso(0); zero_prefix[0x47] = 0;
    assert(!read(Directory(zero_prefix)));

    // Real manual provider, three updates: every insertion order selects the newest actual main.
    const std::array<FileSys::VirtualFile, 3> versions{Update(2), Update(3), Update(4)};
    std::array<unsigned, 3> order{0, 1, 2};
    do {
        updates.ClearAllEntries();
        for (auto n : order) updates.AddEntryWithVersion(FileSys::TitleType::Update,
            FileSys::ContentRecordType::Program, title | 0x800, (n + 1) * 65536, "same label", versions[n]);
        assert(read(Directory(Nso(1))).build[0] == 4);
    } while (std::next_permutation(order.begin(), order.end()));
    assert(read(Directory(Nso(1)), false).build[0] == 1); // extracted NSPs do not override
    updates.ClearAllEntries();

    const auto replacement = root / "mods/0100000000001000/Replacement/exefs";
    Write(replacement / "main", Nso(8));
    Write(replacement / "main.npdm", Npdm(title + 1));
    const auto replaced = read(Directory(Nso(1)));
    assert(replaced && replaced.program == title && replaced.build[0] == 8 && replaced.build[31] == 0x99);
    Settings::values.disabled_addons[title] = {"Replacement"};
    assert(read(Directory(Nso(1))).build[0] == 1);
    Settings::values.disabled_addons[title].clear();
    Write(replacement / "main", Nso(9));
    assert(read(Directory(Nso(1))).build[0] == 9); // no cached previous-build fallback
    Write(replacement / "main", Bytes(32));
    assert(!read(Directory(Nso(1)))); // broken replacement is not silently the base build
    Write(replacement / "main", Nso(9));
    Write(replacement / "main.npdm", corrupt);
    assert(!read(Directory(Nso(1))));
    assert(!system.IsPoweredOn()); // identity lookup never initializes or boots the guest
    assert(!Eden::ProbeCheatIdentity((root / "absent.nsp").string(), "", title));
    assert(!Eden::ProbeCheatIdentity((root / "demo.nro").string(), "", title));

    // Full on-disk container probe, including real CNMT/container/manual/external providers.
    // Everything is invented, plaintext and local; this is not commercial/BKTR qualification.
    const Eden::CheatProbePaths paths{(root / "keys").string(), (root / "mods").string(),
        (root / "cheats").string(), (root / "updates").string(), {}, (root / "settings.json").string()};
    fs::create_directories(paths.updates);
    const auto rom = root / "probe.nsp";
    Write(rom, Container(5));
    Write(replacement / "main.npdm", Npdm(title + 1));
    assert(Eden::SaveModEnabled(title, "Replacement", false, paths.settings));
    Settings::values.disabled_addons[title] = {"Unrelated state"};
    Settings::values.external_content_dirs = {"Unrelated directory"};
    Settings::values.dump_exefs.SetValue(true);
    Eden::Mods::SessionTitle() = 123; Eden::Mods::SessionGameFolder() = "Unrelated folder";
    const auto probe_path = [&](const fs::path& file, const Eden::CheatProbePaths& selected) {
        const auto result = Eden::ProbeCheatIdentity(file.string(), title, selected);
        assert(Settings::values.disabled_addons[title] == std::vector<std::string>{"Unrelated state"});
        assert(Settings::values.external_content_dirs == std::vector<std::string>{"Unrelated directory"});
        assert(Settings::values.dump_exefs.GetValue());
        assert(Eden::Mods::SessionTitle() == 123 && Eden::Mods::SessionGameFolder() == "Unrelated folder");
        assert(Common::FS::GetEdenPath(EdenPath::KeysDir) == root / "keys");
        assert(Common::FS::GetEdenPath(EdenPath::LoadDir) == root / "mods");
        return result;
    };
    const auto probe = [&] { return probe_path(rom, paths); };
    assert(probe().build[0] == 5);
    Write(root / "updates/older.nsp", Container(6, true, 65536));
    Write(root / "updates/newer.nsp", Container(7, true, 131072));
    assert(probe().build[0] == 7);
    assert(Eden::SaveModEnabled(title, "Replacement", true, paths.settings));
    assert(probe().build[0] == 9); // selected main replacement beats the update
    Write(rom, Package({{"main", Nso(10)}, {"main.npdm", Npdm()}}));
    assert(probe().build[0] == 10); // extracted NSP follows boot's no-update/no-override path
    Write(rom, Package({{"main", Nso(10)}, {"main.npdm", corrupt}}));
    assert(!probe()); // bounds also apply before early NSP title discovery
    FileSys::NSP malformed(File(Package({{"main", Nso(10)}, {"main.npdm", corrupt}}), "bad.nsp"));
    assert(!malformed.GetProgramTitleID());
    Write(rom, Bytes(8)); assert(!probe());
    // Two installed copies of one program must use the selected file's own native-VFS
    // updates and mods, not the last folder entered in a title-ID-only launcher map.
    assert(Eden::SaveModEnabled(title, "Replacement", false, paths.settings));
    for (unsigned copy = 0; copy < 2; ++copy) {
        auto local = paths;
        const auto folder = root / "roms" / ("Copy " + std::to_string(copy));
        local.game_folder = folder.string();
        const auto local_rom = folder / "game.nsp";
        Write(local_rom, Container(11 + copy));
        Write(folder / "update.nsp", Container(13 + copy, true, (3 + copy) * 65536));
        assert(probe_path(local_rom, local).build[0] == 13 + copy);
        const auto mod = folder / "mods" / ("Local " + std::to_string(copy)) / "exefs";
        Write(mod / "main", Nso(15 + copy));
        Write(mod / "main.npdm", Npdm(title + 1));
        assert(probe_path(local_rom, local).build[0] == 15 + copy);
        assert(Eden::SaveModEnabled(title, "Local " + std::to_string(copy), false, paths.settings));
        assert(probe_path(local_rom, local).build[0] == 13 + copy);
    }
    auto secure = paths;
    secure.mods = (root / "secure-mods").string(); secure.updates = (root / "secure-updates").string();
    fs::create_directories(secure.mods); fs::create_directories(secure.updates);
    const auto encrypted_rom = root / "secure-game.nsp";
    Write(encrypted_rom, Container(21, false, 0, true));
    assert(probe_path(encrypted_rom, secure).build[0] == 21);
    Write(root / "secure-updates/update.nsp", Container(22, true, 9 * 65536, true));
    assert(probe_path(encrypted_rom, secure).build[0] == 22);
    auto bad_header = header_key; bad_header[0] ^= 1;
    load_keys(bad_header, area);
    assert(!probe_path(encrypted_rom, secure)); // cannot use plaintext fallback or a stale build
    auto bad_area = area; bad_area[0] ^= 1;
    load_keys(header_key, bad_area);
    assert(!probe_path(encrypted_rom, secure));
    load_keys(header_key, area);
    assert(probe_path(encrypted_rom, secure).build[0] == 22);
    assert(fs::is_empty(root / "dump"));
    fs::remove_all(root);
    std::puts("Cheat identity PASS: real NPDM/NCA/PatchManager, six update orders, plaintext and independent encrypted NSP/CNMT/update fixtures with wrong-key refusal, native-VFS per-copy updates/mods, main replacement/disable, extracted NSP, original program binding, early malformed NPDM and unknown-build refusal, restored globals and no guest boot/dumps");
}
