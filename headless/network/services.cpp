// SPDX-License-Identifier: GPL-3.0-or-later
#include "prosperoeden/eden_services.h"
#include "assets_dir.h"

Eden::Network::Status EdenServices::network_shares() { return Eden::Network::Current(); }
void EdenServices::reconnect_network_shares() {
    (void)network_editor_connection(true);
}
void EdenServices::disconnect_network_shares() { (void)network_editor_connection(false); }

Eden::Network::EditorResult EdenServices::WithNetworkMutation(
    const std::function<Eden::Network::EditorResult()>& operation) {
    const std::lock_guard lock(network_mutation_mutex_);
    if (!Eden::FilesystemAccess() || launch_committed_) return Eden::Network::EditorResult::blocked;
    network_mutation_active_ = true;
    struct Finish { std::atomic<bool>& active; ~Finish() { active = false; } } finish{network_mutation_active_};
    try { return operation(); }
    catch (...) { return Eden::Network::EditorResult::write; } // no raw exceptions/credential excerpts
}
Eden::Network::EditorResult EdenServices::network_editor_open() {
    return WithNetworkMutation([&] { return network_editor_.Open(Eden::ConfigFile("network-shares.json")); });
}
Eden::Network::EditorView EdenServices::network_editor_view() { return network_editor_.View(); }
Eden::Network::EditorResult EdenServices::network_editor_begin(const std::string& id) {
    return WithNetworkMutation([&] { return network_editor_.Begin(id); });
}
Eden::Network::EditorResult EdenServices::network_editor_change(Eden::Network::Field field) {
    return WithNetworkMutation([&] { return network_editor_.Change(field); });
}
Eden::Network::EditorResult EdenServices::network_editor_password(Eden::Network::PasswordChoice choice) {
    return WithNetworkMutation([&] { return network_editor_.Password(choice); });
}
Eden::Network::EditorResult EdenServices::network_editor_enter(Eden::Network::Field field, const std::atomic<bool>& stop) {
    return WithNetworkMutation([&] { return network_editor_.Enter(field, stop); });
}
Eden::Network::EditorResult EdenServices::network_editor_save(bool reconnect, const std::atomic<bool>& stop) {
    return WithNetworkMutation([&] {
        const auto result = network_editor_.Save(stop);
        if (result == Eden::Network::EditorResult::ok || result == Eden::Network::EditorResult::saved_sync_warning)
            Eden::Network::Configure(Eden::ConfigFile("network-shares.json"), reconnect);
        return result;
    });
}
Eden::Network::EditorResult EdenServices::network_editor_remove(const std::string& id, const std::atomic<bool>& stop) {
    return WithNetworkMutation([&] {
        const auto result = network_editor_.Remove(id, stop);
        if (result == Eden::Network::EditorResult::ok || result == Eden::Network::EditorResult::saved_sync_warning)
            Eden::Network::Configure(Eden::ConfigFile("network-shares.json"), false);
        return result;
    });
}
Eden::Network::EditorResult EdenServices::network_editor_connection(bool connect) {
    return WithNetworkMutation([&] {
        if (connect) Eden::Network::Configure(Eden::ConfigFile("network-shares.json"));
        else Eden::Network::Disconnect();
        return Eden::Network::EditorResult::ok;
    });
}
void EdenServices::network_editor_cancel() { network_editor_.Cancel(); }
