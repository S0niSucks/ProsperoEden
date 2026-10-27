// SPDX-License-Identifier: GPL-3.0-or-later
// Real launcher input loop with synthetic transports. No console calls execute.
#include "pro_controller.h"
#include "prosperoeden/radio_input.h"
#include "ps5_pad.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <time.h>

static pe_pro_state pro;
static bool opt_in = true, active = false;
static int ds_handle = -1, ds_count = 1, stops = 0, ds_reads = 0;
static unsigned rumble = 0, ds_rumble = 0;
static std::uint64_t now_ms = 1000;
static ps5::pad::Data ds_sample = ps5::pad::neutral_data();
extern "C" {
int clock_gettime(clockid_t, timespec* time) {
    time->tv_sec = static_cast<time_t>(now_ms / 1000);
    time->tv_nsec = static_cast<long>((now_ms % 1000) * 1000000);
    return 0;
}
bool pe_pro_usb_start() { return opt_in; }
bool pe_pro_usb_poll(pe_pro_state* out) { *out = pro; return active; }
void pe_pro_usb_rumble(std::uint8_t large, std::uint8_t small) { rumble = large + small; }
void pe_pro_usb_stop() { ++stops; rumble = 0; }
int sceUserServiceInitialize(void*) { return 0; }
int sceUserServiceGetInitialUser(int* user) { *user = 1; return 0; }
int sceUserServiceTerminate() { return 0; }
int sceUserServiceGetLoginUserIdList(void* list) {
    const int users[4] = {1, -1, -1, -1}; std::memcpy(list, users, sizeof(users)); return 0;
}
int scePadInit() { return 0; }
int scePadOpen(int, int, int, const void*) { return ds_handle; }
int scePadGetHandle(int, int, int) { return ds_handle; }
int scePadClose(int) { return 0; }
int scePadSetVibrationMode(int, int) { return 0; }
int scePadSetVibration(int, const ps5::pad::Vibration* level) { ds_rumble = level->large_motor; return 0; }
int scePadRead(int, ps5::pad::Data* samples, int capacity) {
    ++ds_reads;
    if (ds_count > 0 && ds_count <= capacity) samples[0] = ds_sample;
    return ds_count;
}
int scePadReadState(int, ps5::pad::Data* data) { *data = ds_sample; return 0; }
}
static bool Event(radio_input_key_t key, bool down) {
    radio_input_event_t event;
    while (radio_input_next(&event)) if (event.key == key && event.pressed == down) return true;
    return false;
}
int main() {
    assert(radio_input_init()); // no DualSense: opted-in physical USB may connect later
    pro.connected = true;
    std::memset(pro.sticks, 128, sizeof(pro.sticks));
    pro.buttons = ps5::pad::kButtonCross;
    active = true;
    radio_input_poll();
    assert(Event(RADIO_INPUT_CROSS, true) && radio_input_pressed(RADIO_INPUT_CROSS));
    assert(radio_input_controllers() & 1);
    radio_input_pulse(200);
    assert(rumble == 255);
    now_ms += 351; radio_input_poll(); assert(rumble == 0);
    active = false; radio_input_poll();
    assert(Event(RADIO_INPUT_CROSS, false) && !radio_input_pressed(RADIO_INPUT_CROSS));
    assert(!(radio_input_controllers() & 1));
    active = true; pro.buttons = ps5::pad::kButtonCircle;
    radio_input_poll(); assert(Event(RADIO_INPUT_CIRCLE, true));
    radio_input_shutdown(); assert(stops == 1 && rumble == 0);
    // Existing DualSense remains usable with the feature off, and after USB removal.
    opt_in = false; active = false; ds_handle = 7;
    ds_sample.connected = 1; ds_sample.buttons = ps5::pad::kButtonCross;
    assert(radio_input_init()); radio_input_poll();
    assert(Event(RADIO_INPUT_CROSS, true));
    ds_count = 65; radio_input_poll(); assert(Event(RADIO_INPUT_CROSS, false)); // bounded SDK count
    ds_count = 1; radio_input_poll(); assert(Event(RADIO_INPUT_CROSS, true));
    ds_count = -1; radio_input_poll(); assert(Event(RADIO_INPUT_CROSS, false));
    ds_count = 1;
    radio_input_pulse(10); assert(ds_rumble > 0);
    radio_input_shutdown();
    opt_in = true; assert(radio_input_init());
    active = true; pro.buttons = ps5::pad::kButtonTriangle;
    const int reads_before_usb = ds_reads;
    radio_input_poll(); assert(Event(RADIO_INPUT_TRIANGLE, true));
    assert(ds_reads == reads_before_usb + 1 && !radio_input_pressed(RADIO_INPUT_CROSS));
    active = false; radio_input_poll();
    assert(Event(RADIO_INPUT_TRIANGLE, false));
    assert(Event(RADIO_INPUT_CROSS, true));
    radio_input_shutdown();
    std::puts("Pro Controller launcher source/fallback/pulse/lifecycle PASS");
}
