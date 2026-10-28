// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <string>

namespace Eden::Nextendo {
// Deliberately contains no account identifier, username or bearer credential.
struct Status {
    bool linked = false;
    bool enabled = false;
    bool checked = false; // only the explicit account check, not game interoperability
    std::size_t routes = 0;
    std::string error;
};
enum class Action { import_session, enable, disable, check_account, unlink, link_browser };
} // namespace Eden::Nextendo
