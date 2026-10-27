// SPDX-License-Identifier: GPL-3.0-or-later
#include "amiibo_catalog.h"
#include "file_amiibo.h"
#include "quick_menu.h"
#include "core/hle/service/nfc/common/amiibo_crypto.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>

int main(int argc, char** argv) {
    assert(argc == 2); // Existing project PNG, no external artwork or tag data.
    using namespace Service::NFP;
    using Eden::AmiiboCatalog;
    namespace fs = std::filesystem;
    EncryptedNTAG215File raw{};
    raw.uuid.crc_check1 = 0x88;
    raw.static_lock = 0xe00f;
    raw.compatibility_container = 0xeeff10f1;
    raw.dynamic_lock = 0x0f0001;
    raw.CFG0 = 0x04000000;
    raw.CFG1 = 0x5f;
    constexpr std::array<unsigned char, 8> model{0, 0, 0, 0, 0, 0, 0, 2};
    std::memcpy(&raw.user_memory.model_info, model.data(), model.size());
    assert(AmiiboCrypto::IsAmiiboValid(raw));
    const auto packed = AmiiboCrypto::NfcDataToEncodedData(raw);
    assert(AmiiboCrypto::IsAmiiboValid(packed));
    std::vector<unsigned char> bytes(sizeof(raw));
    std::memcpy(bytes.data(), &raw, sizeof(raw));
    const auto dump = bytes;
    constexpr const char* id = "0000000000000002";
    assert(Eden::AmiiboModelId(bytes) == id);
    bytes.resize(532); assert(Eden::AmiiboModelId(bytes) == id);
    bytes = dump; bytes.resize(572, 0xee); assert(Eden::AmiiboModelId(bytes) == id);
    std::memcpy(bytes.data(), &packed, sizeof(packed)); assert(Eden::AmiiboModelId(bytes) == id);
    // An unrelated plausible ID at the other layout's offset is never the identification rule.
    bytes[0x54 + 2] = 9; assert(Eden::AmiiboModelId(bytes) == id);
    bytes = dump; bytes[0x1dc + 2] = 7; assert(Eden::AmiiboModelId(bytes) == id);
    bytes = dump; bytes[0x54 + 2] = 1;
    assert(Eden::AmiiboModelId(bytes) == "0000010000000002");
    bytes = dump; bytes[0x54 + 6] = 9;
    assert(Eden::AmiiboModelId(bytes) == "0000000000000902");
    bytes = dump; bytes[3] ^= 1; assert(Eden::AmiiboModelId(bytes).empty());
    bytes.assign(540, 7); assert(Eden::AmiiboModelId(bytes).empty());
    bytes.resize(531); assert(Eden::AmiiboModelId(bytes).empty());

    const std::string compact = R"({"amiibos":{"0x0000000000000002":{"name":"Mario"},
        "0x0000010000000002":{"name":"Mario - Variant"}},"amiibo_series":{"0x00":"Super Smash Bros."}})";
    const auto catalog = AmiiboCatalog::Parse(compact);
    assert(catalog.Size() == 2 && catalog.Find(id)->name == "Mario");
    assert(catalog.Find(id)->series == "Super Smash Bros.");
    assert(!catalog.Find("0000000000000902"));
    const auto api = AmiiboCatalog::Parse(R"({"amiibo":[{"head":"00000000","tail":"00000002",
        "name":"Mario","amiiboSeries":"Super Smash Bros.","image":"https://untrusted.invalid/tag.bin"}]})");
    assert(api.Size() == 1 && api.Find(id)->name == "Mario");
    assert(AmiiboCatalog::Parse(R"({"amiibo":{"head":"00000000","tail":"00000002","name":"Mario"}})").Size() == 1);
    for (const auto* text : {"{", "[]", "null", R"({"amiibos":false})",
         R"({"amiibos":{"0x0000000000000002":{"name":7}}})",
         R"({"amiibos":{"0x0000000000000002":{"name":"bad\nlabel"}}})",
         R"({"amiibos":{"invalid":{"name":"Mario"}}})",
         R"({"amiibos":{"0000000000000a02":{"name":"One"},"0000000000000A02":{"name":"Two"}}})",
         R"({"amiibos":{"0000000000000002":{"name":"One"},"0000000000000002":{"name":"Two"}}})"})
        assert(AmiiboCatalog::Parse(text).Size() == 0);
    assert(AmiiboCatalog::Parse(std::string(2 * 1024 * 1024 + 1, ' ')).Size() == 0);
    assert(AmiiboCatalog::Parse(std::string(64, '[') + "0" + std::string(64, ']')).Size() == 0);
    nlohmann::json oversized{{"amiibos", nlohmann::json::object()}};
    for (unsigned n = 0; n < 4097; ++n) {
        char key[17]; std::snprintf(key, sizeof(key), "%016x", n);
        oversized["amiibos"][key] = {{"name", "Test"}};
    }
    assert(AmiiboCatalog::Parse(oversized.dump()).Size() == 0);

    const auto root = fs::temp_directory_path() / ("eden-amiibo-catalog-" + std::to_string(getpid()));
    fs::create_directories(root / "Zelda");
    fs::create_directories(root / "Mario - Second");
    const auto save = [&](const fs::path& path, const auto& data) {
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(data.data()), data.size());
    };
    const auto tag = root / "Zelda/tag.bin";
    save(tag, dump); save(root / "Mario - Second/tag.bin", dump);
    save(root / "amiibo.json", compact);
    assert(AmiiboCatalog::Read(root.string()).Size() == 2);
    fs::create_directory_symlink(root / "Zelda", root / "alias");
    fs::create_symlink(tag, root / "alias.bin");
    fs::create_directories(root / ".hidden"); save(root / ".hidden/tag.bin", dump);
    auto deep = root / "deep";
    for (unsigned n = 0; n < 9; ++n) { fs::create_directories(deep); deep /= "child"; }
    fs::create_directories(deep); save(deep / "tag.bin", dump);
    const auto files = Eden::AmiiboFiles(root.string());
    assert(files.size() == 2 && std::is_sorted(files.begin(), files.end()));
    const auto description = Eden::DescribeAmiibo(tag.string(), root.string(), catalog);
    assert(description.id == id && description.label == "Mario / Super Smash Bros.");
    assert(Eden::DescribeAmiibo(tag.string(), root.string(), {}).label == "Zelda");
    const auto cover = root / "Zelda/cover.png";
    fs::copy_file(argv[1], cover);
    assert(Eden::DescribeAmiibo(tag.string(), root.string(), catalog).image_path == cover.string());
    save(root / "Zelda/second.bin", dump);
    assert(Eden::DescribeAmiibo(tag.string(), root.string(), catalog).image_path.empty());
    const auto pair = root / "Zelda/tag.png";
    fs::copy_file(argv[1], pair);
    assert(Eden::DescribeAmiibo(tag.string(), root.string(), catalog).image_path == pair.string());
    fs::remove(pair); fs::create_symlink(cover, pair);
    fs::create_directories(root / "images");
    const auto cached = root / "images/icon_00000000-00000002.png";
    fs::copy_file(argv[1], cached);
    assert(Eden::DescribeAmiibo(tag.string(), root.string(), catalog).image_path == cached.string());
    fs::remove(cached); save(cached, std::string("not a png"));
    assert(Eden::DescribeAmiibo(tag.string(), root.string(), catalog).image_path.empty());
    fs::remove(root / "Zelda/second.bin");
    {
        Eden::FileAmiibo engine;
        Eden::QuickMenu menu(engine, root.string());
        menu.Toggle(); assert(menu.SelectedAmiibo().label == "Mario / Super Smash Bros.");
        assert(engine.State() == Eden::FileAmiibo::Driver::State::Disabled); // browsing never scans
        menu.Press(ps5::pad::kButtonRight);
        assert(menu.SelectedAmiibo().label == "Mario / Super Smash Bros."); // duplicate ID keeps two files
        assert(Eden::QuickOverlay::Read().image); // actual selected PNG decoded, not only a path
        menu.Press(ps5::pad::kButtonDown); assert(!Eden::QuickOverlay::Read().image);
        menu.Press(ps5::pad::kButtonUp); assert(Eden::QuickOverlay::Read().image);
        menu.Toggle(); assert(!Eden::QuickOverlay::Read().image);
    }
    fs::rename(root / "amiibo.json", root / "catalog.json");
    fs::create_symlink(root / "catalog.json", root / "amiibo.json");
    assert(AmiiboCatalog::Read(root.string()).Size() == 0);
    fs::remove(root / "amiibo.json");
    assert(mkfifo((root / "amiibo.json").c_str(), 0600) == 0);
    assert(AmiiboCatalog::Read(root.string()).Size() == 0);
    // The entry budget is global: two folders plus at most 4094 files, not 4096 per folder.
    const auto limits = root / "limits";
    for (const auto* folder : {"A", "B"}) {
        fs::create_directories(limits / folder);
        for (unsigned n = 0; n < 2500; ++n) save(limits / folder / (std::to_string(n) + ".bin"), dump);
    }
    assert(Eden::AmiiboFiles(limits.string()).size() == 4094);
    fs::remove_all(root);
    std::puts("Amiibo catalog PASS: actual core raw/packed layout validation, full model IDs/variants, "
              "bounded compact/API metadata, folders, duplicate dumps, PNG pairing and offline fallbacks");
}
