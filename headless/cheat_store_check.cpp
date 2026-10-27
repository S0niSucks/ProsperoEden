// SPDX-License-Identifier: GPL-3.0-or-later
#include "cheat_store.h"
#include "cheat_download.h"
#include "mods.h"
#include "remote/http.h"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>

namespace {
namespace fs = std::filesystem;
using namespace Eden;
constexpr uint64_t program = 0x0100000000001234;
const std::string source{kCheatCatalogSource};
std::array<uint8_t, 32> Build() { std::array<uint8_t, 32> id{}; id[0] = 0xab; id[31] = 1; return id; }
CheatCatalog Catalog(unsigned value = 1) {
    return {CheatCatalogStatus::Ready, {{"Local test", "04000000 00000000 0000000" + std::to_string(value), false},
                                       {"Master test", "04000000 00000004 00000002", true}}};
}
void Write(const fs::path& file, const std::string& data) { std::ofstream(file, std::ios::binary) << data; }
std::string Read(const fs::path& file) { std::ifstream in(file, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }
fs::path OneFile(const fs::path& directory, const char* extension) {
    for (const auto& file : fs::recursive_directory_iterator(directory))
        if (file.path().extension() == extension) return file.path();
    return {};
}
size_t TextFiles(const fs::path& directory) {
    size_t count{};
    if (fs::exists(directory)) for (const auto& file : fs::recursive_directory_iterator(directory))
        count += file.path().extension() == ".txt";
    return count;
}
void NoPending(const fs::path& directory) {
    for (const auto& file : fs::recursive_directory_iterator(directory))
        assert(!file.path().filename().string().starts_with(".cheat-pending-"));
}
enum class Fault { None, Short, InterruptedWrite, WriteError, Sync, InterruptedRead, Eof, ReadError, Link, Rename };
thread_local Fault fault = Fault::None;
}
extern "C" int remote_http_get(const remote_http_request*, remote_http_result*) { std::abort(); } // store tests never fetch
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" ssize_t __wrap_write(int fd, const void* data, size_t size) {
    if (fault == Fault::WriteError) { errno = EIO; return -1; }
    if (fault == Fault::InterruptedWrite) { fault = Fault::None; errno = EINTR; return -1; }
    return __real_write(fd, data, fault == Fault::Short ? std::min<size_t>(size, 3) : size);
}
extern "C" ssize_t __real_pread(int, void*, size_t, off_t);
extern "C" ssize_t __wrap_pread(int fd, void* data, size_t size, off_t offset) {
    if (fault == Fault::ReadError) { errno = EIO; return -1; }
    if (fault == Fault::Eof) return 0;
    if (fault == Fault::InterruptedRead) { fault = Fault::None; errno = EINTR; return -1; }
    return __real_pread(fd, data, fault == Fault::Short ? std::min<size_t>(size, 3) : size, offset);
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) { if (fault == Fault::Sync) { errno = EIO; return -1; } return __real_fsync(fd); }
extern "C" int __real_linkat(int, const char*, int, const char*, int);
extern "C" int __wrap_linkat(int from, const char* source_name, int to, const char* destination, int flags) {
    if (fault == Fault::Link) { errno = EPERM; return -1; }
    return __real_linkat(from, source_name, to, destination, flags);
}
extern "C" int __real_renameat(int, const char*, int, const char*);
extern "C" int __wrap_renameat(int from, const char* source_name, int to, const char* destination) {
    if (fault == Fault::Rename) { errno = EIO; return -1; }
    return __real_renameat(from, source_name, to, destination);
}
int main() {
    using namespace Eden;
    char temporary[] = "/tmp/eden-cheat-store-XXXXXX"; assert(mkdtemp(temporary));
    const fs::path root = temporary, cache = root / "cache", mods = root / "mods", title = mods / "0100000000001234";
    const auto build = Build(); const auto catalog = Catalog();
    assert(SaveCheatCache(cache.string(), source, program, build, catalog));
    const auto cached_text = LoadCheatCache(cache.string(), source, program, build).Text();
    assert(cached_text.starts_with("{Master test}") && cached_text.find("[Local test]") != cached_text.npos);
    assert(LoadCheatCache(cache.string(), source + "?different", program, build).status == CheatCatalogStatus::Invalid);
    assert(LoadCheatCache(cache.string(), source, program + 1, build).status == CheatCatalogStatus::Invalid);
    auto changed_build = build; changed_build[31] ^= 1;
    assert(LoadCheatCache(cache.string(), source, program, changed_build).status == CheatCatalogStatus::Invalid);
    const auto record = OneFile(cache, ".json"); const auto original = Read(record);
    for (auto mode : {Fault::Short, Fault::InterruptedWrite}) {
        fault = mode; assert(SaveCheatCache(cache.string(), source, program, build, Catalog(2)));
        fault = Fault::None; assert(LoadCheatCache(cache.string(), source, program, build).Text().find("00000002") != std::string::npos);
    }
    for (auto mode : {Fault::WriteError, Fault::Sync, Fault::Rename}) {
        const auto before = Read(record); fault = mode;
        assert(!SaveCheatCache(cache.string(), source, program, build, Catalog(3)));
        fault = Fault::None; assert(Read(record) == before); NoPending(cache);
    }
    for (auto mode : {Fault::Short, Fault::InterruptedRead}) {
        fault = mode; assert(LoadCheatCache(cache.string(), source, program, build).status == CheatCatalogStatus::Ready); fault = Fault::None;
    }
    for (auto mode : {Fault::Eof, Fault::ReadError}) {
        fault = mode; assert(LoadCheatCache(cache.string(), source, program, build).status == CheatCatalogStatus::Invalid); fault = Fault::None;
    }
    Write(record, std::string(2 * 1024 * 1024 + 1, ' '));
    assert(LoadCheatCache(cache.string(), source, program, build).status == CheatCatalogStatus::Invalid);
    assert(!SaveCheatCache(cache.string(), source, program, build, catalog)); // don't overwrite unknown data
    Write(record, "{}"); assert(!SaveCheatCache(cache.string(), source, program, build, catalog));
    auto tampered = nlohmann::json::parse(original); tampered["build"] = std::string(64, '0'); Write(record, tampered.dump());
    assert(LoadCheatCache(cache.string(), source, program, build).status == CheatCatalogStatus::Invalid);
    tampered = nlohmann::json::parse(original); tampered["text_sha256"] = std::string(64, '0'); Write(record, tampered.dump());
    assert(LoadCheatCache(cache.string(), source, program, build).status == CheatCatalogStatus::Invalid);
    Write(record, "{\"deep\":" + std::string(20000, '[') + "0" + std::string(20000, ']') + "}");
    assert(LoadCheatCache(cache.string(), source, program, build).status == CheatCatalogStatus::Invalid);
    assert(!SaveCheatCache(cache.string(), source, program, build, catalog));
    Write(record, "{\"schema\":0," + original.substr(1));
    assert(LoadCheatCache(cache.string(), source, program, build).status == CheatCatalogStatus::Invalid);
    Write(record, original);
    auto bad = catalog; bad.entries[0].name = "bad] [inject";
    assert(!SaveCheatCache(cache.string(), source, program, build, bad));
    assert(Read(record) == original);
    fs::create_directories(root / "outside"); Write(root / "outside/user.json", "user file");
    fs::remove(record); fs::create_symlink(root / "outside/user.json", record);
    assert(!SaveCheatCache(cache.string(), source, program, build, catalog));
    assert(LoadCheatCache(cache.string(), source, program, build).status == CheatCatalogStatus::Invalid);
    assert(Read(root / "outside/user.json") == "user file"); fs::remove(record);
    assert(mkfifo(record.c_str(), 0600) == 0);
    assert(!SaveCheatCache(cache.string(), source, program, build, catalog)); fs::remove(record);
    fs::create_directory_symlink(root / "outside", root / "linked");
    assert(!SaveCheatCache((root / "linked/cache").string(), source, program, build, catalog));
    assert(!SaveCheatCache((root / "../bad").string(), source, program, build, catalog));
    assert(!fs::exists(root / "outside/cache"));

    std::atomic<bool> stop{false}; int checks{};
    const auto install = [&](const fs::path& directory, const CheatCatalog& data) {
        return InstallCheatCatalog(directory.string(), source, program, build, data, stop, [&] { ++checks; return true; });
    };
    const auto first = install(title, catalog);
    assert(first.status == CheatInstallStatus::Installed && checks == 1);
    assert(IsDownloadedCheatMod(first.mod_name));
    assert(!IsDownloadedCheatMod("Downloaded cheats - My own"));
    auto manual = Mods::WithCheats("Downloaded cheats - My own", Mods::kCheats,
        Patches::ParseCheats("[Hand copied]\n04000000 00000000 00000001\n", "Downloaded cheats - My own"));
    assert(manual.cheats.empty() && manual.cheat == "Hand copied");
    assert(Mods::Switches({manual}, {}, {}, {}, true, true).mods_off.empty());
    const auto text_file = title / first.mod_name / "cheats/ab00000000000000.txt";
    assert(Read(text_file) == cached_text);
    const auto provenance = nlohmann::json::parse(Read(title / first.mod_name / "provenance.json"));
    assert(provenance["source"] == source && provenance["default"] == "off" && provenance["build"].get<std::string>().size() == 64);
    assert(install(title, catalog).status == CheatInstallStatus::AlreadyPresent);
    auto list = Mods::List(mods.string(), program);
    assert(list.size() == 1 && list[0].cheats.size() == 1 && list[0].cheat.empty());
    const auto switched = Mods::Switches(list, {}, {}, {}, true, true);
    assert(Mods::Has(switched.mods_off, first.mod_name)); // master and lone optional code OFF, no settings write
    const auto chosen = Mods::ChooseCheat(list[0], "Local test", true, {});
    assert(!Mods::Has(Mods::Switches(list, {}, chosen, {}, true, true).mods_off, first.mod_name));
    assert(Mods::Has(Mods::Switches(list, {}, chosen, {}, true, false).mods_off, first.mod_name));
    const auto second = install(title, Catalog(2));
    assert(second.status == CheatInstallStatus::Installed && second.mod_name != first.mod_name);
    list = Mods::List(mods.string(), program);
    assert(Mods::Has(Mods::Switches(list, {}, chosen, {}, true, true).mods_off, second.mod_name));
    // Existing user files, modified managed data and hostile final symlinks are never overwritten.
    Write(text_file, "user-edited cheat");
    assert(install(title, catalog).status == CheatInstallStatus::Conflict && Read(text_file) == "user-edited cheat");
    fs::remove(text_file); fs::create_symlink(root / "outside/user.json", text_file);
    assert(install(title, catalog).status == CheatInstallStatus::Conflict && Read(root / "outside/user.json") == "user file");
    fs::remove(text_file); assert(mkfifo(text_file.c_str(), 0600) == 0);
    assert(install(title, catalog).status == CheatInstallStatus::Conflict); fs::remove(text_file);
    fs::create_directory_symlink(root / "outside", root / "alias-mods");
    assert(install(root / "alias-mods/title", catalog).status == CheatInstallStatus::Failed);
    assert(!fs::exists(root / "outside/title"));
    stop = true; assert(install(root / "cancel-before", catalog).status == CheatInstallStatus::Cancelled);
    assert(!fs::exists(root / "cancel-before")); stop = false;
    assert(InstallCheatCatalog((root / "stale").string(), source, program, build, catalog, stop,
        [] { return false; }).status == CheatInstallStatus::StaleIdentity);
    assert(TextFiles(root / "stale") == 0);
    assert(InstallCheatCatalog((root / "cancel-late").string(), source, program, build, catalog, stop,
        [&] { stop = true; return true; }).status == CheatInstallStatus::Cancelled); stop = false;
    assert(TextFiles(root / "cancel-late") == 0);
    for (auto mode : {Fault::WriteError, Fault::Sync, Fault::Link}) {
        const auto folder = root / ("fault-" + std::to_string(int(mode))); fault = mode;
        const auto result = install(folder, catalog); fault = Fault::None;
        assert(result.status != CheatInstallStatus::Installed && result.status != CheatInstallStatus::AlreadyPresent);
        assert(TextFiles(folder) == 0); NoPending(folder);
    }
    assert(install(root / "invalid", bad).status == CheatInstallStatus::Failed);
    auto master_only = catalog; master_only.entries.erase(master_only.entries.begin());
    assert(install(root / "master-only", master_only).status == CheatInstallStatus::Failed);
    // Renaming an ancestor and substituting a symlink cannot redirect held-descriptor writes.
    const auto held = root / "held", moved = root / "held-moved";
    const auto replaced = InstallCheatCatalog(held.string(), source, program, build, catalog, stop, [&] {
        fs::rename(held, moved); fs::create_directory_symlink(root / "outside", held); return true;
    });
    assert(replaced.status == CheatInstallStatus::Installed && TextFiles(moved) == 1 && TextFiles(root / "outside") == 0);
    std::vector<std::thread> threads;
    for (unsigned n = 0; n < 8; ++n) threads.emplace_back([&] {
        const auto result = InstallCheatCatalog((root / "parallel").string(), source, program, build, catalog, stop, [] { return true; });
        assert(result.status == CheatInstallStatus::Installed || result.status == CheatInstallStatus::AlreadyPresent);
    });
    for (auto& thread : threads) thread.join();
    assert(TextFiles(root / "parallel") == 1); NoPending(root);
    fs::remove_all(root);
    std::puts("Cheat store PASS: bound offline cache, bounded/revalidated records, short/EINTR/error I/O, failed-write preservation, no-clobber provenance/install, symlink/FIFO/ancestor attacks, stale/cancelled publication, eight installers and default-OFF masters/single codes");
}
