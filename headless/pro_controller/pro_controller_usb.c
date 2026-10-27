// SPDX-License-Identifier: GPL-3.0-or-later
#include "pro_controller.h"

#if defined(PS5_NATIVE)
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Real FreeBSD USB FS ABI from the Payload SDK, not fabricated SCE exports.
 * We never detach an interface driver, reset a USB device, or reconfigure it.
 * In particular the internal Bluetooth chip can never pass the VID/PID gate. */
typedef struct {
    int fd;
    bool initialized, busy[2], enabled, normal_stop, rumble_seen;
    uint8_t last_counter;
    struct usb_fs_endpoint ep[2];
    void *buffers[2];
    uint32_t lengths[2];
    uint8_t packets[2][64];
    char node[64];
    unsigned scans;
} native_usb;
static native_usb usb = {.fd = -1};
static pe_pro_session session;

static uint64_t milliseconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
}
static int failed(native_usb *u, const char *operation) {
    fprintf(stderr, "EDEN_PRO_USB operation=%s node=%s errno=%d\n", operation, u->node, errno);
    return -1;
}
static int start_transfer(native_usb *u, unsigned index, unsigned length);
/* Normal teardown only: give the neutral rumble a bounded chance to leave the
 * USB queue before UNINIT cancels it. No sleeping or draining during Poll. */
static void drain_output(native_usb *u) {
    const uint64_t until = milliseconds() + 20;
    for (unsigned i = 0; u->busy[1] && i < 20 && milliseconds() < until; ++i) {
        struct usb_fs_complete complete = {0};
        if (ioctl(u->fd, USB_FS_COMPLETE, &complete) == 0) {
            if (complete.ep_index >= 2) break;
            u->busy[complete.ep_index] = false;
        } else if (errno != EAGAIN && errno != EBUSY && errno != EINTR) break;
        usleep(1000);
    }
}
static void native_close(void *context) {
    native_usb *u = context;
    if (u->fd >= 0) {
        if (u->initialized) {
            if (u->normal_stop && u->rumble_seen) {
                drain_output(u);
                if (!u->busy[1]) {
                    memset(u->packets[1], 0, 64);
                    u->packets[1][0] = 0x10;
                    u->packets[1][1] = (u->last_counter + 1) & 15;
                    u->packets[1][3] = u->packets[1][7] = 1;
                    u->packets[1][4] = u->packets[1][5] = 0x40;
                    u->packets[1][8] = u->packets[1][9] = 0x40;
                    if (start_transfer(u, 1, 64) == 1) drain_output(u);
                }
            }
            const struct usb_fs_uninit uninit = {0};
            /* Cancels all in-flight transfers before their memory can be reused. */
            (void)ioctl(u->fd, USB_FS_UNINIT, &uninit);
        }
        close(u->fd);
    }
    u->fd = -1;
    u->initialized = u->busy[0] = u->busy[1] = false;
    u->normal_stop = u->rumble_seen = false;
}
static int start_transfer(native_usb *u, unsigned index, unsigned length) {
    const struct usb_fs_start start = {.ep_index = (uint8_t)index};
    u->lengths[index] = length;
    u->ep[index].nFrames = 1;
    u->ep[index].aFrames = 0;
    u->ep[index].status = 0;
    if (ioctl(u->fd, USB_FS_START, &start) != 0) return failed(u, "USB_FS_START");
    u->busy[index] = true;
    return 1;
}
static bool ugen_name(const char *name) {
    if (strncmp(name, "ugen", 4) != 0) return false;
    const char *p = name + 4;
    unsigned digits = 0;
    while (*p >= '0' && *p <= '9' && digits < 3) { ++p; ++digits; }
    if (!digits || *p++ != '.') return false;
    digits = 0;
    while (*p >= '0' && *p <= '9' && digits < 3) { ++p; ++digits; }
    return digits && !*p;
}
static int native_open(void *context) {
    native_usb *u = context;
    DIR *devices = opendir("/dev");
    if (!devices) {
        if (u->scans++ < 3) failed(u, "opendir-dev");
        return 0;
    }
    struct dirent *entry;
    bool matched = false;
    int denied = 0;
    unsigned count = 0;
    while ((entry = readdir(devices)) != NULL && count++ < 4096) {
        if (!ugen_name(entry->d_name)) continue;
        snprintf(u->node, sizeof(u->node), "/dev/%.32s", entry->d_name);
        u->fd = open(u->node, O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (u->fd < 0) { if (errno == EACCES || errno == EPERM) ++denied; continue; }
        struct usb_device_descriptor descriptor;
        memset(&descriptor, 0, sizeof(descriptor));
        matched = ioctl(u->fd, USB_GET_DEVICE_DESC, &descriptor) == 0 &&
                  UGETW(descriptor.idVendor) == 0x057e && UGETW(descriptor.idProduct) == 0x2009;
        if (matched) break;
        native_close(u);
    }
    closedir(devices);
    if (!matched) {
        if (u->scans++ < 3 || u->scans % 60 == 0)
            fprintf(stderr, "EDEN_PRO_USB unavailable scans=%u denied_nodes=%d expected=057e:2009\n", u->scans, denied);
        return 0;
    }
    uint8_t config[4096];
    struct usb_gen_descriptor get = {0};
    get.ugd_data = config;
    get.ugd_maxlen = sizeof(config);
    get.ugd_config_index = 0xff; /* current configuration, do not change it */
    pe_pro_endpoints endpoints;
    if (ioctl(u->fd, USB_GET_FULL_DESC, &get) != 0) {
        failed(u, "USB_GET_FULL_DESC"); native_close(u); return 0;
    }
    if (!pe_pro_find_endpoints(config, get.ugd_actlen, &endpoints)) {
        fprintf(stderr, "EDEN_PRO_USB rejected node=%s reason=unsupported-descriptor bytes=%u\n", u->node, get.ugd_actlen);
        native_close(u); return 0;
    }
    memset(u->ep, 0, sizeof(u->ep));
    for (unsigned i = 0; i < 2; ++i) {
        u->buffers[i] = u->packets[i];
        u->lengths[i] = 64;
        u->ep[i].ppBuffer = &u->buffers[i];
        u->ep[i].pLength = &u->lengths[i];
        u->ep[i].nFrames = 1;
        u->ep[i].flags = USB_FS_FLAG_SINGLE_SHORT_OK;
        u->ep[i].timeout = i ? 250 : USB_FS_TIMEOUT_NONE;
    }
    struct usb_fs_init init = {.pEndpoints = u->ep, .ep_index_max = 2};
    if (ioctl(u->fd, USB_FS_INIT, &init) != 0) {
        failed(u, "USB_FS_INIT"); native_close(u); return 0;
    }
    u->initialized = true;
    for (unsigned i = 0; i < 2; ++i) {
        struct usb_fs_open open_endpoint = {0};
        open_endpoint.ep_index = (uint8_t)i;
        open_endpoint.ep_no = i ? endpoints.out : endpoints.in;
        open_endpoint.max_bufsize = 64;
        open_endpoint.max_frames = 1;
        if (ioctl(u->fd, USB_FS_OPEN, &open_endpoint) != 0) {
            failed(u, "USB_FS_OPEN-no-driver-detach"); native_close(u); return 0;
        }
    }
    if (start_transfer(u, 0, 64) < 0) { native_close(u); return 0; }
    fprintf(stderr, "EDEN_PRO_USB opened node=%s in=%02x out=%02x\n", u->node, endpoints.in, endpoints.out);
    return 1;
}
static int native_read(void *context, uint8_t report[64]) {
    native_usb *u = context;
    /* Reap at most two completions per call; input budget is four calls/tick. */
    for (unsigned i = 0; i < 2; ++i) {
        struct usb_fs_complete complete = {0};
        if (ioctl(u->fd, USB_FS_COMPLETE, &complete) != 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EBUSY || errno == EINTR) return 0;
            return failed(u, "USB_FS_COMPLETE");
        }
        const unsigned index = complete.ep_index;
        if (index >= 2 || !u->busy[index]) return -1;
        u->busy[index] = false;
        if (u->ep[index].status != 0 || u->ep[index].aFrames != 1 || u->lengths[index] > 64) {
            fprintf(stderr, "EDEN_PRO_USB transfer-failed index=%u status=%d frames=%u bytes=%u\n",
                    index, u->ep[index].status, u->ep[index].aFrames, u->lengths[index]);
            return -1;
        }
        if (index == 0) {
            const int size = (int)u->lengths[0];
            memcpy(report, u->packets[0], (size_t)size);
            if (start_transfer(u, 0, 64) < 0) return -1;
            return size;
        }
    }
    return 0;
}
static int native_write(void *context, const uint8_t report[64]) {
    native_usb *u = context;
    if (u->busy[1]) return 0;
    u->last_counter = report[1];
    if (report[0] == 0x10) u->rumble_seen = true;
    memcpy(u->packets[1], report, 64);
    return start_transfer(u, 1, 64);
}

bool pe_pro_usb_start(void) {
    pe_pro_usb_stop();
    /* No change to the global settings schema. A regular, empty marker opts in;
     * symlinks and large files are refused. Re-read at each ownership boundary. */
    struct stat info;
    if (lstat("/data/prosperoeden/pro-controller-usb.enabled", &info) != 0 ||
        !S_ISREG(info.st_mode) || info.st_size != 0) return false;
    usb.enabled = true;
    usb.scans = 0;
    const pe_pro_transport io = {&usb, native_open, native_read, native_write, native_close};
    pe_pro_session_init(&session, io);
    fprintf(stderr, "EDEN_PRO_USB enabled transport=FreeBSD-USB-FS hardware-qualified=no\n");
    return true;
}
bool pe_pro_usb_poll(pe_pro_state *out) {
    if (!usb.enabled) return false;
    const bool before = session.state.connected;
    pe_pro_session_poll(&session, milliseconds());
    if (before != session.state.connected)
        fprintf(stderr, "EDEN_PRO_USB connected=%d stick_cal=%d motion=%d rumble=%d retry_ms=%u\n",
                session.state.connected, session.cal.sticks_calibrated,
                session.imu_enabled, session.rumble_enabled, session.retry_ms);
    *out = session.state;
    return out->connected;
}
void pe_pro_usb_rumble(uint8_t large, uint8_t small) {
    if (usb.enabled) pe_pro_session_rumble(&session, large, small);
}
void pe_pro_usb_stop(void) {
    if (!usb.enabled) return;
    usb.normal_stop = true;
    pe_pro_session_stop(&session);
    usb.enabled = false;
}
#else
bool pe_pro_usb_start(void) { return false; }
bool pe_pro_usb_poll(pe_pro_state *out) { (void)out; return false; }
void pe_pro_usb_rumble(uint8_t large, uint8_t small) { (void)large; (void)small; }
void pe_pro_usb_stop(void) {}
#endif
