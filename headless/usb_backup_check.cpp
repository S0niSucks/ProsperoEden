// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic host filesystem evidence, not PS5 qualification. No games/keys/personal data.
#include "usb_backup.h"
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <fcntl.h>
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
bool privacy_probe = false;
std::size_t forbidden_probes = 0;
std::vector<std::string> protected_paths;
bool PrivateProbe(int fd, const char* name = nullptr) {
    if (!privacy_probe) return false;
    std::string path;
    if (name && name[0] == '/') path = name;
    else {
        char target[4096]{};
        const auto n = readlink(("/proc/self/fd/" + std::to_string(fd)).c_str(), target, sizeof(target) - 1);
        if (n <= 0) return false;
        path.assign(target, n);
        if (name && *name) path += "/" + std::string(name);
    }
    path = fs::path(path).lexically_normal().string();
    while (!path.empty() && path.back() == '/') path.pop_back();
    for (const auto& secret : protected_paths) {
        if (path == secret || path.starts_with(secret + "/")) {
            ++forbidden_probes; errno = EACCES; return true;
        }
    }
    return false;
}
extern "C" int __real_openat(int, const char*, int, ...);
extern "C" int __wrap_openat(int fd, const char* name, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) { va_list args; va_start(args, flags); mode = va_arg(args, mode_t); va_end(args); }
    if (PrivateProbe(fd, name)) return -1;
    return __real_openat(fd, name, flags, mode);
}
extern "C" int __real_fstatvfs(int, struct statvfs*);
extern "C" int __wrap_fstatvfs(int fd, struct statvfs* s) {
    int n = __real_fstatvfs(fd, s); if (!n && low_space) s->f_bavail = 0; return n;
}
extern "C" int __real_fstatat(int, const char*, struct stat*, int);
extern "C" int __wrap_fstatat(int fd, const char* path, struct stat* s, int flags) {
    if (PrivateProbe(fd, path)) return -1;
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
    if (PrivateProbe(fd)) return -1;
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
    // Exercise real inventory/copy/hash/restore, not the private Excluded predicate.
    const auto private_data = root / "nextendo-fixture";
    const std::string synthetic_secret = "SYNTHETIC_BEARER_EQUIVALENT_DO_NOT_COPY";
    const std::string synthetic_credentials = "{\"password\":\"SYNTHETIC_SMB_PASSWORD_DO_NOT_COPY\"}";
    const std::vector<std::pair<std::string, std::string>> public_files = {
        {"config/prosperoeden.json", "nonsecret settings"},
        {"config/nextendo-public.json", "nonsecret sibling stays included"},
        {"config/network-shares-public.json", "harmless network-share sibling"},
        {"config/network-shares.json-safe", "hyphen is not the staging suffix boundary"},
        {"config/network-shares.jsonx", "longer extension is not the staging suffix boundary"},
        {"config/network-shares.json-safe-dir/public", "harmless directory child"},
        {"config/unrelated.json.tmp", "do not generically exclude staged config"}};
    std::uint64_t public_bytes = 0;
    for (const auto& [path, bytes] : public_files) { Write(private_data / path, bytes); public_bytes += bytes.size(); }
    const auto protect = [&](const std::string& path) { protected_paths.push_back((private_data / path).string()); };
    for (const auto* subtree : {"config/nextendo", "CONFIG/NeXtEnDo", "config./NEXTENDO ", "config/Nextendo."}) {
        protect(subtree);
        for (const auto* file : {"import.txt", "account.json", "routes/oauth.json", ".hidden", ".staging/unknown-name"})
            Write(private_data / subtree / file, synthetic_secret);
        fs::create_symlink("/missing-must-not-be-traversed", private_data / subtree / "must-not-scan");
    }
    fs::create_directory_symlink("/missing-must-not-be-opened", private_data / "config/NEXTENDO");
    protect("config/NEXTENDO");
    for (const auto* file : {"config/network-shares.json", "CONFIG/Network-Shares.Json", "config./NETWORK-SHARES.JSON "}) {
        Write(private_data / file, synthetic_credentials); protect(file);
    }
    Write(private_data / "config/Network-Shares.Json./.staging/unknown", synthetic_credentials);
    fs::create_symlink("/missing-must-not-be-traversed", private_data / "config/Network-Shares.Json./must-not-scan");
    fs::create_symlink("/missing-must-not-be-opened", private_data / "config/NETWORK-SHARES.JSON");
    protect("config/Network-Shares.Json."); protect("config/NETWORK-SHARES.JSON");
    fs::create_directories(private_data / "CONFIG. "); // include alias parents in the control estimate
    const auto public_estimate = B::Inspect(private_data, usb, stop);
    assert(public_estimate.files == public_files.size() && public_estimate.bytes == public_bytes);
    const std::vector<std::string> staged_files = {
        "config/network-shares.json.tmp", "config/network-shares.json.bak", "config/network-shares.json.staged.foo",
        "CONFIG/NeTwOrK-ShArEs.JsOn.TmP", "config./NETWORK-SHARES.JSON.BAK  ", "CONFIG. /network-shares.json.staged.foo. "};
    for (const auto& path : staged_files) { Write(private_data / path, synthetic_credentials); protect(path); }
    const auto private_estimate = B::Inspect(private_data, usb, stop);
    std::vector<B::Progress> reported;
    const auto report = [&](const B::Progress& p) { reported.push_back(p); };
    const auto private_backup = B::Backup(private_data, usb, stop, report);
    // On the unmodified backend this is a COMPLETE backup containing six synthetic passwords.
    std::size_t copied_credentials = 0;
    for (const auto& path : staged_files) {
        if (fs::exists(fs::path(private_backup) / "data" / path)) {
            assert(Read(fs::path(private_backup) / "data" / path) == synthetic_credentials);
            ++copied_credentials;
        }
    }
    if (copied_credentials) std::cerr << "USB privacy baseline: actual COMPLETE backup copied " << copied_credentials
        << " synthetic credential files; inventory counted " << private_estimate.files << " files / " << private_estimate.bytes << " bytes\n";
    assert(!copied_credentials);
    assert(private_estimate.files == public_estimate.files && private_estimate.bytes == public_estimate.bytes &&
           private_estimate.required == public_estimate.required);
    const auto private_manifest = fs::path(private_backup) / "manifest.json";
    const auto clean_manifest = Read(private_manifest);
    assert(clean_manifest.find(synthetic_secret) == std::string::npos);
    assert(clean_manifest.find(synthetic_credentials) == std::string::npos);
    assert(clean_manifest.find(Digest(synthetic_secret)) == std::string::npos);
    assert(clean_manifest.find(Digest(synthetic_credentials)) == std::string::npos);
    for (auto& entry : fs::recursive_directory_iterator(fs::path(private_backup) / "data")) {
        assert(entry.path().filename() != "must-not-scan");
        if (entry.is_regular_file()) {
            assert(Read(entry.path()).find(synthetic_secret) == std::string::npos);
            assert(Read(entry.path()).find(synthetic_credentials) == std::string::npos);
        }
    }
    const auto check_reports = [&] {
        for (const auto& p : reported) {
            assert(p.total == public_bytes && p.done <= public_bytes);
            assert(p.path.empty() || std::any_of(public_files.begin(), public_files.end(), [&](const auto& file) { return file.first == p.path; }));
        }
    };
    check_reports();
    for (const auto& [path, bytes] : public_files) assert(Read(fs::path(private_backup) / "data" / path) == bytes);
    // Hostile aliases contain invalid names, unknown/hidden/staging files, broken links and a FIFO.
    // Any attempted open/stat/read of these exact fixture nodes is counted and denied, independent
    // of the production policy; the guarded backup must still succeed with an identical manifest.
    for (const auto* subtree : {"config/network-shares.json.tmp.dir", "CONFIG/NETWORK-SHARES.JSON.BAK. ",
                                "config./Network-Shares.Json.Staged.Foo. "}) {
        protect(subtree);
        for (const auto* file : {"unknown", ".hidden", ".staging/anything", "invalid:name"})
            Write(private_data / subtree / file, synthetic_credentials);
        fs::create_symlink("/missing-must-not-be-traversed", private_data / subtree / "must-not-scan");
        assert(mkfifo((private_data / subtree / "fifo").c_str(), 0600) == 0);
    }
    fs::create_symlink("/missing-must-not-be-opened", private_data / "config/NETWORK-SHARES.JSON.TMP ");
    protect("config/NETWORK-SHARES.JSON.TMP ");
    const auto probe_path = (private_data / staged_files.front()).string();
    const int probe_fd = open(probe_path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    assert(probe_fd >= 0);
    privacy_probe = true;
    struct stat probe_stat{}; char probe_byte{};
    assert(openat(AT_FDCWD, probe_path.c_str(), O_RDONLY) == -1 && errno == EACCES);
    assert(fstatat(AT_FDCWD, probe_path.c_str(), &probe_stat, AT_SYMLINK_NOFOLLOW) == -1 && errno == EACCES);
    assert(read(probe_fd, &probe_byte, 1) == -1 && errno == EACCES);
    assert(forbidden_probes == 3); close(probe_fd); forbidden_probes = 0;
    const auto guarded_estimate = B::Inspect(private_data, usb, stop);
    assert(guarded_estimate.files == public_estimate.files && guarded_estimate.bytes == public_bytes &&
           guarded_estimate.required == public_estimate.required);
    const auto guarded_backup = B::Backup(private_data, usb, stop, report);
    assert(Read(fs::path(guarded_backup) / "manifest.json") == clean_manifest);
    check_reports();
    assert(!forbidden_probes);
    // Even an unchanged valid manifest cannot legitimize extra credential payload nodes.
    for (const auto& path : staged_files) {
        const auto injected = fs::path(private_backup) / "data" / path;
        Write(injected, synthetic_credentials); protected_paths.push_back(injected.string());
    }
    Fails([&] { B::InspectRestore(private_data, private_backup, stop); });
    Fails([&] { B::StageRestore(private_data, private_backup, stop); });
    assert(!forbidden_probes && !fs::exists(root / ".nextendo-fixture-restores/pending.json"));
    for (const auto& path : staged_files) assert(fs::remove(fs::path(private_backup) / "data" / path));
    const auto private_recovery = B::StageRestore(private_data, private_backup, stop);
    assert(B::ApplyPending(private_data, error));
    assert(Read(private_data / "config/nextendo/import.txt") == synthetic_secret);
    assert(Read(private_data / "config/network-shares.json") == synthetic_credentials);
    assert(!fs::exists(fs::path(private_recovery) / "config/nextendo"));
    assert(!fs::exists(fs::path(private_recovery) / "config/network-shares.json"));
    for (const auto& path : staged_files) {
        assert(Read(private_data / path) == synthetic_credentials);
        assert(!fs::exists(fs::path(private_recovery) / path));
    }
    assert(!forbidden_probes);
    // Self-consistent malicious manifests must be rejected before opening any payload file.
    protected_paths.push_back((fs::path(private_backup) / "data").string());
    for (const auto* alias : {"config/nextendo", "CONFIG/NeXtEnDo", "config./Nextendo ", "config/nextendo.",
                              "config/./nextendo", "config/x/../nextendo", "config\\nextendo", "/config/nextendo",
                              "config/network-shares.json", "CONFIG/Network-Shares.Json", "config./NETWORK-SHARES.JSON ", "config/network-shares.json.",
                              "config/./network-shares.json", "config/x/../network-shares.json", "config\\network-shares.json", "/config/network-shares.json",
                              "config/network-shares.json.tmp", "config/network-shares.json.bak", "config/network-shares.json.staged.foo",
                              "CONFIG/NeTwOrK-ShArEs.JsOn.TmP", "config./NETWORK-SHARES.JSON.BAK  ", "CONFIG. /network-shares.json.staged.foo. ",
                              "config/network-shares.json.tmp/unknown", "config./NETWORK-SHARES.JSON.BAK. /.hidden",
                              "config/./network-shares.json.tmp", "config/x/../network-shares.json.bak",
                              "config\\network-shares.json.staged.foo", "/config/network-shares.json.tmp"}) {
        for (bool directory : {true, false}) {
            auto injected = nlohmann::json::parse(clean_manifest);
            injected["entries"].push_back({{"path", alias}, {"directory", directory},
                {"size", directory ? 0u : synthetic_credentials.size()}, {"sha256", directory ? "" : Digest(synthetic_credentials)}, {"had", false}});
            Write(private_manifest, injected.dump()); Write(fs::path(private_backup) / "COMPLETE", Digest(injected.dump()));
            const bool policy_alias = std::string(alias).find("..") == std::string::npos &&
                std::string(alias).find("/./") == std::string::npos && std::string(alias).find('\\') == std::string::npos && alias[0] != '/';
            const auto rejects = [&](auto operation) {
                bool rejected = false;
                try { operation(); } catch (const std::exception& e) {
                    rejected = true;
                    assert(std::string(e.what()).find(synthetic_secret) == std::string::npos);
                    assert(std::string(e.what()).find(synthetic_credentials) == std::string::npos);
                    if (policy_alias) assert(std::string(e.what()).find("Excluded or duplicate manifest path") != std::string::npos);
                }
                assert(rejected);
            };
            rejects([&] { B::InspectRestore(private_data, private_backup, stop); });
            rejects([&] { B::StageRestore(private_data, private_backup, stop); });
            assert(!fs::exists(root / ".nextendo-fixture-restores/pending.json"));
            assert(!forbidden_probes);
        }
    }
    privacy_probe = false;
    Write(private_manifest, clean_manifest); Write(fs::path(private_backup) / "COMPLETE", Digest(clean_manifest));
    std::cout << "Nextendo whole-tree exclusion/alias/restore-injection tests passed (synthetic secrets)\n";
    std::cout << "Network-share config/staged-sibling exclusion: actual copy/estimate/progress/hash, zero forbidden open/stat/read probes, hostile aliases and restore reinjection passed (synthetic credentials)\n";
    std::cout << "USB privacy control inventory: " << public_estimate.files << " harmless files / " << public_bytes << " bytes, unchanged after adding credential files/hostile subtrees\n";
    std::cout << "USB backup/restore synthetic filesystem tests passed\n";
}
