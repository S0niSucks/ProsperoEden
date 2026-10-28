// SPDX-License-Identifier: GPL-3.0-or-later
#include "nextendo_http.h"

namespace Eden::Nextendo {
AccountResult CheckAccountHttp(const Account&, const std::atomic<bool>& stop) {
    // Public docs describe NEX as game-only, not a Bearer for account APIs. We retain no
    // web credential and have no documented NEX introspection route. Never guess one or
    // turn a 401 from the wrong API into deletion of a user's imported game session.
    return stop.load() ? AccountResult::cancelled : AccountResult::unavailable;
}
AccountResult CheckAccountHttpWithClient(const Account& account, const std::string&,
                                        const std::atomic<bool>& stop) {
    return CheckAccountHttp(account, stop);
}
} // namespace Eden::Nextendo
