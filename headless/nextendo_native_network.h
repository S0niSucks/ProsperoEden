// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <netdb.h>

// The existing HTTP object deliberately renames these PS5 resolver functions. Reuse them
// explicitly for opted-in guest networking, without replacing libc for unrelated code.
extern "C" int console_curl_getaddrinfo(const char*, const char*, const addrinfo*, addrinfo**)
    __attribute__((weak));
extern "C" void console_curl_freeaddrinfo(addrinfo*) __attribute__((weak));

namespace Eden::Nextendo {
class NativeLookup {
public:
    explicit NativeLookup(bool console) : console_(console) {}
    int Resolve(const char* node, const char* service, const addrinfo* hints, addrinfo** result) const {
        if (!console_) return ::getaddrinfo(node, service, hints, result);
        *result = nullptr;
        if (!console_curl_getaddrinfo || !console_curl_freeaddrinfo) return EAI_FAIL;
        return console_curl_getaddrinfo(node, service, hints, result);
    }
    void Free(addrinfo* result) const {
        if (console_) {
            if (console_curl_freeaddrinfo) console_curl_freeaddrinfo(result);
        } else ::freeaddrinfo(result);
    }
private:
    const bool console_; // allocator pairing remains fixed even if teardown is requested
};
} // namespace Eden::Nextendo
