// SPDX-License-Identifier: GPL-3.0-or-later
#include "cheat_store.h"
#include "cheat_download.h"
#include "downloaded_cheats.h"
#include <openssl/evp.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <optional>
#include <span>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace Eden {
namespace {
using Json = nlohmann::json;
constexpr size_t kRecordLimit = 2 * 1024 * 1024;
class Fd {
public:
    explicit Fd(int value = -1) : value(value) {}
    ~Fd() { if (value >= 0) close(value); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : value(std::exchange(other.value, -1)) {}
    Fd& operator=(Fd&& other) noexcept {
        if (value >= 0) close(value);
        value = std::exchange(other.value, -1); return *this;
    }
    int Get() const { return value; }
    explicit operator bool() const { return value >= 0; }
private:
    int value;
};
Fd DirectoryAt(int parent, const std::string& name, bool create) {
    if (create && mkdirat(parent, name.c_str(), 0700) != 0 && errno != EEXIST) return Fd{};
    return Fd{openat(parent, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)};
}
Fd Directory(const std::string& path, bool create) {
    if (path.empty() || path.size() > 4095 || path.find('\0') != path.npos) return Fd{};
    Fd fd{open(path.front() == '/' ? "/" : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC)};
    std::string_view remaining{path}; unsigned depth = 0;
    while (fd && !remaining.empty()) {
        if (remaining.front() == '/') { remaining.remove_prefix(1); continue; }
        const auto end = remaining.find('/');
        const auto part = remaining.substr(0, end);
        if (part == "." || part == ".." || part.size() > 255 || ++depth > 64) return Fd{};
        fd = DirectoryAt(fd.Get(), std::string{part}, create);
        if (end == remaining.npos) break;
        remaining.remove_prefix(end + 1);
    }
    return fd;
}
std::string Hex(std::span<const uint8_t> data) {
    std::string hex;
    for (auto byte : data) { hex += "0123456789abcdef"[byte >> 4]; hex += "0123456789abcdef"[byte & 15]; }
    return hex;
}
std::string Program(uint64_t value) {
    std::array<uint8_t, 8> bytes{};
    for (auto& byte : bytes) { byte = static_cast<uint8_t>(value >> 56); value <<= 8; }
    return Hex(bytes);
}
std::string Hash(std::string_view text) {
    std::array<uint8_t, EVP_MAX_MD_SIZE> bytes{}; unsigned size{};
    if (EVP_Digest(text.data(), text.size(), bytes.data(), &size, EVP_sha256(), nullptr) != 1 || size != 32) return {};
    return Hex(std::span{bytes}.first(size));
}
std::string Key(const std::string& source, uint64_t program, const std::array<uint8_t, 32>& build) {
    if (!CheatSourceAllowed(source) || !program ||
        std::all_of(build.begin(), build.begin() + 8, [](auto b) { return b == 0; })) return {};
    return Hash(source + '\0' + Program(program) + Hex(build));
}
Json CatalogJson(uint64_t program, const std::array<uint8_t, 32>& build, const CheatCatalog& catalog) {
    if (catalog.status != CheatCatalogStatus::Ready || catalog.entries.empty() || catalog.entries.size() > 512) return {};
    Json entries = Json::object();
    unsigned index{};
    for (const auto& entry : catalog.entries) {
        if (entry.name.size() > 63 || entry.code.size() > 4096) return {};
        std::array<uint8_t, 32> id{}; id[30] = index >> 8; id[31] = index; ++index;
        entries[Hex(id)] = {{"title", (entry.master ? "{" : "[") + entry.name + (entry.master ? "}" : "]")},
                            {"source", entry.code}};
    }
    Json data = {{Program(program), {{Hex(std::span{build}.first(8)), std::move(entries)}}}};
    // Do not trust a caller-constructed CatalogCheat to bypass the bounded parser contract.
    if (ReadCheatCatalog(data.dump(), program, build).status != CheatCatalogStatus::Ready) return {};
    return data;
}
std::optional<std::string> Read(int directory, const std::string& name, size_t limit) {
    Fd fd{openat(directory, name.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)};
    struct stat info{};
    if (!fd || fstat(fd.Get(), &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 ||
        static_cast<uint64_t>(info.st_size) > limit) return {};
    std::string text(static_cast<size_t>(info.st_size), '\0'); size_t done = 0;
    while (done < text.size()) {
        const auto n = pread(fd.Get(), text.data() + done, text.size() - done, done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return {};
        done += static_cast<size_t>(n);
    }
    char extra;
    ssize_t n;
    do { n = pread(fd.Get(), &extra, 1, done); } while (n < 0 && errno == EINTR);
    if (n != 0) return {};
    return text;
}
Json Envelope(const std::string& source, uint64_t program, const std::array<uint8_t, 32>& build) {
    return {{"schema", 1}, {"source", source}, {"program", Program(program)}, {"build", Hex(build)}};
}
bool Matches(const Json& record, const Json& identity) {
    if (!record.is_object()) return false;
    for (const auto& [key, value] : identity.items())
        if (!record.contains(key) || record.at(key) != value) return false;
    return true;
}
Json ParseRecord(std::string_view text) {
    std::array<std::set<std::string>, 18> keys;
    size_t events{};
    // Disk cache is untrusted too. Stop before building an arbitrarily deep DOM; do not
    // merely discard a deep subtree and keep parsing it. Duplicate metadata is ambiguous.
    return Json::parse(text, [&](int depth, Json::parse_event_t event, Json& value) {
        if (depth < 0 || depth > 16 || ++events > 50000)
            throw std::runtime_error("Cache structure limit");
        if (event == Json::parse_event_t::object_start) keys.at(depth + 1).clear();
        if (event == Json::parse_event_t::key && !keys.at(depth).insert(value.get<std::string>()).second)
            throw std::runtime_error("Duplicate cache field");
        if (value.is_string() && value.get_ref<const std::string&>().size() > 8192)
            throw std::runtime_error("Cache string limit");
        return true;
    }, false);
}
CheatCatalog DecodeRecord(const Json& record, const Json& identity, uint64_t program,
    const std::array<uint8_t, 32>& build) {
    if (!Matches(record, identity) || !record.contains("catalog") || !record.contains("text_sha256")) return {};
    auto result = ReadCheatCatalog(record.at("catalog").dump(), program, build);
    if (result.status != CheatCatalogStatus::Ready) return {};
    const auto checksum = Hash(result.Text());
    if (checksum.empty() || record.at("text_sha256") != checksum) return {};
    return result;
}
class Temporary {
public:
    Temporary(int directory, std::string_view text) : directory(directory) {
        static std::atomic<unsigned> sequence{};
        for (unsigned tries = 0; tries < 32; ++tries) {
            name = ".cheat-pending-" + std::to_string(getpid()) + "-" + std::to_string(sequence++);
            fd = Fd{openat(directory, name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600)};
            if (fd || errno != EEXIST) break;
        }
        if (!fd) { name.clear(); return; }
        size_t done{};
        while (done < text.size()) {
            const auto n = write(fd.Get(), text.data() + done, text.size() - done);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) return;
            done += static_cast<size_t>(n);
        }
        ready = fsync(fd.Get()) == 0;
    }
    ~Temporary() { if (!name.empty()) unlinkat(directory, name.c_str(), 0); }
    Temporary(const Temporary&) = delete;
    Temporary& operator=(const Temporary&) = delete;
    bool Publish(const std::string& destination, bool replace) {
        if (!ready) return false;
        if (replace) {
            if (renameat(directory, name.c_str(), directory, destination.c_str()) != 0) return false;
            name.clear(); return true;
        }
        return linkat(directory, name.c_str(), directory, destination.c_str(), 0) == 0;
    }
private:
    int directory;
    Fd fd;
    std::string name;
    bool ready{};
};
bool Immutable(int directory, const std::string& name, const std::string& text) {
    if (const auto existing = Read(directory, name, kRecordLimit)) return *existing == text;
    Temporary temp(directory, text);
    if (temp.Publish(name, false)) return true;
    const auto existing = Read(directory, name, kRecordLimit);
    return existing && *existing == text;
}
} // namespace

bool SaveCheatCache(const std::string& folder, const std::string& source, uint64_t program,
    const std::array<uint8_t, 32>& build, const CheatCatalog& catalog) try {
    const auto key = Key(source, program, build);
    const auto data = CatalogJson(program, build, catalog);
    if (key.empty() || data.is_null()) return false;
    const auto identity = Envelope(source, program, build);
    auto record = identity;
    record["catalog"] = data;
    const auto checksum = Hash(ReadCheatCatalog(data.dump(), program, build).Text());
    if (checksum.empty()) return false;
    record["text_sha256"] = checksum;
    const auto text = record.dump();
    if (text.size() > kRecordLimit) return false;
    const auto dir = Directory(folder, true);
    if (!dir) return false;
    const auto name = key + ".json";
    // Only replace a valid record of this same cache key, never an unrelated existing file.
    Fd old{openat(dir.Get(), name.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)};
    if (old) {
        const auto previous = Read(dir.Get(), name, kRecordLimit);
        if (!previous || DecodeRecord(ParseRecord(*previous), identity, program, build).status != CheatCatalogStatus::Ready) return false;
    } else if (errno != ENOENT) return false;
    Temporary temp(dir.Get(), text);
    return temp.Publish(name, true);
} catch (const std::exception&) { return false; }

CheatCatalog LoadCheatCache(const std::string& folder, const std::string& source, uint64_t program,
    const std::array<uint8_t, 32>& build) try {
    const auto key = Key(source, program, build);
    if (key.empty()) return {};
    const auto dir = Directory(folder, false);
    if (!dir) return {};
    const auto text = Read(dir.Get(), key + ".json", kRecordLimit);
    if (!text) return {};
    return DecodeRecord(ParseRecord(*text), Envelope(source, program, build), program, build);
} catch (const std::exception&) { return {}; }

CheatInstallation InstallCheatCatalog(const std::string& folder, const std::string& source,
    uint64_t program, const std::array<uint8_t, 32>& build, const CheatCatalog& catalog,
    const std::atomic<bool>& stop, const std::function<bool()>& identity_still_current) try {
    const auto failure = [](CheatInstallStatus status, const char* message) {
        return CheatInstallation{status, {}, message};
    };
    if (stop.load()) return failure(CheatInstallStatus::Cancelled, "Cancelled");
    const auto key = Key(source, program, build);
    const auto data = CatalogJson(program, build, catalog);
    if (key.empty() || data.is_null() || !identity_still_current ||
        std::none_of(catalog.entries.begin(), catalog.entries.end(), [](const auto& e) { return !e.master; }))
        return failure(CheatInstallStatus::Failed, "No compatible optional cheats to install");
    const auto normalized = ReadCheatCatalog(data.dump(), program, build).Text();
    const auto digest = Hash(normalized);
    const auto mod_key = Hash(key + normalized);
    if (digest.empty() || mod_key.empty()) return failure(CheatInstallStatus::Failed, "Cannot hash the selected cheats");
    const auto name = std::string{kDownloadedCheatPrefix} + Hex(std::span{build}.first(8)) + " - " + mod_key;
    const auto file_name = Hex(std::span{build}.first(8)) + ".txt";
    auto provenance = Envelope(source, program, build);
    provenance["text_sha256"] = digest;
    provenance["default"] = "off";
    const auto manifest = provenance.dump();
    const auto title = Directory(folder, true);
    if (!title) return failure(CheatInstallStatus::Failed, "The mods folder cannot be opened safely");
    const auto mod = DirectoryAt(title.Get(), name, true);
    if (!mod) return failure(CheatInstallStatus::Conflict, "A different file or link occupies the managed mod name");
    if (!Immutable(mod.Get(), "provenance.json", manifest))
        return failure(CheatInstallStatus::Conflict, "The managed mod's provenance differs or cannot be saved");
    const auto cheats = DirectoryAt(mod.Get(), "cheats", true);
    if (!cheats) return failure(CheatInstallStatus::Conflict, "The managed cheats folder cannot be opened safely");
    Temporary temp(cheats.Get(), normalized);
    if (stop.load()) return failure(CheatInstallStatus::Cancelled, "Cancelled");
    // Recheck even on idempotent reinstalls; a cache/preview for a previous build is not current.
    if (!identity_still_current()) return failure(CheatInstallStatus::StaleIdentity, "The game's effective build changed; check again");
    if (stop.load()) return failure(CheatInstallStatus::Cancelled, "Cancelled");
    if (temp.Publish(file_name, false)) return {CheatInstallStatus::Installed, name, {}};
    if (const auto existing = Read(cheats.Get(), file_name, 1024 * 1024); existing && *existing == normalized)
        return {CheatInstallStatus::AlreadyPresent, name, {}};
    return failure(CheatInstallStatus::Conflict, "An existing cheat file was left untouched, or publication failed");
} catch (const std::exception&) {
    return {CheatInstallStatus::Failed, {}, "The selected cheats could not be installed"};
}
} // namespace Eden
