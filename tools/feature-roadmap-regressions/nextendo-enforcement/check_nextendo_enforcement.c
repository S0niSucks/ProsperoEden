// SPDX-License-Identifier: GPL-3.0-or-later
// P06 Nextendo enforcement fixture -- external candidate r1 (A06).
//
// Consumes ONLY the real functions of nextendo_enforcement.c and the real
// nextendo_auth_local.c stub: no mocks of the code under test, no network,
// no DNS, no sockets, no credentials. Checks marked CONTRACT-VALUE pass
// nx_auth_satisfied_local directly as a value because NO code in this
// increment can produce it; they exercise enforcement logic only and never
// claim authentication or online play works.
#include "nextendo_enforcement.h"

#include <stdio.h>

static int failures;
static unsigned checks;

static void check(int ok, const char* name) {
    ++checks;
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}

// player-a's only pinned destination.
#define A_HOST "account.nextendo.example"
#define A_ADDR "10.0.0.10"
#define A_PORT 443

int main(void) {
    // --- 1. real stub: never satisfies, for any user ---
    check(nx_auth_check_local("player-a") == nx_auth_unavailable &&
              nx_auth_check_local("player-b") == nx_auth_unavailable &&
              nx_auth_check_local("") == nx_auth_unavailable,
          "stub_local_auth_never_satisfies");

    nx_policy pa;
    check(nx_policy_init(&pa, "player-a") == 0 &&
              nx_policy_pin(&pa, "account.Nextendo.Example", A_ADDR, A_PORT,
                            nx_protocol_tcp) == 0 &&
              nx_policy_publish(&pa) == 0,
          "policy_build_and_publish");

    const int exact = nx_policy_allows(&pa, "player-a", A_HOST, A_ADDR,
                                       A_PORT, nx_protocol_tcp);

    // --- 2. true default-off: fresh session denies even the pinned tuple ---
    nx_session sa;
    nx_session_disable(&sa);
    check(!nx_session_allows(&sa, "player-a", A_HOST, A_ADDR, A_PORT,
                             nx_protocol_tcp),
          "fresh_session_denies_default_off");

    // --- 3. begin refused with the REAL stub result; still denies ---
    check(nx_session_begin(&sa, &pa,
                           nx_auth_check_local("player-a")) != 0 &&
              !nx_session_allows(&sa, "player-a", A_HOST, A_ADDR, A_PORT,
                                 nx_protocol_tcp),
          "begin_refused_with_real_stub");

    // --- 4. publish/begin refuse empty or invalid policies ---
    nx_policy bad, empty;
    int bad_ok = nx_policy_init(&bad, "bad user!") != 0 &&
                 nx_policy_publish(&bad) != 0 &&
                 nx_session_begin(&sa, &bad, nx_auth_satisfied_local) != 0;
    int empty_ok = nx_policy_init(&empty, "player-e") == 0 &&
                   nx_policy_publish(&empty) != 0 && // no pins: refuses
                   nx_session_begin(&sa, &empty, nx_auth_satisfied_local) != 0;
    check(bad_ok && empty_ok, "publish_requires_user_and_pins");

    // --- 5. exact match passes; case and trailing dot normalize ---
    check(exact &&
              nx_policy_allows(&pa, "player-a", "ACCOUNT.Nextendo.Example.",
                               A_ADDR, A_PORT, nx_protocol_tcp),
          "policy_allows_exact_canonical");

    // --- 6. another user is refused the same tuple ---
    check(!nx_policy_allows(&pa, "player-b", A_HOST, A_ADDR, A_PORT,
                            nx_protocol_tcp),
          "policy_denies_other_user");

    // --- 7. suffix and lookalike hosts are NOT matches ---
    check(!nx_policy_allows(&pa, "player-a", "sub." A_HOST, A_ADDR, A_PORT,
                            nx_protocol_tcp) &&
              !nx_policy_allows(&pa, "player-a", "xaccount.nextendo.example",
                                A_ADDR, A_PORT, nx_protocol_tcp),
          "policy_denies_suffix_and_lookalike");

    // --- 8. DNS rewrite: right host at another address is refused ---
    check(!nx_policy_allows(&pa, "player-a", A_HOST, "10.0.0.11", A_PORT,
                            nx_protocol_tcp),
          "policy_denies_dns_rewrite_address");

    // --- 9. wrong port and wrong protocol are refused ---
    check(!nx_policy_allows(&pa, "player-a", A_HOST, A_ADDR, A_PORT + 1,
                            nx_protocol_tcp) &&
              !nx_policy_allows(&pa, "player-a", A_HOST, A_ADDR, A_PORT,
                                nx_protocol_udp),
          "policy_denies_wrong_port_and_protocol");

    // --- 10. direct-IP and wildcard queries, malformed addresses ---
    check(!nx_policy_allows(&pa, "player-a", A_ADDR, A_ADDR, A_PORT,
                            nx_protocol_tcp) &&
              !nx_policy_allows(&pa, "player-a", "ac*.nextendo.example",
                                A_ADDR, A_PORT, nx_protocol_tcp) &&
              !nx_policy_allows(&pa, "player-a", A_HOST, "10.0.0", A_PORT,
                                nx_protocol_tcp) &&
              !nx_policy_allows(&pa, "player-a", A_HOST, "10.0.0.256",
                                A_PORT, nx_protocol_tcp),
          "policy_denies_direct_ip_wildcard_bad_addr");

    // --- 11. pin refuses invalid entries and duplicates ---
    nx_policy junk;
    nx_policy_init(&junk, "player-j");
    int pin_bad = nx_policy_pin(&junk, "10.0.0.10", "10.0.0.10", 80,
                                nx_protocol_tcp) != 0 && // IP-literal host
                  nx_policy_pin(&junk, "*.nextendo.example", "10.0.0.10", 80,
                                nx_protocol_tcp) != 0 && // wildcard
                  nx_policy_pin(&junk, "bad..name", "10.0.0.10", 80,
                                nx_protocol_tcp) != 0 && // empty label
                  nx_policy_pin(&junk, "nodot", "10.0.0.10", 80,
                                nx_protocol_tcp) != 0 && // no dot
                  nx_policy_pin(&junk, "ok.example", "10.0.0.999", 80,
                                nx_protocol_tcp) != 0 && // bad address
                  nx_policy_pin(&junk, "ok.example", "10.0.0.10", 0,
                                nx_protocol_tcp) != 0 && // port 0
                  nx_policy_pin(&junk, "ok.example", "10.0.0.10", 80,
                                (nx_protocol)7) != 0; // bad protocol
    check(pin_bad && junk.count == 0 &&
              nx_policy_pin(&junk, "dup.example", "10.0.0.20", 80,
                            nx_protocol_udp) == 0 &&
              nx_policy_pin(&junk, "dup.example", "10.0.0.20", 80,
                            nx_protocol_udp) != 0 && // duplicate
              junk.count == 1,
          "pin_refuses_invalid_and_duplicate");

    // --- 12. unfrozen policy denies even the exact tuple ---
    nx_policy unfrozen;
    nx_policy_init(&unfrozen, "player-a");
    nx_policy_pin(&unfrozen, A_HOST, A_ADDR, A_PORT, nx_protocol_tcp);
    check(!nx_policy_allows(&unfrozen, "player-a", A_HOST, A_ADDR, A_PORT,
                            nx_protocol_tcp),
          "unfrozen_policy_denies");

    // --- 13. CONTRACT-VALUE: enabled session allows ONLY the pinned tuple ---
    check(nx_session_begin(&sa, &pa, nx_auth_satisfied_local) == 0 &&
              nx_session_allows(&sa, "player-a", A_HOST, A_ADDR, A_PORT,
                                nx_protocol_tcp) &&
              !nx_session_allows(&sa, "player-a", "sub." A_HOST, A_ADDR,
                                 A_PORT, nx_protocol_tcp) &&
              !nx_session_allows(&sa, "player-a", A_HOST, "10.0.0.11",
                                 A_PORT, nx_protocol_tcp),
          "session_contract_allows_only_pinned");

    // --- 14. CONTRACT-VALUE: per-user/per-session isolation ---
    nx_policy pb;
    nx_session sb;
    nx_policy_init(&pb, "player-b");
    nx_policy_pin(&pb, "match.example", "10.0.0.20", 443, nx_protocol_tcp);
    nx_policy_publish(&pb);
    nx_session_disable(&sb);
    nx_session_begin(&sb, &pb, nx_auth_satisfied_local);
    check(nx_session_allows(&sb, "player-b", "match.example", "10.0.0.20",
                            443, nx_protocol_tcp) &&
              !nx_session_allows(&sb, "player-a", A_HOST, A_ADDR, A_PORT,
                                 nx_protocol_tcp) && // a's tuple in b
              !nx_session_allows(&sb, "player-b", A_HOST, A_ADDR, A_PORT,
                                 nx_protocol_tcp) && // b asks a's host
              !nx_session_allows(&sa, "player-a", "match.example",
                                 "10.0.0.20", 443, nx_protocol_tcp), // b in a
          "per_session_user_isolation");

    // --- 15. CONTRACT-VALUE: disable revokes; other session unaffected ---
    nx_session_disable(&sa);
    check(!nx_session_allows(&sa, "player-a", A_HOST, A_ADDR, A_PORT,
                             nx_protocol_tcp) &&
              nx_session_allows(&sb, "player-b", "match.example", "10.0.0.20",
                                443, nx_protocol_tcp),
          "disable_revokes_only_target_session");

    // --- 16. null/invalid arguments stay fail-closed ---
    nx_session_begin(&sb, NULL, nx_auth_satisfied_local); // resets, refuses
    check(!nx_session_allows(NULL, "player-a", A_HOST, A_ADDR, A_PORT,
                             nx_protocol_tcp) &&
              !nx_policy_allows(NULL, "player-a", A_HOST, A_ADDR, A_PORT,
                                nx_protocol_tcp) &&
              !nx_policy_allows(&pa, NULL, A_HOST, A_ADDR, A_PORT,
                                nx_protocol_tcp) &&
              nx_session_begin(NULL, &pa, nx_auth_satisfied_local) != 0 &&
              !nx_session_allows(&sb, "player-b", "match.example",
                                 "10.0.0.20", 443, nx_protocol_tcp),
          "null_arguments_fail_closed");

    printf("RESULT: %u/%u %s\n", checks - failures, checks,
           failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
