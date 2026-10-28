// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "nextendo.h"
namespace Eden::Nextendo {
// Game-only credentials have no documented account-API validation route. These compatibility
// entrypoints return unavailable/cancelled without network; neither consumes the client ID.
AccountResult CheckAccountHttp(const Account& account, const std::atomic<bool>& stop);
AccountResult CheckAccountHttpWithClient(const Account& account, const std::string& client_id,
                                        const std::atomic<bool>& stop);
}
