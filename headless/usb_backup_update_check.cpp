// SPDX-License-Identifier: GPL-3.0-or-later
// Actual update-check lifecycle, synthetic catalog/network collaborators; no network requests.
#include "update_notice.h"
#include "update_check/self_update.h"
#include "update_check/update_check.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>

std::mutex catalog_lock;
std::condition_variable catalog_changed;
bool entered = false, release_catalog = false, returned = false;
std::atomic<bool> sequence_saved{false};
std::atomic<unsigned> calls{};
extern "C" self_update_check_result self_update_check_self(self_update_offer* offer) {
    ++calls;
    std::unique_lock lock(catalog_lock);
    entered = true; catalog_changed.notify_all();
    catalog_changed.wait(lock, [] { return release_catalog; });
    sequence_saved = true; // the real kit persists its accepted catalog sequence before returning
    std::strcpy(offer->available, "99.999.999");
    return SELF_UPDATE_AVAILABLE;
}
extern "C" int update_check_version_compare(const char*, const char*, int* comparable) { *comparable = 1; return 1; }
extern "C" const self_update_platform* self_update_console() { return nullptr; }
extern "C" int self_update_start(self_update_job*, const self_update_platform*, const self_update_offer*) { return 0; }
extern "C" void self_update_poll(self_update_job*, self_update_status*) {}
extern "C" void self_update_cancel(self_update_job*) {}
extern "C" int self_update_apply(self_update_job*) { return 0; }
extern "C" void self_update_finish(self_update_job*) {}

int main() {
    using namespace std::chrono_literals;
    Eden::UpdateNotice::WaitForCheck(); // no check started: never block
    Eden::UpdateNotice::Start();
    {
        std::unique_lock lock(catalog_lock);
        assert(catalog_changed.wait_for(lock, 5s, [] { return entered; }));
    }
    std::thread waiter([] {
        Eden::UpdateNotice::WaitForCheck();
        assert(sequence_saved);
        { const std::lock_guard lock(catalog_lock); returned = true; }
        catalog_changed.notify_all();
    });
    {
        std::unique_lock lock(catalog_lock);
        assert(!catalog_changed.wait_for(lock, 100ms, [] { return returned; }));
        release_catalog = true; catalog_changed.notify_all();
        assert(catalog_changed.wait_for(lock, 5s, [] { return returned; }));
    }
    waiter.join();
    Eden::UpdateNotice::Start(); Eden::UpdateNotice::WaitForCheck();
    assert(calls == 1);
    Eden::UpdateNotice::Offer offer;
    assert(Eden::UpdateNotice::Take(&offer) && offer.available == "99.999.999");
    assert(!Eden::UpdateNotice::Take(&offer));
    std::cout << "USB update-check drain test passed (actual lifecycle, synthetic catalog)\n";
}
