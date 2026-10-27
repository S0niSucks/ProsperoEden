// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EDEN_PRO_MONOTONIC_CLOCK_H
#define EDEN_PRO_MONOTONIC_CLOCK_H
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Leave the output unchanged on error, including malformed/overflowing results.
 * Keep room for the session's millisecond-to-microsecond timestamp conversion.
 * Callers must stop time-dependent input/output rather than use a false epoch. */
static inline bool pe_pro_monotonic_ms(uint64_t *out) {
    struct timespec now = {0, 0};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 ||
        now.tv_nsec < 0 || now.tv_nsec >= 1000000000L) return false;
    const uint64_t seconds = (uint64_t)now.tv_sec;
    if (seconds > (UINT64_MAX / 1000 - 999) / 1000) return false;
    *out = seconds * 1000 + (uint64_t)now.tv_nsec / 1000000;
    return true;
}
#endif
