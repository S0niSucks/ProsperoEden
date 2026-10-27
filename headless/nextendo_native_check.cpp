// SPDX-License-Identifier: GPL-3.0-or-later
#include "nextendo_native_network.h"
#include "nextendo_tls.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <string>

namespace {
int lookups = 0, frees = 0;
const char* ca_file = nullptr;
}
#ifndef NEXTENDO_NO_BRIDGE
extern "C" int console_curl_getaddrinfo(const char* host, const char* service,
                                       const addrinfo* hints, addrinfo** result) {
    ++lookups;
    assert(std::string(host) == "synthetic.nextendo.network" && std::string(service) == "443");
    assert(hints->ai_family == AF_INET);
    *result = static_cast<addrinfo*>(std::calloc(1, sizeof(addrinfo)));
    assert(*result);
    return 0;
}
extern "C" void console_curl_freeaddrinfo(addrinfo* result) { ++frees; std::free(result); }
extern "C" const char* console_curl_ca_file() { return ca_file; }
#endif

int main(int argc, char** argv) {
    assert(argc == 2);
    addrinfo hints{}, *result = nullptr;
    hints.ai_family = AF_INET;
    Eden::Nextendo::NativeLookup console(true), host(false);
#ifdef NEXTENDO_NO_BRIDGE
    assert(console.Resolve("synthetic.nextendo.network", "443", &hints, &result) == EAI_FAIL);
    assert(!result);
#else
    assert(console.Resolve("synthetic.nextendo.network", "443", &hints, &result) == 0);
    console.Free(result);
    assert(lookups == 1 && frees == 1);
#endif
    assert(host.Resolve("127.0.0.1", "443", &hints, &result) == 0);
    host.Free(result);
    auto* ctx = SSL_CTX_new(TLS_client_method());
    auto* ssl = SSL_new(ctx);
    assert(!Eden::Nextendo::LoadGuestTrust(ssl)); // no bridge/no CA: fail closed
#ifndef NEXTENDO_NO_BRIDGE
    ca_file = "/not-a-certificate";
    assert(!Eden::Nextendo::LoadGuestTrust(ssl));
    ca_file = argv[1];
    assert(Eden::Nextendo::LoadGuestTrust(ssl));
#else
    (void)argv;
    (void)lookups; (void)frees; (void)ca_file;
#endif
    SSL_free(ssl); SSL_CTX_free(ctx);
}
