// SPDX-License-Identifier: GPL-3.0-or-later
// NOT_RUN. Actual candidate header + frozen app dependencies, no settings/parser mocks.
#include "settings_store.h"

#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <sys/resource.h>

namespace {
namespace fs = std::filesystem;
using Json = Eden::Settings::Json;
constexpr std::uint64_t kGame = 0x0100ABCD00001000ULL;
constexpr const char* kKey = "0100ABCD00001000";
constexpr const char* kMod = "Focus";
constexpr const char* kSentinel = "SYNTHETIC SENTINEL: DO NOT REPLACE\n";

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void write(const fs::path& path, const std::string& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    require(!output.fail(), "fixture setup write failed");
}

std::string read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(input.is_open(), "fixture read failed");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

Json seed() {
    return Json{
        {"version", 1},
        {"unrelated", {{"nested", Json::array({"future", 42, nullptr})}}},
        {"video", {{"renderer", "opengl"}, {"future_video", 37}}},
        {"games", {
            {kKey, {{"mods", false}, {"mods_off", Json::array({"Disabled", kMod, "Disabled"})},
                    {"renderer", "opengl"}, {"future_game", Json::array({1, 2})}}},
            {"0100FFFF00002000", {{"mods", false}, {"mods_off", Json::array({"Other"})}}}
        }}
    };
}

bool save(const std::string& writer, bool enabled, const fs::path& path) {
    if (writer == "mod") return Eden::SaveModEnabled(kGame, kMod, enabled, path.string());
    return Eden::SaveModsEnabled(kGame, enabled, path.string());
}

Json expected(Json document, const std::string& writer, bool enabled) {
    document["version"] = 1;
    auto& game = document["games"][kKey];
    if (writer == "mod") {
        game["mods_off"] = enabled ? Json::array({"Disabled"}) : Json::array({"Disabled", kMod});
    } else if (enabled) {
        game.erase("mods"); // retain the existing default-on schema, not a new global policy
    } else {
        game["mods"] = false;
    }
    return document;
}

struct Identity {
    dev_t device;
    ino_t inode;
    nlink_t links;
    mode_t mode;
};

Identity identity(const fs::path& path) {
    struct stat info{};
    require(::lstat(path.c_str(), &info) == 0, "fixture lstat failed");
    return {info.st_dev, info.st_ino, info.st_nlink, info.st_mode};
}

void unchanged_node(const fs::path& path, Identity before) {
    const auto after = identity(path);
    require(after.device == before.device && after.inode == before.inode &&
            after.links == before.links && after.mode == before.mode,
            "non-owned fixture node changed or was removed");
}

fs::path staging(const fs::path& file, unsigned sequence) {
    return file.string() + ".tmp." + std::to_string(::getpid()) + "." + std::to_string(sequence);
}

void alias(const fs::path& sentinel, const fs::path& link, bool hard) {
    if (hard) fs::create_hard_link(sentinel, link);
    else fs::create_symlink(sentinel, link);
}

void no_owned_staging(const fs::path& root, const fs::path& file) {
    for (const auto& entry : fs::directory_iterator(root))
        require(!entry.path().filename().string().starts_with(file.filename().string() + ".tmp."),
                "owned staging leaked");
}

void exercise(const std::string& writer, const std::string& test, bool enabled, const fs::path& root) {
    const fs::path file = root / "settings.json";
    const fs::path sentinel = root / "sentinel.txt";
    const auto initial = seed();
    const std::string initial_bytes = initial.dump(2) + "\n";
    write(file, initial_bytes);

    if (test == "corrupt-open" || test == "corrupt-array") {
        const std::string bytes = test == "corrupt-open" ? "{" : "[]";
        write(file, bytes);
        require(!save(writer, enabled, file), "corrupt/non-object settings were accepted");
        require(read(file) == bytes, "refused settings bytes changed");
        require(!fs::exists(file.string() + ".tmp"), "legacy staging was created");
        no_owned_staging(root, file);
    } else if (test == "preserve") {
        require(save(writer, enabled, file), "normal save failed");
        require(Json::parse(read(file)) == expected(initial, writer, enabled),
                "save changed unrelated fields, master policy or selection semantics");
        no_owned_staging(root, file);
    } else if (test == "legacy-symlink" || test == "legacy-hardlink") {
        write(sentinel, kSentinel);
        const fs::path link = file.string() + ".tmp";
        alias(sentinel, link, test == "legacy-hardlink");
        const auto sentinel_before = identity(sentinel);
        const auto link_before = identity(link);
        // Existing legacy .tmp is not this writer's staging: ignore, never follow/remove.
        require(save(writer, enabled, file), "legacy non-owned alias prevented independent owned save");
        require(Json::parse(read(file)) == expected(initial, writer, enabled), "saved selection mismatch");
        require(read(sentinel) == kSentinel, "legacy alias truncated sentinel");
        unchanged_node(sentinel, sentinel_before);
        unchanged_node(link, link_before);
        no_owned_staging(root, file);
    } else if (test == "owned-symlink" || test == "owned-hardlink") {
        write(sentinel, kSentinel);
        std::vector<fs::path> links;
        for (unsigned sequence = 0; sequence < 8; ++sequence) {
            links.push_back(staging(file, sequence));
            alias(sentinel, links.back(), test == "owned-hardlink");
        }
        std::vector<Identity> identities;
        for (const auto& link : links) identities.push_back(identity(link));
        const auto sentinel_before = identity(sentinel);
        require(!save(writer, enabled, file), "occupied owned staging did not refuse");
        require(read(file) == initial_bytes && read(sentinel) == kSentinel,
                "occupied staging changed target or sentinel");
        unchanged_node(sentinel, sentinel_before);
        for (std::size_t i = 0; i < links.size(); ++i) unchanged_node(links[i], identities[i]);
    } else if (test == "write-failure") {
        struct rlimit previous{};
        require(::getrlimit(RLIMIT_FSIZE, &previous) == 0, "getrlimit failed");
        const auto old_signal = std::signal(SIGXFSZ, SIG_IGN);
        require(old_signal != SIG_ERR, "SIGXFSZ setup failed");
        const struct rlimit limit{0, previous.rlim_max};
        require(::setrlimit(RLIMIT_FSIZE, &limit) == 0, "setrlimit failed");
        const bool saved = save(writer, enabled, file); // real ::write must fail, not a settings mock
        require(::setrlimit(RLIMIT_FSIZE, &previous) == 0, "restore rlimit failed");
        require(std::signal(SIGXFSZ, old_signal) != SIG_ERR, "restore SIGXFSZ failed");
        require(!saved && read(file) == initial_bytes, "write failure published or changed target");
        no_owned_staging(root, file);
    } else if (test == "missing") {
        fs::remove(file);
        require(save(writer, enabled, file), "missing settings could not be initialized");
        Json wanted{{"version", 1}, {"games", {{kKey, Json::object()}}}};
        if (writer == "mod" && !enabled) wanted["games"][kKey]["mods_off"] = Json::array({kMod});
        if (writer == "all" && !enabled) wanted["games"][kKey]["mods"] = false;
        require(Json::parse(read(file)) == wanted, "missing-document schema/defaults changed");
        no_owned_staging(root, file);
    } else if (test == "invalid-arguments") {
        require(!Eden::SaveModEnabled(0, kMod, enabled, file.string()), "zero mod title accepted");
        require(!Eden::SaveModsEnabled(0, enabled, file.string()), "zero master title accepted");
        require(!Eden::SaveModEnabled(kGame, "", enabled, file.string()), "empty mod name accepted");
        require(!Eden::SaveModEnabled(kGame, std::string(256, 'x'), enabled, file.string()), "long mod name accepted");
        require(read(file) == initial_bytes, "invalid arguments changed settings");
        no_owned_staging(root, file);
    } else if (test == "known-gap-A14-05") {
        // Same two-save order as the unchanged UI, NOT a UI/provider execution claim.
        require(Eden::SaveModsEnabled(kGame, true, file.string()), "first UI-order save failed");
        write(sentinel, kSentinel);
        for (unsigned sequence = 1; sequence <= 8; ++sequence) alias(sentinel, staging(file, sequence), true);
        require(!Eden::SaveModEnabled(kGame, kMod, true, file.string()), "second UI-order save unexpectedly succeeded");
        require(Json::parse(read(file)) == expected(initial, "all", true), "known partial-persistence gap changed");
        require(Eden::LoadModsEnabled(kGame, file.string()), "known global re-enable gap was not observed");
        require(read(sentinel) == kSentinel, "known-gap staging touched sentinel");
        std::cout << "KNOWN_GAP_A14_05: first publication remains after second fails\n";
    } else {
        throw std::runtime_error("unknown fixture case");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 5, "usage: fixture mod|all CASE 0|1 EXISTING_EMPTY_SCRATCH");
        const std::string writer = argv[1];
        require(writer == "mod" || writer == "all", "unknown writer");
        require(std::string(argv[3]) == "0" || std::string(argv[3]) == "1", "invalid enabled argument");
        const fs::path root = argv[4];
        require(fs::is_directory(root) && fs::is_empty(root), "scratch must be an existing empty directory");
        exercise(writer, argv[2], std::string(argv[3]) == "1", root);
        std::cout << "CASE_OBSERVED " << writer << " " << argv[2] << " " << argv[3] << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
