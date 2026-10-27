// SPDX-License-Identifier: GPL-3.0-or-later
// Independently implemented from public protocol facts. References/licenses:
// docs/PRO_CONTROLLER.md. No MissionControl or vendor driver code is imported.
#include "pro_controller.h"
#include <math.h>
#include <string.h>

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static int signed16(const uint8_t *p) {
    const unsigned n = le16(p);
    return n >= 32768 ? (int)n - 65536 : (int)n;
}
static unsigned packed(const uint8_t *p, unsigned axis) {
    return axis ? (unsigned)(p[1] >> 4 | (unsigned)p[2] << 4) :
                  (unsigned)(p[0] | (p[1] & 15u) << 8);
}

void pe_pro_calibration_reset(pe_pro_calibration *cal) {
    memset(cal, 0, sizeof(*cal));
    /* Conservative nominal travel; never learn the centre from a held stick. */
    for (unsigned i = 0; i < 4; ++i) cal->axes[i] = (pe_pro_axis){2048, 1600, 1600};
}

static bool stick(const uint8_t *p, bool right, pe_pro_axis out[2]) {
    for (unsigned i = 0; i < 2; ++i) {
        const unsigned center = packed(p + (right ? 0 : 3), i);
        const unsigned below = packed(p + (right ? 3 : 6), i);
        const unsigned above = packed(p + (right ? 6 : 0), i);
        if (center < 512 || center > 3583 || below < 256 || above < 256 ||
            below > center || above > 4095 - center) return false;
        out[i] = (pe_pro_axis){(uint16_t)center, (uint16_t)below, (uint16_t)above};
    }
    return true;
}

bool pe_pro_stick_calibration(pe_pro_calibration *cal, const uint8_t *p,
                             size_t size, bool user) {
    if (!p || size != (user ? 22u : 18u)) return false;
    pe_pro_axis axes[4];
    memcpy(axes, cal->axes, sizeof(axes));
    bool accepted = false;
    for (unsigned side = 0; side < 2; ++side) {
        const uint8_t *data = p + side * (user ? 11 : 9);
        if (user) {
            if (data[0] != 0xb2 || data[1] != 0xa1) continue;
            data += 2;
        }
        if (!stick(data, side != 0, axes + side * 2)) return false;
        accepted = true;
    }
    if (!accepted) return false;
    memcpy(cal->axes, axes, sizeof(axes));
    cal->sticks_calibrated = true;
    return true;
}

bool pe_pro_imu_calibration(pe_pro_calibration *cal, const uint8_t *p, size_t size) {
    if (!p || size != 24) return false;
    pe_pro_calibration next = *cal;
    for (unsigned i = 0; i < 3; ++i) {
        const int ao = signed16(p + i * 2), as = signed16(p + 6 + i * 2);
        const int go = signed16(p + 12 + i * 2), gs = signed16(p + 18 + i * 2);
        const int ad = as - ao, gd = gs - go;
        /* Reject erased flash, tiny/negative denominators, implausible origins. */
        if (ao < -4096 || ao > 4096 || go < -4096 || go > 4096 ||
            ad < 8192 || ad > 24576 || gd < 8192 || gd > 24576) return false;
        next.accel_origin[i] = (int16_t)ao;
        next.gyro_origin[i] = (int16_t)go;
        next.accel_scale[i] = 4.0f / (float)ad;
        next.gyro_scale[i] = (936.0f * 0.017453292519943295f) / (float)gd;
    }
    next.imu_calibrated = true;
    *cal = next;
    return true;
}

static uint8_t axis(unsigned raw, pe_pro_axis cal, bool invert) {
    if (cal.center > 4095 || cal.below == 0 || cal.above == 0)
        cal = (pe_pro_axis){2048, 1600, 1600};
    int delta = (int)raw - cal.center;
    float value = (float)delta / (float)(delta < 0 ? cal.below : cal.above);
    if (invert) value = -value;
    if (value < -1) value = -1;
    if (value > 1) value = 1;
    return (uint8_t)lroundf(128 + value * (value < 0 ? 128 : 127));
}

bool pe_pro_decode(const pe_pro_calibration *cal, const uint8_t *p, size_t size,
                   uint64_t timestamp_us, pe_pro_state *out) {
    if (!cal || !p || !out || size > 64 || size == 0) return false;
    if ((p[0] != 0x30 || size < 49) && (p[0] != 0x3f || size < 12)) return false;
    pe_pro_state next = {.sticks = {128, 128, 128, 128}, .connected = true,
                         .timestamp_us = timestamp_us};
    uint32_t bits = 0;
    if (p[0] == 0x30) {
        /* A/B/X/Y preserve Nintendo labels through kDefaultMapping. */
        static const uint32_t right[8] = {0x8000, 0x1000, 0x4000, 0x2000, 0, 0, 0x800, 0x200};
        static const uint32_t shared[8] = {1, 8, 4, 2, 0, 0x100000, 0, 0};
        static const uint32_t left[8] = {0x40, 0x10, 0x20, 0x80, 0, 0, 0x400, 0x100};
        for (unsigned i = 0; i < 8; ++i) {
            if (p[3] & (1u << i)) bits |= right[i];
            if (p[4] & (1u << i)) bits |= shared[i];
            if (p[5] & (1u << i)) bits |= left[i];
        }
        for (unsigned i = 0; i < 4; ++i)
            next.sticks[i] = axis(packed(p + (i < 2 ? 6 : 9), i % 2), cal->axes[i], i % 2 != 0);
        /* Last of three 5ms IMU samples. Do not fabricate motion without valid
         * factory calibration; the session also requires the enable ACK. */
        if (cal->imu_calibrated) {
            float a[3], g[3];
            for (unsigned i = 0; i < 3; ++i) {
                a[i] = (signed16(p + 37 + i * 2) - cal->accel_origin[i]) * cal->accel_scale[i];
                g[i] = (signed16(p + 43 + i * 2) - cal->gyro_origin[i]) * cal->gyro_scale[i];
            }
            /* Nintendo -> SDL -> existing DualSense bridge axes. */
            next.acceleration[0] = -a[1]; next.acceleration[1] = a[2]; next.acceleration[2] = -a[0];
            next.gyro[0] = -g[1]; next.gyro[1] = g[2]; next.gyro[2] = -g[0];
            next.motion = true;
        }
    } else {
        if (p[3] > 8) return false;
        static const uint32_t buttons[8] = {0x4000, 0x2000, 0x8000, 0x1000, 0x400, 0x800, 0x100, 0x200};
        static const uint32_t shared[6] = {1, 8, 2, 4, 0, 0x100000};
        static const uint32_t hat[9] = {0x10, 0x30, 0x20, 0x60, 0x40, 0xc0, 0x80, 0x90, 0};
        for (unsigned i = 0; i < 8; ++i) if (p[1] & (1u << i)) bits |= buttons[i];
        for (unsigned i = 0; i < 6; ++i) if (p[2] & (1u << i)) bits |= shared[i];
        bits |= hat[p[3]];
        for (unsigned i = 0; i < 4; ++i) next.sticks[i] = (uint8_t)(le16(p + 4 + i * 2) >> 8);
    }
    /* Opposing directions cannot become contradictory guest inputs. */
    if ((bits & 0x50) == 0x50) bits &= ~0x50u;
    if ((bits & 0xa0) == 0xa0) bits &= ~0xa0u;
    next.buttons = bits;
    next.triggers[0] = bits & 0x100 ? 255 : 0;
    next.triggers[1] = bits & 0x200 ? 255 : 0;
    *out = next;
    return true;
}

void pe_pro_pad_sample(const pe_pro_state *state, uint8_t out[120]) {
    memset(out, 0, 120);
    out[4] = out[5] = out[6] = out[7] = 128;
    const float one = 1;
    memcpy(out + 24, &one, 4); /* orientation.w */
    if (!state || !state->connected) return;
    memcpy(out, &state->buttons, 4);
    memcpy(out + 4, state->sticks, 4);
    memcpy(out + 8, state->triggers, 2);
    if (state->motion) {
        memcpy(out + 28, state->acceleration, 12);
        memcpy(out + 40, state->gyro, 12);
        memcpy(out + 80, &state->timestamp_us, 8);
    }
    const int32_t connected = 1;
    memcpy(out + 76, &connected, 4);
}

bool pe_pro_find_endpoints(const uint8_t *p, size_t size, pe_pro_endpoints *out) {
    if (!p || !out || size < 9 || size > 4096 || p[0] != 9 || p[1] != 2 || le16(p + 2) != size) return false;
    pe_pro_endpoints candidate = {0}, found = {0};
    bool hid = false;
    for (size_t offset = 9; offset < size;) {
        if (size - offset < 2 || p[offset] < 2 || p[offset] > size - offset) return false;
        const uint8_t *d = p + offset;
        if (d[1] == 4) {
            if (d[0] < 9) return false;
            if (candidate.in && candidate.out) {
                if (found.in) return false; /* ambiguous composite device */
                found = candidate;
            }
            candidate = (pe_pro_endpoints){0};
            hid = d[3] == 0 && d[5] == 3 && d[6] == 0 && d[7] == 0;
        } else if (d[1] == 5 && hid) {
            if (d[0] < 7) return false;
            const unsigned max_packet = le16(d + 4);
            if ((d[3] & 3) != 3 || !(d[2] & 15) || (d[2] & 0x70) || max_packet != 64) return false;
            uint8_t *endpoint = d[2] & 0x80 ? &candidate.in : &candidate.out;
            if (*endpoint) return false;
            *endpoint = d[2];
        }
        offset += d[0];
    }
    if (candidate.in && candidate.out) {
        if (found.in) return false;
        found = candidate;
    }
    if (!found.in || !found.out) return false;
    *out = found;
    return true;
}

/* Small, finite USB setup sequence. Optional SPI/IMU/vibration failures degrade
 * capabilities, never block buttons indefinitely. No SPI writes or pairing. */
enum { STATUS, HANDSHAKE, SPEED, HANDSHAKE2, FORCE_USB, DEVICE_INFO,
       FACTORY_STICKS, USER_STICKS, FACTORY_IMU, ENABLE_IMU, ENABLE_RUMBLE, INPUT_MODE, RUNNING };
static const uint8_t commands[] = {1, 2, 3, 2, 4, 2, 0x10, 0x10, 0x10, 0x40, 0x48, 3};
static const uint32_t addresses[] = {0x603d, 0x8010, 0x6020};
static const uint8_t lengths[] = {18, 22, 24};
static const uint8_t rumble[4][4] = {
    {0x00, 0x01, 0x40, 0x40}, /* stop: 160/320Hz carrier, no amplitude */
    {0x00, 0x21, 0x40, 0x48}, /* conservative ~0.12 amplitude */
    {0x00, 0x47, 0xc0, 0x51}, /* ~0.25 */
    {0x00, 0x65, 0x40, 0x59}, /* ~0.40; never overdrive clones */
};
static void neutral(pe_pro_session *s) {
    memset(&s->state, 0, sizeof(s->state));
    memset(s->state.sticks, 128, sizeof(s->state.sticks));
}
static void lost(pe_pro_session *s, uint64_t now) {
    if (s->opened) s->io.close(s->io.context);
    s->opened = s->ready = s->waiting = s->imu_enabled = s->rumble_enabled = false;
    s->rumble_level = 0;
    neutral(s);
    s->retry_at = now + s->retry_ms;
    if (s->retry_ms < 30000) s->retry_ms = s->retry_ms > 15000 ? 30000 : s->retry_ms * 2;
}
void pe_pro_session_init(pe_pro_session *s, pe_pro_transport io) {
    memset(s, 0, sizeof(*s));
    s->io = io;
    s->retry_ms = 1000;
    pe_pro_calibration_reset(&s->cal);
    neutral(s);
}
static void output_header(pe_pro_session *s, uint8_t p[64], uint8_t id, unsigned level) {
    memset(p, 0, 64);
    p[0] = id;
    p[1] = s->counter++ & 15;
    memcpy(p + 2, rumble[level], 4);
    memcpy(p + 6, rumble[level], 4);
}
static bool setup_packet(pe_pro_session *s, uint8_t p[64]) {
    if (s->step >= RUNNING) return false;
    output_header(s, p, 1, 0);
    if (s->step <= FORCE_USB) {
        memset(p, 0, 64);
        p[0] = 0x80; p[1] = commands[s->step];
    } else {
        p[10] = commands[s->step];
        if (s->step >= FACTORY_STICKS && s->step <= FACTORY_IMU) {
            const unsigned index = s->step - FACTORY_STICKS;
            const uint32_t address = addresses[index];
            for (unsigned i = 0; i < 4; ++i) p[11 + i] = (uint8_t)(address >> (8 * i));
            p[15] = lengths[index];
        } else if (s->step == ENABLE_IMU || s->step == ENABLE_RUMBLE) p[11] = 1;
        else if (s->step == INPUT_MODE) p[11] = 0x30;
    }
    return true;
}
static void advance(pe_pro_session *s, uint64_t now) {
    ++s->step;
    s->waiting = false;
    s->attempts = 0;
    if (s->step == ENABLE_IMU && !s->cal.imu_calibrated) ++s->step;
    if (s->step == RUNNING) {
        s->ready = true;
        s->last_input = now;
    }
}
static void reply(pe_pro_session *s, const uint8_t *p, size_t size, uint64_t now) {
    if (!s->waiting || s->step >= RUNNING) return;
    if (s->step <= FORCE_USB) {
        if (size < 2 || p[0] != 0x81 || p[1] != commands[s->step]) return;
        if (s->step == STATUS && (size < 10 || p[3] != 3)) return;
    } else {
        if (size < 15 || p[0] != 0x21 || p[14] != commands[s->step]) return;
        if (!(p[13] & 0x80)) {
            if (s->step == INPUT_MODE || s->step == DEVICE_INFO) return;
            advance(s, now); return;
        }
        if (s->step == DEVICE_INFO && (size < 27 || p[17] != 3)) return;
        if (s->step >= FACTORY_STICKS && s->step <= FACTORY_IMU) {
            const unsigned index = s->step - FACTORY_STICKS;
            if (size < 20u + lengths[index] || p[19] != lengths[index]) return;
            uint32_t address = 0;
            for (unsigned i = 0; i < 4; ++i) address |= (uint32_t)p[15 + i] << (8 * i);
            if (address != addresses[index]) return;
            if (s->step == FACTORY_IMU) (void)pe_pro_imu_calibration(&s->cal, p + 20, lengths[index]);
            else (void)pe_pro_stick_calibration(&s->cal, p + 20, lengths[index], s->step == USER_STICKS);
        }
        if (s->step == ENABLE_IMU) s->imu_enabled = true;
        if (s->step == ENABLE_RUMBLE) s->rumble_enabled = true;
    }
    advance(s, now);
}
void pe_pro_session_poll(pe_pro_session *s, uint64_t now) {
    if (!s->opened) {
        if (now < s->retry_at) return;
        if (s->io.open(s->io.context) != 1) { lost(s, now); return; }
        s->opened = true;
        s->step = STATUS; s->attempts = 0; s->waiting = false; s->counter = 0;
        s->last_output = 0;
        s->setup_until = now + 6000;
        pe_pro_calibration_reset(&s->cal);
    }
    if (!s->ready && now >= s->setup_until) { lost(s, now); return; }
    uint8_t p[64];
    /* Bound work on the 4ms input thread, even if a malicious device floods. */
    for (unsigned i = 0; i < 4; ++i) {
        const int size = s->io.read(s->io.context, p);
        if (size < 0 || size > 64) { lost(s, now); return; }
        if (size == 0) break;
        reply(s, p, (size_t)size, now);
        pe_pro_state sample;
        if (s->ready && pe_pro_decode(&s->cal, p, (size_t)size, now * 1000, &sample)) {
            sample.motion &= s->imu_enabled;
            sample.vibration = s->rumble_enabled;
            s->state = sample;
            s->last_input = now;
            s->retry_ms = 1000;
        }
    }
    if (s->ready) {
        if (now - s->last_input >= 1000) { lost(s, now); return; }
        if (s->rumble_enabled && now - s->last_rumble >= 50 && now - s->last_output >= 30) {
            output_header(s, p, 0x10, s->rumble_level);
            const int result = s->io.write(s->io.context, p);
            if (result < 0) { lost(s, now); return; }
            if (result == 1) s->last_rumble = s->last_output = now;
        }
        return;
    }
    if (s->waiting && now < s->deadline) return;
    if (s->waiting && s->attempts >= 3) {
        /* High speed, factory/user calibration and feedback are optional. */
        if (s->step == SPEED || s->step == HANDSHAKE2 ||
            (s->step >= FACTORY_STICKS && s->step <= ENABLE_RUMBLE)) advance(s, now);
        else { lost(s, now); return; }
    }
    if (s->last_output && now - s->last_output < 30) return;
    if (!setup_packet(s, p)) return;
    const int result = s->io.write(s->io.context, p);
    if (result < 0) { lost(s, now); return; }
    if (result == 1) {
        s->last_output = now;
        s->deadline = now + 150;
        ++s->attempts;
        s->waiting = true;
        /* Force USB does not reply on genuine controllers. */
        if (s->step == FORCE_USB) advance(s, now);
    }
}
void pe_pro_session_rumble(pe_pro_session *s, uint8_t large, uint8_t small) {
    const unsigned level = large > small ? large : small;
    s->rumble_level = level == 0 ? 0 : level < 85 ? 1 : level < 170 ? 2 : 3;
}
void pe_pro_session_stop(pe_pro_session *s) {
    if (s->opened) {
        uint8_t p[64];
        output_header(s, p, 0x10, 0);
        if (s->rumble_enabled) (void)s->io.write(s->io.context, p);
        s->io.close(s->io.context);
    }
    const pe_pro_transport io = s->io;
    pe_pro_session_init(s, io);
}
