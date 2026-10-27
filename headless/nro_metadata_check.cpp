// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the real metadata bridge and Eden's NACP reader without keys or firmware.
#include "metadata_bridge.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/loader/nro.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>

int main(int argc, char** argv) {
    assert(argc == 2);
    std::ifstream png(argv[1], std::ios::binary);
    const std::vector<unsigned char> icon{std::istreambuf_iterator<char>(png), {}};
    assert(!icon.empty() && icon.size() <= 1024 * 1024);
    char directory[] = "/tmp/eden-nro-metadata-XXXXXX";
    assert(mkdtemp(directory));
    const std::string path = std::string{directory} + "/Demo.NRO";
    const std::string cover = std::string{directory} + "/cover.tga";
    const std::string no_keys = std::string{directory} + "/no-keys";
    std::vector<unsigned char> bytes(0x80 + 0x38 + 0x4000);
    std::memcpy(bytes.data() + 0x10, "NRO0", 4);
    bytes[0x18] = 0x80;
    std::memcpy(bytes.data() + 0x80, "ASET", 4);
    bytes[0x98] = 0x38;
    bytes[0xa1] = 0x40;
    FileSys::RawNACP raw{};
    std::strcpy(raw.language_entries.language_entries[0].application_name.data(), "Homebrew fixture");
    raw.supported_languages = static_cast<FileSys::SupportedLanguage>(1);
    raw.save_data_owner_id = 0x010000000000f123ULL;
    std::memcpy(bytes.data() + 0xb8, &raw, sizeof(raw));
    const auto write = [&] {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        assert(out.good());
    };
    write();
    char title[513]{};
    assert(eden_extract_game_metadata(path.c_str(), no_keys.c_str(), cover.c_str(), title, sizeof(title)) ==
           EDEN_METADATA_TITLE);
    assert(std::string{title} == "Homebrew fixture");
    assert(eden_game_title_id(path.c_str()) == 0x010000000000f123ULL);
    assert(eden_game_supported_languages(path.c_str(), no_keys.c_str()) == 1);
    assert(!std::filesystem::exists(no_keys) && !std::filesystem::exists(cover));
    // Append an icon to this exact ASET, then exercise both the library and the real loader.
    const auto put = [&](std::size_t at, std::uint64_t value) {
        for (unsigned n = 0; n < 8; ++n) bytes[at + n] = static_cast<unsigned char>(value >> (n * 8));
    };
    put(0x88, 0x4038); put(0x90, icon.size());
    bytes.insert(bytes.end(), icon.begin(), icon.end()); write();
    assert(eden_extract_game_metadata(path.c_str(), no_keys.c_str(), cover.c_str(), title, sizeof(title)) ==
           (EDEN_METADATA_TITLE | EDEN_METADATA_COVER));
    assert(std::filesystem::file_size(cover) > 18);
    {
        FileSys::RealVfsFilesystem vfs;
        Loader::AppLoader_NRO loader(vfs.OpenFile(path, FileSys::OpenMode::Read));
        std::vector<u8> loaded_icon;
        assert(loader.ReadIcon(loaded_icon) == Loader::ResultStatus::Success && loaded_icon == icon);
        u64 program = 0;
        assert(loader.ReadProgramId(program) == Loader::ResultStatus::Success && program == 0x010000000000f123ULL);
    }
    std::filesystem::remove(cover);
    bytes.resize(0x80); write();
    assert(eden_extract_game_metadata(path.c_str(), no_keys.c_str(), cover.c_str(), title, sizeof(title)) == 0);
    assert(!title[0] && eden_game_title_id(path.c_str()) == 0);
    assert(eden_game_supported_languages(path.c_str(), no_keys.c_str()) == 0);
    std::filesystem::remove_all(directory);
    std::puts("Actual NRO metadata bridge and loader: icon, NACP title, program ID and languages without keys; bare NRO fallback PASS");
}
