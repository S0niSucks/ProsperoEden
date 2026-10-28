// SPDX-License-Identifier: GPL-3.0-or-later
#include "prosperoeden/eden_services.h"
#include "assets_dir.h"

namespace {
using Eden::Network::EditorResult;
bool Saved(EditorResult result) {
    return result == EditorResult::ok || result == EditorResult::saved_sync_warning;
}
// Configure starts a discovery thread. Keep admission through its LAST callback/join, not
// just Configure's return. Only a worker calls this; neither launcher mutex is held here.
bool ConfigureAndDrain(const std::string& file, bool connect, const std::atomic<bool>& stop) {
    try {
        Eden::Network::Configure(file, connect && !stop);
        while (!stop && Eden::Network::Current().busy) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        Eden::Network::StopDiscovery();
        return true;
    } catch (...) {
        Eden::Network::Disconnect(); // also joins a partially started discovery, before token release
        return false; // never publish raw transport/configuration exception text
    }
}
}
EdenServices::NetworkAdmission::~NetworkAdmission() {
    owner.network_reservation_ = nullptr;
    owner.network_mutation_active_ = false; // final publication; no admission re-entry from a worker
}
pe::ui::NetworkReservation EdenServices::reserve_network_job() {
    // Try, don't queue the UI behind an incumbent writer's bridge/HTTP/Stop. A retry is explicit.
    const std::unique_lock admission(job_admission_mutex_, std::try_to_lock);
    if (!admission || usb_active_ || launch_committed_ || nextendo_busy_ || network_mutation_active_ ||
        network_closing_ || !Eden::FilesystemAccess()) return {};
    auto job = std::make_unique<NetworkAdmission>(*this, Eden::ConfigFile("network-shares.json"));
    network_reservation_ = job.get();
    network_mutation_active_ = true;
    return job;
}
bool EdenServices::network_job_busy() { return network_mutation_active_; }
Eden::Network::Status EdenServices::network_shares() {
    auto status = Eden::Network::Current(); status.busy |= network_job_busy(); return status;
}
Eden::Network::EditorResult EdenServices::WithNetworkMutation(pe::ui::NetworkReservation job,
    const std::function<EditorResult(const std::string&)>& operation) {
    if (!job || job.get() != network_reservation_.load()) return EditorResult::blocked;
    // Only reserve_network_job can publish this identity; foreign/consumed tokens cannot enter.
    try { return operation(static_cast<const NetworkAdmission&>(*job).file); }
    catch (...) { return EditorResult::write; }
}
Eden::Network::EditorResult EdenServices::WithNetworkMutation(
    const std::function<EditorResult(const std::string&)>& operation) {
    return WithNetworkMutation(reserve_network_job(), operation);
}
void EdenServices::reconnect_network_shares() { (void)network_editor_connection(true); }
void EdenServices::disconnect_network_shares() { (void)network_editor_connection(false); }
Eden::Network::EditorResult EdenServices::network_editor_connection(bool connect) {
    // Startup/legacy callers also dispatch asynchronously, with the lease published FIRST.
    const std::unique_lock bookkeeping(network_background_lock_, std::try_to_lock);
    if (!bookkeeping) return EditorResult::blocked;
    auto job = reserve_network_job();
    if (!job) return EditorResult::blocked;
    if (network_background_.valid()) {
        if (network_background_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return EditorResult::blocked;
        try { (void)network_background_.get(); } catch (...) {}
    }
    network_background_stop_ = false;
    try {
        network_background_ = std::async(std::launch::async, [this, connect, job = std::move(job)]() mutable {
            return network_connection_reserved(std::move(job), connect, network_background_stop_);
        });
    } catch (...) { return EditorResult::unavailable; } // failed dispatch destroys the captured lease
    return EditorResult::ok;
}
void EdenServices::StopNetworkJobs() {
    network_closing_ = true;
    std::future<EditorResult> background;
    {
        const std::lock_guard bookkeeping(network_background_lock_);
        network_background_stop_ = true;
        background = std::move(network_background_);
    }
    if (background.valid()) { try { (void)background.get(); } catch (...) {} }
    Eden::Network::StopDiscovery();
}
Eden::Network::EditorResult EdenServices::network_editor_open() {
    return WithNetworkMutation([&](const auto& file) { return network_editor_.Open(file); });
}
Eden::Network::EditorView EdenServices::network_editor_view() { return network_editor_.View(); }
Eden::Network::EditorResult EdenServices::network_editor_begin(const std::string& id) {
    return network_editor_begin_named(id, std::string{Eden::Network::DefaultNetworkLocationName()});
}
Eden::Network::EditorResult EdenServices::network_editor_begin_named(const std::string& id, std::string name) {
    return WithNetworkMutation([&](const auto&) { return network_editor_.Begin(id, name); });
}
Eden::Network::EditorResult EdenServices::network_editor_change(Eden::Network::Field field) {
    return WithNetworkMutation([&](const auto&) { return network_editor_.Change(field); });
}
Eden::Network::EditorResult EdenServices::network_editor_password(Eden::Network::PasswordChoice choice) {
    return WithNetworkMutation([&](const auto&) { return network_editor_.Password(choice); });
}
Eden::Network::EditorResult EdenServices::network_editor_enter(Eden::Network::Field field, const std::atomic<bool>& stop) {
    return WithNetworkMutation([&](const auto&) { return network_editor_.Enter(field, stop); });
}
Eden::Network::EditorResult EdenServices::network_enter_reserved(pe::ui::NetworkReservation job,
    Eden::Network::Field field, const std::atomic<bool>& stop, Eden::Network::KeyboardPrompts prompts) {
    return WithNetworkMutation(std::move(job), [&](const auto&) {
        return network_editor_.Enter(field, stop, Eden::Network::MakeNativeNetworkKeyboard(std::move(prompts)));
    });
}
Eden::Network::EditorResult EdenServices::network_editor_save(bool reconnect, const std::atomic<bool>& stop) {
    return network_save_reserved(reserve_network_job(), reconnect, stop);
}
Eden::Network::EditorResult EdenServices::network_save_reserved(pe::ui::NetworkReservation job, bool reconnect,
    const std::atomic<bool>& stop) {
    return WithNetworkMutation(std::move(job), [&](const auto& file) {
        const auto result = network_editor_.Save(stop);
        // Once published, cancellation/discovery failure does NOT pretend the saved file rolled
        // back. The connection status reports disconnected/failed separately from successful save.
        if (Saved(result)) (void)ConfigureAndDrain(file, reconnect, stop);
        return result;
    });
}
Eden::Network::EditorResult EdenServices::network_editor_remove(const std::string& id, const std::atomic<bool>& stop) {
    return network_remove_reserved(reserve_network_job(), id, stop);
}
Eden::Network::EditorResult EdenServices::network_remove_reserved(pe::ui::NetworkReservation job, const std::string& id,
    const std::atomic<bool>& stop) {
    return WithNetworkMutation(std::move(job), [&](const auto& file) {
        const auto result = network_editor_.Remove(id, stop);
        if (Saved(result)) (void)ConfigureAndDrain(file, false, stop);
        return result;
    });
}
Eden::Network::EditorResult EdenServices::network_connection_reserved(pe::ui::NetworkReservation job, bool connect,
    const std::atomic<bool>& stop) {
    return WithNetworkMutation(std::move(job), [&](const auto& file) {
        if (stop) return EditorResult::cancelled;
        if (connect) { if (!ConfigureAndDrain(file, true, stop)) return EditorResult::write; }
        else Eden::Network::Disconnect();
        return stop ? EditorResult::cancelled : EditorResult::ok;
    });
}
void EdenServices::network_editor_cancel() { network_editor_.Cancel(); }
