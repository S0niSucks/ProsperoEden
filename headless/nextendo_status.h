// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <string>

namespace Eden::Nextendo {
// Stable msgids / integration contract: frontend must display the consent description and
// pass explicit confirmation to LinkBrowser. No permission to change guest routing is implied.
inline constexpr char GameLinkConsentMessage[] = "Link a Nextendo game session? Share your identity and allow game matchmaking (identity, game.matchmaking). No friends, presence or profile editing permission is requested. Game routing stays off until you enable it separately.";
inline constexpr char GameLinkConsentRequired[] = "Confirm identity and game matchmaking permission before linking a Nextendo game session.";
inline constexpr char GameCheckUnavailable[] = "Remote checks are unavailable for game-only Nextendo sessions. Link again to obtain a fresh game session; the saved session has not been removed.";
inline constexpr char GameSessionExpired[] = "The Nextendo game session has expired or the clock is incorrect. Link again before enabling or launching.";
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
