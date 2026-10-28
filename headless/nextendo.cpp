// SPDX-License-Identifier: GPL-3.0-or-later
// Session format and account API adapted from Citron-Nextendo; see docs/features/NEXTENDO.md.
#include "nextendo.h"
#include "nextendo_json.h"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>

namespace Eden::Nextendo {
namespace {
constexpr std::size_t MaxFile = 32768;
std::mutex store_mutex;
// SDK 0.42 libc++ supports shared_ptr atomic free functions, not atomic<shared_ptr>.
std::shared_ptr<const Session> guest;
using Json = nlohmann::json;

bool Domain(std::string_view value) {
    if (value.empty() || value.size() > 253) return false;
    std::size_t label = 0;
    unsigned char previous = 0;
    for (const unsigned char c : value) {
        if (c == '.') {
            if (!label || label > 63 || previous == '-') return false;
            label = 0;
        } else {
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
            if (c == '-' && label == 0) return false;
            ++label;
        }
        previous = c;
    }
    return label > 0 && label <= 63 && previous != '-';
}
bool Under(std::string_view host, std::string_view domain) {
    return host == domain || (host.size() > domain.size() && host.ends_with(domain) &&
                              host[host.size() - domain.size() - 1] == '.');
}
bool SafeToken(std::string_view token) {
    if (token.size() < 16 || token.size() > 8192) return false;
    return std::all_of(token.begin(), token.end(), [](unsigned char c) { return c >= 33 && c <= 126; });
}
std::string AccountName(std::string_view profile) {
    return std::string(profile) + ".account";
}
struct Directory {
    int fd = -1;
    bool missing = false;
    explicit Directory(const std::string& root, bool create) {
        if (root.empty() || root.size() > 4095 || root.find('\0') != root.npos) return;
        int parent = open(root.front() == '/' ? "/" : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (parent < 0) return;
        std::string_view remaining(root);
        unsigned depth = 0;
        while (!remaining.empty()) {
            if (remaining.front() == '/') { remaining.remove_prefix(1); continue; }
            const auto slash = remaining.find('/');
            const auto part = remaining.substr(0, slash);
            if (part == "." || part == ".." || part.size() > 255 || ++depth > 64) break;
            const bool last = slash == remaining.npos;
            const std::string component(part);
            if (last && create && mkdirat(parent, component.c_str(), 0700) != 0 && errno != EEXIST) break;
            const int next = openat(parent, component.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            const int error = errno;
            close(parent);
            parent = -1;
            if (next < 0) { missing = error == ENOENT; return; }
            if (last) { fd = next; break; }
            parent = next;
            remaining.remove_prefix(slash + 1);
        }
        if (parent >= 0) close(parent);
        if (fd < 0) return;
        struct stat st{};
        if (fstat(fd, &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != geteuid() || (st.st_mode & 077)) {
            close(fd);
            fd = -1;
        }
    }
    ~Directory() { if (fd >= 0) close(fd); }
    Directory(const Directory&) = delete;
    Directory& operator=(const Directory&) = delete;
};
// O_NOFOLLOW + fstat prevents reading a substituted link/device. Never follow a session out
// of this held directory, even if somebody renames its path, and never allocate by a disk count.
bool ReadFile(int directory, const std::string& name, std::string& bytes, bool secret, bool* missing = nullptr) {
    if (missing) *missing = false;
    const int fd = openat(directory, name.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) { if (missing) *missing = errno == ENOENT; return false; }
    struct stat st{};
    bool ok = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_nlink == 1 &&
              st.st_uid == geteuid() && st.st_size >= 0 && st.st_size <= static_cast<off_t>(MaxFile) &&
              (!secret || (st.st_mode & 077) == 0);
    bytes.clear();
    char buffer[2048];
    while (ok) {
        const auto count = read(fd, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { ok = false; break; }
        if (count == 0) break;
        if (bytes.size() + static_cast<std::size_t>(count) > MaxFile) { ok = false; break; }
        bytes.append(buffer, static_cast<std::size_t>(count));
    }
    close(fd);
    if (!ok) bytes.clear();
    return ok;
}
bool Save(int directory, const std::string& name, const Account& account,
          const std::atomic<bool>* stop = nullptr,
          std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max(),
          bool require_current_game = false) {
    // Exclusive staging refuses stale files/symlinks rather than truncating somebody else's file.
    const std::string staged = name + ".new";
    const int fd = openat(directory, staged.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return false;
    Json json{{"version", account.expires_at ? 2 : 1}, {"pid", account.pid},
              {"token", account.token}, {"enabled", account.enabled}};
    if (account.expires_at) {
        json["credential_kind"] = "nex";
        json["issued_at"] = account.issued_at;
        json["expires_at"] = account.expires_at;
    }
    const std::string bytes = json.dump();
    std::size_t at = 0;
    bool ok = true;
    while (at < bytes.size()) {
        const auto count = write(fd, bytes.data() + at, bytes.size() - at);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ok = false; break; }
        at += static_cast<std::size_t>(count);
    }
    if (ok) ok = fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    // Last check before the atomic commit: cancellation/deadline during fsync preserves
    // the previous account. Cancellation after rename cannot roll back a committed link.
    if ((stop && stop->load()) || std::chrono::steady_clock::now() >= deadline) ok = false;
    if (require_current_game && !GameSessionCurrent(account)) ok = false;
    if (ok) ok = renameat(directory, staged.c_str(), directory, name.c_str()) == 0;
    if (!ok) unlinkat(directory, staged.c_str(), 0);
    return ok;
}
bool Remove(int directory, const std::string& name) {
    return unlinkat(directory, name.c_str(), 0) == 0 || errno == ENOENT;
}

} // namespace

bool ValidProfile(std::string_view key) {
    return key.size() == 32 && std::all_of(key.begin(), key.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
    }) && key != std::string(32, '0');
}
std::string CanonicalHost(std::string_view host) {
    if (host.empty() || host.size() > 254) return {};
    std::string out(host);
    for (char& c : out) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (!out.empty() && out.back() == '.') out.pop_back();
    return Domain(out) ? out : std::string{};
}
bool IsOfficialHost(std::string_view host) {
    return Under(host, "nintendo.net") || Under(host, "nintendo.com") || Under(host, "nintendowifi.net");
}
bool IsNextendoHost(std::string_view host) { return Under(host, "nextendo.network"); }

bool ParseAccount(std::string_view bytes, Account& account) {
    account = {};
    if (bytes.empty() || bytes.size() > MaxFile) return false;
    try {
        Account parsed;
        if (bytes.front() == '{') {
            const auto json = Detail::ParseUniqueJson(bytes);
            if (!json.is_object() || !json.contains("version") || !json["version"].is_number_unsigned() ||
                (json["version"] != 1 && json["version"] != 2) ||
                !json.contains("pid") || !json["pid"].is_number_unsigned() ||
                !json.contains("token") || !json["token"].is_string()) return false;
            parsed.pid = json["pid"].get<std::uint64_t>();
            parsed.token = json["token"].get<std::string>();
            parsed.enabled = json.value("enabled", false);
            if (json["version"] == 2) {
                if (json.value("credential_kind", std::string{}) != "nex" ||
                    !json.at("issued_at").is_number_unsigned() || !json.at("expires_at").is_number_unsigned()) return false;
                parsed.issued_at = json["issued_at"].get<std::uint64_t>();
                parsed.expires_at = json["expires_at"].get<std::uint64_t>();
                if (!parsed.issued_at || parsed.expires_at <= parsed.issued_at ||
                    parsed.expires_at - parsed.issued_at > 86400 || !parsed.token.starts_with("nx2.")) return false;
            }
        } else {
            // User-owned Citron-Nextendo session export. No password/official account input.
            bool have_pid = false, have_token = false;
            while (!bytes.empty()) {
                const auto end = bytes.find('\n');
                auto line = bytes.substr(0, end);
                if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
                const auto eq = line.find('=');
                if (eq != line.npos) {
                    const auto key = line.substr(0, eq), value = line.substr(eq + 1);
                    if (key == "pid") {
                        if (have_pid) return false;
                        const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed.pid);
                        if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) return false;
                        have_pid = true;
                    } else if (key == "token") {
                        if (have_token) return false;
                        parsed.token = value;
                        have_token = true;
                    } else if (key != "username" && key != "friend_code") return false;
                } else if (!line.empty()) return false;
                if (end == bytes.npos) break;
                bytes.remove_prefix(end + 1);
            }
        }
        // Refuse bare-PID test credentials, header injection, partial integers and empty sessions.
        if (!parsed.pid || !SafeToken(parsed.token) || parsed.token == std::to_string(parsed.pid)) return false;
        account = std::move(parsed);
        return true;
    } catch (const Json::exception&) { return false; } // exceptions may contain secrets
}
bool GameSessionCurrent(const Account& account) {
    if (!account.issued_at && !account.expires_at) return true; // legacy expiry is unknown
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return now > 0 && account.issued_at <= static_cast<std::uint64_t>(now) &&
        static_cast<std::uint64_t>(now) < account.expires_at && account.expires_at > account.issued_at &&
        account.expires_at - account.issued_at <= 86400;
}
bool ParseRoutes(std::string_view bytes, Routes& routes) {
    routes.clear();
    if (bytes.size() > MaxFile) return false;
    try {
        const auto json = Detail::ParseUniqueJson(bytes);
        if (!json.is_object() || json.size() > 64) return false;
        Routes parsed;
        for (const auto& [from, to] : json.items()) {
            if (!to.is_string()) return false;
            const auto source = CanonicalHost(from), target = CanonicalHost(to.get<std::string>());
            if (source.empty() || target.empty() || !IsOfficialHost(source) || !IsNextendoHost(target) ||
                !parsed.emplace(source, target).second) return false;
        }
        routes = std::move(parsed);
        return true;
    } catch (const Json::exception&) { return false; }
}
std::string RouteHost(const Session& session, std::string_view host) {
    if (!session.account.enabled) return std::string(host);
    const auto canonical = CanonicalHost(host);
    if (canonical.empty()) return {};
    if (const auto route = session.routes.find(canonical); route != session.routes.end()) return route->second;
    if (IsOfficialHost(canonical)) return {};
    return canonical;
}

namespace {
std::shared_ptr<const Session> LoadSession(int directory, std::string_view profile, std::string& error) {
    std::string bytes;
    bool missing = false;
    if (!ReadFile(directory, AccountName(profile), bytes, true, &missing)) {
        if (!missing) error = "Cannot read the private Nextendo session (mode 0600 required).";
        return {};
    }
    auto session = std::make_shared<Session>();
    session->profile = profile;
    if (!ParseAccount(bytes, session->account)) { error = "Invalid Nextendo session. Import it again."; return {}; }
    if (ReadFile(directory, "routes.json", bytes, false, &missing)) {
        if (!ParseRoutes(bytes, session->routes)) error = "Invalid Nextendo routes. Use exact official hosts and Nextendo DNS names.";
    } else if (!missing) error = "Cannot read Nextendo routes.json.";
    return session;
}
} // namespace
Store::Store(std::string root, AccountCheck check) : root_(std::move(root)), check_(check) {}
Store::Store(std::string root, AccountCheckFunction check) : root_(std::move(root)), check_(std::move(check)) {}
bool Store::ReadRegistration(std::string& bytes, std::string& error) const {
    bytes.clear();
    error.clear();
    const Directory directory(root_, false);
    if (directory.fd < 0 || !ReadFile(directory.fd, "oauth.json", bytes, true)) {
        error = "Configure your registered client in private nextendo/oauth.json (mode 0600). No client ID is bundled.";
        return false;
    }
    return true;
}
Status Store::Link(std::string_view profile, Account account, const std::atomic<bool>& stop,
                   std::chrono::steady_clock::time_point deadline) {
    std::lock_guard lock(store_mutex);
    Status result;
    if (!ValidProfile(profile) || !account.pid || !SafeToken(account.token) ||
        account.token == std::to_string(account.pid) || (account.expires_at && !account.token.starts_with("nx2."))) {
        result.error = "The account server returned an incomplete or invalid session.";
        return result;
    }
    if (stop.load()) { result.error = "Nextendo action cancelled."; return result; }
    if (std::chrono::steady_clock::now() >= deadline) { result.error = "Browser sign-in expired. Start again on this PS5."; return result; }
    if (!GameSessionCurrent(account)) { result.error = GameSessionExpired; return result; }
    const Directory directory(root_, true);
    account.enabled = false; // linking never consents to guest service routing
    if (directory.fd < 0 || !Save(directory.fd, AccountName(profile), account, &stop, deadline, true)) {
        result.error = stop.load() ? "Nextendo action cancelled." :
            std::chrono::steady_clock::now() >= deadline ? "Browser sign-in expired. Start again on this PS5." :
            !GameSessionCurrent(account) ? GameSessionExpired :
            "Cannot save the private Nextendo session. Check storage and stale .new files.";
        return result;
    }
    result.linked = true;
    return result;
}
std::shared_ptr<const Session> Store::Load(std::string_view profile, std::string& error) const {
    error.clear();
    if (!ValidProfile(profile)) { error = "Choose a player profile first."; return {}; }
    const Directory directory(root_, false);
    if (directory.fd < 0) {
        if (!directory.missing) error = "Nextendo folder must be private (mode 0700), not a link.";
        return {};
    }
    return LoadSession(directory.fd, profile, error);
}
Status Store::Get(std::string_view profile) const {
    std::lock_guard lock(store_mutex);
    Status status;
    const auto session = Load(profile, status.error);
    if (session) {
        status.linked = true;
        status.enabled = session->account.enabled;
        status.routes = session->routes.size();
    }
    return status;
}
Status Store::Apply(std::string_view profile, Action action, const std::atomic<bool>& stop) {
    std::lock_guard lock(store_mutex);
    Status result;
    auto fail = [&](const char* text) { result.error = text; return result; };
    if (stop.load()) return fail("Nextendo action cancelled.");
    if (!ValidProfile(profile)) return fail("Choose a player profile first.");
    const Directory directory(root_, true);
    if (directory.fd < 0) return fail("Nextendo folder must be private (mode 0700), not a link.");
    const auto file = AccountName(profile);
    if (action == Action::unlink) {
        if (!Remove(directory.fd, file) || !Remove(directory.fd, file + ".new")) return fail("Cannot remove the Nextendo session. Check storage access.");
        return result;
    }
    if (action == Action::disable) {
        // Always allow the safety switch, even if routes.json was damaged after enabling.
        std::string bytes;
        Account account;
        if (!ReadFile(directory.fd, file, bytes, true) || !ParseAccount(bytes, account))
            return fail("Cannot read the session. Unlink it to remove a damaged configuration.");
        account.enabled = false;
        if (!Save(directory.fd, file, account)) return fail("Cannot save the Nextendo setting.");
        result.linked = true;
        return result;
    }
    if (action == Action::import_session) {
        std::string bytes;
        Account account;
        if (!ReadFile(directory.fd, "import.txt", bytes, true) || !ParseAccount(bytes, account))
            return fail("Put your own Nextendo session in nextendo/import.txt (mode 0600), then retry. Never enter a website password.");
        account.enabled = false; // import never consents to online service use
        if (stop.load()) return fail("Nextendo action cancelled.");
        if (!Save(directory.fd, file, account)) return fail("Cannot save the private Nextendo session. Check storage and stale .new files.");
        result.linked = true;
        if (!Remove(directory.fd, "import.txt")) return fail("Session imported, but import.txt could not be removed. Delete it securely.");
    }
    std::string error;
    const auto session = LoadSession(directory.fd, profile, error);
    if (session) {
        result.linked = true;
        result.enabled = session->account.enabled;
        result.routes = session->routes.size();
    }
    if (!error.empty()) return fail(error.c_str());
    if (!session) return fail("Import a Nextendo session for this profile first.");
    if ((action == Action::enable || action == Action::check_account) && !GameSessionCurrent(session->account))
        return fail(GameSessionExpired);
    if (action == Action::enable || action == Action::disable) {
        if (action == Action::enable && session->routes.empty()) return fail("Configure nextendo/routes.json with verified Nextendo endpoints before enabling.");
        auto account = session->account;
        account.enabled = action == Action::enable;
        if (!Save(directory.fd, file, account)) return fail("Cannot save the Nextendo setting.");
        result.enabled = account.enabled;
    } else if (action == Action::check_account) {
        if (!check_) return fail("Nextendo account checks are unavailable in this frontend.");
        const auto checked = check_(session->account, stop);
        if (stop.load() || checked == AccountResult::cancelled) return fail("Nextendo action cancelled.");
        if (checked == AccountResult::unavailable) return fail(GameCheckUnavailable);
        if (checked == AccountResult::rejected) {
            if (!Remove(directory.fd, file) || !Remove(directory.fd, file + ".new"))
                return fail("Session rejected. Could not remove it; unlink before retrying.");
            result.linked = result.enabled = false;
            return fail("Nextendo session expired or was revoked. Import a new session.");
        }
        if (checked != AccountResult::accepted) return fail("Nextendo account check failed. Check connectivity and trusted certificates, then retry.");
        result.checked = true;
    }
    return result;
}

bool BeginGame(const Store& store, std::string_view profile, bool local_room, std::string& error) {
    std::lock_guard lock(store_mutex);
    std::atomic_store_explicit(&guest, std::shared_ptr<const Session>{}, std::memory_order_seq_cst);
    error.clear();
    if (!ValidProfile(profile)) return true; // CLI/no user chosen: cannot activate an account
    auto session = store.Load(profile, error);
    if (session && !session->account.enabled) { error.clear(); return true; }
    if (!error.empty()) return false; // never silently lose an explicit online configuration
    if (!session || !session->account.enabled) return true;
    if (!GameSessionCurrent(session->account)) { error = GameSessionExpired; return false; }
    if (local_room) { error = "Leave the local room or turn Nextendo off before launching."; return false; }
    if (session->routes.empty()) { error = "Nextendo needs verified service routes before launching."; return false; }
    std::atomic_store_explicit(&guest, std::move(session), std::memory_order_seq_cst);
    return true;
}
void EndGame() {
    std::atomic_store_explicit(&guest, std::shared_ptr<const Session>{}, std::memory_order_seq_cst);
}
std::shared_ptr<const Session> GuestSession() {
    return std::atomic_load_explicit(&guest, std::memory_order_seq_cst);
}
bool GuestEnabled() { return static_cast<bool>(GuestSession()); }
std::uint64_t GuestPid() { const auto s = GuestSession(); return s ? s->account.pid : 0; }
std::uint64_t GuestPidForUser(const std::array<std::uint8_t, 16>& user) {
    const auto session = GuestSession();
    if (!session) return 0;
    std::uint64_t low = 0, high = 0;
    std::memcpy(&low, user.data(), 8);
    std::memcpy(&high, user.data() + 8, 8);
    char key[33]{};
    std::snprintf(key, sizeof(key), "%016llX%016llX", static_cast<unsigned long long>(high),
                  static_cast<unsigned long long>(low));
    return session->profile == key ? session->account.pid : 0;
}
std::string GuestHost(std::string_view host) {
    const auto session = GuestSession();
    return session ? RouteHost(*session, host) : std::string(host);
}
} // namespace Eden::Nextendo
