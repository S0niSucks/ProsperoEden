// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic host filesystem evidence, not PS5 qualification. No games/keys/personal data.
#include "usb_backup.h"
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <cassert>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
namespace fs = std::filesystem;
namespace B = Eden::UsbBackup;
bool low_space = false, deny_stat = false, fail_sync = false, fail_read = false, short_write = false;
bool fail_write = false, fail_complete_sync = false, read_eintr = false, write_eintr = false;
bool fail_pending_sync = false, fail_pending_unlink = false;
bool fail_next_sync = false;
int fail_rename = 0, fail_sync_after_rename = 0;
extern "C" int __real_fstatvfs(int, struct statvfs*);
extern "C" int __wrap_fstatvfs(int fd, struct statvfs* s) {
    int n = __real_fstatvfs(fd, s); if (!n && low_space) s->f_bavail = 0; return n;
}
extern "C" int __real_fstatat(int, const char*, struct stat*, int);
extern "C" int __wrap_fstatat(int fd, const char* path, struct stat* s, int flags) {
    if (deny_stat) { errno = ENOSYS; return -1; } return __real_fstatat(fd, path, s, flags);
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
    if (fail_next_sync) { fail_next_sync = false; errno = EIO; return -1; }
    char path[1024]{};
    if (fail_complete_sync || fail_pending_sync) {
        auto n = readlink(("/proc/self/fd/" + std::to_string(fd)).c_str(), path, sizeof(path) - 1);
        if (n > 0 && fail_complete_sync && std::string(path, n).ends_with("/COMPLETE")) { errno = EIO; return -1; }
        if (n > 0 && fail_pending_sync && std::string(path, n).ends_with("/pending.json")) { errno = EIO; return -1; }
    }
    if (fail_sync) { errno = EIO; return -1; } return __real_fsync(fd);
}
extern "C" ssize_t __real_read(int, void*, size_t);
extern "C" ssize_t __wrap_read(int fd, void* p, size_t n) {
    if (read_eintr) { read_eintr = false; errno = EINTR; return -1; }
    if (fail_read) { errno = EIO; return -1; } return __real_read(fd, p, n);
}
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" ssize_t __wrap_write(int fd, const void* p, size_t n) {
    if (write_eintr) { write_eintr = false; errno = EINTR; return -1; }
    if (fail_write) { errno = ENOSPC; return -1; }
    return __real_write(fd, p, short_write ? std::min(n, size_t{7}) : n);
}
extern "C" int __real_renameat(int, const char*, int, const char*);
extern "C" int __wrap_renameat(int a, const char* b, int c, const char* d) {
    if (fail_rename && --fail_rename == 0) { errno = EIO; return -1; }
    auto result = __real_renameat(a, b, c, d);
    if (!result && fail_sync_after_rename && --fail_sync_after_rename == 0) fail_next_sync = true;
    return result;
}
extern "C" int __real_unlinkat(int, const char*, int);
extern "C" int __wrap_unlinkat(int fd, const char* name, int flags) {
    if (fail_pending_unlink && std::string(name) == "pending.json") { errno = EIO; return -1; }
    return __real_unlinkat(fd, name, flags);
}
void Write(const fs::path& p, const std::string& text) { fs::create_directories(p.parent_path()); std::ofstream(p, std::ios::binary) << text; }
std::string Read(const fs::path& p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
template<class F> void Fails(F f) { bool failed = false; try { f(); } catch (const std::exception&) { failed = true; } assert(failed); }
std::string Digest(const std::string& text) {
    unsigned char bytes[32]; unsigned length{};
    assert(EVP_Digest(text.data(), text.size(), bytes, &length, EVP_sha256(), nullptr) == 1 && length == 32);
    std::string result;
    for (auto b : bytes) { result += "0123456789abcdef"[b >> 4]; result += "0123456789abcdef"[b & 15]; }
    return result;
}
int main() {
    std::atomic<bool> stop{false};
    const fs::path root = "/work/usb-check", usb = "/usb/usb-check";
    fs::remove_all(root); fs::remove_all(usb); fs::create_directories(root); fs::create_directories(usb);
    const auto data = root / "data";
    std::string error;
    deny_stat = true; assert(B::ApplyPending(data, error)); deny_stat = false;
    Write(data / "config/prosperoeden.json", "synthetic settings with profiles and saved paths");
    Write(data / "user/nand/user/save/profile-a/title/save.bin", std::string(700000, 'a'));
    Write(data / "user/nand/user/save/profile-b/title/save.bin", "second profile");
    Write(data / "user/nand/system/save/index", "system save");
    Write(data / "covers/custom.tga", "user art");
    for (auto p : {"roms/dummy.nsp", "updates/dummy.nsp", "switch/dummy.nro", "logs/session.log", "user/cache/unpacked.bin",
                   "user/shader/title/cache", "library.json", "other/game.NSZ"}) Write(data / p, "excluded synthetic content");
    auto estimate = B::Inspect(data, usb, stop);
    assert(estimate.files == 5 && estimate.required > estimate.bytes);
    Fails([&] { B::Inspect(data, root, stop); }); // fake USB on same filesystem
    Fails([&] { B::Inspect(data, "/missing-usb", stop); });
    low_space = true; assert(B::Inspect(data, usb, stop).available == 0);
    Fails([&] { B::Backup(data, usb, stop); }); low_space = false; assert(B::Backups(usb, stop).empty());
    auto backup = B::Backup(data, usb, stop);
    assert(B::Backups(usb, stop).size() == 1 && fs::exists(fs::path(backup) / "COMPLETE"));
    assert(!fs::exists(fs::path(backup) / "data/roms"));
    auto second = B::Backup(data, usb, stop); assert(second != backup);
    assert(Read(fs::path(backup) / "data/config/prosperoeden.json") == Read(data / "config/prosperoeden.json"));
    // Cancellation/failed read never publish COMPLETE; existing backups remain unchanged.
    Fails([&] { B::Backup(data, usb, stop, [&](const B::Progress& p) { if (p.done > 0) stop = true; }); }); stop = false;
    assert(B::Backups(usb, stop).size() == 2);
    fail_read = true; Fails([&] { B::Backup(data, usb, stop); }); fail_read = false;
    fail_sync = true; Fails([&] { B::Backup(data, usb, stop); }); fail_sync = false;
    fail_write = true; Fails([&] { B::Backup(data, usb, stop); }); fail_write = false;
    fail_complete_sync = true; Fails([&] { B::Backup(data, usb, stop); }); fail_complete_sync = false;
    assert(B::Backups(usb, stop).size() == 2);
    read_eintr = write_eintr = true;
    short_write = true; auto short_copy = B::Backup(data, usb, stop); short_write = false;
    assert(!short_copy.empty());
    auto restore_estimate = B::InspectRestore(data, backup, stop);
    assert(restore_estimate.required >= estimate.required + estimate.bytes); // immutable stage + installed copy
    Fails([&] { B::StageRestore(data, backup, stop, {}, std::string(64, '0')); });
    // Self-consistent hostile manifests are still rejected: checksums are integrity, not authority.
    const auto manifest_path = fs::path(short_copy) / "manifest.json", complete_path = fs::path(short_copy) / "COMPLETE";
    const auto original_manifest = Read(manifest_path);
    auto malicious = nlohmann::json::parse(original_manifest);
    malicious["entries"][0]["path"] = "../escape";
    Write(manifest_path, malicious.dump()); Write(complete_path, Digest(malicious.dump()));
    Fails([&] { B::InspectRestore(data, short_copy, stop); });
    std::string deep(40, '['); deep += "0"; deep += std::string(40, ']');
    Write(manifest_path, deep); Write(complete_path, Digest(deep));
    Fails([&] { B::InspectRestore(data, short_copy, stop); });
    Write(manifest_path, original_manifest); Write(complete_path, Digest(original_manifest));
    // Refuse symlinks at file, directory and absolute-path ancestor levels, and hardlinks/FIFOs.
    fs::create_symlink("/etc/passwd", data / "hostile"); Fails([&] { B::Inspect(data, usb, stop); }); fs::remove(data / "hostile");
    fs::create_directory_symlink("/etc", data / "hostile"); Fails([&] { B::Inspect(data, usb, stop); }); fs::remove(data / "hostile");
    fs::create_directory_symlink(data, root / "alias"); Fails([&] { B::Inspect(root / "alias", usb, stop); });
    fs::create_hard_link(data / "covers/custom.tga", data / "hard"); Fails([&] { B::Inspect(data, usb, stop); }); fs::remove(data / "hard");
    assert(mkfifo((data / "fifo").c_str(), 0600) == 0); Fails([&] { B::Inspect(data, usb, stop); }); fs::remove(data / "fifo");
    Write(fs::path(second) / "data/extra", "unexpected"); Fails([&] { B::InspectRestore(data, second, stop); }); fs::remove(fs::path(second) / "data/extra");
    Write(fs::path(second) / "data/roms/extra.nsp", "excluded injection"); Fails([&] { B::InspectRestore(data, second, stop); }); fs::remove_all(fs::path(second) / "data/roms");
    Write(fs::path(second) / "data/covers/custom.tga", "tampered"); Fails([&] { B::StageRestore(data, second, stop); });
    assert(!fs::exists(root / ".data-restores/pending.json"));
    // Prepare a merge: current settings/save must be recoverable; absent files remain untouched.
    Write(data / "config/prosperoeden.json", "current settings and paths");
    Write(data / "user/nand/user/save/profile-a/title/save.bin", "current save");
    Write(data / "user/nand/user/save/profile-c/new/save.bin", "new save absent from backup");
    fs::remove(data / "covers/custom.tga");
    auto old_settings = Read(data / "config/prosperoeden.json");
    deny_stat = true; Fails([&] { B::InspectRestore(data, backup, stop); }); deny_stat = false;
    low_space = true; Fails([&] { B::StageRestore(data, backup, stop); }); low_space = false;
    Fails([&] { B::StageRestore(data, backup, stop, [&](const B::Progress& p) { if (p.phase == "copying" && p.done) stop = true; }); }); stop = false;
    assert(!fs::exists(root / ".data-restores/pending.json"));
    auto recovery = B::StageRestore(data, backup, stop);
    assert(Read(data / "config/prosperoeden.json") == old_settings); // active state NEVER replaced
    Fails([&] { B::StageRestore(data, backup, stop); }); // pending transaction exclusive
    const auto marker = root / ".data-restores/pending.json";
    auto original_pending = Read(marker); Write(marker, "{\"id\":\"../../escape\",\"sha256\":\"bad\"}");
    assert(!B::ApplyPending(data, error)); assert(Read(data / "config/prosperoeden.json") == old_settings);
    Write(marker, original_pending);
    // Interrupt after moving original away; next cold-start resumes without losing it.
    fail_rename = 2; assert(!B::ApplyPending(data, error));
    assert(fs::exists(root / ".data-restores/pending.json")); fail_rename = 0;
    assert(B::ApplyPending(data, error)); assert(!fs::exists(root / ".data-restores/pending.json"));
    assert(Read(fs::path(recovery) / "config/prosperoeden.json") == old_settings);
    assert(Read(fs::path(recovery) / "user/nand/user/save/profile-a/title/save.bin") == "current save");
    assert(Read(data / "config/prosperoeden.json") == Read(fs::path(backup) / "data/config/prosperoeden.json"));
    assert(Read(data / "user/nand/user/save/profile-c/new/save.bin") == "new save absent from backup");
    assert(Read(data / "roms/dummy.nsp") == "excluded synthetic content");
    assert(B::ApplyPending(data, error)); // ordinary startup/no pending is idempotent
    deny_stat = true; assert(B::ApplyPending(data, error)); deny_stat = false;
    // Stage again; hostile target must stop cold startup without following it.
    B::StageRestore(data, backup, stop);
    fs::rename(data / "config", data / "config-safe"); fs::create_directory_symlink("/etc", data / "config");
    assert(!B::ApplyPending(data, error)); fs::remove(data / "config"); fs::rename(data / "config-safe", data / "config");
    assert(B::ApplyPending(data, error));
    // Inject every preservation/install rename boundary. Retry must resume, never overwrite originals.
    for (int boundary = 1; boundary <= 10; ++boundary) {
        auto current = root / ("case-" + std::to_string(boundary));
        fs::copy(fs::path(backup) / "data", current, fs::copy_options::recursive);
        std::vector<fs::path> files;
        for (auto& entry : fs::recursive_directory_iterator(current)) if (entry.is_regular_file()) files.push_back(fs::relative(entry.path(), current));
        for (auto& file : files) Write(current / file, "original " + file.string());
        auto previous = B::StageRestore(current, backup, stop);
        fail_rename = boundary; assert(!B::ApplyPending(current, error)); fail_rename = 0;
        assert(B::ApplyPending(current, error));
        for (auto& file : files) {
            assert(Read(fs::path(previous) / file) == "original " + file.string());
            assert(Read(current / file) == Read(fs::path(backup) / "data" / file));
        }
    }
    // Also interrupt immediately AFTER every rename, at its durability barrier. Retrying must
    // re-sync already-preserved/installed files and directories, not just skip matching names.
    for (int boundary = 1; boundary <= 10; ++boundary) {
        auto current = root / ("sync-case-" + std::to_string(boundary));
        fs::copy(fs::path(backup) / "data", current, fs::copy_options::recursive);
        std::vector<fs::path> files;
        for (auto& entry : fs::recursive_directory_iterator(current)) if (entry.is_regular_file()) files.push_back(fs::relative(entry.path(), current));
        for (auto& file : files) Write(current / file, "original " + file.string());
        auto previous = B::StageRestore(current, backup, stop);
        fail_sync_after_rename = boundary; assert(!B::ApplyPending(current, error));
        assert(!fail_next_sync && fail_sync_after_rename == 0);
        assert(B::ApplyPending(current, error));
        for (auto& file : files) {
            assert(Read(fs::path(previous) / file) == "original " + file.string());
            assert(Read(current / file) == Read(fs::path(backup) / "data" / file));
        }
    }
    // Failed marker publication may be withdrawn, but if withdrawal itself fails the caller
    // receives a distinct restart-lock condition and the validated stage must not be deleted.
    fail_pending_sync = true;
    Fails([&] { B::StageRestore(data, backup, stop); });
    assert(!fs::exists(marker));
    fail_pending_unlink = true; bool locked = false;
    try { B::StageRestore(data, backup, stop); }
    catch (const B::RestorePending& e) { locked = true; assert(fs::exists(fs::path(e.recovery).parent_path() / "data")); }
    assert(locked && fs::exists(marker)); fail_pending_sync = fail_pending_unlink = false;
    fail_sync = true; assert(!B::ApplyPending(data, error)); fail_sync = false;
    assert(B::ApplyPending(data, error));
    // Session secrets are a whole-tree policy, not a filename allow/deny list. A hostile child
    // makes any accidental traversal fail, and progress/manifest/payload must contain no secret.
    const auto private_data = root / "nextendo-fixture";
    const std::string synthetic_secret = "SYNTHETIC_BEARER_EQUIVALENT_DO_NOT_COPY";
    Write(private_data / "config/prosperoeden.json", "nonsecret settings");
    Write(private_data / "config/nextendo-public.json", "nonsecret sibling stays included");
    for (const auto* subtree : {"config/nextendo", "CONFIG/NeXtEnDo", "config./NEXTENDO ", "config/Nextendo."}) {
        for (const auto* file : {"import.txt", "account.json", "routes/oauth.json", ".hidden", ".staging/unknown-name"})
            Write(private_data / subtree / file, synthetic_secret);
        fs::create_symlink("/missing-must-not-be-traversed", private_data / subtree / "must-not-scan");
    }
    fs::create_directory_symlink("/missing-must-not-be-opened", private_data / "config/NEXTENDO");
    assert(B::Inspect(private_data, usb, stop).files == 2);
    std::vector<std::string> reported_paths;
    const auto private_backup = B::Backup(private_data, usb, stop, [&](const B::Progress& p) { reported_paths.push_back(p.path); });
    const auto private_manifest = fs::path(private_backup) / "manifest.json";
    const auto clean_manifest = Read(private_manifest);
    assert(clean_manifest.find(synthetic_secret) == std::string::npos);
    for (auto& entry : fs::recursive_directory_iterator(fs::path(private_backup) / "data")) {
        assert(entry.path().filename() != "must-not-scan");
        if (entry.is_regular_file()) assert(Read(entry.path()).find(synthetic_secret) == std::string::npos);
    }
    for (const auto& path : reported_paths)
        assert(path.empty() || path == "config/prosperoeden.json" || path == "config/nextendo-public.json");
    assert(Read(fs::path(private_backup) / "data/config/nextendo-public.json") == "nonsecret sibling stays included");
    const auto private_recovery = B::StageRestore(private_data, private_backup, stop);
    assert(B::ApplyPending(private_data, error));
    assert(Read(private_data / "config/nextendo/import.txt") == synthetic_secret);
    assert(!fs::exists(fs::path(private_recovery) / "config/nextendo"));
    // Self-consistent malicious manifests must be rejected before opening any payload file.
    for (const auto* alias : {"config/nextendo", "CONFIG/NeXtEnDo", "config./Nextendo ", "config/nextendo.",
                              "config/./nextendo", "config/x/../nextendo", "config\\nextendo", "/config/nextendo"}) {
        auto injected = nlohmann::json::parse(clean_manifest);
        injected["entries"] = nlohmann::json::array({{{"path", alias}, {"directory", true}, {"size", 0u}, {"sha256", ""}, {"had", false}}});
        Write(private_manifest, injected.dump()); Write(fs::path(private_backup) / "COMPLETE", Digest(injected.dump()));
        const bool policy_alias = std::string(alias).find("..") == std::string::npos &&
            std::string(alias).find("/./") == std::string::npos && std::string(alias).find('\\') == std::string::npos && alias[0] != '/';
        const auto rejects = [&](auto operation) {
            bool rejected = false;
            try { operation(); } catch (const std::exception& e) {
                rejected = true;
                assert(std::string(e.what()).find(synthetic_secret) == std::string::npos);
                if (policy_alias) assert(std::string(e.what()).find("Excluded or duplicate manifest path") != std::string::npos);
            }
            assert(rejected);
        };
        rejects([&] { B::InspectRestore(private_data, private_backup, stop); });
        rejects([&] { B::StageRestore(private_data, private_backup, stop); });
        assert(!fs::exists(root / ".nextendo-fixture-restores/pending.json"));
    }
    Write(private_manifest, clean_manifest); Write(fs::path(private_backup) / "COMPLETE", Digest(clean_manifest));
    std::cout << "Nextendo whole-tree exclusion/alias/restore-injection tests passed (synthetic secrets)\n";
    std::cout << "USB backup/restore synthetic filesystem tests passed\n";
}
