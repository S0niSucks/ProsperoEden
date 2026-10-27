// SPDX-License-Identifier: GPL-3.0-or-later
// Actual launcher input loop, synthetic user service/clock/USB/native pads only.
#include "pro_controller.h"
#include "monotonic_clock.h"
#include "prosperoeden/radio_input.h"
#include "ps5_pad.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

enum class Clock { valid, fail_untouched, negative_seconds, negative_nsec, invalid_nsec, overflow };
static Clock clock_mode = Clock::valid;
static std::uint64_t now_ms = 1000;
static pe_pro_state pro;
static bool opt_in, active, usb_enabled;
static unsigned rumble, usb_starts, usb_stops;
static std::array<int, 4> signed_users;
struct Device {
    int handle = 7, count = 1, state_result = 0;
    bool available = false, opened = false, fail_open = false, reuse_handle = false;
    unsigned reads = 0, state_reads = 0, closes = 0, gets = 0, mode_calls = 0, rumble = 0;
    ps5::pad::Data sample = ps5::pad::neutral_data();
    std::vector<std::uint64_t> attempts;
};
static std::array<Device, 2> devices;
static Device& User(int user) {
    assert(user >= 1 && user <= 2);
    return devices[static_cast<std::size_t>(user - 1)];
}
static Device& Handle(int handle) {
    auto it = std::find_if(devices.begin(), devices.end(), [=](const auto& d) { return d.handle == handle && d.opened; });
    assert(it != devices.end() && "closed/stale handle reused by launcher");
    return *it;
}
extern "C" {
int clock_gettime(clockid_t, timespec* time) {
    if (clock_mode == Clock::fail_untouched) return -1;
    time->tv_sec = static_cast<time_t>(now_ms / 1000);
    time->tv_nsec = static_cast<long>((now_ms % 1000) * 1000000);
    if (clock_mode == Clock::negative_seconds) time->tv_sec = -1;
    if (clock_mode == Clock::negative_nsec) time->tv_nsec = -1;
    if (clock_mode == Clock::invalid_nsec) time->tv_nsec = 1000000000L;
    if (clock_mode == Clock::overflow) time->tv_sec = std::numeric_limits<time_t>::max();
    return 0;
}
bool pe_pro_usb_start() { ++usb_starts; usb_enabled = opt_in; return usb_enabled; }
bool pe_pro_usb_poll(pe_pro_state* out) {
    *out = pro; out->connected = usb_enabled && active; return out->connected;
}
void pe_pro_usb_rumble(std::uint8_t large, std::uint8_t small) { rumble = large + small; }
void pe_pro_usb_stop() { ++usb_stops; usb_enabled = false; rumble = 0; }
int sceUserServiceInitialize(void*) { return 0; }
int sceUserServiceGetInitialUser(int* user) { *user = 1; return 0; }
int sceUserServiceTerminate() { return 0; }
int sceUserServiceGetLoginUserIdList(void* list) { std::memcpy(list, signed_users.data(), sizeof(signed_users)); return 0; }
int scePadInit() { return 0; }
int scePadOpen(int user, int, int, const void*) {
    auto& d = User(user); d.attempts.push_back(now_ms);
    if (!d.available || d.fail_open || d.opened) return -1;
    d.opened = true; return d.handle;
}
int scePadGetHandle(int user, int, int) {
    auto& d = User(user); ++d.gets;
    if (!d.available || (!d.opened && !d.reuse_handle)) return -1;
    d.opened = true; return d.handle;
}
int scePadClose(int handle) { auto& d = Handle(handle); ++d.closes; d.opened = false; return 0; }
int scePadSetVibrationMode(int handle, int mode) { assert(mode == 2); ++Handle(handle).mode_calls; return 0; }
int scePadSetVibration(int handle, const ps5::pad::Vibration* level) {
    Handle(handle).rumble = level->large_motor; return 0;
}
int scePadRead(int handle, ps5::pad::Data* samples, int capacity) {
    auto& d = Handle(handle); ++d.reads;
    if (!d.available) return -1;
    if (d.count > 0 && d.count <= capacity) std::fill_n(samples, d.count, d.sample);
    return d.count;
}
int scePadReadState(int handle, ps5::pad::Data* data) {
    auto& d = Handle(handle); ++d.state_reads;
    if (!d.available) return -1;
    *data = d.sample; return d.state_result;
}
}

using Event = std::pair<radio_input_key_t, bool>;
static void Events(std::initializer_list<Event> expected) {
    std::vector<Event> actual;
    radio_input_event_t event;
    while (radio_input_next(&event)) actual.emplace_back(event.key, event.pressed);
    assert(actual == std::vector<Event>(expected));
}
static void Poll(std::uint64_t time) { now_ms = time; radio_input_poll(); }
static void Reset(bool enable_usb) {
    opt_in = enable_usb; active = false; usb_enabled = false;
    rumble = usb_starts = usb_stops = 0;
    clock_mode = Clock::valid; now_ms = 1000;
    devices = {};
    devices[0].handle = 7; devices[1].handle = 17;
    for (auto& d : devices) { d.sample.connected = 1; d.sample.buttons = ps5::pad::kButtonCircle; }
    signed_users = {1, -1, -1, -1};
    pro = {}; pro.connected = true;
    std::memset(pro.sticks, 128, sizeof(pro.sticks));
}
static void ClockValidation() {
    for (auto mode : {Clock::fail_untouched, Clock::negative_seconds, Clock::negative_nsec,
                      Clock::invalid_nsec, Clock::overflow}) {
        clock_mode = mode;
        std::uint64_t out = 42;
        assert(!pe_pro_monotonic_ms(&out) && out == 42);
    }
    clock_mode = Clock::valid; now_ms = 1234567;
    std::uint64_t out = 0;
    assert(pe_pro_monotonic_ms(&out) && out == now_ms);
    now_ms = std::numeric_limits<std::uint64_t>::max() / 1000 + 1;
    out = 42;
    assert(!pe_pro_monotonic_ms(&out) && out == 42); // Must also fit the USB timestamp in us.
}

static void UsbOnlyToDualSense() {
    Reset(true); active = true; pro.buttons = ps5::pad::kButtonCross;
    auto& ds = devices[0];
    assert(radio_input_init() && ds.attempts.size() == 1);
    Poll(1000); Events({{RADIO_INPUT_CROSS, true}});
    assert(radio_input_controllers() == 1 && ds.attempts.size() == 1);
    radio_input_pulse(200); assert(rumble == 255);
    Poll(1350); assert(rumble == 0); Events({});
    // Another signed-in user can own player 2; the reserved initial user cannot.
    signed_users[1] = 2; devices[1].available = true;
    Poll(1500); assert(radio_input_controllers() == 3 && ds.attempts.size() == 1);
    assert(devices[1].attempts.size() == 1);
    ds.available = true;
    Poll(1999); assert(ds.attempts.size() == 1 && ds.reads == 0);
    Poll(2000); assert(ds.attempts.size() == 2 && ds.reads == 1);
    Events({}); // Native Circle is drained while USB Cross owns player 1.
    assert(!radio_input_pressed(RADIO_INPUT_CIRCLE) && radio_input_controllers() == 3);
    Poll(2001); assert(ds.reads == 2); Events({});
    radio_input_pulse(100); assert(rumble == 255 && ds.rumble == 0);
    active = false;
    Poll(2010); Events({{RADIO_INPUT_CROSS, false}, {RADIO_INPUT_CIRCLE, true}});
    assert(rumble == 0 && radio_input_controllers() == 3 && ds.reads == 3);
    assert(ds.attempts == (std::vector<std::uint64_t>{1000, 2000}));
    radio_input_pulse(10); assert(ds.rumble > 0 && ds.mode_calls == 1);
    Poll(2359); assert(ds.rumble > 0); Events({});
    Poll(2360); assert(ds.rumble == 0); Events({});
    // Reconnect USB and switch back again; keys and native pulse must not leak.
    radio_input_pulse(20); assert(ds.rumble > 0);
    active = true; pro.buttons = ps5::pad::kButtonTriangle;
    Poll(2361); Events({{RADIO_INPUT_CIRCLE, false}, {RADIO_INPUT_TRIANGLE, true}});
    assert(ds.rumble == 0);
    signed_users[1] = -1;
    Poll(2500); assert(radio_input_controllers() == 1 && devices[1].closes == 1); Events({});
    active = false;
    Poll(2501); Events({{RADIO_INPUT_TRIANGLE, false}, {RADIO_INPUT_CIRCLE, true}});
    radio_input_pulse(100); assert(ds.rumble > 0);
    radio_input_shutdown(); assert(ds.closes == 1 && !ds.opened && rumble == 0 && ds.rumble == 0);
}

static void FailedOpenBackoff() {
    Reset(true); active = true;
    auto& ds = devices[0];
    assert(radio_input_init()); Poll(1000); Events({});
    std::uint64_t due = 2000, interval = 1000;
    for (unsigned i = 0; i < 10; ++i) {
        const auto count = ds.attempts.size();
        Poll(due - 1); assert(ds.attempts.size() == count);
        Poll(due); assert(ds.attempts.size() == count + 1 && ds.attempts.back() == due);
        for (unsigned j = 0; j < 50; ++j) radio_input_poll();
        assert(ds.attempts.size() == count + 1 && radio_input_controllers() == 1);
        interval = std::min<std::uint64_t>(interval * 2, 30000); due += interval;
    }
    assert(ds.gets == ds.attempts.size());
    pro.buttons = ps5::pad::kButtonCross;
    Poll(due - 1); Events({{RADIO_INPUT_CROSS, true}});
    radio_input_pulse(100); assert(rumble > 0);
    active = false; radio_input_poll(); Events({{RADIO_INPUT_CROSS, false}});
    assert(radio_input_controllers() == 0 && rumble == 0);
    active = true; radio_input_poll(); Events({{RADIO_INPUT_CROSS, true}});
    assert(radio_input_controllers() == 1 && ds.attempts.size() == 11);
    radio_input_shutdown(); assert(ds.closes == 0);
}

static void BadHandlesWhileUsbOwnsInput() {
    Reset(true); active = true; pro.buttons = ps5::pad::kButtonCross;
    auto& ds = devices[0]; ds.available = true;
    assert(radio_input_init()); Poll(1000); Events({{RADIO_INPUT_CROSS, true}});
    radio_input_pulse(100);
    ds.count = -1;
    Poll(1001); Poll(1002); Poll(1003); Events({});
    assert(ds.closes == 1 && rumble == 255 && radio_input_controllers() == 1);
    Poll(2002); assert(ds.attempts.size() == 1);
    Poll(2003); assert(ds.attempts.size() == 2); // Open succeeds, but reads still fail.
    Poll(2004); Poll(2005); assert(ds.closes == 2);
    Poll(4004); assert(ds.attempts.size() == 2); // No false reset of backoff on Open.
    ds.count = 1; ds.handle = 8;
    Poll(4005); assert(ds.attempts.size() == 3 && ds.opened);
    assert(radio_input_controllers() == 1 && !radio_input_pressed(RADIO_INPUT_CIRCLE));
    active = false; Poll(4006);
    Events({{RADIO_INPUT_CROSS, false}, {RADIO_INPUT_CIRCLE, true}});
    radio_input_shutdown(); assert(ds.closes == 3);
}

static void UsbDisabledAndReadRecovery() {
    Reset(false);
    auto& ds = devices[0];
    assert(!radio_input_init());
    ds.available = true; Poll(2000);
    assert(ds.attempts.size() == 1); // USB disabled still requires initial native init.
    assert(radio_input_init()); Poll(2000); Events({{RADIO_INPUT_CIRCLE, true}});
    assert(radio_input_controllers() == 1);
    ds.count = 64; radio_input_poll(); Events({}); // All valid native samples remain accepted.
    // One malformed count does not churn a valid handle, nor read past the buffer.
    ds.count = 65; Poll(2001); Events({{RADIO_INPUT_CIRCLE, false}});
    ds.count = 1; Poll(2002); Events({{RADIO_INPUT_CIRCLE, true}});
    assert(ds.attempts.size() == 2 && ds.closes == 0);
    radio_input_pulse(20); assert(ds.rumble > 0);
    ds.count = -1;
    Poll(2003); Events({{RADIO_INPUT_CIRCLE, false}}); assert(ds.rumble == 0);
    Poll(2004); Poll(2005); assert(ds.closes == 1 && radio_input_controllers() == 0);
    const auto count = ds.attempts.size();
    Poll(3004); assert(ds.attempts.size() == count);
    ds.count = 1; ds.handle = 8;
    Poll(3005); Events({{RADIO_INPUT_CIRCLE, true}});
    assert(ds.attempts.size() == count + 1 && radio_input_controllers() == 1);
    radio_input_pulse(10); assert(ds.rumble > 0 && ds.mode_calls == 2);
    Poll(3355); assert(ds.rumble == 0); Events({});
    // ReadState failures independently invalidate/recover a closed native handle.
    ds.state_result = -1;
    Poll(3356); Events({{RADIO_INPUT_CIRCLE, false}});
    Poll(3357); Poll(3358); assert(ds.closes == 2 && !ds.opened);
    ds.available = false;
    Poll(4358); assert(ds.attempts.back() == 4358 && !ds.opened);
    ds.available = true; ds.state_result = 0; ds.handle = 9;
    Poll(6357); assert(!ds.opened);
    Poll(6358); Events({{RADIO_INPUT_CIRCLE, true}});
    assert(ds.opened && radio_input_controllers() == 1);
    ds.count = 0; ds.sample.connected = 0;
    Poll(6359); Events({{RADIO_INPUT_CIRCLE, false}}); assert(radio_input_controllers() == 0);
    radio_input_shutdown(); assert(ds.closes == 3);
    // A valid already-held handle can be reused; it is not a second player.
    Reset(false); devices[0].available = devices[0].fail_open = devices[0].reuse_handle = true;
    assert(radio_input_init()); Poll(1000); Events({{RADIO_INPUT_CIRCLE, true}});
    assert(devices[0].gets == 1 && radio_input_controllers() == 1);
    radio_input_shutdown(); assert(devices[0].closes == 1);
}

static void ClockFailureRecovery() {
    Reset(true); active = true; pro.buttons = ps5::pad::kButtonCross;
    auto& ds = devices[0]; ds.available = true;
    assert(radio_input_init()); Poll(1000); Events({{RADIO_INPUT_CROSS, true}});
    radio_input_pulse(100); assert(rumble > 0);
    clock_mode = Clock::fail_untouched;
    Poll(1100); Events({{RADIO_INPUT_CROSS, false}});
    assert(rumble == 0 && !usb_enabled && radio_input_controllers() == 0);
    const auto attempts = ds.attempts.size();
    const auto starts = usb_starts;
    for (unsigned i = 0; i < 20; ++i) radio_input_poll();
    radio_input_pulse(100);
    assert(ds.attempts.size() == attempts && usb_starts == starts && rumble == 0);
    clock_mode = Clock::valid;
    Poll(1200); Events({{RADIO_INPUT_CROSS, true}});
    assert(usb_starts == starts + 1 && radio_input_controllers() == 1);
    Poll(1199); Events({{RADIO_INPUT_CROSS, false}}); // Reject clock regression too.
    assert(radio_input_controllers() == 0);
    Poll(1201); Events({{RADIO_INPUT_CROSS, true}});
    radio_input_shutdown();
    Reset(false); devices[0].available = true;
    assert(radio_input_init()); Poll(1000); Events({{RADIO_INPUT_CIRCLE, true}});
    radio_input_pulse(100); assert(devices[0].rumble > 0);
    clock_mode = Clock::invalid_nsec;
    Poll(1100); Events({{RADIO_INPUT_CIRCLE, false}});
    assert(devices[0].rumble == 0 && radio_input_controllers() == 0);
    clock_mode = Clock::valid;
    Poll(1200); Events({{RADIO_INPUT_CIRCLE, true}});
    assert(usb_starts == 1 && radio_input_controllers() == 1);
    radio_input_shutdown();
    Reset(false); devices[0].available = true; clock_mode = Clock::fail_untouched;
    assert(!radio_input_init() && devices[0].attempts.empty() && usb_starts == 0);
    radio_input_shutdown();
    clock_mode = Clock::valid;
}
int main() {
    ClockValidation(); UsbOnlyToDualSense(); FailedOpenBackoff();
    BadHandlesWhileUsbOwnsInput(); UsbDisabledAndReadRecovery(); ClockFailureRecovery();
    std::puts("Pro Controller launcher reservation/reopen/backoff/fallback/pulse/clock PASS");
}
