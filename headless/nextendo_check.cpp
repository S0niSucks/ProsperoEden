// SPDX-License-Identifier: GPL-3.0-or-later
// All identities, credentials, endpoints and certificates in this check are synthetic.
#include "nextendo.h"
#include "nextendo_http.h"
#include "nextendo_tls.h"
#include "remote/http.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <openssl/x509v3.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace Eden::Nextendo;
namespace fs = std::filesystem;
namespace {
const std::string profile = "0123456789ABCDEF23456789ABCDEF01";
const std::string other = "1123456789ABCDEF23456789ABCDEF01";
const std::string token = "nx2.synthetic-test-only-never-accepted-by-a-server";
int response_status = 200;
std::string response_body = R"({"username":"synthetic"})";
int requests = 0;
bool cancel_during_request = false;
std::atomic<bool> stop{false};
AccountResult SyntheticCheck(const Account& account, const std::atomic<bool>& cancel) {
    return CheckAccountHttpWithClient(account, "synthetic-registered-client", cancel);
}
void Write(const fs::path& path, std::string_view bytes, int mode = 0600) {
    { std::ofstream out(path, std::ios::binary); out << bytes; assert(out.good()); }
    assert(chmod(path.c_str(), mode) == 0);
}
std::string Read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
void Import(Store& store, const fs::path& root, const std::string& user = profile) {
    Write(root / "import.txt", "pid=9223372036854775809\nusername=synthetic\nfriend_code=not-real\ntoken=" + token + "\n");
    const auto result = store.Apply(user, Action::import_session, stop);
    assert(result.error.empty() && result.linked && !result.enabled);
    assert(!fs::exists(root / "import.txt"));
}
EVP_PKEY* Key() {
    auto* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    EVP_PKEY* key = nullptr;
    assert(ctx && EVP_PKEY_keygen_init(ctx) == 1 && EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) == 1);
    assert(EVP_PKEY_keygen(ctx, &key) == 1);
    EVP_PKEY_CTX_free(ctx);
    return key;
}
X509* Certificate(EVP_PKEY* key, bool expired) {
    auto* cert = X509_new();
    assert(cert);
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_getm_notBefore(cert), -86400 * 2);
    X509_gmtime_adj(X509_getm_notAfter(cert), expired ? -86400 : 86400);
    assert(X509_set_pubkey(cert, key) == 1);
    auto* name = X509_get_subject_name(cert);
    assert(X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("game.nextendo.network"), -1, -1, 0) == 1);
    assert(X509_set_issuer_name(cert, name) == 1);
    auto* san = X509V3_EXT_conf_nid(nullptr, nullptr, NID_subject_alt_name,
                                   const_cast<char*>("DNS:game.nextendo.network"));
    assert(san && X509_add_ext(cert, san, -1) == 1);
    X509_EXTENSION_free(san);
    assert(X509_sign(cert, key, EVP_sha256()) > 0);
    return cert;
}
bool Handshake(std::string_view host, bool trust, bool expired, bool try_bypass) {
    auto* key = Key();
    auto* cert = Certificate(key, expired);
    auto* server_ctx = SSL_CTX_new(TLS_server_method());
    auto* client_ctx = SSL_CTX_new(TLS_client_method());
    assert(server_ctx && client_ctx && SSL_CTX_use_certificate(server_ctx, cert) == 1 &&
           SSL_CTX_use_PrivateKey(server_ctx, key) == 1);
    if (trust) assert(X509_STORE_add_cert(SSL_CTX_get_cert_store(client_ctx), cert) == 1);
    auto* client = SSL_new(client_ctx);
    auto* server = SSL_new(server_ctx);
    BIO *client_bio = nullptr, *server_bio = nullptr;
    assert(BIO_new_bio_pair(&client_bio, 0, &server_bio, 0) == 1);
    SSL_set_bio(client, client_bio, client_bio);
    SSL_set_bio(server, server_bio, server_bio);
    SSL_set_connect_state(client);
    SSL_set_accept_state(server);
    assert(ConfigureGuestTls(client, host));
    if (try_bypass) SSL_set_verify(client, SSL_VERIFY_NONE, nullptr);
    assert(EnforceGuestTls(client));
    assert(SSL_get_verify_mode(client) == SSL_VERIFY_PEER);
    bool success = false;
    for (int i = 0; i < 100; ++i) {
        const int c = SSL_do_handshake(client);
        if (c == 1) { success = SSL_get_verify_result(client) == X509_V_OK; break; }
        const int ce = SSL_get_error(client, c);
        if (ce != SSL_ERROR_WANT_READ && ce != SSL_ERROR_WANT_WRITE) break;
        const int s = SSL_do_handshake(server);
        const int se = SSL_get_error(server, s);
        if (s != 1 && se != SSL_ERROR_WANT_READ && se != SSL_ERROR_WANT_WRITE) break;
    }
    SSL_free(client); SSL_free(server);
    SSL_CTX_free(client_ctx); SSL_CTX_free(server_ctx);
    X509_free(cert); EVP_PKEY_free(key);
    return success;
}
}

extern "C" int remote_http_run(const remote_http_request* request, remote_http_result* result) {
    ++requests;
    assert(std::string(request->url) == "https://nextendo.network/api/profile");
    assert(std::string(request->authorization) == "Bearer " + token);
    assert(request->https_only && request->no_redirect && request->timeout_ms == 15000);
    assert(std::string(request->nextendo_client_id) == "synthetic-registered-client");
    if (cancel_during_request) stop = true;
    if (request->stop(request->user)) { result->stopped = 1; return -1; }
    result->status = response_status;
    if (!request->sink(request->user, response_body.data(), response_body.size())) return -1;
    return 0;
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const fs::path root = fs::path(argv[1]) / "nextendo";
    Store store(root, SyntheticCheck);
    assert(!store.Get(profile).linked && !GuestEnabled());
    std::string error;
    assert(BeginGame(store, {}, false, error) && !GuestEnabled());
    assert(BeginGame(store, profile, false, error) && !GuestEnabled());
    assert(requests == 0);
    fs::create_directory(root);
    chmod(root.c_str(), 0700);
    const auto alias = root.parent_path() / "alias";
    fs::create_directory_symlink(root.parent_path(), alias);
    assert(!Store(alias / "nextendo").Get(profile).error.empty());
    fs::remove(alias);
    Account account;
    assert(CheckAccountHttp(account, stop) == AccountResult::failed && requests == 0);
    assert(ParseAccount("pid=9\ntoken=" + token, account) && account.pid == 9);
    assert(!ParseAccount("pid=9oops\ntoken=" + token, account));
    assert(!ParseAccount("pid=9\npid=8\ntoken=" + token, account));
    assert(!ParseAccount("pid=9\ntoken=9", account));
    assert(!ParseAccount("pid=9\ntoken=bad\r\nAuthorization=leak", account));
    assert(!ParseAccount(R"({"version":1,"pid":-1,"token":"synthetic-test-token"})", account));
    assert(!ParseAccount(std::string(32769, 'x'), account));
    assert(!ValidProfile("../../session") && !ValidProfile(std::string(32, '0')));
    Routes routes;
    assert(ParseRoutes(R"({"AUTH.LP1.NINTENDO.NET.":"game.nextendo.network"})", routes));
    for (const char* invalid : {
        R"({"auth.lp1.nintendo.net":"nextendo.network.evil.test"})",
        R"({"auth.lp1.nintendo.net":"http://nextendo.network"})",
        R"({"auth.lp1.nintendo.net":"127.0.0.1"})",
        R"({"auth.lp1.nintendo.net.evil.test":"game.nextendo.network"})",
        R"({"*.nintendo.net":"game.nextendo.network"})",
        R"({"auth.nintendo.net":"game.nextendo.network","AUTH.NINTENDO.NET":"game.nextendo.network"})"})
        assert(!ParseRoutes(invalid, routes));
    assert(CanonicalHost("A..nextendo.network").empty());
    assert(!IsNextendoHost("evilnextendo.network"));
    Import(store, root);
    assert(!store.Get(other).linked);
    assert(store.Apply(profile, Action::enable, stop).enabled == false);
    Write(root / "routes.json", R"({"auth.lp1.nintendo.net":"game.nextendo.network"})");
    assert(store.Apply(profile, Action::enable, stop).enabled);
    assert(!BeginGame(store, profile, true, error) && !error.empty());
    assert(BeginGame(store, profile, false, error) && GuestEnabled());
    assert(GuestPid() == 9223372036854775809ULL);
    std::array<std::uint8_t, 16> user{};
    const std::uint64_t low = 0x23456789ABCDEF01ULL, high = 0x0123456789ABCDEFULL;
    std::memcpy(user.data(), &low, 8); std::memcpy(user.data() + 8, &high, 8);
    assert(GuestPidForUser(user) == GuestPid());
    user[0] ^= 1;
    assert(GuestPidForUser(user) == 0);
    assert(GuestHost("AUTH.LP1.NINTENDO.NET.") == "game.nextendo.network");
    assert(GuestHost("unmapped.nintendo.com").empty());
    assert(GuestHost("nintendo.com.evil.test") == "nintendo.com.evil.test");
    assert(GuestHost("third-party.example") == "third-party.example");
    assert(GuestHost("127.0.0.1") == "127.0.0.1");
    assert(Handshake("auth.lp1.nintendo.net", true, false, false));
    assert(!Handshake("wrong.nextendo.network", true, false, true));
    assert(!Handshake("auth.lp1.nintendo.net", false, false, true));
    assert(!Handshake("auth.lp1.nintendo.net", true, true, true));
    auto* no_host_ctx = SSL_CTX_new(TLS_client_method());
    auto* no_host = SSL_new(no_host_ctx);
    assert(!EnforceGuestTls(no_host) && !ConfigureGuestTls(no_host, "127.0.0.1"));
    assert(ConfigureGuestTls(no_host, "game.nextendo.network"));
    assert(!ConfigureGuestTls(no_host, "unmapped.nintendo.com") && !EnforceGuestTls(no_host));
    SSL_free(no_host); SSL_CTX_free(no_host_ctx);
    assert(store.Apply(profile, Action::disable, stop).error.empty());
    assert(GuestEnabled()); // immutable launch session until teardown
    EndGame();
    assert(!GuestEnabled() && GuestHost("auth.lp1.nintendo.net") == "auth.lp1.nintendo.net");
    assert(BeginGame(store, other, false, error) && !GuestEnabled());
    assert(store.Apply(profile, Action::check_account, stop).checked);
    response_body = std::string(32769, 'x');
    assert(!store.Apply(profile, Action::check_account, stop).checked);
    response_body = "{\"token\":\"" + token + "\"}";
    const auto malformed = store.Apply(profile, Action::check_account, stop);
    assert(!malformed.checked && malformed.error.find(token) == malformed.error.npos);
    response_status = 302;
    assert(!store.Apply(profile, Action::check_account, stop).checked && store.Get(profile).linked);
    cancel_during_request = true;
    assert(!store.Apply(profile, Action::check_account, stop).checked && store.Get(profile).linked);
    cancel_during_request = false; stop = false;
    response_status = 401;
    assert(!store.Apply(profile, Action::check_account, stop).linked && !store.Get(profile).linked);
    Import(store, root);
    const auto file = root / (profile + ".account");
    const auto before = Read(file);
    Write(file.string() + ".new", "do not overwrite");
    assert(!store.Apply(profile, Action::enable, stop).error.empty() && Read(file) == before);
    fs::remove(file.string() + ".new");
    Write(root / "routes.json", "broken");
    assert(store.Apply(profile, Action::disable, stop).error.empty());
    assert(BeginGame(store, profile, false, error) && !GuestEnabled());
    chmod(file.c_str(), 0644);
    assert(!store.Get(profile).linked);
    chmod(file.c_str(), 0600);
    fs::create_hard_link(file, root / "hardlink");
    assert(!store.Get(profile).linked);
    fs::remove(root / "hardlink");
    fs::rename(file, root / "held.account");
    fs::create_symlink(root / "held.account", file);
    assert(!store.Get(profile).linked);
    assert(store.Apply(profile, Action::unlink, stop).error.empty() && fs::exists(root / "held.account"));
    assert(requests > 0);
    std::cout << "Nextendo: profile isolation, private atomic storage, import/off/unlink, routing, cancellation, revoked sessions: PASS\n"
                 "Nextendo TLS: trusted hostname accepted; wrong hostname, untrusted/expired cert, missing SNI and bypass rejected: PASS\n";
}
