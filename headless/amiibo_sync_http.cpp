// SPDX-License-Identifier: GPL-3.0-or-later
#include "amiibo_sync.h"
#include "remote/http.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace Eden {
namespace {
std::string Origin(std::string_view url) {
    if (url.size() > 2048 || !url.starts_with("https://")) throw std::runtime_error("Use an HTTPS Amiibo source");
    for (const unsigned char c : url)
        if (c <= 32 || c == 127 || c == '\\' || c == '#' || c >= 128)
            throw std::runtime_error("Invalid Amiibo source URL");
    const auto end = url.find_first_of("/?", 8);
    std::string authority{url.substr(8, end == url.npos ? url.size() - 8 : end - 8)};
    if (authority.empty()) throw std::runtime_error("Missing Amiibo source host");
    // Deliberately narrow host syntax; credentials/escaped authorities are never accepted.
    for (char& c : authority) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '-' || c == ':')) throw std::runtime_error("Invalid Amiibo source authority");
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    }
    const auto colon = authority.find(':');
    if (colon != std::string::npos) {
        const auto port = authority.substr(colon + 1);
        if (colon == 0 || port.empty() || port.size() > 5 ||
            port.find_first_not_of("0123456789") != std::string::npos ||
            std::stoul(port) == 0 || std::stoul(port) > 65535)
            throw std::runtime_error("Invalid Amiibo source port");
        if (std::stoul(port) == 443) authority.resize(colon);
    }
    return authority;
}
struct Transfer {
    const std::atomic<bool>& stop;
    AmiiboSyncProgress progress;
    std::size_t limit;
    std::string body;
    std::uint64_t length = 0;
    int status = 0;
    bool invalid = false;
    static int Stop(void* p) { return static_cast<Transfer*>(p)->stop.load(); }
    static int Begin(void* p, int status, std::uint64_t length) {
        auto& t = *static_cast<Transfer*>(p);
        t.status = status; t.length = length;
        if (t.stop || length > t.limit) { t.invalid = true; return 0; }
        return status == 200 || status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
    }
    static int Sink(void* p, const void* data, std::size_t size) {
        auto& t = *static_cast<Transfer*>(p);
        if (t.stop || size > t.limit - t.body.size()) { t.invalid = true; return 0; }
        try {
            t.body.append(static_cast<const char*>(data), size);
            if (t.status == 200 && t.progress) t.progress(t.body.size(), t.length);
            return 1;
        } catch (...) { t.invalid = true; return 0; } // never unwind through C
    }
};
}
std::string FetchAmiiboSync(const std::string& source, std::size_t limit,
    const std::atomic<bool>& stop, AmiiboSyncProgress progress) {
    const auto origin = Origin(source);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    std::string url = source;
    for (unsigned redirects = 0; redirects <= 5; ++redirects) {
        if (stop) throw std::runtime_error("Cancelled");
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) throw std::runtime_error("Amiibo download time limit exceeded");
        if (Origin(url) != origin) throw std::runtime_error("Cross-origin Amiibo redirects are not allowed");
        Transfer transfer{stop, progress, limit, {}};
        remote_http_request request{};
        request.url = url.c_str(); request.timeout_ms = static_cast<long>(remaining);
        request.raw = 1; request.https_only = 1; request.no_redirect = 1;
        request.begin = Transfer::Begin; request.sink = Transfer::Sink;
        request.stop = Transfer::Stop; request.user = &transfer;
        remote_http_result result{};
        if (remote_http_get(&request, &result) || transfer.invalid || stop)
            throw std::runtime_error("Amiibo transfer failed, cancelled or exceeded its limits; local files are unchanged");
        if (result.status == 200) {
            if (transfer.body.empty() || (transfer.length && transfer.length != transfer.body.size()))
                throw std::runtime_error("Incomplete Amiibo response");
            return std::move(transfer.body);
        }
        if (redirects == 5 || !result.redirect_url[0]) throw std::runtime_error("Invalid or excessive Amiibo redirect");
        url = result.redirect_url; // validated before the next request; curl never follows it itself
    }
    throw std::runtime_error("Amiibo download failed");
}
} // namespace Eden
