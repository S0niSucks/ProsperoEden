// SPDX-License-Identifier: GPL-3.0-or-later
#include "devices.h"
#include "controller_applet.h"
#include "mock_devices.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <stdexcept>
#include "common/fs/path_util.h"
#include "common/input.h"
#include "common/logging.h"
#include "common/settings.h"
#include "audio_core/sink/sink_details.h"
#include "core/core.h"
#include "hid_core/resources/shared_memory_format.h"
#include "audio_core/common/audio_renderer_parameter.h"
#include "audio_core/renderer/voice/voice_info.h"
#include "audio_core/renderer/voice/voice_channel_resource.h"
#include "audio_core/renderer/mix/mix_info.h"
#include "audio_core/renderer/sink/sink_info_base.h"
#include "audio_core/renderer/performance/performance_manager.h"
#include "audio_core/renderer/memory/pool_mapper.h"
#include "audio_core/renderer/command/resample/resample.h"

// Freestanding renderer fixture layout must match the pinned core's IPC ABI.
static_assert(sizeof(AudioCore::AudioRendererParameterInternal) == 0x34);
using namespace AudioCore::Renderer;
static_assert(sizeof(BehaviorInfo::InParameter) == 0x10);
static_assert(sizeof(BehaviorInfo::OutStatus) == 0xb0);
static_assert(offsetof(BehaviorInfo::OutStatus, error_count) == 0xa0);
static_assert(sizeof(MemoryPoolInfo::InParameter) == 0x20);
static_assert(sizeof(MemoryPoolInfo::OutStatus) == 0x10);
static_assert(sizeof(VoiceChannelResource::InParameter) == 0x70);
static_assert(sizeof(VoiceInfo::InParameter) == 0x170);
static_assert(offsetof(VoiceInfo::InParameter, wave_buffer_internal) == 0x60);
static_assert(offsetof(VoiceInfo::InParameter, channel_resource_ids) == 0x140);
static_assert(offsetof(VoiceInfo::InParameter, flags) == 0x15c);
static_assert(offsetof(VoiceInfo::InParameter, src_quality) == 0x15e);
static_assert(static_cast<unsigned>(AudioCore::SrcQuality::Low) == 2);
static_assert(static_cast<unsigned>(AudioCore::SrcQuality::High) == 1);
static_assert(offsetof(VoiceChannelResource::InParameter, mix_volumes) == 4);
static_assert(offsetof(VoiceChannelResource::InParameter, in_use) == 0x64);
static_assert(offsetof(SinkInfoBase::InParameter, device) == 0x20);
static_assert(offsetof(SinkInfoBase::DeviceInParameter, inputs) == 0x104);
static_assert(static_cast<unsigned>(AudioCore::SampleFormat::PcmInt16) == 2);
static_assert(sizeof(VoiceInfo::OutStatus) == 0x10);
static_assert(sizeof(MixInfo::InParameter) == 0x930);
static_assert(offsetof(MixInfo::InParameter, dest_mix_id) == 0x924);
static_assert(sizeof(SinkInfoBase::InParameter) == 0x140);
static_assert(sizeof(SinkInfoBase::OutStatus) == 0x20);
static_assert(sizeof(PerformanceManager::InParameter) == 0x10);
static_assert(sizeof(PerformanceManager::OutStatus) == 0x10);

// Guest fixture reads this documented shared-memory ABI without linking a guest SDK.
static_assert(offsetof(Service::HID::SharedMemoryFormat, npad) == 0x9a00);
static_assert(offsetof(Service::HID::NpadInternalState, fullkey_lifo) == 0x28);
static_assert(sizeof(Service::HID::NPadGenericState) == 0x28);
static_assert(offsetof(Service::HID::NPadGenericState, npad_buttons) == 8);
static_assert(offsetof(Service::HID::NPadGenericState, connection_status) == 0x20);
using GuestPadRing = Service::HID::Lifo<Service::HID::NPadGenericState, 17>;
static_assert(offsetof(GuestPadRing, entries) == 0x20);
static_assert(sizeof(GuestPadRing::entries[0]) == 0x30);

#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)
using Eden::Mock::state;
using namespace AudioCore::Sink;
using namespace std::chrono_literals;
template<class F> void Reject(F&& f) {
    bool rejected = false;
    try { f(); } catch (const std::exception&) { rejected = true; }
    CHECK(rejected);
}

// A game's "connect controllers" screen: every combination it may allow gets a controller, except
// the two with no answer (nothing allowed, or only the handheld while docked).
void CheckControllerStyle() {
    using Core::HID::NpadStyleIndex;
    Core::Frontend::ControllerParameters allowed{};
    CHECK(!Eden::ControllerStyle(allowed, 0, true) && !Eden::ControllerStyle(allowed, 0, false));
    allowed.allow_handheld = true;
    CHECK(!Eden::ControllerStyle(allowed, 0, true));
    CHECK(Eden::ControllerStyle(allowed, 0, false) == NpadStyleIndex::Handheld);
    CHECK(!Eden::ControllerStyle(allowed, 1, false));
    allowed.allow_right_joycon = true;
    CHECK(Eden::ControllerStyle(allowed, 0, true) == NpadStyleIndex::JoyconRight);
    CHECK(Eden::ControllerStyle(allowed, 0, false) == NpadStyleIndex::JoyconRight);
    allowed.allow_right_joycon = false; allowed.allow_left_joycon = true;
    CHECK(Eden::ControllerStyle(allowed, 1, true) == NpadStyleIndex::JoyconLeft);
    allowed.allow_right_joycon = true;
    CHECK(Eden::ControllerStyle(allowed, 0, true) == NpadStyleIndex::JoyconLeft);
    CHECK(Eden::ControllerStyle(allowed, 1, true) == NpadStyleIndex::JoyconRight);
    CHECK(Eden::ControllerStyle(allowed, 2, true) == NpadStyleIndex::JoyconLeft);
    allowed.allow_dual_joycons = true;
    CHECK(Eden::ControllerStyle(allowed, 1, true) == NpadStyleIndex::JoyconDual);
    allowed.allow_pro_controller = true;
    CHECK(Eden::ControllerStyle(allowed, 3, false) == NpadStyleIndex::Fullkey);
    std::puts("Controller applet styles: pro, pair, single Joy-Cons, handheld, and the two unanswerable requests PASS");
}

void CheckPad() {
    using namespace ps5::pad;
    Reject([] { Eden::Pad pad(1.0f); });
    {
        Eden::Pad pad;
        CHECK(pad.Open()); CHECK(pad.Open()); CHECK(state.pad_opens == 1);
        CHECK(state.last_pad_user == 84); // Foreground user differs from initial login (42).
        auto sample = neutral_data(); sample.connected = 1;
        const auto consume = [&] { pad.Consume({&sample, 1}); };
        const std::pair<ButtonMask, int> mapping[] = {
            {kButtonCircle, 0}, {kButtonCross, 1}, {kButtonTriangle, 2}, {kButtonSquare, 3},
            {kButtonL3, 4}, {kButtonR3, 5}, {kButtonL1, 6}, {kButtonR1, 7},
            {kButtonL2, 8}, {kButtonR2, 9}, {kButtonOptions, 10},
            {kButtonLeft, 12}, {kButtonUp, 13}, {kButtonRight, 14}, {kButtonDown, 15},
        };
        for (auto [mask, button] : mapping) {
            sample.buttons = mask; consume();
            // L1 and R1 are also SL and SR of a single Joy-Con (left: 16 and 17, right: 20 and 21).
            const int sl_sr = mask == kButtonL1 ? 16 : mask == kButtonR1 ? 17 : -1;
            for (int i = 0; i < 22; ++i)
                CHECK(pad.Engine().GetButton({}, i) == (i == button || (sl_sr >= 0 && (i == sl_sr || i == sl_sr + 4))));
        }
        sample.buttons = 0;
        sample.left_stick = {0, 255}; sample.right_stick = {255, 0}; consume();
        CHECK(pad.Engine().GetAxis({}, 0) == -1); CHECK(pad.Engine().GetAxis({}, 1) == -1);
        CHECK(pad.Engine().GetAxis({}, 2) == 1); CHECK(pad.Engine().GetAxis({}, 3) == 1);
        sample.left_stick = {128, 132}; sample.triggers = {127, 128}; consume();
        CHECK(pad.Engine().GetAxis({}, 0) == 0); CHECK(pad.Engine().GetAxis({}, 1) == 0);
        CHECK(!pad.Engine().GetButton({}, 8)); CHECK(pad.Engine().GetButton({}, 9));

        const auto none_pressed = [&] {
            for (int i = 0; i < 22; ++i) if (pad.Engine().GetButton({}, i)) return false;
            return true;
        };
        sample.triggers = {};
        sample.buttons = kButtonTouchPad | kButtonL1; consume();
        CHECK(pad.TakeReturnToMenu()); CHECK(!pad.TakeReturnToMenu());
        CHECK(none_pressed());
        consume(); CHECK(!pad.TakeReturnToMenu());
        sample.buttons = 0; consume();
        sample.buttons = kButtonTouchPad | kButtonR1; consume();
        CHECK(pad.TakeHudToggle()); CHECK(!pad.TakeHudToggle());
        CHECK(none_pressed());
        sample.buttons = 0; consume();
        // A shortcut is never a press of Minus, however long its keys take to come up.
        for (int i = 0; i < 80; ++i) { consume(); CHECK(none_pressed()); }

        // The touchpad as Select: a tap presses Minus (11) on release, for about 100 ms...
        sample.buttons = kButtonTouchPad; consume(); CHECK(none_pressed());
        for (int i = 0; i < 20; ++i) { consume(); CHECK(none_pressed()); }
        sample.buttons = 0; consume();
        for (int i = 0; i < 24; ++i) { CHECK(pad.Engine().GetButton({}, 11)); consume(); }
        consume(); CHECK(none_pressed());
        // ...a long press holds it until the release...
        sample.buttons = kButtonTouchPad;
        for (int i = 0; i < 59; ++i) { consume(); CHECK(none_pressed()); }
        consume(); CHECK(pad.Engine().GetButton({}, 11));
        for (int i = 0; i < 30; ++i) { consume(); CHECK(pad.Engine().GetButton({}, 11)); }
        sample.buttons = 0; consume(); CHECK(none_pressed());
        for (int i = 0; i < 30; ++i) { consume(); CHECK(none_pressed()); }
        // ...a shortcut started slowly gives none, and a controller that goes away is not a tap.
        sample.buttons = kButtonTouchPad; consume(); consume();
        sample.buttons = kButtonTouchPad | kButtonR1; consume(); CHECK(pad.TakeHudToggle());
        sample.buttons = kButtonTouchPad; consume();
        sample.buttons = 0; consume(); CHECK(none_pressed());
        sample.buttons = kButtonTouchPad; consume();
        sample.connected = 0; consume(); CHECK(none_pressed());
        sample.connected = 1; sample.buttons = 0; consume(); CHECK(none_pressed());
        // The real PS5 Create button now becomes virtual Switch Capture,
        // not an unexpected Minus. No Nintendo OS screenshot is emitted.
        sample.buttons = kButtonCreate; consume();
        CHECK(pad.Engine().GetButton({}, 19)); CHECK(!pad.Engine().GetButton({}, 11));
        sample.buttons = 0; consume(); CHECK(none_pressed());
        // Sony PS/Home itself is not exposed by scePadRead; Touchpad+Options
        // provides guest HOME. Neither physical component may leak Plus/Minus.
        sample.buttons = kButtonTouchPad | kButtonOptions; consume();
        CHECK(pad.Engine().GetButton({}, 18));
        CHECK(!pad.Engine().GetButton({}, 10) && !pad.Engine().GetButton({}, 11));
        for (int i = 0; i < 80; ++i) {
            consume(); CHECK(pad.Engine().GetButton({}, 18));
            CHECK(!pad.Engine().GetButton({}, 10) && !pad.Engine().GetButton({}, 11));
        }
        sample.buttons = kButtonTouchPad; consume(); CHECK(none_pressed());
        sample.buttons = 0; consume(); CHECK(none_pressed());
        sample.buttons = kButtonTouchPad | kButtonL1; consume(); CHECK(pad.TakeReturnToMenu());
        sample.buttons = 0; consume(); CHECK(none_pressed());

        // Motion: Eden's SDL mapping of a DualSense (G, turns per second, microsecond deltas).
        constexpr float pi = std::numbers::pi_v<float>;
        sample.acceleration = {0.25f, 1.0f, -0.5f};
        sample.angular_velocity = {pi, 0.0f, -2.0f * pi};
        auto motion_device = Common::Input::CreateInputDeviceFromString("engine:virtual_gamepad,port:0,motion:0");
        int motion_updates = 0;
        motion_device->SetCallback({[&](const auto&) { ++motion_updates; }});
        sample.timestamp_us = 1000; consume(); // The first sample only starts the clock.
        CHECK(motion_updates == 0);
        sample.timestamp_us = 5000; consume();
        CHECK(motion_updates == 1);
        auto motion = pad.Engine().GetMotion({}, 0);
        CHECK(motion.delta_timestamp == 4000);
        CHECK(motion.accel_x == -0.25f); CHECK(motion.accel_y == -0.5f); CHECK(motion.accel_z == -1.0f);
        CHECK(motion.gyro_x == 0.5f); CHECK(motion.gyro_y == 1.0f); CHECK(motion.gyro_z == 0.0f);
        consume(); CHECK(motion_updates == 1); // A repeated sample is not new motion.
        sample.connected = 0; consume(); // Away: at rest instead of turning on.
        CHECK(motion_updates == 2);
        motion = pad.Engine().GetMotion({}, 0);
        CHECK(motion.gyro_x == 0.0f); CHECK(motion.gyro_y == 0.0f); CHECK(motion.accel_z == -1.0f);
        sample.connected = 1; sample.acceleration = {}; sample.angular_velocity = {}; sample.timestamp_us = 0;

        // Rumble: both guest sides on one DualSense; low band -> large motor, high band -> small.
        {
            using Common::Input::DriverResult;
            using Type = Common::Input::VibrationAmplificationType;
            auto left = Common::Input::CreateOutputDeviceFromString("engine:virtual_gamepad,port:0,pad:1");
            auto right = Common::Input::CreateOutputDeviceFromString("engine:virtual_gamepad,port:0,pad:2");
            auto handheld = Common::Input::CreateOutputDeviceFromString("engine:virtual_gamepad,port:8,pad:1");
            CHECK(left->IsVibrationEnabled());
            state.vibrations.clear();
            CHECK(pad.Poll()); CHECK(state.vibrations.empty());
            CHECK(left->SetVibration({.low_amplitude = 1.0f, .type = Type::Exponential}) == DriverResult::Success);
            CHECK(right->SetVibration({.high_amplitude = 0.5f, .type = Type::Linear}) == DriverResult::Success);
            CHECK(pad.Poll()); CHECK(state.vibrations.size() == 1);
            CHECK(state.vibrations[0].large_motor == 255); CHECK(state.vibrations[0].small_motor == 154);
            CHECK(pad.Poll()); CHECK(state.vibrations.size() == 1); // Unchanged levels are not resent.
            CHECK(left->SetVibration({}) == DriverResult::Success);
            CHECK(right->SetVibration({}) == DriverResult::Success);
            CHECK(pad.Poll()); CHECK(state.vibrations.size() == 2);
            CHECK(state.vibrations[1].large_motor == 0); CHECK(state.vibrations[1].small_motor == 0);
            CHECK(handheld->SetVibration({.high_amplitude = 1.0f, .type = Type::Exponential}) == DriverResult::Success);
            CHECK(pad.Poll()); CHECK(state.vibrations.size() == 3); CHECK(state.vibrations[2].small_motor == 255);
        }

        auto button = Common::Input::CreateInputDeviceFromString("engine:virtual_gamepad,port:0,button:0");
        std::vector<bool> changes;
        button->SetCallback({[&](const auto& status) { changes.push_back(status.button_status.value); }});
        for (int i = 0; i < 64; ++i) {
            sample.buttons = i % 2 == 0 ? kButtonCircle : 0;
            state.pad_samples.push_back(sample);
        }
        CHECK(pad.Poll()); CHECK(changes.size() == 64);
        for (int i = 0; i < 64; ++i) CHECK(changes[i] == (i % 2 == 0));
        sample.buttons = kButtonCircle; consume(); CHECK(pad.Poll());
        CHECK(pad.Engine().GetButton({}, 0)); // Empty reads preserve the last sample.
        sample.connected = 0; consume(); CHECK(!pad.Engine().GetButton({}, 0));
        sample.connected = 1; consume(); CHECK(pad.Engine().GetButton({}, 0));
        sample.buttons |= kButtonIntercepted; consume(); CHECK(!pad.Engine().GetButton({}, 0));
        sample.buttons = kButtonCircle;
        for (int error : {-1, 65}) {
            consume(); state.read_result = error; CHECK(!pad.Poll());
            CHECK(!pad.Engine().GetButton({}, 0)); CHECK(!pad.Engine().GetButton({}, 9));
        }
        state.read_result = 0; consume(); pad.Close(); pad.Close();
        CHECK(!pad.Engine().GetButton({}, 9));
        CHECK(state.vibrations.size() == 4); // Closing stops the motors.
        CHECK(state.vibrations[3].large_motor == 0); CHECK(state.vibrations[3].small_motor == 0);
        CHECK(state.pad_closes == 1); CHECK(state.user_terminations == 1);
    }
    state.user_init_result = -1; // A service owned by the embedding application.
    { Eden::Pad pad; CHECK(pad.Open()); }
    CHECK(state.pad_closes == 2); CHECK(state.user_terminations == 1);
    state.user_init_result = 0; state.user_result = -1;
    { Eden::Pad pad; CHECK(!pad.Open()); }
    CHECK(state.user_terminations == 2);
    state.user_result = 0; state.pad_open_result = -1;
    { Eden::Pad pad; CHECK(!pad.Open()); }
    CHECK(state.user_terminations == 3); CHECK(state.pad_closes == 2);
    state.pad_open_result = 7;
    std::puts("Pad mappings, calibration, motion, rumble, 64-sample edges, disconnect and ownership PASS");
}

void CheckPadRecovery() {
    using namespace ps5::pad;
    const auto reset = [] {
        state.pad_samples.clear();
        state.user_init_result = state.user_result = state.read_result = 0;
        state.foreground_user = 84;
        state.pad_open_result = 7;
        state.pad_get_handle_result = -1;
        state.user_terminations = state.pad_closes = state.pad_opens = 0;
        state.login_users.clear();
        state.pad_open_results.clear(); state.pad_read_results.clear(); state.pad_reads.clear();
        state.pad_open_users.clear(); state.pad_get_handle_users.clear(); state.pad_closed_handles.clear();
        state.vibrations.clear(); state.vibration_handles.clear();
        state.vibration_mode_handles.clear(); state.motion_handles.clear();
        state.guest_input = false;
    };
    const auto poll = [](Eden::Pad& pad, unsigned count) {
        for (unsigned i = 0; i < count; ++i) (void)pad.Poll();
    };
    auto pressed = neutral_data(1000);
    pressed.connected = 1;
    pressed.buttons = kButtonCircle | kButtonTouchPad;
    pressed.triggers.l2 = pressed.triggers.r2 = 255;
    pressed.left_stick = {255, 0}; pressed.right_stick = {0, 255};
    pressed.angular_velocity.x = 1.0f;

    // Generic failure, a mock no-device error, and an impossible sample count.
    // Empty reads and disconnected reports are successful reads, not retry triggers.
    for (const int error : {-1, -19, 65}) {
        reset();
        Eden::Pad pad;
        pad.SetMapping(Eden::Assign(Eden::kDefaultMapping, Eden::game_a, Eden::pad_circle));
        CHECK(pad.Open()); CHECK(pad.TakeConnectionChanges() == 1u);
        poll(pad, 300);
        auto disconnected = neutral_data();
        for (unsigned i = 0; i < 300; ++i) {
            state.pad_samples.push_back(disconnected);
            CHECK(pad.Poll());
        }
        CHECK(state.pad_opens == 1 && state.pad_closes == 0);
        state.pad_samples.push_back(pressed); CHECK(pad.Poll());
        auto moving = pressed; moving.timestamp_us = 5000;
        state.pad_samples.push_back(moving); CHECK(pad.Poll());
        CHECK(pad.Engine().GetButton({}, 0));
        CHECK(pad.Engine().GetAxis({}, 0) == 1.0f);
        CHECK(pad.Engine().GetMotion({}, 0).gyro_x != 0.0f);
        state.read_result = error;
        state.pad_open_result = 9;
        CHECK(!pad.Poll());
        for (int button = 0; button < 22; ++button) CHECK(!pad.Engine().GetButton({}, button));
        for (int axis = 0; axis < 4; ++axis) CHECK(pad.Engine().GetAxis({}, axis) == 0.0f);
        CHECK(pad.Engine().GetMotion({}, 0).gyro_x == 0.0f);
        CHECK(pad.Engine().GetMotion({}, 0).accel_z == -1.0f);
        poll(pad, 248);
        CHECK(state.pad_opens == 1 && state.pad_closes == 0);
        CHECK(!pad.Poll()); // Failure 250: exactly one close/open, same user and player.
        CHECK(state.pad_opens == 2 && state.pad_closed_handles == std::vector<int>{7});
        CHECK(state.last_pad_user == 84 && state.pad_reads[9] == 0);
        CHECK(state.vibration_mode_handles.back() == 9 && state.motion_handles.back() == 9);
        CHECK(pad.ConnectedPlayers() == 1u && pad.TakeConnectionChanges() == 0u);
        state.read_result = 0;
        state.pad_samples.push_back(pressed); CHECK(pad.Poll());
        CHECK(pad.Engine().GetButton({}, 0));
        poll(pad, 300); // Working empty reads retain held input and open nothing.
        CHECK(pad.Engine().GetButton({}, 0) && state.pad_opens == 2);
        state.read_result = error;
        poll(pad, 249); CHECK(state.pad_opens == 2);
        state.read_result = 0; CHECK(pad.Poll()); // Even zero samples reset the error streak.
        state.read_result = error;
        state.pad_open_result = 11;
        poll(pad, 249); CHECK(state.pad_opens == 2);
        CHECK(!pad.Poll()); CHECK(state.pad_opens == 3);
        state.read_result = 0;
        pad.Close(); pad.Close();
        CHECK((state.pad_closed_handles == std::vector<int>{7, 9, 11}));
        CHECK(state.user_terminations == 1);
    }

    // A successful reopen alone is not recovery: failed reads on each new handle
    // must still back off, rather than retrying every 250 polls forever.
    reset();
    {
        Eden::Pad pad;
        CHECK(pad.Open()); (void)pad.TakeConnectionChanges();
        state.read_result = -1; state.pad_open_result = 9;
        poll(pad, 250);
        CHECK(state.pad_opens == 2 && state.pad_closes == 1);
        for (unsigned delay : {500u, 1000u, 2000u, 4000u, 8000u, 8000u}) {
            const int opens = state.pad_opens, closes = state.pad_closes;
            ++state.pad_open_result;
            poll(pad, delay - 1);
            CHECK(state.pad_opens == opens && state.pad_closes == closes);
            CHECK(!pad.Poll());
            CHECK(state.pad_opens == opens + 1 && state.pad_closes == closes + 1);
            CHECK(pad.ConnectedPlayers() == 1u && pad.TakeConnectionChanges() == 0u);
        }
        pad.Close(); pad.Close();
        CHECK(state.pad_closes == state.pad_opens);
    }

    reset();
    {
        Eden::Pad pad;
        pad.SetMapping(Eden::Assign(Eden::kDefaultMapping, Eden::game_a, Eden::pad_circle));
        CHECK(pad.Open()); (void)pad.TakeConnectionChanges();
        state.read_result = -19;
        state.pad_open_result = -1;
        poll(pad, 250);
        CHECK(state.pad_opens == 2 && state.pad_closes == 1 && state.pad_reads[7] == 250);
        CHECK(pad.Open()); CHECK(state.pad_opens == 2); // Stale Open is still idempotent.
        pad.SetMapping(Eden::Assign(Eden::kDefaultMapping, Eden::game_a, Eden::pad_cross));
        for (unsigned delay : {500u, 1000u, 2000u, 4000u, 8000u, 8000u}) {
            const int opens = state.pad_opens;
            poll(pad, delay - 1);
            CHECK(state.pad_opens == opens);
            CHECK(!pad.Poll()); CHECK(state.pad_opens == opens + 1);
            CHECK(state.pad_closed_handles == std::vector<int>{7});
            CHECK(state.pad_reads[7] == 250); // Released handles are never read again.
            CHECK(pad.ConnectedPlayers() == 1u && pad.TakeConnectionChanges() == 0u);
        }
        // The fallback may supply a handle when Open still fails. No read on the retry tick.
        state.pad_get_handle_result = 35;
        const int opens = state.pad_opens;
        poll(pad, 7999); CHECK(state.pad_opens == opens);
        CHECK(!pad.Poll()); CHECK(state.pad_opens == opens + 1 && state.pad_reads[35] == 0);
        CHECK(state.vibration_mode_handles.back() == 35 && state.motion_handles.back() == 35);
        state.read_result = 0;
        state.pad_samples.push_back(pressed); CHECK(pad.Poll());
        CHECK(pad.Engine().GetButton({}, 0)); // Mapping remained locked while stale.
        CHECK(pad.Engine().SetVibration({.port = 0, .pad = 1}, {.low_amplitude = 1.0f}) ==
              Common::Input::DriverResult::Success);
        CHECK(pad.Poll());
        CHECK(state.vibration_handles.back() == 35 && state.vibrations.back().large_motor == 255);
        state.read_result = -1;
        state.pad_get_handle_result = -1;
        state.pad_open_result = 36;
        poll(pad, 249); CHECK(state.pad_opens == opens + 1);
        CHECK(!pad.Poll()); CHECK(state.pad_opens == opens + 2); // Backoff reset after success.
        pad.Close(); pad.Close();
        CHECK((state.pad_closed_handles == std::vector<int>{7, 35, 36}));
        for (const auto& [handle, reads] : state.pad_reads) { (void)reads; CHECK(handle >= 0); }
    }

    // A released handle number may already belong to a new user's controller. A stale
    // player remains reserved, is not duplicated by Rescan, and never closes that number.
    reset();
    {
        state.login_users = {84, 42};
        state.pad_open_results = {{84, 7}, {42, 8}};
        state.pad_read_results = {{8, -19}};
        Eden::Pad pad;
        CHECK(pad.Open()); CHECK(pad.TakeConnectionChanges() == 3u);
        state.pad_open_results[42] = -1;
        poll(pad, 250);
        CHECK(pad.ConnectedPlayers() == 3u && pad.TakeConnectionChanges() == 0u);
        CHECK(state.pad_closed_handles == std::vector<int>{8});
        state.login_users = {84, 42, 99};
        state.pad_open_results[99] = 8; // Reuse the released number, not the stale player's slot.
        state.pad_read_results[8] = 0;
        poll(pad, 250);
        CHECK(pad.ConnectedPlayers() == 7u && pad.TakeConnectionChanges() == 4u);
        CHECK(std::count(state.pad_open_users.begin(), state.pad_open_users.end(), 42) == 2);
        poll(pad, 250); // Stale retry fails; the new user's handle 8 must stay open.
        CHECK(state.pad_closed_handles == std::vector<int>{8});
        CHECK(std::count(state.pad_open_users.begin(), state.pad_open_users.end(), 42) == 3);
        CHECK(pad.Engine().SetVibration({.port = 2, .pad = 1}, {.low_amplitude = 1.0f}) ==
              Common::Input::DriverResult::Success);
        CHECK(pad.Poll()); CHECK(state.vibration_handles.back() == 8);
        state.login_users = {84, 99};
        poll(pad, 250);
        CHECK(pad.ConnectedPlayers() == 5u && pad.TakeConnectionChanges() == 2u);
        CHECK(state.pad_closed_handles == std::vector<int>{8}); // Stale logout closes nothing.
        poll(pad, 1250); // No old-user retries after logout, even past its next deadline.
        CHECK(std::count(state.pad_open_users.begin(), state.pad_open_users.end(), 42) == 3);
        state.login_users = {84, 99, 101}; state.pad_open_results[101] = 9;
        poll(pad, 250);
        CHECK(pad.ConnectedPlayers() == 7u && pad.TakeConnectionChanges() == 2u);
        CHECK(pad.Engine().SetVibration({.port = 1, .pad = 1}, {.low_amplitude = 1.0f}) ==
              Common::Input::DriverResult::Success);
        CHECK(pad.Poll()); CHECK(state.vibration_handles.back() == 9);
        pad.Close(); pad.Close();
        CHECK((state.pad_closed_handles == std::vector<int>{8, 7, 9, 8}));
    }

    // Primary recovery must not stop other users' login/logout scans. Full Close also
    // releases stale reservations, allowing the same Pad instance to open a new session.
    reset();
    {
        state.login_users = {84, 42}; state.pad_open_results = {{84, 7}, {42, 8}};
        Eden::Pad pad;
        CHECK(pad.Open()); (void)pad.TakeConnectionChanges();
        state.pad_read_results[7] = -19; state.pad_open_results[84] = -1;
        poll(pad, 250);
        state.login_users = {84};
        poll(pad, 250);
        CHECK(pad.ConnectedPlayers() == 1u && pad.TakeConnectionChanges() == 2u);
        pad.Close(); pad.Close();
        CHECK(pad.ConnectedPlayers() == 0u);
        CHECK((state.pad_closed_handles == std::vector<int>{7, 8}));
        CHECK(state.user_terminations == 1);
        state.pad_open_results[84] = 9; state.pad_read_results.clear();
        pad.SetMapping(Eden::Assign(Eden::kDefaultMapping, Eden::game_a, Eden::pad_cross));
        CHECK(pad.Open()); CHECK(state.pad_opens == 4 && state.last_pad_user == 84);
        auto cross = neutral_data(); cross.connected = 1; cross.buttons = kButtonCross;
        state.pad_samples.push_back(cross); CHECK(pad.Poll());
        CHECK(pad.Engine().GetButton({}, 0));
        pad.Close(); pad.Close();
        CHECK((state.pad_closed_handles == std::vector<int>{7, 8, 9}));
        CHECK(state.user_terminations == 2);
    }
    reset();
    std::puts("Pad recovery: errors/neutralization, recover250, backoff500/1000/cap8000, successful reset, stale ownership, handle reuse and cleanup PASS");
}

void CheckAudio() {
    Core::System system;
    Settings::values.volume = 100;
    auto selected_sink = CreateSinkFromID(Settings::AudioEngine::Null, "ps5");
    auto& sink = *selected_sink;
    Reject([&] { sink.AcquireSinkStream(system, 3, "invalid", StreamType::Out); });
    state.fail_audio_open = true;
    Reject([&] { sink.AcquireSinkStream(system, 2, "open failure", StreamType::Out); });
    state.fail_audio_open = false; state.fail_volume = true;
    Reject([&] { sink.AcquireSinkStream(system, 2, "volume failure", StreamType::Out); });
    state.fail_volume = false; CHECK(state.audio_closes == state.audio_opens);
    for (u32 channels : {1u, 2u, 6u}) {
        sink.SetDeviceVolume(0.5f); sink.SetSystemVolume(1);
        auto* stream = sink.AcquireSinkStream(system, channels, "PCM check", StreamType::Out);
        const int handle = 100 + state.audio_opens;
        CHECK(stream->IsPaused()); CHECK(stream->GetDeviceChannels() == 2);
        std::vector<s16> pcm(256 * channels, 1000);
        SinkBuffer buffer{256, 0, channels, false};
        SinkBuffer invalid{257, 0, 0, false};
        Reject([&] { stream->AppendBuffer(invalid, pcm); });
        stream->AppendBuffer(buffer, pcm);
        CHECK(stream->GetQueueSize() == 1);
        stream->Start();
        {
            std::unique_lock lock(state.mutex);
            CHECK(state.wake.wait_for(lock, 2s, [&] { return state.audio[handle].size() >= 2; }));
        }
        stream->Stop(); CHECK(stream->IsPaused()); CHECK(stream->GetQueueSize() == 0);
        {
            std::scoped_lock lock(state.mutex);
            const s16 expected = channels == 6 ? 1328 : 500;
            for (auto value : state.audio[handle][0]) CHECK(std::abs(value - expected) <= 1);
            // Existing Eden underrun policy repeats the last complete frame.
            CHECK(state.audio[handle][1] == state.audio[handle][0]);
        }
        pcm.assign(256 * channels, 2000); buffer.tag += 10;
        stream->AppendBuffer(buffer, pcm); CHECK(stream->GetQueueSize() == 1);
        std::size_t count;
        { std::scoped_lock lock(state.mutex); count = state.audio[handle].size(); }
        stream->Start(true);
        {
            std::unique_lock lock(state.mutex);
            CHECK(state.wake.wait_for(lock, 2s, [&] { return state.audio[handle].size() > count; }));
        }
        stream->Stop(); CHECK(stream->GetQueueSize() == 0);
        stream->Finalize(); stream->Finalize();
        Reject([&] { stream->AppendBuffer(buffer, pcm); });
        sink.CloseStream(stream);
        CHECK(state.audio_closes == state.audio_opens);
    }
    {
        auto* stream = sink.AcquireSinkStream(system, 2, "queue capacity", StreamType::Out);
        const int handle = 100 + state.audio_opens;
        sink.SetDeviceVolume(1);
        for (int i = 1; i <= 4; ++i) {
            std::vector<s16> pcm(512, i * 1000);
            SinkBuffer buffer{256, 0, static_cast<u64>(i), false};
            stream->AppendBuffer(buffer, pcm);
        }
        CHECK(stream->GetQueueSize() == 4); stream->Start();
        {
            std::unique_lock lock(state.mutex);
            CHECK(state.wake.wait_for(lock, 2s, [&] { return state.audio[handle].size() >= 4; }));
        }
        stream->Stop();
        for (int i = 0; i < 4; ++i)
            for (auto value : state.audio[handle][i]) CHECK(value == (i + 1) * 1000);
        stream->ClearQueue();
        std::vector<s16> full(65536, 1234); SinkBuffer large{32768, 0, 10, false};
        stream->AppendBuffer(large, full);
        std::array<s16, 2> extra{}; SinkBuffer one{1, 0, 11, false};
        Reject([&] { stream->AppendBuffer(one, extra); });
        CHECK(stream->GetQueueSize() == 1); stream->ClearQueue();
        CHECK(stream->GetQueueSize() == 0); sink.CloseStream(stream);
    }
    {
        auto* stream = sink.AcquireSinkStream(system, 2, "partial block tail", StreamType::Out);
        const int handle = 100 + state.audio_opens;
        std::vector<s16> pcm(257 * 2, 1000);
        pcm[512] = pcm[513] = 0;
        SinkBuffer buffer{257, 0, 98, false};
        stream->AppendBuffer(buffer, pcm); stream->Start();
        {
            std::unique_lock lock(state.mutex);
            CHECK(state.wake.wait_for(lock, 2s, [&] { return state.audio[handle].size() >= 3; }));
        }
        stream->Stop();
        for (auto value : state.audio[handle][0]) CHECK(value == 1000);
        for (int block : {1, 2})
            for (auto value : state.audio[handle][block]) CHECK(value == 0);
        sink.CloseStream(stream);
    }
    auto* stream = static_cast<Eden::AudioStream*>(sink.AcquireSinkStream(system, 2, "failure", StreamType::Out));
    { std::scoped_lock lock(state.mutex); state.fail_output = true; }
    stream->Start();
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (stream->Healthy() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    CHECK(!stream->Healthy()); stream->Stop();
    std::array<s16, 512> pcm{}; SinkBuffer buffer{256, 0, 99, false};
    Reject([&] { stream->AppendBuffer(buffer, pcm); });
    sink.CloseStreams(); state.fail_output = false;
    const int opened = state.audio_opens;
    sink.AcquireSinkStream(system, 2, "input null", StreamType::In);
    sink.CloseStreams(); CHECK(state.audio_opens == opened);
    CHECK(state.audio_opens == state.audio_closes); CHECK(state.audio_drains == 6);
    std::puts("Audio PCM mono/stereo/6-channel, volume, queue, underrun, pause/resume and failure cleanup PASS");
}
void CheckVoiceFlags() {
    VoiceInfo voice;
    VoiceInfo::InParameter parameter{};
    PoolMapper mapper(nullptr, false);
    BehaviorInfo behavior;
    BehaviorInfo::ErrorInfo error{};
    behavior.SetUserLibRevision(0x35564552);
    for (unsigned mask : {0, 1, 2, 3, 0}) {
        parameter.flags.IsVoicePlayedSampleCountResetAtLoopPointSupported = mask & 1;
        parameter.flags.IsVoicePitchAndSrcSkippedSupported = (mask >> 1) & 1;
        voice.UpdateParameters(error, parameter, mapper, behavior);
        CHECK(error.error_code.IsSuccess()); CHECK(voice.flags == mask);
    }
    behavior.SetUserLibRevision(0x34564552);
    parameter.flags = {1, 1};
    voice.UpdateParameters(error, parameter, mapper, behavior);
    CHECK(voice.flags == 0);
    std::vector<std::int16_t> pcm(960);
    for (unsigned i = 0; i < 480; ++i) {
        const auto sign = i % 96 < 48 ? 1 : -1;
        const auto second = i % 48 < 24 ? 1 : -1;
        pcm[i * 2] = sign * 512 + second * 128;
        pcm[i * 2 + 1] = sign * 256 + second * 256;
    }
    CHECK(Eden::Mock::RendererPcmMatches(pcm));
    CHECK(!Eden::Mock::RendererPcmMatches(pcm, true)); // High filter must not silently use Low.
    auto high = Eden::Mock::HighRendererReference();
    CHECK(Eden::Mock::RendererPcmMatches(high, true));
    CHECK(!Eden::Mock::RendererPcmMatches(high));
    high[100] += 1; CHECK(!Eden::Mock::RendererPcmMatches(high, true));
    high = Eden::Mock::HighRendererReference();
    for (unsigned i = 0; i < 480; ++i) std::swap(high[2*i], high[2*i+1]);
    CHECK(!Eden::Mock::RendererPcmMatches(high, true));
    high.fill(0); CHECK(!Eden::Mock::RendererPcmMatches(high, true));
    const auto valid = pcm;
    for (unsigned i = 0; i < 480; ++i) std::swap(pcm[i * 2], pcm[i * 2 + 1]);
    CHECK(!Eden::Mock::RendererPcmMatches(pcm));
    pcm = valid; pcm[100] = 0; CHECK(!Eden::Mock::RendererPcmMatches(pcm));
    for (unsigned missing = 0; missing < 2; ++missing) {
        for (unsigned i = 0; i < 480; ++i) {
            const auto sign = i % (missing ? 96 : 48) < (missing ? 48 : 24) ? 1 : -1;
            pcm[i * 2] = sign * (missing ? 512 : 128);
            pcm[i * 2 + 1] = sign * 256;
        }
        CHECK(!Eden::Mock::RendererPcmMatches(pcm));
    }
    for (unsigned i = 0; i < 480; ++i) {
        const auto first = i % 48 < 24 ? 1 : -1;
        const auto second = i % 24 < 12 ? 1 : -1;
        pcm[i * 2] = first * 512 + second * 128;
        pcm[i * 2 + 1] = first * 256 + second * 256;
    }
    CHECK(!Eden::Mock::RendererPcmMatches(pcm)); // Bypassing SRC doubles both frequencies.
    pcm.assign(960, 0); CHECK(!Eden::Mock::RendererPcmMatches(pcm));
    CHECK(!Eden::Mock::RendererPcmMatches({}));
    std::puts("Two-voice PCM window rejects missing voices, silence, swapped gains and corruption PASS");
    std::puts("Renderer voice flags remain independent and revision-gated PASS");
}
void CheckResampling() {
    using Fixed = Common::FixedPoint<49, 15>;
    constexpr unsigned count = 2400, split = 137;
    constexpr s32 sentinel = 123456789;
    for (auto quality : {AudioCore::SrcQuality::Low, AudioCore::SrcQuality::Medium,
                         AudioCore::SrcQuality::High}) {
        for (unsigned rate : {24000, 32000, 44100, 48000, 57600, 96000}) {
            const std::int64_t step = std::int64_t(rate) * 32768 / 48000;
            const auto ratio = Fixed::from_base(step);
            for (bool tone : {false, true}) {
                std::vector<s16> input(8192);
                if (tone) for (unsigned i = 0; i < input.size(); ++i)
                    input[i] = static_cast<s16>(std::lround(10000 * std::sin(
                        2 * std::numbers::pi * 500 * i / rate)));
                std::vector<s32> whole(count + 2, sentinel), chunks(count + 2, sentinel);
                auto fraction = Fixed::from_base(8192); // Start at one quarter sample.
                Resample(std::span{whole}.subspan(1, count), input, ratio, fraction, count, quality);
                CHECK(whole.front() == sentinel && whole.back() == sentinel);
                CHECK(fraction.get_frac() == (8192 + step * count) % 32768);
                auto partial = Fixed::from_base(8192);
                Resample(std::span{chunks}.subspan(1, split), input, ratio, partial, split, quality);
                const auto consumed = (8192 + step * split) / 32768;
                Resample(std::span{chunks}.subspan(1 + split, count - split),
                         std::span<const s16>{input}.subspan(consumed), ratio, partial,
                         count - split, quality);
                CHECK(chunks == whole && partial == fraction);
                // An empty output request must preserve phase and surrounding samples.
                Resample(std::span{chunks}.subspan(1, 0), input, ratio, partial, 0, quality);
                CHECK(chunks == whole && partial == fraction);
                if (!tone) {
                    CHECK(std::all_of(whole.begin() + 1, whole.end() - 1,
                                      [](auto value) { return value == 0; }));
                    continue;
                }
                if (quality == AudioCore::SrcQuality::Low) {
                    for (unsigned i = 0; i < count; ++i)
                        CHECK(whole[i + 1] == input[(8192 + step * i + 16384) / 32768]);
                }
                unsigned crossings = 0;
                double energy = 0;
                for (unsigned i = 1; i <= count; ++i) {
                    energy += double(whole[i]) * whole[i];
                    if (i > 1 && (whole[i] >= 0) != (whole[i - 1] >= 0)) ++crossings;
                }
                // 50 ms of a 500 Hz tone: approximately 25 cycles, unity RMS gain.
                CHECK(crossings >= 49 && crossings <= 51);
                CHECK(energy / count >= 45000000 && energy / count <= 55000000);
            }
        }
    }
    std::puts("SRC 3 qualities x 6 rates: silence, 500 Hz level/frequency, exact low-quality samples, split continuity and phase PASS");
}
int main() {
    Common::FS::CreateEdenPaths(); Common::Log::Initialize(); Common::Log::Start();
    int status = 0;
    try { CheckResampling(); CheckVoiceFlags(); CheckControllerStyle(); CheckPad(); CheckPadRecovery(); CheckAudio(); }
    catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); status = 1; }
    Common::Log::Stop();
    return status;
}
