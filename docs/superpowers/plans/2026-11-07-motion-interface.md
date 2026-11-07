# Motion Interface Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline. One final
> fresh review of the feature; preserve the established checkout.

**Goal:** Deliver bounded two-wheel open-loop requests, latched STOP/expiry and
an opt-in local exercise, without adding MQTT movement.
**Architecture:** Portable policy -> synchronized Motion owner -> shared ordered
PWM/alarm driver. The reporter reads snapshots only.
**Tech Stack:** C, micro T-Kernel, RP2040 PWM/timer, GCC host/Arm, SDK2.3.0.
**Spec:** docs/superpowers/specs/2026-11-07-motion-interface-design.md.

## Global constraints and review focus

-100ms timestamp/lease expiry, signed -50..50 duty,10kHz, latched STOP until
 reset; no encoder feedback, automatic upload or actuation.
- Reject stale/future/duplicate-conflicting requests across timestamp/sequence
 wrap; a duplicate cannot extend the deadline.
- Both polarities/wheels stop without transient reversal. Same-duty refresh
 does not interrupt PWM; no-drive remains coasted.
- STOP and command updates share a lock; only owner applies hardware. Watchdog
 expiry cannot be silently restarted by continued task execution.
- Opt-in profile has separate objects/filename/banner and preserves older UF2s.

## Task1: portable control and local producer

Create motion_control.[ch], motion_button.[ch] and test_motion_control.c.
Interfaces: init, submit(command,now), stop(reason), step(now,stop,healthy,
driver_active,output); command contains issued_ms,sequence,left/right_pct.
Local producer exposes init/update and emits at most one two-second run per
boot after a fresh debounced GP20 press. It never emits on a boot-held press.

- [x] Write/run failing policy tests for start, expiry, wrap, duplicates,
  validation, STOP precedence and driver loss; implement and make green.
- [x] Write/run producer tests for boot/hold/debounce and finite renewal.

## Task2: paired hardware output

Extend motor_pulse.[ch] with motor_drive_set(left_pct,right_pct,lease_ms).
Extend actual-driver MMIO checks in test_motor_pulse.c for pairs, zero coast,
same-duty renewal, cutoff, setup failure, invalid input and old/probe profiles.

- [x] Observe a failing pair-output regression, then implement shared ordered
  output/lease handling and pass normal/probe checks.

## Task3: task interface and build/delivery

Add AAI05_MOTION_APP to app.h and make profile, subsystem2 submission/STOP
adapter and owner path, snapshot/report fields and local-exercise banner.
Extend build_wsl.py with --motion-app, strict new-module checks and distinct
AAI05_Motion_v0_7_LOCAL artifacts; keep legacy defaults/artifacts.

- [x] Integrate APIs and run affected controller/producer/pulse/probe host
  checks and both firmware builds. Verify preserved UF2 hashes and profile.
- [x] Review whole feature once, address material findings, write a short
  two-case physical procedure and save truthful status/evidence.

Ruling: current v0.6.1 work was saved as local66b4fbd before this feature.
No push/merge is included. The local exercise validates only the open-loop
interface; accurate powered encoder feedback is a separate prerequisite.

Final review: three Important timing races, no Critical findings. Every issue
was reproduced by an assertion failure before fixes: post-IRQ renewal, expiry
during masked renewal, and a newer other-core timestamp before owner-lock entry.
Fix pass adds a driver-owned expiry latch, checks the old deadline through
rearm, and samples owner time under the state lock. Final verification prefix:
2026-11-07-v0_7-motion-final. No new-image physical pass is claimed.

Final verification:12 executed stages exit0/zero warnings;11 unchanged stages
explicitly skipped;20 preserved UF2 hashes unchanged. All three Important
findings are fixed and covered by failed-before/passed-after assertions.
No Minor findings were reported. Full preparation evidence is recorded in
testing/integration/logs/2026-11-07-v0_7-motion-preparation.md.
