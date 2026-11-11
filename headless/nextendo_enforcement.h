// SPDX-License-Identifier: GPL-3.0-or-later
// P06 Nextendo destination enforcement -- external candidate r1 (A06).
//
// Pure decision engine for a default-off, per-user/session Nextendo
// exception (NET-01/POLICY-02). It answers ONE question: is this exact
// destination pinned for this user's active session? It performs no I/O,
// no DNS, no socket operation, no logging and no heap allocation, and it
// never contacts a service.
//
// A DNS answer alone never authorizes anything here: a destination passes
// only when hostname, resolved IPv4 address, port AND protocol all exactly
// match one pinned entry of the session's frozen policy. Everything else --
// suffix domains, lookalikes, wildcards, direct-IP routes, rewrites to a
// different address, wrong ports/protocols and any malformed input -- is
// refused. There is no fallthrough to official or alternate destinations.
//
// This is a cooperative policy engine like the app-owned
// headless/network_domain_rules.c: it is not a defense against a
// compromised host process, and callers must serialize access (it keeps
// no locks). Production callers obtain the session gate value ONLY from
// the registered nx_auth_check_local implementation; the stub shipped in
// this increment never satisfies it, so with this code alone no session
// can ever be enabled.
#pragma once
#include "nextendo_auth_local.h"
#ifdef __cplusplus
extern "C" {
#endif

#define NX_USER_MAX 32 // opaque user ids: 1..31 chars [A-Za-z0-9_-]
#define NX_HOST_MAX 64 // pinned/query hostnames: canonical, <=63 chars
#define NX_ADDR_MAX 16 // "255.255.255.255" + NUL
#define NX_DEST_MAX 8  // small session-scoped pinned list by design

typedef enum {
    nx_protocol_tcp = 1,
    nx_protocol_udp = 2
} nx_protocol;

typedef struct {
    char host[NX_HOST_MAX]; // canonical pinned hostname (lowercase)
    char addr[NX_ADDR_MAX]; // canonical dotted-quad IPv4 the host must be at
    unsigned short port;
    nx_protocol protocol;
} nx_destination;

// One emulated user's frozen destination policy. Build with init+pin(s),
// then publish; after publish nothing changes it.
typedef struct {
    char user[NX_USER_MAX];
    nx_destination destinations[NX_DEST_MAX];
    unsigned count;
    int frozen;
} nx_policy;

// A session starts disabled (zero it or call nx_session_disable) and only
// nx_session_begin can enable it. disable wipes the pinned copy.
typedef struct {
    int active; // 0 = disabled: the default and only fail-closed state
    char user[NX_USER_MAX];
    nx_policy policy; // private frozen copy
} nx_session;

// Returns 0 on success; -1 (policy left zeroed) on invalid user.
int nx_policy_init(nx_policy* p, const char* user);

// Pins one exact destination (host AND the address it must resolve to).
// Refuses after publish, when full, on duplicates, IP-literal hostnames,
// wildcards, malformed hosts/addresses, port 0 and unknown protocols.
int nx_policy_pin(nx_policy* p, const char* host, const char* addr,
                  unsigned short port, nx_protocol protocol);

// Freezes a non-empty initialized policy. Returns 0, or -1 and leaves it
// unfrozen when the user is invalid or no destination is pinned.
int nx_policy_publish(nx_policy* p);

// 1 only when user owns p and host+addr+port+protocol exactly match one
// pin of the frozen policy. 0 otherwise; never a suffix/prefix match.
int nx_policy_allows(const nx_policy* p, const char* user, const char* host,
                     const char* addr, unsigned short port,
                     nx_protocol protocol);

// Returns the session to the disabled default, wiping its pinned copy.
void nx_session_disable(nx_session* s);

// Enables a session from a frozen policy. Refused (session left disabled)
// unless auth is nx_auth_satisfied_local AND the policy is frozen and
// non-empty. The value nx_auth_satisfied_local is a contract value for a
// future verified local-auth implementation; the shipped stub never
// produces it, so this call always refuses with this increment alone.
int nx_session_begin(nx_session* s, const nx_policy* p, nx_auth_result auth);

// 1 only for an active session whose user and exact destination match its
// private policy copy. 0 otherwise (including any disabled session).
int nx_session_allows(const nx_session* s, const char* user, const char* host,
                      const char* addr, unsigned short port,
                      nx_protocol protocol);

#ifdef __cplusplus
}
#endif
