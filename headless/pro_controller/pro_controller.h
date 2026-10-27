// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EDEN_PRO_CONTROLLER_H
#define EDEN_PRO_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Physical Nintendo 057e:2009 protocol; no guest controller-type selection here.
 * Buttons use the existing ps5_pad.hpp bridge's masks. A/B/X/Y map by label to
 * its default guest mapping, not by physical position. Capture owns shortcuts. */
typedef struct {
    uint32_t buttons;
    uint8_t sticks[4]; /* left x/y, right x/y; 128 centre, Y down */
    uint8_t triggers[2];
    bool connected, motion, vibration;
    float acceleration[3]; /* bridge axes, G */
    float gyro[3];         /* bridge axes, rad/s */
    uint64_t timestamp_us;
} pe_pro_state;

typedef struct { uint16_t center, below, above; } pe_pro_axis;
typedef struct {
    pe_pro_axis axes[4];
    int16_t accel_origin[3], gyro_origin[3];
    float accel_scale[3], gyro_scale[3];
    bool sticks_calibrated, imu_calibrated;
} pe_pro_calibration;

void pe_pro_calibration_reset(pe_pro_calibration *cal);
/* Exact SPI payload sizes, not entire input packets. Invalid data is atomic:
 * it never replaces the last accepted calibration. User overrides need magic. */
bool pe_pro_stick_calibration(pe_pro_calibration *cal, const uint8_t *data,
                             size_t size, bool user);
bool pe_pro_imu_calibration(pe_pro_calibration *cal, const uint8_t *data, size_t size);
bool pe_pro_decode(const pe_pro_calibration *cal, const uint8_t *report, size_t size,
                   uint64_t timestamp_us, pe_pro_state *out);
/* Fixed legacy pad ABI shared with radio_input.c and third_party/ps5_pad.hpp. */
void pe_pro_pad_sample(const pe_pro_state *state, uint8_t out[120]);

typedef struct { uint8_t in, out; } pe_pro_endpoints;
bool pe_pro_find_endpoints(const uint8_t *config, size_t size, pe_pro_endpoints *out);

/* All callbacks run on the input owner, without threads. open: 1 connected,
 * 0 absent/denied, -1 fatal; read: bytes/0 would-block/-1 disconnected;
 * write: 1 queued/0 busy/-1 failure. Buffers must be copied before returning.
 * USB framing includes the report ID (no Bluetooth HID transaction prefix). */
typedef struct {
    void *context;
    int (*open)(void *context);
    int (*read)(void *context, uint8_t report[64]);
    int (*write)(void *context, const uint8_t report[64]);
    void (*close)(void *context);
} pe_pro_transport;

typedef struct {
    pe_pro_transport io;
    pe_pro_calibration cal;
    pe_pro_state state;
    uint64_t retry_at, deadline, setup_until, last_input, last_output, last_rumble;
    uint32_t retry_ms;
    unsigned step, attempts;
    uint8_t counter, rumble_level;
    bool opened, waiting, ready, imu_enabled, rumble_enabled;
} pe_pro_session;

void pe_pro_session_init(pe_pro_session *session, pe_pro_transport io);
void pe_pro_session_poll(pe_pro_session *session, uint64_t now_ms);
void pe_pro_session_rumble(pe_pro_session *session, uint8_t large, uint8_t small);
void pe_pro_session_stop(pe_pro_session *session);

/* Process-local native service, explicitly opted in by a regular marker file
 * /data/prosperoeden/pro-controller-usb.enabled. Start/stop at launcher/game
 * boundaries; never concurrently. The host implementation is always disabled. */
bool pe_pro_usb_start(void);
bool pe_pro_usb_poll(pe_pro_state *out);
void pe_pro_usb_rumble(uint8_t large, uint8_t small);
void pe_pro_usb_stop(void);

#ifdef __cplusplus
}
#endif
#endif
