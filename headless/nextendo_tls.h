// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "nextendo.h"
#include <openssl/ssl.h>

#ifdef EDEN_NEXTENDO_NATIVE
extern "C" const char* console_curl_ca_file() __attribute__((weak));
#endif

namespace Eden::Nextendo {
inline bool LoadGuestTrust(::SSL* ssl) {
#ifdef EDEN_NEXTENDO_NATIVE
    if (!console_curl_ca_file) return false;
    const char* file = console_curl_ca_file();
    auto* store = X509_STORE_new();
    if (!store) return false;
    const bool ok = file && *file && X509_STORE_load_locations(store, file, nullptr) == 1 &&
                    SSL_set1_verify_cert_store(ssl, store) == 1;
    X509_STORE_free(store);
    return ok;
#else
    (void)ssl;
    return true; // host uses the existing backend's default trust store
#endif
}
// Used by the derived guest backend and the offline TLS handshake test. Neither game verify
// options nor a route may disable chain/hostname verification. No custom CA is silently trusted.
inline bool ConfigureGuestTls(::SSL* ssl, std::string_view hostname) {
    const auto routed = GuestHost(hostname);
    const auto host = CanonicalHost(routed);
    SSL_set_verify(ssl, SSL_VERIFY_PEER, nullptr);
    SSL_set_hostflags(ssl, 0);
    if (!host.empty() && host.find_first_not_of("0123456789.") != host.npos &&
        SSL_set1_host(ssl, host.c_str()) == 1 && SSL_set_tlsext_host_name(ssl, host.c_str()) == 1)
        return true;
    // A rejected hostname must not leave an older valid SNI available to a caller that ignores
    // SetHostName's failure and attempts the handshake anyway.
    SSL_set1_host(ssl, nullptr);
    SSL_set_tlsext_host_name(ssl, nullptr);
    return false;
}
inline bool EnforceGuestTls(::SSL* ssl) {
    const char* hostname = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    return hostname && ConfigureGuestTls(ssl, hostname);
}
} // namespace Eden::Nextendo
