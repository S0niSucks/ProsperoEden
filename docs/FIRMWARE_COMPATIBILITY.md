# Firmware compatibility boundaries

There are two independent questions:

| Axis | Current evidence boundary |
| --- | --- |
| **PS5 host firmware** | Repository history identifies 13.60 as an exercised target for particular earlier artifacts. This is not qualification of the current working tree. Other versions remain unqualified; SDK offset lists/package metadata are not support evidence. |
| **Switch guest firmware** | User-supplied keys/NCAs can pass current startup structure/decryption checks. This does not establish every firmware version, service, title, update or save behavior. Game compatibility is checked separately at load/runtime. |
| **Switch 2** | No integrated/qualified PS5 backend. Eglantine/Takoroka experiments are research candidates, not a compatible replacement core. Oboromi explicitly reports no commercial-game/firmware execution. |

The launcher reports **Guest firmware checks passed**, not universal firmware
readiness. Diagnostics keeps the PS5 host version **Not observed** unless an
actual supported host adapter supplies it; guest SystemVersion/key revisions
are never used to guess the host. No new offsets, nearest-version fallback,
spoofing, permissive unknown-host path or automatic firmware/key download is added.

Required elevation rollback/identity checks, writable-root proof and sparse-JIT
admission remain in place. Safe Launch does not bypass them. Optional feature
availability is separate from firmware qualification; desktop screenshots,
metadata HTTP, a successful compile or a package receipt do not prove online
guest service, controller/keyboard ABI or native gameplay.

Broader support requires exact application/helper/SDK/graphics/loader identities,
validated native imports/semantics and separately authorized tests on each host
firmware, plus an independent guest-version/title matrix. No console operations
were performed for these application changes.
