// SPDX-License-Identifier: GPL-3.0-or-later
#include "cheat_download.h"
#include "remote/http.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <chrono>

namespace {
constexpr uint64_t program = 0x0100000000001234;
std::array<uint8_t, 32> Build() { std::array<uint8_t, 32> id{}; id[0] = 0xab; return id; }
const std::string body = "{\"0100000000001234\":{\"ab00000000000000\":{\"" + std::string(64, 'a') +
    "\":{\"title\":\"[Local test]\",\"source\":\"04000000 00000000 00000001\"}}}}";
#ifndef CHEAT_HTTP_REAL
std::string content;
int status = 200, transport = 0, calls = 0;
uint64_t length = 0;
bool no_header = false;
std::atomic<bool> stop{false};
#endif
}

#ifndef CHEAT_HTTP_REAL
extern "C" int remote_http_get(const remote_http_request* request, remote_http_result* response) {
    ++calls;
    assert(request->timeout_ms == 60000 && request->raw && request->https_only);
    assert(!request->resume_from && !request->authorization && !request->body && !request->method);
    response->status = status;
    if (!no_header && !request->begin(request->user, status, length)) return -1;
    for (size_t n = 0; n < content.size(); n += 113) {
        if (request->stop(request->user)) return -1;
        if (!request->sink(request->user, content.data() + n, std::min<size_t>(113, content.size() - n))) return -1;
    }
    return transport;
}
int main() {
    using namespace Eden;
    const auto get = [&](std::string url = std::string{kCheatCatalogSource}) {
        return DownloadCheatCatalog(url, program, Build(), stop);
    };
    content = body; length = body.size();
    assert(get().status == CheatDownloadStatus::Ready);
    length = 0; assert(get().status == CheatDownloadStatus::Ready); // chunked/unknown size
    assert(DownloadCheatCatalog(std::string{kCheatCatalogSource}, program + 1, Build(), stop).status == CheatDownloadStatus::NoMatch);
    for (const auto& url : {std::string{"http://example.test/catalog"}, std::string{"file:///data/code"},
            std::string{"https://user:pass@example.test/code"}, std::string{"https://example.test\\path"},
            std::string{"https://example.test/#fragment"}, std::string{"https://example.test/\0hidden", 28},
            std::string{"https://"}, std::string(2049, 'a')}) {
        const auto before = calls; assert(get(url).status == CheatDownloadStatus::Failed && calls == before);
    }
    assert(DownloadCheatCatalog(std::string{kCheatCatalogSource}, 0, Build(), stop).status == CheatDownloadStatus::Failed);
    assert(DownloadCheatCatalog(std::string{kCheatCatalogSource}, program, {}, stop).status == CheatDownloadStatus::Failed);
    for (int code : {206, 301, 404, 500}) { status = code; assert(get().status == CheatDownloadStatus::Failed); }
    status = 200; length = body.size() + 1; assert(get().status == CheatDownloadStatus::Failed);
    length = 16 * 1024 * 1024 + 1; assert(get().status == CheatDownloadStatus::Failed);
    length = 0; content = std::string(16 * 1024 * 1024 + 1, ' '); assert(get().status == CheatDownloadStatus::Failed);
    content = "{"; assert(get().status == CheatDownloadStatus::Failed);
    content.clear(); assert(get().status == CheatDownloadStatus::Failed);
    content = body; transport = -1; assert(get().status == CheatDownloadStatus::Failed); transport = 0;
    no_header = true; assert(get().status == CheatDownloadStatus::Failed); no_header = false;
    stop = true; const auto before = calls; assert(get().status == CheatDownloadStatus::Cancelled && calls == before);
    stop = false;
    assert(DownloadCheatCatalog(std::string{kCheatCatalogSource}, program, Build(), stop,
        [&](auto done, auto total) { assert(done > 0 && total == 0); stop = true; }).status == CheatDownloadStatus::Cancelled);
    stop = false;
    assert(DownloadCheatCatalog(std::string{kCheatCatalogSource}, program, Build(), stop,
        [](auto, auto) { throw std::runtime_error("callback failure"); }).status == CheatDownloadStatus::Failed);
    std::puts("Cheat download PASS: opt-in inert HTTPS request, exact binding, status/size/stream limits, cancellation and callback-exception containment");
}
#else
#include "cheat_manager.h"
#include "cheat_store.h"
#include "mods.h"
#include <curl/curl.h>
#include <cstdlib>
// Test-only CA path. TLS peer and host verification remain enabled. No production trust changes.
extern "C" void console_curl_setup(CURL* easy) {
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_CAINFO, std::getenv("CHEAT_TEST_CA"));
}
int main(int argc, char** argv) {
    assert(argc == 4);
    std::atomic<bool> cancelled{false};
    const int wanted = std::atoi(argv[2]);
    std::thread cancel;
    if (std::atoi(argv[3])) cancel = std::thread([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); cancelled = true;
    });
    const auto start = std::chrono::steady_clock::now();
    const auto result = Eden::DownloadCheatCatalog(argv[1], program, Build(), cancelled);
    if (cancel.joinable()) cancel.join();
    if (int(result.status) != wanted)
        std::fprintf(stderr, "Unexpected download status=%d wanted=%d error=%s\n", int(result.status), wanted, result.error.c_str());
    assert(int(result.status) == wanted);
    if (cancelled) assert(std::chrono::steady_clock::now() - start < std::chrono::seconds(5));
    if (result.status == Eden::CheatDownloadStatus::Ready) {
        // Exercise the real transport -> bounded parser -> offline cache -> no-clobber install
        // path. The fixture identity is synthetic; complete container probing has its own test.
        char temporary[] = "/tmp/eden-cheat-download-install-XXXXXX"; assert(mkdtemp(temporary));
        const std::filesystem::path root = temporary;
        assert(Eden::SaveCheatCache((root / "cache").string(), argv[1], program, Build(), result.catalog));
        const auto offline = Eden::LoadCheatCache((root / "cache").string(), argv[1], program, Build());
        assert(offline.status == Eden::CheatCatalogStatus::Ready);
        const auto installed = Eden::InstallCheatCatalog((root / "mods/0100000000001234").string(),
            argv[1], program, Build(), offline, cancelled, [] { return true; });
        assert(installed.status == Eden::CheatInstallStatus::Installed);
        const auto mods = Eden::Mods::List((root / "mods").string(), program);
        assert(mods.size() == 1 && mods[0].cheats.size() == 1);
        assert(Eden::Mods::Has(Eden::Mods::Switches(mods, {}, {}, {}, true, true).mods_off, installed.mod_name));
        std::filesystem::remove_all(root);
    }
    // Exercise the actual owner/worker boundary with the same real TLS/libcurl transport.
    // Probe identity is an invented build here; the real-container resolver has separate tests.
    char manager_temporary[] = "/tmp/eden-cheat-tls-manager-XXXXXX"; assert(mkdtemp(manager_temporary));
    const std::filesystem::path manager_root = manager_temporary;
    std::mutex bridge;
    Eden::CheatManager manager(bridge, [](const auto&) { return Build(); });
    Eden::CheatManagerRequest request{"fixture.nsp", "/synthetic/fixture.nsp", {}, argv[1],
        (manager_root / "cache").string(), (manager_root / "mods/0100000000001234").string(), program};
    assert(manager.Start(request, false));
    if (std::atoi(argv[3])) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        manager.Cancel();
    }
    const auto wait = [&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (manager.Status().busy && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        assert(!manager.Status().busy);
        return manager.Status();
    };
    auto state = wait();
    assert(state.phase == (wanted == 3 ? "ready" : wanted == 1 ? "cancelled" : "failed"));
    if (state.phase == "ready") {
        assert(state.entries == 1 && state.done != 0 && !state.offline && state.cached);
        assert(!std::filesystem::exists(manager_root / "mods"));
        // The explicit offline action goes through the same real normalized cache.
        assert(manager.Start(request, true));
        state = wait(); assert(state.phase == "ready" && state.offline && state.done == 0);
        assert(manager.Install(request)); assert(wait().phase == "installed");
        const auto mods = Eden::Mods::List((manager_root / "mods").string(), program);
        assert(mods.size() == 1 && Eden::Mods::Has(Eden::Mods::Switches(mods, {}, {}, {}, true, true).mods_off, mods[0].name));
    } else assert(!manager.Install(request));
    manager.Stop(); std::filesystem::remove_all(manager_root);
}
#endif
