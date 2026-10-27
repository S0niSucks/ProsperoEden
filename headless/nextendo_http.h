// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "nextendo.h"
namespace Eden::Nextendo {
AccountResult CheckAccountHttp(const Account& account, const std::atomic<bool>& stop);
}
