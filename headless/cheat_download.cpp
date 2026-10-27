// SPDX-License-Identifier: GPL-3.0-or-later
#include "cheat_download.h"
#include "remote/http.h"
#include <algorithm>
#include <exception>
#include <utility>

namespace Eden {
bool CheatSourceAllowed(std::string_view url) {
    if (url.size() > 2048 || !url.starts_with("https://")) return false;
    for (const unsigned char c : url) if (c <= 32 || c == 127 || c == '\\' || c == '#') return false;
    const auto end = url.find('/', 8);
    const auto authority = url.substr(8, end == url.npos ? url.size() - 8 : end - 8);
    if (authority.empty()) return false;
    // No embedded credentials, encoded authority, query-as-host or alternate URL syntax. Curl
    // still validates the actual DNS/IP/port syntax; every redirect is HTTPS-only as well.
    for (const unsigned char c : authority)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '-' || c == ':' || c == '[' || c == ']')) return false;
    return true;
}
namespace {
constexpr std::size_t kLimit = 16 * 1024 * 1024;
struct Transfer {
    const std::atomic<bool>& stop;
    std::function<void(std::uint64_t, std::uint64_t)> progress;
    std::string body;
    const char* error{};
    std::uint64_t length{};
    bool begun{};
    static int Stop(void* user) { return static_cast<Transfer*>(user)->stop.load(); }
    static int Begin(void* user, int status, std::uint64_t length) {
        auto& t = *static_cast<Transfer*>(user);
        if (t.stop.load()) return 0;
        if (status != 200) { t.error = "The cheat source did not return a complete catalog (HTTP 200 required)"; return 0; }
        if (length > kLimit) { t.error = "The cheat catalog exceeds 16 MiB"; return 0; }
        t.begun = true; t.length = length;
        return 1;
    }
    static int Sink(void* user, const void* data, std::size_t size) {
        auto& t = *static_cast<Transfer*>(user);
        if (t.stop.load()) return 0;
        if (!t.begun || size > kLimit - t.body.size()) {
            t.error = "The cheat catalog exceeds 16 MiB or has no valid response header"; return 0;
        }
        try {
            t.body.append(static_cast<const char*>(data), size);
            if (t.progress) t.progress(t.body.size(), t.length);
            return 1;
        } catch (...) {
            // No C callback may unwind through libcurl.
            t.error = "The cheat catalog could not be stored or progress reporting failed";
            return 0;
        }
    }
};
} // namespace

CheatDownload DownloadCheatCatalog(const std::string& source, std::uint64_t program,
    const std::array<std::uint8_t, 32>& main_build, const std::atomic<bool>& stop,
    std::function<void(std::uint64_t, std::uint64_t)> progress) try {
    if (stop.load()) return {CheatDownloadStatus::Cancelled, {}, {}};
    if (!CheatSourceAllowed(source)) return {CheatDownloadStatus::Failed, {}, "Choose an HTTPS cheat catalog URL without embedded credentials"};
    if (!program || std::all_of(main_build.begin(), main_build.begin() + 8, [](auto byte) { return byte == 0; }))
        return {CheatDownloadStatus::Failed, {}, "The game's effective main build ID is unknown"};
    Transfer transfer{stop, std::move(progress), {}, nullptr, 0, false};
    remote_http_request request{};
    request.url = source.c_str(); request.timeout_ms = 60000;
    request.raw = 1; request.https_only = 1;
    request.begin = Transfer::Begin; request.sink = Transfer::Sink;
    request.stop = Transfer::Stop; request.user = &transfer;
    remote_http_result response{};
    const int result = remote_http_get(&request, &response);
    if (stop.load()) return {CheatDownloadStatus::Cancelled, {}, {}};
    if (result || response.status != 200 || !transfer.begun || transfer.body.empty() ||
        (transfer.length && transfer.length != transfer.body.size())) {
        return {CheatDownloadStatus::Failed, {}, transfer.error ? transfer.error : "The cheat catalog transfer failed or was incomplete"};
    }
    auto catalog = ReadCheatCatalog(transfer.body, program, main_build);
    if (stop.load()) return {CheatDownloadStatus::Cancelled, {}, {}};
    if (catalog.status == CheatCatalogStatus::Invalid)
        return {CheatDownloadStatus::Failed, {}, "The cheat catalog is invalid or exceeds supported limits"};
    const auto status = catalog.status == CheatCatalogStatus::Ready
        ? CheatDownloadStatus::Ready : CheatDownloadStatus::NoMatch;
    return {status, std::move(catalog), {}};
} catch (const std::exception&) {
    return {CheatDownloadStatus::Failed, {}, "The cheat catalog could not be read"};
}
} // namespace Eden
