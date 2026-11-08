# Forward and Steering Motion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a bounded forward/steering C API and equivalent practical MQTT CLI without changing existing raw motor commands.

**Architecture:** Pure C mixer produces motion_command_t; a task-context wrapper submits through the existing Motion owner. Python move converts at the CLI boundary and reuses existing MQTT v2 leases and stop confirmation.

**Tech Stack:** C11, micro T-Kernel/Pico SDK2.3.0, Python3, WSL gcc/arm-none-eabi, fatal ASan/UBSan.

**Spec:** docs/superpowers/specs/2026-11-08-chassis-motion-design.md

## Global Constraints

- Existing aai05/bring-up checkout, no push/merge/upload/actuation.
- Forward0..50, turn-50..50; positive turn right; no reverse/pivot.
- Common scaling at peak50; raw left negative/right positive for forward.
- Preserve raw drive, MQTT v2, one writer, STOP/rearm, TTL100ms and driver lease.
- Strict diagnostics on new C, no float/heap, no guessed calibration/trim.
- Previously delivered UF2s must remain unchanged; hardware results pending.

## Review Focus

- Narrow integer conversion must not wrap before input validation; mixed extreme inputs must remain bounded.
- Logical and raw producers share sequences; switching API cannot restart stopped/expired/faulted output.
- Zero forward must never create a pivot or silently act as latched STOP.
- Python/C rounding and steering direction must match over the full valid domain.
- Invalid CLI input must fail before any bridge/network side effect; old raw command semantics must remain unchanged.

### Task 1: Logical mixer and guarded Motion adapter

**Files:** Create subsystem2 motion_chassis.[ch], host test_motion_chassis.c; modify motion_runtime.[ch], test_motion_runtime.c, build_make/mtkernel_3/app_program/subdir.mk and tools/check_encoder_pio.py.

**Interfaces:** Consumes motion_command_t, motion_result_t, subsystem2_motion_submit. Produces motion_chassis_command_t, motion_chassis_convert(const motion_chassis_command_t *, motion_command_t *) and subsystem2_motion_submit_chassis(const motion_chassis_command_t *).

- [x] Write literal vector, null/range, metadata-wrap and exhaustive invariant tests; write actual runtime checks proving raw polarity at the driver boundary and unchanged STOP/expiry/duplicate/conflict/readiness/fault behavior.
- [x] Run tests against unimplemented behavior and preserve RED evidence.
- [x] Implement pure conversion, wrapper and strict object integration; link new source into existing runtime host runner. Preserve input metadata and reject without output mutation.
- [x] Run strict/sanitized C tests, real runtime LOCAL/REMOTE regressions and freestanding ARM compile. Expected: all PASS, no warnings/sanitizer findings.
- [x] Commit independently tested C deliverable.

### Task 2: Practical move CLI and complete compatibility evidence

**Files:** Create tools/mqtt/chassis.py, testing/host/test_chassis_cli.py, docs/CHASSIS_MOTION.md; modify tools/mqtt/drive_cli.py, tools/check_encoder_pio.py and status documentation.

**Interfaces:** Consumes the spec's mixer contract and existing run_drive. Produces chassis_duties(forward, turn)->(raw_left,raw_right) and CLI move with --forward/--turn/--seconds.

- [x] Write literal conversion/type/range tests, subprocess argument rejection before bridge creation, exhaustive parity against the real C vector executable, and run_drive publication/STOP-confirmation checks with controlled transport.
- [x] Run against missing behavior and preserve RED evidence.
- [x] Implement conversion and CLI mapping without new wire opcodes or mutation of existing drive behavior; add shared vector evidence to host runner.
- [x] Run tools/check_encoder_pio.py with a fresh label, SDK path, Windows Tcl Python and --firmware. Expected: complete C/Python regression PASS and four normal/TEST PIO/GPIO compatibility builds, zero warnings, all old UF2 hashes unchanged.
- [ ] Document callable API, exact duty/steering/zero/rounding semantics, commands for the future bounded ground run and hardware limits. Record actual software results; commit, then obtain one independent whole-change review and address material findings with RED-to-GREEN regressions.

No task changes the deployed image, starts a broker, sends a motor request or claims physical tracking.
