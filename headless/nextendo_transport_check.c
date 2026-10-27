// SPDX-License-Identifier: GPL-3.0-or-later
// The HTTP fixture is loopback-only and uses a made-up bearer, never an account session.
#include "remote/http.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int sink(void* user, const void* data, size_t size) {
    (void)user; (void)data; (void)size;
    return 1;
}
int main(int argc, char** argv) {
    assert(argc == 4 || argc == 5);
    char long_bearer[sizeof("Bearer ") + 8192];
    memcpy(long_bearer, "Bearer ", 7);
    memset(long_bearer + 7, 'x', 8192);
    long_bearer[sizeof(long_bearer) - 1] = '\0';
    remote_http_request request = {0};
    request.url = argv[1];
    request.no_redirect = atoi(argv[2]);
    request.authorization = argc == 5 ? long_bearer : "Bearer nextendo-synthetic-local-test";
    request.timeout_ms = 2000;
    request.sink = sink;
    remote_http_result result = {0};
    assert(remote_http_run(&request, &result) == 0);
    assert(result.status == atoi(argv[3]));
    return 0;
}
