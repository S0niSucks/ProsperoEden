// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "nextendo_status.h"
#include <atomic>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace Eden::Nextendo {
struct Account {
    std::uint64_t pid = 0; // bearer-equivalent; never log, draw, or put in diagnostics
    std::string token;
    bool enabled = false;
};
enum class AccountResult { accepted, rejected, failed, cancelled };
using AccountCheck = AccountResult (*)(const Account&, const std::atomic<bool>&);
using Routes = std::map<std::string, std::string>;
struct Session {
    Account account;
    Routes routes;
    std::string profile;
};
// root must be app-owned, never a USB/share/game directory. Tests use a private temp root.
class Store {
public:
    explicit Store(std::string root, AccountCheck check = nullptr);
    Status Get(std::string_view profile) const;
    Status Apply(std::string_view profile, Action action, const std::atomic<bool>& stop);
    std::shared_ptr<const Session> Load(std::string_view profile, std::string& error) const;
private:
    std::string root_;
    AccountCheck check_;
};

bool ValidProfile(std::string_view key);
std::string CanonicalHost(std::string_view host);
bool IsOfficialHost(std::string_view host);
bool IsNextendoHost(std::string_view host);
bool ParseAccount(std::string_view bytes, Account& account);
bool ParseRoutes(std::string_view bytes, Routes& routes);
// Empty means reject without falling through to the official service. A numeric literal is
// unchanged; this is not an IP firewall. No wildcard or IP routing is inferred from a domain.
std::string RouteHost(const Session& session, std::string_view host);

// Frozen at launch; launcher edits cannot switch identity or transport under a running guest.
bool BeginGame(const Store& store, std::string_view profile, bool local_room, std::string& error);
void EndGame();
std::shared_ptr<const Session> GuestSession();
bool GuestEnabled();
std::uint64_t GuestPid();
std::uint64_t GuestPidForUser(const std::array<std::uint8_t, 16>& user);
std::string GuestHost(std::string_view host);
} // namespace Eden::Nextendo
