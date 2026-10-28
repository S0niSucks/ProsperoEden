// SPDX-License-Identifier: GPL-3.0-or-later
// Generated header contains ONLY the shared synthetic writers from cheat_identity_check.cpp.
// ProbeCheatIdentity, NPDM/NCA/PatchManager, manager, HTTP, parser and store are real linked code.
#include "cheat_identity_fixtures.h"
#include "cheat_manager.h"
#include "cheat_store.h"
#include "downloaded_cheats.h"
#include <curl/curl.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <thread>

#ifdef NDEBUG
#error This qualification must execute assertions
#endif

// Only redirect certificate trust to this run's ephemeral loopback CA. Never disable TLS checks.
extern "C" void console_curl_setup(CURL* easy) {
    assert(curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L) == CURLE_OK);
    assert(curl_easy_setopt(easy, CURLOPT_CAINFO, std::getenv("CHEAT_TEST_CA")) == CURLE_OK);
    assert(curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1L) == CURLE_OK);
    assert(curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 2L) == CURLE_OK);
}

namespace {
using namespace Eden;
using namespace std::chrono_literals;
namespace fs = std::filesystem;
fs::path root, controls;
std::string server;
std::mutex bridge;
const auto owner = std::this_thread::get_id();
std::atomic<unsigned> probes{};

void Until(const std::function<bool()>& ready) {
    const auto deadline = std::chrono::steady_clock::now() + 20s;
    while (!ready() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(2ms);
    assert(ready());
}
CheatManagerStatus Wait(CheatManager& manager, const std::string& phase) {
    Until([&] { return !manager.Status().busy; });
    const auto status = manager.Status();
    if (status.phase != phase) std::fprintf(stderr, "phase=%s wanted=%s error=%s\n",
        status.phase.c_str(), phase.c_str(), status.error.c_str());
    assert(status.phase == phase);
    return status;
}
std::string Text(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
unsigned Requests() {
    const auto text = Text(controls / "requests.log");
    return std::count(text.begin(), text.end(), '\n');
}
void Touch(const fs::path& file) { Write(file, {}); }
std::string Hex(const auto& bytes) {
    std::string out;
    for (auto b : bytes) { out += "0123456789ABCDEF"[b >> 4]; out += "0123456789ABCDEF"[b & 15]; }
    return out;
}
struct Selected {
    CheatProbePaths paths;
    CheatManagerRequest request;
    Selected(std::string name, std::string endpoint = "/ok") {
        const auto folder = root / name;
        paths = {(root / "keys").string(), (folder / "mods").string(),
            (folder / "cheats").string(), (folder / "updates").string(), {},
            (folder / "settings.json").string()};
        fs::create_directories(paths.mods); fs::create_directories(paths.updates);
        request = {name + "/game.nsp", (folder / "game.nsp").string(), {}, server + endpoint,
            (folder / "cache").string(), (fs::path(paths.mods) / "0100000000001000").string(), title};
    }
    CheatManager::Build Probe(const CheatManagerRequest& req) const {
        auto selected = paths;
        selected.game_folder = req.game_folder;
        const auto identity = ProbeCheatIdentity(req.path, req.program, selected);
        return identity.program == req.program ? identity.build : CheatManager::Build{};
    }
    CheatManager::Probe Worker() const {
        return [this](const auto& req) {
            assert(std::this_thread::get_id() != owner);
            ++probes;
            return Probe(req); // No constant, cached, or fabricated identity result.
        };
    }
    void Base(u8 id, bool encrypted = false, bool bktr = false) const {
        Write(request.path, Container(id, false, 0, encrypted, bktr));
    }
    void UpdateFile(u8 id, u32 version, bool encrypted = false, bool bktr = false) const {
        Write(fs::path(paths.updates) / "update.nsp", Container(id, true, version, encrypted, bktr));
    }
};

void InstalledOff(const Selected& selected, const CheatManagerStatus& ready) {
    const auto mods = Mods::List({selected.paths.mods, selected.paths.cheats, selected.request.game_folder}, title);
    const auto switches = Mods::Switches(mods, LoadDisabledMods(title, selected.paths.settings),
        LoadChosenCheats(title, selected.paths.settings), LoadCheatsOff(title, selected.paths.settings), true, true);
    unsigned downloaded{};
    for (const auto& mod : mods) {
        if (!IsDownloadedCheatMod(mod.name)) continue;
        ++downloaded;
        assert(Mods::Has(switches.mods_off, mod.name) && mod.cheats.size() == 1);
        assert(Mods::Has(switches.cheats_off, Cheats::Key(mod.name, mod.cheats[0].name)));
        // The real saved choice, not a UI mock, is the only way this managed download becomes on.
        const auto chosen = Mods::ChooseCheat(mod, mod.cheats[0].name, true, {});
        assert(SaveChosenCheats(title, chosen, selected.paths.settings));
        const auto enabled = Mods::Switches(mods, {}, LoadChosenCheats(title, selected.paths.settings), {}, true, true);
        assert(!Mods::Has(enabled.mods_off, mod.name));
        assert(SaveChosenCheats(title, {}, selected.paths.settings));
    }
    assert(downloaded != 0);
    bool found = false;
    for (const auto& entry : fs::recursive_directory_iterator(selected.request.title_mods_folder)) {
        if (entry.path().extension() == ".txt") {
            assert(Text(entry.path()) == "[Local optional]\n04000000 00000000 00000001\n\n");
            if (Mods::Lower(entry.path().stem().string()) == Mods::Lower(ready.build.substr(0, 16))) found = true;
        }
        if (entry.path().filename() == "provenance.json") assert(Text(entry.path()).find("\"default\":\"off\"") != std::string::npos);
        assert(!entry.path().filename().string().starts_with(".cheat-pending-"));
    }
    assert(found);
}
CheatManagerStatus Preview(CheatManager& manager, const Selected& selected, bool offline, u8 first) {
    assert(manager.Start(selected.request, offline));
    const auto ready = Wait(manager, "ready");
    const auto build = selected.Probe(selected.request);
    assert(build[0] == first && build[7] == 0x77 && build[31] == 0x99);
    assert(ready.build == Hex(build) && ready.entries == 1 && ready.cached && ready.offline == offline);
    assert(ready.names == std::vector<std::string>{"Local optional"});
    assert(offline ? ready.done == 0 : ready.done > 0);
    return ready;
}
void Success(Selected& selected, u8 effective) {
    CheatManager manager(bridge, selected.Worker());
    const auto count = Requests();
    const auto ready = Preview(manager, selected, false, effective);
    assert(Requests() == count + 1 && !fs::exists(selected.request.title_mods_folder));
    const auto settings_before = Text(selected.paths.settings);
    Preview(manager, selected, true, effective);
    assert(Requests() == count + 1 && !fs::exists(selected.request.title_mods_folder));
    assert(manager.Install(selected.request)); Wait(manager, "installed");
    assert(Text(selected.paths.settings) == settings_before); // Install never chooses/enables a cheat.
    InstalledOff(selected, ready);
    assert(!manager.Install(selected.request));
    manager.Stop();
}

void Flows() {
    Selected plain("plain"); plain.Base(5); plain.UpdateFile(6, 65536);
    Success(plain, 6);
    // The exact same encrypted/BKTR files used below must also merge real base/patch bytes.
    Selected encrypted("encrypted-bktr"); encrypted.Base(23, true, true);
    encrypted.UpdateFile(24, 10 * 65536, true, true);
    auto vfs = std::make_shared<FileSys::RealVfsFilesystem>();
    FileSys::NSP base_package(vfs->OpenFile(encrypted.request.path, FileSys::OpenMode::Read));
    FileSys::NSP patch_package(vfs->OpenFile((fs::path(encrypted.paths.updates) / "update.nsp").string(), FileSys::OpenMode::Read));
    const auto base = base_package.GetNCA(title, FileSys::ContentRecordType::Program);
    const auto patch = patch_package.GetNCA(title | 0x800, FileSys::ContentRecordType::Program, FileSys::TitleType::Update);
    assert(base && patch && patch->GetStatus() == Loader::ResultStatus::ErrorMissingBKTRBaseRomFS);
    // Reopen the actual patch file inside the real NSP, not another reconstructed fixture.
    const FileSys::NCA merged(patch->GetBaseFile(), base.get());
    assert(merged.GetStatus() == Loader::ResultStatus::Success && merged.GetRomFS());
    const auto bytes = merged.GetRomFS()->ReadAllBytes();
    assert(bytes.size() == 0x1000);
    assert(std::all_of(bytes.begin(), bytes.begin() + 0x800, [](u8 b) { return b == 0x31; }));
    assert(std::all_of(bytes.begin() + 0x800, bytes.end(), [](u8 b) { return b == 0x62; }));
    Success(encrypted, 24);

    Selected copy_a("copies"), copy_b = copy_a;
    copy_a.request.path = (root / "copies/Copy A/game.nsp").string();
    copy_b.request.path = (root / "copies/Copy B/game.nsp").string();
    copy_a.request.file = "copies/Copy A/game.nsp"; copy_b.request.file = "copies/Copy B/game.nsp";
    for (auto* copy : {&copy_a, &copy_b}) {
        copy->request.game_folder = fs::path(copy->request.path).parent_path().string();
        copy->Base(copy == &copy_a ? 11 : 12, true);
        const auto folder = fs::path(copy->request.game_folder);
        Write(folder / "update.nsp", Container(copy == &copy_a ? 13 : 14, true, 4 * 65536, true));
        Write(folder / "mods/Local/exefs/main", Nso(copy == &copy_a ? 15 : 16));
        Write(folder / "mods/Local/exefs/main.npdm", Npdm(title + 1));
        assert(copy->Probe(copy->request)[0] == (copy == &copy_a ? 15 : 16));
        assert(SaveModEnabled(title, "Local", false, copy->paths.settings));
        assert(copy->Probe(copy->request)[0] == (copy == &copy_a ? 13 : 14));
        assert(SaveModEnabled(title, "Local", true, copy->paths.settings));
    }
    Success(copy_a, 15);
    CheatManager b(bridge, copy_b.Worker());
    const auto count = Requests();
    assert(b.Start(copy_b.request, true)); Wait(b, "failed"); // A's cache cannot identify B.
    assert(Requests() == count && !b.Install(copy_b.request));
    // A full build mismatch with the same public eight-byte prefix is also refused offline.
    auto changed = Nso(15); changed[0x5f] ^= 1;
    Write(root / "copies/Copy A/mods/Local/exefs/main", changed);
    CheatManager a(bridge, copy_a.Worker());
    assert(copy_a.Probe(copy_a.request)[31] == 0x98);
    assert(a.Start(copy_a.request, true)); Wait(a, "failed");
    assert(Requests() == count && !a.Install(copy_a.request));
    Write(root / "copies/Copy A/mods/Local/exefs/main", Nso(15));
    Preview(a, copy_a, true, 15); assert(Requests() == count);
    auto other_source = copy_a.request; other_source.source += "?different-source";
    assert(a.Start(other_source, true)); Wait(a, "failed"); assert(Requests() == count);
    assert(!a.Install(other_source));
    a.Stop(); b.Stop();
    // B has the same shared title/mod/cache directories but a different real VFS identity.
    // Its preview is independently fetched, then installed default-OFF beside A's download.
    CheatManager second(bridge, copy_b.Worker());
    const auto ready_b = Preview(second, copy_b, false, 16);
    assert(second.Install(copy_b.request)); Wait(second, "installed"); InstalledOff(copy_b, ready_b);
    second.Stop();
    std::puts("cross-layer: plain/update, encrypted BKTR, native-VFS per-copy, offline exact binding and confirmed default-OFF Mods PASS");
}

void Refusals() {
    for (const auto& endpoint : {"/wrong-title", "/wrong-build"}) {
        Selected selected("refusal-" + std::string{endpoint + 1}, endpoint); selected.Base(5);
        CheatManager manager(bridge, selected.Worker());
        assert(manager.Start(selected.request, false)); Wait(manager, "no_match");
        assert(!manager.Install(selected.request) && !fs::exists(selected.request.cache_folder));
        assert(!fs::exists(selected.request.title_mods_folder)); manager.Stop();
    }
    Selected invalid("invalid"); invalid.Base(5);
    CheatManager manager(bridge, invalid.Worker());
    auto mismatch = invalid.request; mismatch.program = title + 1;
    const auto count = Requests();
    assert(manager.Start(mismatch, false)); Wait(manager, "failed");
    assert(Requests() == count && !manager.Install(mismatch));
    auto corrupt = Npdm(); Put(corrupt, 0xb4, 0xfffffffc);
    Write(invalid.request.path, Package({{"main", Nso(5)}, {"main.npdm", corrupt}}));
    assert(manager.Start(invalid.request, false)); Wait(manager, "failed");
    assert(Requests() == count && !manager.Install(invalid.request)); manager.Stop();

    for (const auto& mutation : {"update", "mod", "title", "full-build"}) {
        Selected selected(std::string{"change-"} + mutation, std::string{"/hold/"} + mutation);
        selected.Base(5, true); selected.UpdateFile(6, 65536, true);
        CheatManager job(bridge, selected.Worker());
        assert(job.Start(selected.request, false));
        Until([&] { return fs::exists(controls / (std::string{mutation} + ".entered")); });
        assert(job.Status().busy && job.Status().phase == "downloading");
        { // Mutate the real selected filesystem/settings while HTTP owns no bridge lock.
            const std::lock_guard lock(bridge);
            if (mutation == std::string{"update"}) selected.UpdateFile(7, 131072, true);
            else if (mutation == std::string{"mod"}) {
                Write(fs::path(selected.request.title_mods_folder) / "Replacement/exefs/main", Nso(8));
                Write(fs::path(selected.request.title_mods_folder) / "Replacement/exefs/main.npdm", Npdm());
                assert(SaveModEnabled(title, "Replacement", true, selected.paths.settings));
            } else if (mutation == std::string{"title"}) {
                Write(selected.request.path, Package({{"main", Nso(6)}, {"main.npdm", Npdm(title + 1)}}));
            } else {
                auto main = Nso(6); main[0x5f] ^= 1;
                Write(selected.request.path, Package({{"main", main}, {"main.npdm", Npdm()}}));
            }
        }
        Touch(controls / (std::string{mutation} + ".release"));
        const auto preview = Wait(job, "ready"); assert(preview.build.starts_with("0600000000000077"));
        const auto before_install = probes.load();
        assert(job.Install(selected.request)); const auto refused = Wait(job, "failed");
        assert(refused.error.find("effective build changed") != std::string::npos && probes == before_install + 1);
        if (fs::exists(selected.request.title_mods_folder))
            for (const auto& file : fs::recursive_directory_iterator(selected.request.title_mods_folder)) {
                assert(file.path().extension() != ".txt");
                assert(!file.path().filename().string().starts_with(".cheat-pending-"));
            }
        job.Stop();
    }
    std::puts("cross-layer: real title/build/NPDM mismatches and update/mod/title/full-build mutation during TLS -> install revalidation refusal PASS");
}

void Cancellation() {
    Selected selected("cancellation", "/hold/cancel"); selected.Base(5, true);
    CheatManager manager(bridge, selected.Worker());
    assert(manager.Start(selected.request, false));
    Until([&] { return fs::exists(controls / "cancel.entered"); });
    assert(manager.Status().busy && !manager.Start(selected.request, true) && !manager.Install(selected.request));
    manager.Cancel(); Wait(manager, "cancelled"); manager.Stop();
    Touch(controls / "cancel.release");
    assert(!manager.Status().busy && !fs::exists(selected.request.cache_folder));
    assert(!fs::exists(selected.request.title_mods_folder));
    // A real probe cannot enter while the launch/metadata bridge is owned elsewhere.
    bridge.lock(); const auto before = probes.load();
    assert(manager.Start(selected.request, true)); manager.Cancel();
    assert(manager.Status().busy && probes == before);
    bridge.unlock(); Wait(manager, "cancelled"); assert(probes == before);
    // Delay returning an ACTUAL identity (not a fixed-build stub). Stop must join this worker
    // before the launcher may cross its boot/metadata lifetime boundary.
    std::mutex gate; std::condition_variable cv; bool entered = false, release = false;
    CheatManager gated(bridge, [&](const auto& request) {
        const auto actual = selected.Worker()(request);
        assert(actual[0] == 5);
        std::unique_lock lock(gate); entered = true; cv.notify_all();
        cv.wait(lock, [&] { return release; }); return actual;
    });
    assert(gated.Start(selected.request, true));
    { std::unique_lock lock(gate); assert(cv.wait_for(lock, 20s, [&] { return entered; })); }
    gated.Cancel(); assert(gated.Status().busy);
    std::atomic<bool> joining{}, joined{};
    std::thread joiner([&] { joining = true; gated.Stop(); joined = true; });
    Until([&] { return joining.load(); });
    assert(!joined && gated.Status().busy);
    { const std::lock_guard lock(gate); release = true; } cv.notify_all();
    joiner.join(); assert(joined && !gated.Status().busy && gated.Status().phase == "cancelled");
    assert(!gated.Install(selected.request));
    std::puts("cross-layer: real TLS cancellation, queued real probe, busy launch barrier and Stop/join lifetime boundary PASS");
}
} // namespace

int main(int argc, char** argv) {
    assert(argc == 4); server = argv[1]; root = argv[2]; controls = argv[3];
    assert(server.starts_with("https://localhost:") && !fs::exists(root));
    using Common::FS::EdenPath;
    for (const auto [key, name] : {std::pair{EdenPath::NANDDir, "nand"}, {EdenPath::SDMCDir, "sdmc"},
            {EdenPath::LoadDir, "initial-mods"}, {EdenPath::DumpDir, "dump"}, {EdenPath::KeysDir, "keys"},
            {EdenPath::ConfigDir, "config"}, {EdenPath::LogDir, "log"}}) {
        fs::create_directories(root / name); Common::FS::SetEdenPath(key, root / name);
    }
    Common::Log::Initialize(); Common::Log::Start();
    Core::Crypto::Key128 area{}; area.fill(0x42);
    auto& keys = Core::Crypto::KeyManager::Instance();
    keys.SetKey(Core::Crypto::S128KeyType::KeyArea, area);
    keys.SetKey(Core::Crypto::S256KeyType::Header, HeaderKey());
    assert(keys.GetKey(Core::Crypto::S128KeyType::KeyArea) == area);
    assert(keys.GetKey(Core::Crypto::S256KeyType::Header) == HeaderKey());
    Flows(); Refusals(); Cancellation();
    assert(fs::is_empty(root / "dump"));
    std::printf("Cheat cross-layer qualification PASS: %u actual probes; no guest boot, dumps or external network\n", probes.load());
}
