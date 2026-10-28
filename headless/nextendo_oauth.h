// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "nextendo.h"
#include <chrono>

namespace Eden::Nextendo {
struct Registration {
    std::string client_id;
    std::string redirect_uri; // exact http://127.0.0.1:<port>/path, or registered {port} template
    std::string app;
};
bool ParseRegistration(std::string_view bytes, Registration& registration, std::string& error);
std::string PkceChallenge(std::string_view verifier);
class BrowserLink {
public:
    static std::unique_ptr<BrowserLink> Open(const Registration&, std::string& error,
        std::chrono::milliseconds lifetime = std::chrono::minutes{5});
    ~BrowserLink();
    std::string StartUrl() const; // only the short local entry point; never a code/token/verifier
    std::chrono::steady_clock::time_point Deadline() const;
    bool Finish(Account& account, const std::atomic<bool>& stop, std::string& error);
private:
    struct Impl;
    explicit BrowserLink(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
Status LinkBrowser(Store&, std::string_view profile, const std::atomic<bool>& stop,
                   const std::function<void(const std::string&)>& publish_url,
                   bool confirmed_game_permissions = false);
} // namespace Eden::Nextendo
