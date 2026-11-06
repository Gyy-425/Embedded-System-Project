# Motor Pulse Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline. Steps use checkbox syntax.

**Goal:** Deliver a button-triggered, one-wheel-at-a-time v0.3 motor-direction test with bounded pulse duration and automatic stop.
**Architecture:** Portable debounced four-step controller in Motion; one locked RP2040 PWM/timer driver with independent IRQ0 cutoff. Existing IR/encoder services continue.
**Tech Stack:** C, existing micro T-Kernel/Pico W port, Ubuntu GCC host/Arm toolchains, SDK 2.3.0.
**Spec:** docs/superpowers/specs/2026-11-06-motor-pulse-design.md.

## Global Constraints
- Fixed 25% duty, 400 ms, 1000 ms cooldown, four steps; no drive on boot or held NEXT.
- GP20 NEXT / GP21 latched STOP; no M1/M2 hardware-test buttons and no USB/kernel/shared-library changes.
- Keep motor mappings and v0.2 encoder driver. Never claim physical direction, powered counts or a rail measurement from compilation.

## Review Focus
- Mux/PWM transition cannot energise the wrong wheel or reverse polarity transiently.
- Timer expiry during startup/stop, wrap and IRQ/task state publication cannot re-enable a stopped pulse.
- STOP and sensor faults beat NEXT; held/bouncing inputs cannot chain pulses.
- Timer/PWM ownership conflicts and driver failures leave outputs coasted.
- New snapshot fields/console labels distinguish electrical polarity, request duration and observed elapsed duration.

### Task 1: Controller, pulse driver, integration and delivery
**Files:** Create motor_test.h/c, motor_pulse.h/c and two host test executables; modify subsystem2.c, app.h/app_main.c, subsystem3.c, build runner and guides/logs.
**Interfaces:** motor_test_init/update/abort operate on pure controller state and return NONE/START/STOP; motor_pulse_init/start/stop/read are the sole runtime output driver. Fixed caps live in motor_pulse.h.
- [x] Baseline: run all four existing host executables and verify current feature branch.
- [x] Add tests first and demonstrate missing explicit-start/pulse functionality against v0.2 no-drive adapters.
- [x] Implement controller and real PWM/timer driver; run strict/sanitized host tests, including driver register-operation interleavings.
- [x] Integrate GP20/21 inputs, readiness/fault gating, monotonic time and v0.3 reporting; update runner with distinct logs/images and all regression executables.
- [x] Build normal/TEST variants, obtain one fresh code review, reproduce/fix material findings with regressions and rerun affected checks.
- [x] Verify hashes, write exact raised-wheel procedure, save checkpoint and commit/push existing personal feature branch. Physical results stay pending.

## Ledger
- Ruling: continue authorized solo development inline in the existing feature checkout. Do not ask routine design/worktree questions again.
- Ruling: physical GP20/21 controls replace the contemplated serial trigger because current USB service discards RX; this avoids unauthorized shared-service changes and adds no wiring.
- Pre-flight: driver and controller share fixed caps; Motion translates START into a driver call and latches failure. Report consumes copied status only.
- Baseline: WSL had discarded temporary executables; all four were rebuilt from the unchanged source and passed. This was a missing scratch artifact, not a project test failure.
- RED/GREEN: v0.2 no-drive adapters failed explicit START/driver-init assertions, then the real controller and pulse driver passed. Expected failure logs are 2026-11-06-v0_3-control-red.log and pulse-red.log.
- RED/GREEN: soft-deadline and expired-start regressions exposed early task-clock truncation and a pulse that could leave coast after a setup deadline. The controller now gives the 400 ms hardware cutoff a 100 ms margin and latches a fault at 500 ms, while the driver checks expiry before releasing overrides. Both new executables passed after correction.
- Fresh review motor_pulse_review found two Important cold-boot issues: PWM reset was not released under USE_PTMR=0, and WATCHDOG_TICK's microsecond divider was not established by flash boot. Both were reproduced by reset-aware fakes in cold-pwm-red.log/cold-clock-red.log, then passed after correction.
- Final: fixed PWM initialization — deassert only PWM reset through the atomic alias and require RESET_DONE within 10000 bounded polls. Test preserves unrelated reset bits, models ignored PWM writes during reset and covers a stuck reset.
- Final: fixed timer initialization — priority-1 initial task establishes/validates 12 MHz reference/divider and 1 MHz watchdog-derived tick before yielding to USB/radio services; never reset the shared timer counter. Reject paused/reset/incompatible timebases and an active watchdog when retiming would be required. Validate counter advancement with bounded polling. Pulse initialization revalidates configuration before ready.
- Final suite: all 14 compile/execution/build steps pass with no reported warnings in testing/integration/logs/2026-11-06-v0_3-final-build-result.json. Six host executables use strict warnings and ASan/UBSan; normal and TEST images retain SMP and the radio driver. Hashes independently verified: normal 4a1af5d9b09026536aa1239bfb2ce1b7c5d3a6897837919c0c09a48aef4002e0; TEST 16fc127da7f60f1fd6a11beb0064405f34c573b2fd505770ff39010052bc4a79.
- Final: Ruling: actual motor ratings/current, physical polarity, IRQ latency/max encoder rate, hardware faults and on-board v0.3 acceptance remain physical validation, as the reviewer identified. Fixed low duty and finite pulses support that next test; they are not a measured motor/supply qualification.
- Final: minor (deferred): compact console labels D and us retained; the motor-test guide explicitly defines electrical polarity and software-observed elapsed time rather than implying physical forward or oscilloscope timing.
- Task 1: software complete, image and detailed guide prepared, existing feature branch retained. No merge to main and no agent-performed flash or powered car test.
