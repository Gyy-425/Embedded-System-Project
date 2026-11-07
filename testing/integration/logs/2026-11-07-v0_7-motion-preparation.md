# v0.7 Motion interface preparation

7 November2026. Base66b4fbd saves the previous v0.6.1 source and actual bounded
motor result. This feature's changes are on the existing aai05/bring-up branch.
No upload, power action, connection to MQTT or motor start was performed.

## Delivered scope

Portable signed-duty command policy, finite local GP20 producer, synchronized
task API/owner, paired PWM output and100ms timer-IRQ leases. Public submission
stores a command; only the owner applies hardware. STOP/expiry latch until
reset. No MQTT movement, closed-loop speed control or new encoder qualification.
New C modules use strict warnings and BARR-C conventions. Existing code is
modified only where this feature requires integration.

Policy, button, paired-output and late-fresh-renewal assertions each failed
before the corresponding behavior existed. Initial host/build run passed;
one fresh reviewer then identified three Important races, no Critical issues:

1. Stale active snapshot could permit restart after timer IRQ. Added a
   driver-owned continuous-expiry latch, cleared only at startup.
2. Owner's pre-lock clock could falsely expire a newer other-core submission.
   Owner now resamples time under the shared state lock.
3. Old expiry could be erased during same-duty alarm renewal. Renewal retains
   and checks the previous anchor/deadline through rearm, including the
   interrupt-clear window. Start also includes configuration time in its lease.

Each review regression exited134 on its intended assertion before fixes;
logs are named review-irq-restart-red, review-owner-clock-red and
review-renew-window-red. They are included in the final green host suites.
No second review cycle or unrelated hardware repetition was performed.

## Final evidence

Command: `tools/build_wsl.py --motion-app --motor-focused --label
2026-11-07-v0_7-motion-final`, SDK2.3.0. Twelve executed stages all exit0 with
zero compiler warnings: legacy controller, portable Motion/producer,
real runtime adapter with pthread SMP-lock substitute, actual PWM driver,
PROBE driver (compile/run each), plus normal and TEST firmware builds.
Eleven unrelated stages are SKIPPED_UNCHANGED_SCOPE, not counted as passes.
Host boundary substitutes do not prove real Cortex interrupt latency or WCET.

Final build-result JSON records commands/source base+dirty state/profile/sizes.
The earlier non-final Motion logs and hashes are superseded by the final run.
All20 pre-existing UF2 hashes match their pre-feature saved values, including
known-working v0.5 and v0.6.1. Final normal ELF contains the LOCAL Motion banner,
new report layout and correct100ms/two-second instructions.

| Artifact | SHA256 |
|---|---|
| AAI05_Motion_v0_7_LOCAL.uf2 | 24cdbbbbdaaa4c167262be38d422b7b2034806d47b180bf1d61cf052eba0b347 |
| AAI05_Motion_v0_7_LOCAL_TEST.uf2 | cce14a9ea09c1ba823af7181a0ddf3df267162b34d7061001a1d964d14ed3f5a |

Normal ELF: text282284, data92, bss24636. TEST: text282840, data92, bss24636.
These are whole-feature build sizes, not a representation-savings comparison.
Host native command/control/output sizes are8/12/3 bytes; theoretical bit
ranges and concurrent-update rules are documented in docs/MOTION_INTERFACE.md.

## Physical work pending

Use docs/MOTION_TEST_V0_7.md: one raised-wheel run to let the two-second producer
cease renewal and expire, then one reset/run to press GP21 during motion.
Do not repeat the older four-pulse/wiring/IR/network checks. This image is
local open-loop only. Simultaneous two-wheel load, actual STOP/timer latency,
powered encoder accuracy, MQTT actuation and tracking remain later work.
