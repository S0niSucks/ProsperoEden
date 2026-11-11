// SPDX-License-Identifier: GPL-3.0-or-later
// P06 Nextendo destination enforcement -- external candidate r1 (A06).
#include "nextendo_enforcement.h"

#include <string.h>

static char ascii_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static int user_valid(const char* s) {
    if (!s) return 0;
    size_t n = 0;
    for (const char* p = s; *p; ++p, ++n) {
        const char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
        if (n + 1 >= NX_USER_MAX) return 0;
    }
    return n > 0;
}

// Strict dotted-quad IPv4, canonical form only: four non-empty digit
// fields, no leading zeros, each value 0..255.
static int addr_valid(const char* s) {
    if (!s) return 0;
    unsigned fields = 0, digits = 0;
    unsigned long value = 0;
    for (const char* p = s;; ++p) {
        const char c = *p;
        if (c >= '0' && c <= '9') {
            if (digits == 1 && value == 0) return 0; // leading zero
            value = value * 10 + (unsigned long)(c - '0');
            if (++digits > 3 || value > 255) return 0;
        } else if (c == '.' || c == '\0') {
            if (digits == 0) return 0; // empty field
            if (c == '\0') break;
            ++fields;
            value = 0;
            digits = 0;
        } else {
            return 0;
        }
    }
    return fields == 3;
}

// Canonicalize a hostname: ASCII-lowercase, strip ONE trailing dot,
// reject wildcards/underscores/empty labels/missing dot/hyphen edges.
static int host_canonical(const char* in, char out[NX_HOST_MAX]) {
    if (!in || !out) return 0;
    size_t n = 0;
    int dot_seen = 1; // start-of-label behaves like "after a dot"
    for (const char* p = in; *p; ++p) {
        const char c = ascii_lower(*p);
        if (c == '.') {
            if (dot_seen) return 0; // leading or doubled dot
            dot_seen = 1;
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                   c == '-') {
            dot_seen = 0;
        } else {
            return 0; // '*', '_', non-ASCII, etc.
        }
        if (n + 1 >= NX_HOST_MAX) return 0;
        out[n++] = c;
    }
    if (dot_seen) {
        if (n == 0 || out[n - 1] != '.') return 0; // lone "." / "-"
        --n; // strip the single absolute-name dot
    }
    if (n == 0 || out[0] == '.' || out[0] == '-' || out[n - 1] == '-')
        return 0;
    out[n] = '\0';
    if (!strchr(out, '.')) return 0; // dotted hostnames only, post-strip
    // No label may start or end with '-'.
    size_t start = 0;
    for (size_t i = 0; i <= n; ++i) {
        if (i == n || out[i] == '.') {
            if (i == start || out[start] == '-' || out[i - 1] == '-')
                return 0;
            start = i + 1;
        }
    }
    return 1;
}

static int dest_fill(nx_destination* d, const char* host, const char* addr,
                     unsigned short port, nx_protocol protocol) {
    if (!d || !host || !addr) return -1;
    if (port == 0) return -1;
    if (protocol != nx_protocol_tcp && protocol != nx_protocol_udp) return -1;
    if (!host_canonical(host, d->host)) return -1;
    if (addr_valid(d->host)) return -1; // IP literal as host: direct-IP route
    if (!addr_valid(addr)) return -1;
    const size_t n = strlen(addr);
    if (n + 1 > NX_ADDR_MAX) return -1;
    memcpy(d->addr, addr, n + 1);
    d->port = port;
    d->protocol = protocol;
    return 0;
}

static int dest_same(const nx_destination* a, const nx_destination* b) {
    return strcmp(a->host, b->host) == 0 && strcmp(a->addr, b->addr) == 0 &&
           a->port == b->port && a->protocol == b->protocol;
}

// Bounded exact copy of an already-validated user id (length <= max-1).
static void copy_user(char* dst, const char* src) {
    size_t i = 0;
    for (; i + 1 < NX_USER_MAX && src[i]; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

int nx_policy_init(nx_policy* p, const char* user) {
    if (!p) return -1;
    memset(p, 0, sizeof *p);
    if (!user_valid(user)) return -1;
    copy_user(p->user, user);
    return 0;
}

int nx_policy_pin(nx_policy* p, const char* host, const char* addr,
                  unsigned short port, nx_protocol protocol) {
    if (!p || p->frozen || p->count >= NX_DEST_MAX) return -1;
    nx_destination d;
    if (dest_fill(&d, host, addr, port, protocol) != 0) return -1;
    for (unsigned i = 0; i < p->count; ++i)
        if (dest_same(&p->destinations[i], &d)) return -1; // duplicates
    p->destinations[p->count++] = d;
    return 0;
}

int nx_policy_publish(nx_policy* p) {
    if (!p || p->frozen) return -1;
    if (p->count == 0 || p->count > NX_DEST_MAX) return -1;
    if (!user_valid(p->user)) return -1;
    p->frozen = 1;
    return 0;
}

int nx_policy_allows(const nx_policy* p, const char* user, const char* host,
                     const char* addr, unsigned short port,
                     nx_protocol protocol) {
    if (!p || !p->frozen || p->count == 0 || p->count > NX_DEST_MAX) return 0;
    if (!user || strcmp(user, p->user) != 0) return 0;
    if (!host || !addr || port == 0) return 0;
    if (protocol != nx_protocol_tcp && protocol != nx_protocol_udp) return 0;
    char qhost[NX_HOST_MAX];
    if (!host_canonical(host, qhost)) return 0;
    if (addr_valid(qhost)) return 0; // direct-IP route: refused
    if (!addr_valid(addr)) return 0;
    for (unsigned i = 0; i < p->count; ++i) {
        const nx_destination* d = &p->destinations[i];
        if (d->port == port && d->protocol == protocol &&
            strcmp(d->host, qhost) == 0 && strcmp(d->addr, addr) == 0)
            return 1;
    }
    return 0;
}

void nx_session_disable(nx_session* s) {
    if (!s) return;
    memset(s, 0, sizeof *s); // wipes user and the pinned destination copy
}

int nx_session_begin(nx_session* s, const nx_policy* p, nx_auth_result auth) {
    if (!s) return -1;
    nx_session_disable(s); // fail-closed reset before any validation
    if (!p || auth != nx_auth_satisfied_local) return -1;
    if (!p->frozen || p->count == 0 || p->count > NX_DEST_MAX) return -1;
    if (!user_valid(p->user)) return -1;
    s->policy = *p; // private frozen copy
    copy_user(s->user, p->user);
    s->active = 1;
    return 0;
}

int nx_session_allows(const nx_session* s, const char* user, const char* host,
                      const char* addr, unsigned short port,
                      nx_protocol protocol) {
    if (!s || !s->active) return 0;
    return nx_policy_allows(&s->policy, user, host, addr, port, protocol);
}
