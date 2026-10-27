# Guest-idle timing harness investigation — 2026-10-27

## Finding and scope

**A harness race is demonstrated; no production-policy change is warranted by this evidence.**
The coordinator reported a fresh-object host-suite failure at `Cost`'s
`IdlePollBudget(2) == 0` assertion. This worker's unmodified baseline passed the timing
assertions, so the exact scheduling history of that earlier failure is **not established**.
The deterministic regression below reproduces the same failed criterion without changing
the generated Idle body's policy or the pinned interrupt protocol.

The old `Waiting` starts its producer deadline after `kick.release()`, not after Idle starts.
Idle captures its own `start`; Interrupt stamps the flag transition under `m_guard`.
Adaptation uses `max(raised - start, 0)`, not the producer's requested interval and not
the worker's resume time. A late worker can therefore observe a short or already-raised
interrupt even when the producer intended a 4 ms wait. This correctly resets the run of
long waits: it is not evidence of a lost wake-up or a broken adaptation rule.

With a seeded 500,000 ns poll and 40 attempts (`kIdleLongRun + 8`), **one** pre-raised
interrupt at zero-based round 20 is enough: it resets the run, leaving only 19 long waits.
The old assertion's premise (40 genuinely long waits) was false.

Only `tools/check-guest-idle.py` and this investigation's evidence change. No runtime,
performance policy, CMake generation, Cover Flow/UI, catalogs, releases or aggregate docs
are changed. The original Policy, Wakeups, Cost and Spin assertions remain intact.

## Targeted fix and regression

- A test-only hook publishes the actual `start` immediately **inside** Idle, after the
  production timestamp statement. It is a nonblocking thread-local-pointer check and a
  release-store; the producer acquire-loads it before arming `start + requested_gap`.
  Merely acknowledging immediately before the Idle call would retain the scheduling hole.
- The worker is not parked inside Idle, the production timestamp is not moved, and there
  is no new synchronization in production. `Wakeups` leaves the hook disabled and retains
  all 3,000 unsynchronized before/during/after-entry races plus the independent-core check.
- Each synchronized interval now also asserts `observed >= intended`. Diagnostics distinguish
  intended and observed ranges, pre-raised interrupts, maximum pre-entry lag and worker CPU
  time. Existing spin-accounting and CPU-ratio thresholds are unchanged.
- The regression gates just round 20's worker outside Idle until the old producer deadline
  has passed. The old handshake interrupts before releasing that gate; the corrected one
  releases it, observes actual entry, and only then times the intended wait. The regression
  requires the old budget to remain 500,000 ns and the corrected budget to become **zero**.
  It does not turn a failed normal assertion into a retry or relax any normal threshold.
- Optional `--generated` verifies that the tested Idle and Interrupt/ClearInterrupt bodies
  exactly match the coordinator's saved generated source before adding the test hook.
  `--idle-only` skips the unrelated four-thread lock test; `--sanitizer` selects an explicit
  plain, ASan/UBSan, or TSan run. Existing invocation defaults still select both sanitizers
  and the lock test. Sanitizer errors remain fatal, never skipped or retried automatically.

## Final focused results

| Check | Plain | ASan + UBSan |
| --- | --- | --- |
| One delayed start, old handshake | 1 pre-raised; budget 500,000 ns | 1 pre-raised; budget 500,000 ns |
| Same delayed start, corrected handshake | 0 pre-raised; budget 0 ns | 0 pre-raised; budget 0 ns |
| Corrected delayed-start observed interval | 4,000.11–4,002.35 us | 4,001.17–7,876.48 us |
| 60 × 4 ms CPU, fixed / adaptive | 36.16 / 3.53 ms | 34.52 / 6.56 ms |
| Seeded full poll after 40 real long waits | 0 ns | 0 ns |
| Mixed short handoffs slept after warm-up | 19 / 140 | 8 / 140 |
| 50 us handoffs slept / without polling | 1 / 400; 400 / 400 | 2 / 400; 400 / 400 |
| 50 us handoff CPU / counted polling | 21.56 / 20.67 ms | 22.76 / 20.59 ms |
| Fixed-poll 4 ms CPU / counted polling | 34.13 / 30.22 ms | 34.81 / 30.11 ms |
| All original idle assertions + new regression | **PASS** | **PASS**, no sanitizer reports |

The old budget-zero criterion is asserted **false** within the negative regression, not
allowed to abort and then retried. Final plain and ASan/UBSan invocations each ran once.
An initial all-round-delay experiment also passed and led to the stronger single-delay
regression; those intermediate logs remain in the private volume/ignored build folder.

TSan **did not execute the checks**: the unmodified baseline and the initial synchronized
variant both failed runtime startup with `FATAL: ThreadSanitizer: unexpected memory mapping`
(child exit 66, Python runner exit 1). The final single-delay refinement was not rerun under
that blocked runtime. No TSan success is claimed for this worker's change; no retry-until-green,
ASLR alteration or sanitizer installation was used. The unrelated kernel lock check was
not run in the focused invocations. No full host/native/emulator build or console test ran.

## Provenance and reproduction

- Exclusive worktree: `D:/leonardommello/PS5/ProsperoEden/.local/agent-worktrees/cover-flow`.
  Baseline script came from `903c233f1049431425df70c5cfa0de3ad2fe381d`.
- Docker image `prosperoeden-host:features`, image ID
  `sha256:e6137ec556fe8e63ebf25e7fabe36e573d089c469d0e9d391c3072f2ae04d551`;
  GCC 13.3.0, C++20 `-O2 -Wall -Wextra -Werror -pthread`.
  Every container was capped at **2 CPUs / 1 GiB / 128 PIDs**, with at most two test threads.
- The only writable Docker volume was `prosperoeden-team-cover-flow-20261027`.
  Coordinator volume `prosperoeden-integration-20261027` was mounted **read-only** at `/pinned`.
  No source/build synchronization or writes to that volume occurred.
- Read the pinned `source/src/core/hle/kernel/physical_core.cpp` and the actual generated
  `build/headless/physical_core.cpp` under `/pinned/ps5-eden-headless.2YUJmY/`.
  The final logs record SHA-256 of the script, pinned source, CMake, performance policy and
  generated source; body equality was checked in both final invocations.
- [Unmodified baseline output](evidence/guest-idle/baseline.txt),
  [final plain output](evidence/guest-idle/final-plain.txt),
  [final ASan/UBSan output](evidence/guest-idle/final-asan-ubsan.txt),
  [initial synchronized variant's TSan startup failure](evidence/guest-idle/tsan-startup.txt).
  Logs are unedited. Complete logs remain at `/work/idle-investigation/` in the private
  volume and `build/guest-idle/` in this worktree.

Reproduce in that image with the same caps and mounts (worktree mounted `/input:ro`):

```sh
p=/pinned/ps5-eden-headless.2YUJmY
python3 -B /input/tools/check-guest-idle.py \
  "$p/source/src/core/hle/kernel/physical_core.cpp" \
  --generated "$p/build/headless/physical_core.cpp" \
  --idle-only --sanitizer none
# A separate explicit run uses --sanitizer address (ASan + UBSan).
# --sanitizer thread remains fatal if this host's TSan cannot initialize.
```

This proves the old timing premise is unsound and fixes that premise. It does **not** prove
the coordinator's particular failure had no other cause, eliminate producer overshoot or
all scheduling sensitivity from strict CPU/short-handoff measurements, or establish PS5
runtime correctness/performance. Observed overshoots remain visible rather than filtered.
Coordinator review/cherry-pick and a working TSan environment remain follow-up validation.
