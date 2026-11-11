// SPDX-License-Identifier: GPL-3.0-or-later
// P06 Nextendo LOCAL authentication stub -- external candidate r1 (A06).
#include "nextendo_auth_local.h"

nx_auth_result nx_auth_check_local(const char* user) {
    // No verified local account contract exists in this increment, so this
    // stub deliberately never authenticates: with it, no Nextendo session
    // can be enabled and the feature stays truly default-off. Do not
    // "fix" this to return success; supply a real implementation that
    // honors the header contract instead.
    (void)user;
    return nx_auth_unavailable;
}
