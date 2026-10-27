// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic USB/HID fixtures, NOT captures from hardware. No game data.
#include "pro_controller.h"
#include "ps5_pad.hpp"
#include "button_mapping.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

using Packet = std::array<std::uint8_t, 64>;
static Packet Full() {
    Packet p{};
    p[0] = 0x30;
    p[7] = p[10] = 8; p[8] = p[11] = 128; // 2048,2048
    return p;
}
static void Pair(std::uint8_t* p, unsigned x, unsigned y) {
    p[0] = static_cast<std::uint8_t>(x);
    p[1] = static_cast<std::uint8_t>((x >> 8) | (y << 4));
    p[2] = static_cast<std::uint8_t>(y >> 4);
}
static void Word(std::uint8_t* p, int n) {
    p[0] = static_cast<std::uint8_t>(n); p[1] = static_cast<std::uint8_t>(static_cast<unsigned>(n) >> 8);
}
static std::array<std::uint8_t, 18> Sticks() {
    std::array<std::uint8_t, 18> p{};
    Pair(p.data(), 1500, 1400); Pair(p.data() + 3, 2100, 2200); Pair(p.data() + 6, 1600, 1700);
    Pair(p.data() + 9, 2000, 1900); Pair(p.data() + 12, 1400, 1300); Pair(p.data() + 15, 1500, 1600);
    return p;
}
static std::array<std::uint8_t, 24> Imu() {
    std::array<std::uint8_t, 24> p{};
    for (unsigned i = 0; i < 3; ++i) { Word(p.data() + 6 + 2 * i, 16384); Word(p.data() + 18 + 2 * i, 13371); }
    return p;
}
static ps5::pad::Data Bridge(const pe_pro_state& state) {
    ps5::pad::Data data;
    pe_pro_pad_sample(&state, reinterpret_cast<std::uint8_t*>(&data));
    return data;
}

static void Parser() {
    pe_pro_calibration cal;
    pe_pro_calibration_reset(&cal);
    pe_pro_state state{};
    auto p = Full();
    assert(pe_pro_decode(&cal, p.data(), 49, 1234, &state));
    assert(state.connected && !state.motion && state.buttons == 0 && state.timestamp_us == 1234);
    assert(std::all_of(std::begin(state.sticks), std::end(state.sticks), [](auto n) { return n == 128; }));
    const pe_pro_state before = state;
    for (std::size_t size = 0; size < 49; ++size) {
        assert(!pe_pro_decode(&cal, p.data(), size, 0, &state));
        assert(std::memcmp(&state, &before, sizeof(state)) == 0);
    }
    for (unsigned id = 0; id < 256; ++id) {
        if (id == 0x30 || id == 0x3f) continue;
        p[0] = static_cast<std::uint8_t>(id);
        assert(!pe_pro_decode(&cal, p.data(), 64, 0, &state));
    }
    p = Full();
    assert(!pe_pro_decode(&cal, p.data(), 65, 0, &state));
    assert(!pe_pro_decode(&cal, nullptr, 49, 0, &state));
    for (unsigned bit = 0; bit < 4; ++bit) {
        p = Full(); p[3] = static_cast<std::uint8_t>(1 << bit);
        assert(pe_pro_decode(&cal, p.data(), 64, 0, &state));
        const unsigned masks[] = {ps5::pad::kButtonSquare, ps5::pad::kButtonTriangle,
                                 ps5::pad::kButtonCross, ps5::pad::kButtonCircle};
        assert(state.buttons == masks[bit]);
    }
    // Default guest binding really preserves A/B/X/Y; not the already-existing guest type.
    assert(Eden::kDefaultMapping[Eden::game_a] == Eden::pad_circle);
    assert(Eden::kDefaultMapping[Eden::game_x] == Eden::pad_triangle);
    p = Full(); p[3] = 0xc0; p[4] = 0x2f; p[5] = 0xc0;
    assert(pe_pro_decode(&cal, p.data(), 49, 9999, &state));
    assert(state.triggers[0] == 255 && state.triggers[1] == 255);
    assert((state.buttons & (ps5::pad::kButtonCreate | ps5::pad::kButtonOptions |
                           ps5::pad::kButtonTouchPad | ps5::pad::kButtonL3 | ps5::pad::kButtonR3)) == 0x10000f);
    auto data = Bridge(state);
    assert(data.buttons == state.buttons && data.triggers.l2 == 255 && data.connected == 1);
    assert(data.timestamp_us == 0 && data.orientation.w == 1); // no invented motion
    p = Full(); Pair(p.data() + 6, 0, 4095); Pair(p.data() + 9, 4095, 0);
    assert(pe_pro_decode(&cal, p.data(), 49, 0, &state));
    assert(state.sticks[0] == 0 && state.sticks[1] == 0 && state.sticks[2] == 255 && state.sticks[3] == 255);
    p[5] = 0x0f;
    assert(pe_pro_decode(&cal, p.data(), 49, 0, &state) && (state.buttons & 0xf0) == 0);
    Packet simple{}; simple[0] = 0x3f; simple[3] = 8;
    for (unsigned i = 0; i < 4; ++i) Word(simple.data() + 4 + i * 2, 32768);
    for (unsigned n = 0; n < 12; ++n) assert(!pe_pro_decode(&cal, simple.data(), n, 0, &state));
    assert(pe_pro_decode(&cal, simple.data(), 12, 0, &state) && state.sticks[0] == 128);
    for (unsigned hat = 0; hat <= 8; ++hat) {
        simple[3] = static_cast<std::uint8_t>(hat);
        assert(pe_pro_decode(&cal, simple.data(), 12, 0, &state));
        assert((state.buttons == 0) == (hat == 8));
    }
    simple[3] = 9;
    assert(!pe_pro_decode(&cal, simple.data(), 12, 0, &state));
    std::uint8_t neutral[120]; pe_pro_pad_sample(nullptr, neutral);
    std::memcpy(&data, neutral, sizeof(data));
    assert(!ps5::pad::is_usable(data) && data.buttons == 0 && data.left_stick.x == 128);
}
static void Calibration() {
    pe_pro_calibration cal;
    pe_pro_calibration_reset(&cal);
    const auto sticks = Sticks();
    assert(pe_pro_stick_calibration(&cal, sticks.data(), sticks.size(), false));
    assert(cal.sticks_calibrated && cal.axes[0].center == 2100 && cal.axes[3].center == 1900);
    auto p = Full(); Pair(p.data() + 6, 2100, 2200); Pair(p.data() + 9, 2000, 1900);
    pe_pro_state state;
    assert(pe_pro_decode(&cal, p.data(), 49, 0, &state));
    for (auto axis : state.sticks) assert(axis == 128);
    Pair(p.data() + 6, 500, 3600); Pair(p.data() + 9, 3500, 600);
    assert(pe_pro_decode(&cal, p.data(), 49, 0, &state));
    assert(state.sticks[0] == 0 && state.sticks[1] == 0 && state.sticks[2] == 255 && state.sticks[3] == 255);
    const auto before = cal;
    auto invalid = sticks; invalid.fill(0xff);
    assert(!pe_pro_stick_calibration(&cal, invalid.data(), 18, false));
    assert(!pe_pro_stick_calibration(&cal, sticks.data(), 17, false));
    assert(std::memcmp(&cal, &before, sizeof(cal)) == 0);
    std::array<std::uint8_t, 22> user{};
    std::copy_n(sticks.begin(), 9, user.begin() + 2);
    std::copy_n(sticks.begin() + 9, 9, user.begin() + 13);
    assert(!pe_pro_stick_calibration(&cal, user.data(), 22, true));
    user[0] = user[11] = 0xb2; user[1] = user[12] = 0xa1;
    Pair(user.data() + 5, 2150, 2250);
    assert(pe_pro_stick_calibration(&cal, user.data(), 22, true));
    assert(cal.axes[0].center == 2150);
    const auto imu = Imu();
    assert(pe_pro_imu_calibration(&cal, imu.data(), 24));
    assert(!pe_pro_imu_calibration(&cal, imu.data(), 23));
    std::array<std::uint8_t, 24> zero{};
    assert(!pe_pro_imu_calibration(&cal, zero.data(), 24));
    p = Full(); Word(p.data() + 41, 4096); Word(p.data() + 45, 1000);
    assert(pe_pro_decode(&cal, p.data(), 49, 5000, &state) && state.motion);
    assert(std::abs(state.acceleration[1] - 1.0f) < 0.00001f);
    assert(state.gyro[0] < -1.2f && state.gyro[0] > -1.3f);
    const auto data = Bridge(state);
    assert(data.timestamp_us == 5000 && data.acceleration.y == state.acceleration[1]);
    assert(data.angular_velocity.x == state.gyro[0]);
}
static void Descriptors() {
    // One HID interface, interrupt IN 0x81 and OUT 0x02, 64 byte packets.
    std::array<std::uint8_t, 32> config{9,2,32,0,1,1,0,0x80,50,
        9,4,0,0,2,3,0,0,0, 7,5,0x81,3,64,0,8, 7,5,0x02,3,64,0,8};
    pe_pro_endpoints endpoints{};
    assert(pe_pro_find_endpoints(config.data(), config.size(), &endpoints));
    assert(endpoints.in == 0x81 && endpoints.out == 2);
    for (unsigned n = 0; n < config.size(); ++n) assert(!pe_pro_find_endpoints(config.data(), n, &endpoints));
    auto bad = config; bad[18] = 0; assert(!pe_pro_find_endpoints(bad.data(), bad.size(), &endpoints));
    bad = config; bad[14] = 0xe0; assert(!pe_pro_find_endpoints(bad.data(), bad.size(), &endpoints)); // Bluetooth forbidden
    bad = config; bad[22] = 63; assert(!pe_pro_find_endpoints(bad.data(), bad.size(), &endpoints));
    bad = config; bad[27] = 0x82; assert(!pe_pro_find_endpoints(bad.data(), bad.size(), &endpoints));
    bad = config; bad[12] = 1; assert(!pe_pro_find_endpoints(bad.data(), bad.size(), &endpoints));
}
struct Fake {
    std::deque<std::vector<std::uint8_t>> reads;
    std::vector<Packet> writes;
    unsigned opens = 0, closes = 0;
    bool present = true, disconnected = false, reply = true, calibrated = true;
    bool wrong_spi_address = false, reject_feedback = false, busy = false;
    static int Open(void* context) { auto& f = *static_cast<Fake*>(context); ++f.opens; return f.present ? 1 : 0; }
    static int Read(void* context, std::uint8_t* p) {
        auto& f = *static_cast<Fake*>(context);
        if (f.disconnected) { f.disconnected = false; return -1; }
        if (f.reads.empty()) return 0;
        const auto next = f.reads.front(); f.reads.pop_front();
        std::copy_n(next.begin(), std::min<std::size_t>(next.size(), 64), p);
        return static_cast<int>(next.size());
    }
    static int Write(void* context, const std::uint8_t* p) {
        auto& f = *static_cast<Fake*>(context);
        if (f.busy) return 0;
        Packet sent; std::copy_n(p, 64, sent.begin()); f.writes.push_back(sent);
        if (!f.reply || p[0] == 0x10) return 1;
        Packet ack{};
        if (p[0] == 0x80) {
            if (p[1] == 4) return 1;
            ack[0] = 0x81; ack[1] = p[1]; ack[3] = 3;
        } else {
            ack[0] = 0x21; ack[13] = 0x80; ack[14] = p[10];
            if (p[10] == 2) ack[17] = 3;
            if (p[10] == 0x10) {
                std::copy_n(p + 11, 5, ack.begin() + 15);
                std::fill(ack.begin() + 20, ack.end(), 0xff);
                if (f.calibrated && p[11] == 0x3d) { auto c = Sticks(); std::copy(c.begin(), c.end(), ack.begin() + 20); }
                if (f.calibrated && p[11] == 0x20) { auto c = Imu(); std::copy(c.begin(), c.end(), ack.begin() + 20); }
                if (f.wrong_spi_address) ++ack[15];
            }
            if (f.reject_feedback && (p[10] == 0x40 || p[10] == 0x48)) ack[13] = 0;
        }
        f.reads.emplace_back(ack.begin(), ack.end());
        if (p[0] == 1 && p[10] == 3) { auto full = Full(); f.reads.emplace_back(full.begin(), full.end()); }
        return 1;
    }
    static void Close(void* context) {
        auto& f = *static_cast<Fake*>(context); ++f.closes; f.reads.clear();
    }
    pe_pro_transport Transport() { return {this, Open, Read, Write, Close}; }
};
static std::uint64_t Ready(pe_pro_session& s, std::uint64_t start = 0) {
    for (std::uint64_t now = start; now < start + 6000; now += 10) {
        pe_pro_session_poll(&s, now);
        if (s.state.connected) return now;
    }
    assert(false && "synthetic controller never became ready"); return 0;
}
static void Session() {
    Fake f; pe_pro_session s; pe_pro_session_init(&s, f.Transport());
    auto now = Ready(s);
    assert(s.ready && s.state.connected && s.state.vibration && s.imu_enabled && s.rumble_enabled && s.cal.sticks_calibrated);
    assert(f.opens == 1 && f.writes[0][0] == 0x80 && f.writes[0][1] == 1);
    pe_pro_session_rumble(&s, 255, 0);
    auto full = Full(); full[3] = 8;
    f.reads.emplace_back(full.begin(), full.end());
    now += 60; pe_pro_session_poll(&s, now);
    assert(s.state.buttons == ps5::pad::kButtonCircle && f.writes.back()[0] == 0x10);
    assert(f.writes.back()[3] == 0x65 && f.writes.back()[7] == 0x65);
    const auto outputs = f.writes.size();
    pe_pro_session_poll(&s, now + 1); assert(f.writes.size() == outputs);
    f.reads.push_back({0x30, 0xff}); // malformed must not extend liveness or overwrite held state
    pe_pro_session_poll(&s, now + 10); assert(s.state.buttons == ps5::pad::kButtonCircle);
    pe_pro_session_poll(&s, now + 1000);
    assert(!s.state.connected && s.state.buttons == 0 && f.closes == 1 && s.state.sticks[0] == 128);
    const auto retry = s.retry_at;
    pe_pro_session_poll(&s, retry - 1); assert(f.opens == 1);
    now = Ready(s, retry); assert(f.opens == 2 && s.state.buttons == 0);
    f.disconnected = true; pe_pro_session_poll(&s, now + 10);
    assert(!s.state.connected && !s.imu_enabled && !s.rumble_enabled);
    now = Ready(s, s.retry_at);
    f.reads.emplace_back(65, 0);
    pe_pro_session_poll(&s, now + 10);
    assert(!s.state.connected); // broken transport length cannot overrun the decoder
    now = Ready(s, s.retry_at);
    pe_pro_session_stop(&s);
    assert(!s.opened && !s.state.connected && f.writes.back()[0] == 0x10 && f.writes.back()[3] == 1);
    Fake absent; absent.present = false; pe_pro_session_init(&s, absent.Transport());
    pe_pro_session_poll(&s, 0); assert(absent.opens == 1 && s.retry_at == 1000);
    for (unsigned i = 0; i < 10; ++i) {
        const auto due = s.retry_at; pe_pro_session_poll(&s, due - 1); const auto opens = absent.opens;
        pe_pro_session_poll(&s, due); assert(absent.opens == opens + 1 && s.retry_ms <= 30000);
    }
    Fake timeout; timeout.reply = false; pe_pro_session_init(&s, timeout.Transport());
    for (std::uint64_t t = 0; t < 500; t += 10) pe_pro_session_poll(&s, t);
    assert(timeout.writes.size() == 3 && timeout.closes == 1 && !s.ready);
    Fake busy; busy.busy = true; pe_pro_session_init(&s, busy.Transport());
    pe_pro_session_poll(&s, 0); pe_pro_session_poll(&s, 5999);
    assert(s.opened && busy.writes.empty());
    pe_pro_session_poll(&s, 6000); assert(!s.opened && busy.closes == 1);
    Fake basic; basic.calibrated = false; basic.reject_feedback = true;
    pe_pro_session_init(&s, basic.Transport()); now = Ready(s);
    assert(!s.cal.imu_calibrated && !s.imu_enabled && !s.rumble_enabled && !s.state.motion && !s.state.vibration);
    pe_pro_session_rumble(&s, 255, 255); const auto count = basic.writes.size();
    pe_pro_session_poll(&s, now + 100); assert(basic.writes.size() == count);
    Fake bad_spi; bad_spi.wrong_spi_address = true;
    pe_pro_session_init(&s, bad_spi.Transport()); Ready(s);
    assert(!s.cal.sticks_calibrated && !s.cal.imu_calibrated); // mismatched ACK cannot calibrate
}
static void Fuzz() {
    pe_pro_calibration cal; pe_pro_calibration_reset(&cal);
    std::uint32_t rng = 0x202;
    for (unsigned i = 0; i < 100000; ++i) {
        Packet p;
        for (auto& b : p) { rng = rng * 1664525u + 1013904223u; b = static_cast<std::uint8_t>(rng >> 24); }
        const auto size = rng % 65;
        pe_pro_state state{};
        if (pe_pro_decode(&cal, p.data(), size, i, &state)) {
            assert(state.connected && !state.motion);
            auto data = Bridge(state); assert(ps5::pad::is_usable(data));
        }
        pe_pro_endpoints endpoints;
        (void)pe_pro_find_endpoints(p.data(), size, &endpoints);
        (void)pe_pro_stick_calibration(&cal, p.data(), size, false);
        (void)pe_pro_stick_calibration(&cal, p.data(), size, true);
        (void)pe_pro_imu_calibration(&cal, p.data(), size);
    }
}
int main() {
    Parser(); Calibration(); Descriptors(); Session(); Fuzz();
    assert(!pe_pro_usb_start()); pe_pro_state state; assert(!pe_pro_usb_poll(&state)); pe_pro_usb_stop();
    std::puts("Pro Controller synthetic reports/session/bridge PASS (100000 bounded random packets)");
}
