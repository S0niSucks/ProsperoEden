// SPDX-License-Identifier: GPL-3.0-or-later
// OAuth wire contract: GPL-2.0-or-later Citron-Nextendo 8e761924, nextendo_api.cpp:357-485.
// Local receiver, validation, lifecycle and fail-closed transport are ProsperoEden implementation.
#include "nextendo_oauth.h"
#include "nextendo_json.h"
#include "remote/http.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <fcntl.h>
#include <map>
#include <netinet/in.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace Eden::Nextendo {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::string_view Origin = "https://nextendo.network";
struct Fd {
    int value = -1;
    ~Fd() { if (value >= 0) close(value); }
};
struct Secret {
    std::string value;
    ~Secret() { if (!value.empty()) OPENSSL_cleanse(value.data(), value.size()); }
};
bool Printable(std::string_view value, std::size_t max) {
    return !value.empty() && value.size() <= max && std::all_of(value.begin(), value.end(),
        [](unsigned char c) { return c >= 33 && c <= 126; });
}
bool Redirect(const Registration& r, unsigned& port, std::string& path) {
    constexpr std::string_view prefix = "http://127.0.0.1:";
    std::string_view uri(r.redirect_uri);
    if (!Printable(r.client_id, 256) || !Printable(r.app, 64) || !uri.starts_with(prefix)) return false;
    uri.remove_prefix(prefix.size());
    const auto slash = uri.find('/');
    if (slash == uri.npos) return false;
    const auto number = uri.substr(0, slash);
    port = 0;
    if (number != "{port}") {
        const auto parsed = std::from_chars(number.data(), number.data() + number.size(), port);
        if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() || port < 1024 || port > 65535)
            return false;
        if (std::to_string(port) != number) return false; // preserve the exact registered redirect spelling
    }
    path = uri.substr(slash);
    return path.size() > 1 && path.size() <= 128 && path != "/start" &&
        path.find("..") == path.npos && std::all_of(path.begin(), path.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '/' || c == '-' || c == '_' || c == '.';
        });
}
std::string Base64Url(const unsigned char* bytes, std::size_t count) {
    std::string out(4 * ((count + 2) / 3) + 1, '\0');
    const int length = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out.data()), bytes, static_cast<int>(count));
    if (length < 0) return {};
    out.resize(static_cast<std::size_t>(length));
    while (!out.empty() && out.back() == '=') out.pop_back();
    std::replace(out.begin(), out.end(), '+', '-');
    std::replace(out.begin(), out.end(), '/', '_');
    return out;
}
std::string Random() {
    std::array<unsigned char, 32> bytes{};
    return RAND_bytes(bytes.data(), bytes.size()) == 1 ? Base64Url(bytes.data(), bytes.size()) : std::string{};
}
std::string Encode(std::string_view value) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (const unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') out += c;
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}
bool Decode(std::string_view value, std::string& out) {
    auto hex = [](unsigned char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    out.clear();
    for (std::size_t i = 0; i < value.size(); ++i) {
        unsigned char c = value[i];
        if (c == '%') {
            if (i + 2 >= value.size() || hex(value[i + 1]) < 0 || hex(value[i + 2]) < 0) return false;
            c = static_cast<unsigned char>(hex(value[i + 1]) * 16 + hex(value[i + 2]));
            i += 2;
        } else if (c == '+') c = ' ';
        if (c < 32 || c > 126) return false;
        out += static_cast<char>(c);
    }
    return true;
}
bool Nonblocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0); // the native variadic wrapper consumes an argument even for GETFL
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0 || fcntl(fd, F_SETFD, FD_CLOEXEC) != 0)
        return false;
#ifdef SO_NOSIGPIPE
    const int one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) != 0) return false;
#endif
    return true;
}
bool Wait(int fd, short events, Clock::time_point deadline, const std::atomic<bool>& stop) {
    while (!stop.load() && Clock::now() < deadline) {
        pollfd p{fd, events, 0};
        const int result = poll(&p, 1, 50);
        if (result > 0) return (p.revents & events) != 0;
        if (result < 0 && errno != EINTR) return false;
    }
    return false;
}
std::string ReadRequest(int fd, Clock::time_point deadline, const std::atomic<bool>& stop) {
    std::string request;
    while (request.size() < 8192 && Wait(fd, POLLIN, deadline, stop)) {
        char bytes[1024];
        const auto count = recv(fd, bytes, std::min(sizeof(bytes), 8192 - request.size()), 0);
        if (count <= 0) {
            if (count < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
            return {};
        }
        request.append(bytes, count);
        if (request.find("\r\n\r\n") != request.npos) return request;
    }
    return {};
}
void Respond(int fd, int status, std::string_view body, std::string_view location,
             Clock::time_point deadline, const std::atomic<bool>& stop) {
    std::string reply = "HTTP/1.1 " + std::to_string(status) + " Response\r\nConnection: close\r\n"
        "Cache-Control: no-store\r\nReferrer-Policy: no-referrer\r\nContent-Type: text/plain; charset=utf-8\r\n"
        "Content-Security-Policy: default-src 'none'; frame-ancestors 'none'\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\n";
    if (!location.empty()) reply += "Location: " + std::string(location) + "\r\n";
    reply += "\r\n";
    reply += body;
    std::size_t at = 0;
    while (at < reply.size() && Wait(fd, POLLOUT, deadline, stop)) {
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        const auto count = send(fd, reply.data() + at, reply.size() - at, flags);
        if (count > 0) at += count;
        else if (count == 0 || (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)) break;
    }
}
bool Target(std::string_view request, std::string_view host, std::string& target) {
    const auto first = request.find("\r\n");
    if (!request.starts_with("GET ") || first == request.npos || !request.substr(0, first).ends_with(" HTTP/1.1")) return false;
    target = request.substr(4, first - 4 - 9);
    std::map<std::string, std::string> headers;
    auto rest = request.substr(first + 2);
    while (!rest.starts_with("\r\n")) {
        const auto end = rest.find("\r\n"), colon = rest.find(':');
        if (end == rest.npos || colon == rest.npos || colon >= end) return false;
        std::string name(rest.substr(0, colon));
        for (auto& c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        auto value = rest.substr(colon + 1, end - colon - 1);
        while (!value.empty() && value.front() == ' ') value.remove_prefix(1);
        if (!headers.emplace(name, value).second) return false;
        rest.remove_prefix(end + 2);
    }
    return rest.size() == 2 && headers["host"] == host && !headers.contains("transfer-encoding") &&
        (!headers.contains("content-length") || headers["content-length"] == "0") &&
        (!headers.contains("origin") || headers["origin"] == Origin);
}
bool Params(std::string_view query, std::map<std::string, std::string>& values) {
    while (!query.empty()) {
        const auto end = query.find('&');
        const auto item = query.substr(0, end);
        const auto equals = item.find('=');
        std::string key, value;
        if (equals == item.npos || !Decode(item.substr(0, equals), key) ||
            !Decode(item.substr(equals + 1), value) || !values.emplace(key, value).second) return false;
        if (end == query.npos) break;
        query.remove_prefix(end + 1);
    }
    return true;
}
struct Reply {
    Secret bytes;
    const std::atomic<bool>& stop;
    Clock::time_point deadline;
};
int Sink(void* p, const void* bytes, std::size_t size) {
    auto& reply = *static_cast<Reply*>(p);
    if (size > 32768 - reply.bytes.value.size()) return 0;
    reply.bytes.value.append(static_cast<const char*>(bytes), size);
    return 1;
}
int Stop(void* p) {
    const auto& reply = *static_cast<Reply*>(p);
    return reply.stop.load() || Clock::now() >= reply.deadline;
}
} // namespace

bool ParseRegistration(std::string_view bytes, Registration& r, std::string& error) {
    r = {};
    error.clear();
    try {
        if (bytes.size() <= 4096) {
            const auto json = Detail::ParseUniqueJson(bytes);
            Registration parsed{json.at("client_id").get<std::string>(), json.at("redirect_uri").get<std::string>(),
                                json.at("app").get<std::string>()};
            unsigned port;
            std::string path;
            if (json.value("version", 0) == 1 && Redirect(parsed, port, path)) { r = std::move(parsed); return true; }
        }
    } catch (const nlohmann::json::exception&) {} // never echo configuration or parser input
    error = "Invalid oauth.json. Supply your registered client_id, app and HTTP 127.0.0.1 redirect_uri (port >= 1024 or {port}).";
    return false;
}
std::string PkceChallenge(std::string_view verifier) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    if (!SHA256(reinterpret_cast<const unsigned char*>(verifier.data()), verifier.size(), digest)) return {};
    return Base64Url(digest, sizeof(digest));
}
struct BrowserLink::Impl {
    Fd listener;
    Registration registration;
    Secret verifier;
    std::string state, host, path, redirect, authorize;
    Clock::time_point deadline;
    bool used = false;
};
BrowserLink::BrowserLink(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
BrowserLink::~BrowserLink() = default;
std::string BrowserLink::StartUrl() const { return "http://" + impl_->host + "/start"; }
std::unique_ptr<BrowserLink> BrowserLink::Open(const Registration& r, std::string& error, std::chrono::milliseconds lifetime) {
    error.clear();
    auto impl = std::make_unique<Impl>();
    unsigned port;
    if (!Redirect(r, port, impl->path) || lifetime.count() <= 0 || lifetime > std::chrono::minutes{5}) {
        error = "Invalid registered Nextendo loopback configuration."; return {};
    }
    impl->registration = r;
    impl->verifier.value = Random();
    impl->state = Random();
    if (impl->verifier.value.empty() || impl->state.empty()) { error = "Secure OAuth randomness is unavailable."; return {}; }
    impl->listener.value = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
#ifdef __FreeBSD__
    address.sin_len = sizeof(address);
#endif
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    socklen_t length = sizeof(address);
    if (impl->listener.value < 0 || !Nonblocking(impl->listener.value) ||
        bind(impl->listener.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(impl->listener.value, 4) != 0 ||
        getsockname(impl->listener.value, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        error = "Cannot bind the registered 127.0.0.1 callback port. Close another sign-in or check the configured port.";
        return {};
    }
    impl->host = "127.0.0.1:" + std::to_string(ntohs(address.sin_port));
    impl->redirect = "http://" + impl->host + impl->path;
    impl->authorize = std::string(Origin) + "/api/oauth/authorize?response_type=code&client_id=" + Encode(r.client_id) +
        "&redirect_uri=" + Encode(impl->redirect) + "&scope=identity&app=" + Encode(r.app) + "&state=" + impl->state +
        "&code_challenge=" + PkceChallenge(impl->verifier.value) + "&code_challenge_method=S256";
    impl->deadline = Clock::now() + lifetime;
    return std::unique_ptr<BrowserLink>(new BrowserLink(std::move(impl)));
}
bool BrowserLink::Finish(Account& account, const std::atomic<bool>& stop, std::string& error) {
    account = {};
    error.clear();
    auto& s = *impl_;
    if (s.used) { error = "This browser sign-in has already ended. Start a new sign-in."; return false; }
    s.used = true;
    Secret code;
    for (unsigned attempts = 0; attempts < 64 && Wait(s.listener.value, POLLIN, s.deadline, stop); ++attempts) {
        Fd client{accept(s.listener.value, nullptr, nullptr)};
        if (client.value < 0 || !Nonblocking(client.value)) continue;
        const auto deadline = std::min(s.deadline, Clock::now() + std::chrono::seconds{2});
        const auto request = ReadRequest(client.value, deadline, stop);
        std::string target;
        if (!Target(request, s.host, target)) { Respond(client.value, 400, "Invalid loopback request.", {}, deadline, stop); continue; }
        if (target == "/start") { Respond(client.value, 303, "Continue in the account provider's HTTPS page.", s.authorize, deadline, stop); continue; }
        std::map<std::string, std::string> values;
        const std::string prefix = s.path + "?";
        if (!target.starts_with(prefix) || !Params(std::string_view(target).substr(prefix.size()), values) ||
            values["state"].size() != s.state.size() || CRYPTO_memcmp(values["state"].data(), s.state.data(), s.state.size()) != 0 ||
            (values["code"].empty() == values["error"].empty()) || values["code"].size() > 2048) {
            Respond(client.value, 400, "Invalid or unrelated sign-in callback. Return to the original browser tab.", {}, deadline, stop);
            continue;
        }
        if (!values["error"].empty()) {
            error = values["error"] == "access_denied" ? "Browser sign-in was declined." : "The provider rejected browser sign-in.";
            Respond(client.value, 200, "Sign-in was not completed. Return to ProsperoEden.", {}, deadline, stop);
            break;
        }
        code.value = values["code"];
        Respond(client.value, 200, "Authorization received. Return to ProsperoEden to check whether linking completed.", {}, deadline, stop);
        break;
    }
    close(s.listener.value);
    s.listener.value = -1; // consume once, and stop serving the URL before the token exchange
    if (stop.load()) error = "Nextendo action cancelled.";
    else if (Clock::now() >= s.deadline) error = "Browser sign-in expired. Start again on this PS5.";
    else if (error.empty() && code.value.empty()) error = "Browser sign-in stopped after too many invalid local requests.";
    if (!error.empty()) return false;
    Reply reply{{}, stop, s.deadline};
    Secret form{"grant_type=authorization_code&code=" + Encode(code.value) + "&client_id=" + Encode(s.registration.client_id) +
        "&redirect_uri=" + Encode(s.redirect) + "&code_verifier=" + Encode(s.verifier.value)};
    remote_http_request request{};
    request.url = "https://nextendo.network/api/oauth/token";
    request.nextendo_client_id = s.registration.client_id.c_str();
    request.method = "POST";
    request.body = form.value.c_str();
    request.body_size = form.value.size();
    request.content_type = "application/x-www-form-urlencoded";
    request.https_only = request.no_redirect = 1;
    request.timeout_ms = std::max(1L, std::min(15000L, static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(s.deadline - Clock::now()).count())));
    request.sink = Sink;
    request.stop = Stop;
    request.user = &reply;
    remote_http_result response{};
    const auto result = remote_http_run(&request, &response);
    if (stop.load()) error = "Nextendo action cancelled.";
    else if (Clock::now() >= s.deadline) error = "Browser sign-in expired. Start again on this PS5.";
    else if (result != 0) error = "OAuth token exchange failed. Check connectivity and trusted certificates.";
    else if (response.status != 200) error = "The provider refused the OAuth token exchange. Check client registration and redirect approval.";
    else {
        try {
            const auto json = Detail::ParseUniqueJson(reply.bytes.value);
            const Secret session{nlohmann::json{{"version", 1}, {"pid", json.at("account").at("pid")}, {"token", json.at("nex_token")}}.dump()};
            if (ParseAccount(session.value, account)) return true;
        } catch (const nlohmann::json::exception&) {}
        error = "The account server returned an incomplete or invalid session.";
    }
    return false;
}
Status LinkBrowser(Store& store, std::string_view profile, const std::atomic<bool>& stop,
                   const std::function<void(const std::string&)>& publish_url) {
    Status result;
    if (!ValidProfile(profile)) { result.error = "Choose a player profile first."; return result; }
    if (stop.load()) { result.error = "Nextendo action cancelled."; return result; }
    std::string bytes;
    Registration registration;
    if (!store.ReadRegistration(bytes, result.error) || !ParseRegistration(bytes, registration, result.error)) return result;
    auto flow = BrowserLink::Open(registration, result.error);
    if (!flow) return result;
    struct Clear {
        const std::function<void(const std::string&)>& publish;
        ~Clear() { publish({}); }
    } clear{publish_url};
    publish_url(flow->StartUrl());
    Account account;
    if (!flow->Finish(account, stop, result.error)) return result;
    return store.Link(profile, std::move(account), stop);
}
} // namespace Eden::Nextendo
