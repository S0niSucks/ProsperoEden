// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic service for controller/render tests ONLY. Console uses Eden::AmiiboSync.
Eden::AmiiboSyncStatus amiibo_job;
std::string amiibo_mode;
int amiibo_network_requests = 0, amiibo_polls = 0;
std::vector<std::string> amiibo_requested_ids;
bool start_amiibo_sync(const std::vector<std::string>& ids, bool offline, bool confirmed) override {
    if (amiibo_job.busy || (!offline && !confirmed)) return false;
    auto previous_entries = std::move(amiibo_job.entries);
    amiibo_job = {}; amiibo_job.entries = std::move(previous_entries);
    amiibo_job.offline = offline; amiibo_job.folder = "/synthetic/amiibo";
    if (offline) {
        amiibo_job.phase = "ready";
        amiibo_job.entries.clear();
        for (int i = 0; i < 20; ++i) {
            std::string id(16, '0'); id[15] = "0123456789abcdef"[i % 16]; id[14] = "01"[i / 16];
            amiibo_job.entries.push_back({id, "Synthetic model " + std::to_string(i), "Preview fixture"});
        }
    } else {
        ++amiibo_network_requests; amiibo_requested_ids = ids; amiibo_polls = 0;
        amiibo_job.phase = ids.empty() ? "catalog" : "photo"; amiibo_job.busy = true;
    }
    return true;
}
Eden::AmiiboSyncStatus amiibo_sync() override {
    if (amiibo_job.busy && ++amiibo_polls >= 60 && amiibo_mode != "slow") {
        amiibo_job.busy = false;
        amiibo_job.phase = amiibo_mode == "failed" ? "failed" : "ready";
        if (amiibo_mode == "failed") amiibo_job.error = "Synthetic network failure. Existing local files were kept.";
    }
    if (amiibo_job.busy) { amiibo_job.done = 1024; amiibo_job.total = 8192; }
    return amiibo_job;
}
void cancel_amiibo_sync() override { amiibo_job.busy = false; amiibo_job.phase = "cancelled"; }
