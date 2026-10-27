// SPDX-License-Identifier: GPL-3.0-or-later
#include "nextendo_http.h"
#include "remote/http.h"
#include <nlohmann/json.hpp>

namespace Eden::Nextendo {
namespace {
constexpr std::size_t MaxReply = 32768;
struct Reply { std::string bytes; const std::atomic<bool>& stop; };
int Sink(void* p, const void* data, std::size_t size) {
    auto& reply = *static_cast<Reply*>(p);
    if (size > MaxReply - reply.bytes.size()) return 0;
    reply.bytes.append(static_cast<const char*>(data), size);
    return 1;
}
int Stop(void* p) { return static_cast<Reply*>(p)->stop.load(); }
}
AccountResult CheckAccountHttp(const Account& account, const std::atomic<bool>& stop) {
    if (stop.load()) return AccountResult::cancelled;
    Reply reply{{}, stop};
    const std::string authorization = "Bearer " + account.token;
    remote_http_request request{};
    request.url = "https://nextendo.network/api/profile";
    request.authorization = authorization.c_str();
    request.timeout_ms = 15000;
    request.https_only = 1;
    request.no_redirect = 1;
    request.sink = Sink;
    request.stop = Stop;
    request.user = &reply;
    remote_http_result response{};
    const int code = remote_http_run(&request, &response);
    if (stop.load()) return AccountResult::cancelled;
    if (response.status == 401) return AccountResult::rejected;
    if (code != 0 || response.status != 200) return AccountResult::failed;
    try {
        const auto json = nlohmann::json::parse(reply.bytes);
        if (!json.is_object() || !json.contains("username") || !json["username"].is_string())
            return AccountResult::failed;
    } catch (const nlohmann::json::exception&) { return AccountResult::failed; }
    // Do not propagate server bodies/errors, even when they echo a PID or bearer token.
    return AccountResult::accepted;
}
} // namespace Eden::Nextendo
