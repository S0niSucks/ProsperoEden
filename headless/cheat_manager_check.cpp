// SPDX-License-Identifier: GPL-3.0-or-later
#include "cheat_manager.h"
#include "cheat_download.h"
#include "mods.h"
#include "remote/http.h"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

namespace {
using namespace Eden;
using namespace std::chrono_literals;
const std::string body = "{\"0100000000001234\":{\"ab00000000000000\":{\"" + std::string(64, 'a') +
    "\":{\"title\":\"[Local test]\",\"source\":\"04000000 00000000 00000001\"}}}}";
enum Mode { Valid, NoMatch, Others, Fail, Hold };
std::atomic<Mode> mode{Valid};
std::atomic<unsigned> requests{}, in_http{}, probes{};
CheatManagerStatus Wait(CheatManager& manager) {
    const auto until = std::chrono::steady_clock::now() + 5s;
    while (manager.Status().busy && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(1ms);
    assert(!manager.Status().busy); return manager.Status();
}
void Until(const std::function<bool()>& ready) {
    const auto until = std::chrono::steady_clock::now() + 5s;
    while (!ready() && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(1ms);
    assert(ready());
}
}
extern "C" int remote_http_get(const remote_http_request* request, remote_http_result* response) {
    ++requests; ++in_http;
    struct Leave { ~Leave() { --in_http; } } leave;
    response->status = 200;
    assert(request->https_only);
    if (mode == Fail) return -1;
    const auto text = mode == NoMatch ? std::string{"{}"} : mode == Others ?
        std::string{"{\"0100000000001234\":{\"cd00000000000000\":{\"version\":65536}}}"} : body;
    if (!request->begin(request->user, 200, text.size())) return -1;
    if (mode == Hold) {
        if (!request->sink(request->user, text.data(), 1)) return -1;
        while (!request->stop(request->user)) std::this_thread::sleep_for(1ms);
        return -1;
    }
    return request->sink(request->user, text.data(), text.size()) ? 0 : -1;
}
int main() {
    namespace fs = std::filesystem;
    char temporary[] = "/tmp/eden-cheat-manager-XXXXXX"; assert(mkdtemp(temporary));
    const fs::path root = temporary;
    std::mutex bridge;
    CheatManager::Build build{}; build[0] = 0xab; build[31] = 7;
    bool unknown = false, throwing = false;
    CheatManager manager(bridge, [&](const auto& request) {
        assert(request.program == 0x0100000000001234); ++probes;
        if (throwing) throw std::runtime_error("test probe failure");
        return unknown ? CheatManager::Build{} : build;
    });
    const CheatManagerRequest request{"local.nsp", (root / "local.nsp").string(), {}, std::string{kCheatCatalogSource},
        (root / "cache").string(), (root / "mods/0100000000001234").string(), 0x0100000000001234};
    assert(manager.Status().phase == "idle" && requests == 0 && probes == 0);
    assert(!manager.Install(request));
    assert(manager.Start(request, false));
    auto ready = Wait(manager);
    assert(ready.phase == "ready" && ready.entries == 1 && ready.cached && !ready.offline && ready.done == body.size());
    assert(ready.names == std::vector<std::string>{"Local test"});
    assert(ready.build.size() == 64 && ready.file == "local.nsp" && !fs::exists(root / "mods"));
    assert(manager.Install(request)); assert(Wait(manager).phase == "installed");
    assert(probes == 2 && requests == 1);
    auto mods = Mods::List((root / "mods").string(), request.program);
    assert(mods.size() == 1 && Mods::Has(Mods::Switches(mods, {}, {}, {}, true, true).mods_off, mods[0].name));
    assert(!manager.Install(request)); // no duplicate action once done
    const auto online_requests = requests.load();
    assert(manager.Start(request, true)); assert(Wait(manager).phase == "ready");
    assert(manager.Status().offline && requests == online_requests); // explicit cache has zero network
    assert(manager.Install(request)); assert(Wait(manager).phase == "installed"); // idempotent disk install
    assert(manager.Start(request, true)); assert(Wait(manager).phase == "ready");
    ++build[31]; // full build mismatch, even though the public catalog prefix is unchanged
    assert(manager.Install(request)); assert(Wait(manager).phase == "failed");
    assert(Mods::List((root / "mods").string(), request.program).size() == 1); --build[31];
    assert(manager.Start(request, true)); assert(Wait(manager).phase == "ready");
    auto changed = request; changed.source += "?changed";
    assert(!manager.Install(changed) && manager.Status().phase == "failed");
    mode = NoMatch; assert(manager.Start(request, false)); assert(Wait(manager).phase == "no_match");
    assert(!manager.Install(request)); // never reuse the previous ready catalog
    mode = Others; assert(manager.Start(request, false));
    const auto other_builds = Wait(manager);
    assert(other_builds.phase == "no_match" && other_builds.entries == 0 && other_builds.names.empty());
    assert(other_builds.other_builds == std::vector<std::string>{"cd00000000000000 / v65536"} && !manager.Install(request));
    mode = Fail; assert(manager.Start(request, false)); assert(Wait(manager).phase == "failed");
    assert(!manager.Install(request));
    assert(manager.Start(request, true)); assert(Wait(manager).phase == "ready"); // network failure preserved cache
    unknown = true; const auto before = requests.load();
    assert(manager.Start(request, false)); assert(Wait(manager).phase == "failed" && requests == before);
    unknown = false; throwing = true;
    assert(manager.Start(request, true)); assert(Wait(manager).phase == "failed"); throwing = false;
    auto invalid = request; invalid.source = "http://example.test/catalog";
    assert(manager.Start(invalid, false)); assert(Wait(manager).phase == "failed" && requests == before);
    mode = Hold; assert(manager.Start(request, false)); Until([] { return in_http != 0; });
    assert(manager.Status().busy && !manager.Start(request, true) && !manager.Install(request));
    manager.Cancel(); assert(Wait(manager).phase == "cancelled" && !manager.Install(request));
    // A queued probe cannot enter the core until another metadata caller releases the bridge.
    bridge.lock(); const auto old_probes = probes.load();
    assert(manager.Start(request, true)); assert(manager.Status().busy && probes == old_probes);
    manager.Cancel(); bridge.unlock(); assert(Wait(manager).phase == "cancelled" && probes == old_probes);
    // Cancellation during an uninterruptible probe retains the launch barrier until it returns.
    std::mutex gate; std::condition_variable changed_gate; bool entered = false, release = false;
    CheatManager gated(bridge, [&](const auto&) {
        std::unique_lock lock(gate); entered = true; changed_gate.notify_all();
        changed_gate.wait(lock, [&] { return release; }); return build;
    });
    assert(gated.Start(request, true));
    { std::unique_lock lock(gate); changed_gate.wait(lock, [&] { return entered; }); }
    gated.Cancel(); assert(gated.Status().busy);
    { const std::lock_guard lock(gate); release = true; } changed_gate.notify_all();
    assert(Wait(gated).phase == "cancelled");
    // Destructor cancels/joins a live transfer. Snapshot readers never access mutable catalog state.
    std::atomic<bool> reading{true};
    std::thread reader([&] { while (reading) { const auto status = manager.Status(); assert(status.file.size() < 4096); } });
    for (unsigned n = 0; n < 100; ++n) { assert(manager.Start(request, false)); manager.Cancel(); Wait(manager); }
    reading = false; reader.join();
    {
        CheatManager destroying(bridge, [&](const auto&) { return build; });
        assert(destroying.Start(request, false)); Until([] { return in_http != 0; });
    }
    assert(in_http == 0);
    manager.Stop(); gated.Stop(); fs::remove_all(root);
    std::puts("Cheat manager PASS: explicit network/cache/install, progress, exact full-build and source binding, no stale reuse, cache preservation, serialized probes, cancel/launch barrier, destructor join and concurrent snapshots");
}
