// SPDX-License-Identifier: GPL-3.0-or-later
// P06 Nextendo LOCAL authentication contract -- external candidate r1 (A06).
//
// Explicit interface only. This header defines the result vocabulary and the
// single function every local-auth implementation must supply. It is NOT a
// protocol implementation and invents no ABI, endpoint, token format, key or
// service. "Local" means verifying locally held, user-owned material on this
// console only: implementations must perform no network I/O.
//
// Contract for any future real implementation of nx_auth_check_local:
//  - never fabricate success: nx_auth_satisfied_local may be returned only
//    after a verified local check actually passed for that exact user;
//  - fail closed: errors, missing material and absent contracts are
//    nx_auth_unavailable or nx_auth_local_refused, never success;
//  - no network contact; no credentials or user material in logs, UI,
//    backups or exports;
//  - keep "no verified contract" and "check refused" distinguishable.
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    nx_auth_unavailable = 0,    // no verified local contract exists (default)
    nx_auth_local_refused = 1,  // a local check ran and refused this user
    nx_auth_satisfied_local = 2 // a verified local check passed for this user
} nx_auth_result;

// Checks ONE opaque user id against locally held material. Pure, blocking,
// stateless: no I/O, no allocation, no clock. The stub shipped in this
// increment always returns nx_auth_unavailable.
nx_auth_result nx_auth_check_local(const char* user);

#ifdef __cplusplus
}
#endif
