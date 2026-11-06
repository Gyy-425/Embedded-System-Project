# Encoder Capture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Produce a no-drive v0.2 diagnostic that captures encoder edges independently of the coarse task delay.

**Architecture:** One kernel GPIO IRQ dispatcher on core 0 updates the portable decoder under an interrupt-preserving SMP lock. Motion publishes coherent snapshots; Reporting displays readiness, IRQ and loss diagnostics.

**Tech Stack:** Pico W/RP2040, existing micro T-Kernel port, C, existing WSL Arm GCC/Pico SDK toolchain.

**Spec:** docs/superpowers/specs/2026-11-06-encoder-capture-design.md

## Global Constraints

- Pins: left GP4/5, right GP16/17. Motor drive remains disabled.
- No shared kernel/BSP/radio changes or kernel-tick changes.
- Preserve v0.1 artifacts and user-run evidence; on-target v0.2 execution stays pending until actual logs arrive.
- Continue the existing feature checkout; retain the six pending documentation corrections.

## Review Focus

- GPIO interrupt ownership/core affinity: refuse conflicting event ownership and configure NVIC on the pinned core.
- ISR/task concurrency: snapshot copies and updates share an interrupt-preserving SMP lock, with no blocking kernel objects inside.
- Lost edge ordering: coalesced batches are flagged and resynchronised without invented counts.
- Unrelated GPIOs: acknowledge only the four encoder pins' edge bits, never level bits or unrelated events.
- Startup failure and motor safety: visible capture failure, motor coast retained, no PWM added.

### Task 1: Capture, integrate, and deliver the diagnostic

**Files:** Create app_program/subsystem2/encoder_capture.h, encoder_capture.c, encoder_capture_rp2040.c and testing/host/test_encoder_capture.c. Modify app.h, app_main.c, subsystem2.c, subsystem3.c, tools/build_wsl.py and the bring-up/module/test documents.

**Interfaces:** encoder_capture_reset(void), encoder_capture_irq(void), encoder_capture_read(encoder_capture_t *), int32_t encoder_capture_start(void). The portable capture struct contains both encoder_state_t records, IRQ count and left/right overrun counts. Backend hooks provide lock/unlock, GPIO levels, pending status and W1C acknowledge. Motion snapshot carries capture readiness/error and these diagnostics.

- [x] Run existing host encoder and motor-startup tests as the baseline.
- [x] Add capture tests first; observe failure before implementation. Cover full cycles between snapshots, independent wheels/reverse, initial states, coalesced edges, diagonal rejection and unrelated event preservation.
- [x] Implement the portable capture service and RP2040 backend, then run the host suite with -Wall -Wextra -Werror and ASan/UBSan.
- [x] Pin Motion to processor 1, initialize capture there, publish failure/readiness, remove task-side encoder decoding, and add v0.2 reporting.
- [x] Extend the build runner for explicit version/date labels and capture tests, preserving old logs/artifacts. Run normal and TEST=1 firmware builds with Wi-Fi driver active; inspect all results/hashes.
- [x] Obtain a fresh whole-change review, fix material findings with regression coverage, and rerun affected checks.
- [x] Update evidence/checkpoint and provide the new image plus a short stationary/slow/fast single-wheel retest. Commit the software/evidence milestone; do not claim a board pass.

## Execution ledger

- Ruling: continue the existing feature checkout and inline implementation because this continues the authorized diagnostic workflow; no worktree relocation or repeat design questionnaire.
- Pre-flight: capture API and snapshot fields must agree across Motion/Reporting; backend initialization runs only on pinned processor 1. The two existing portable regression executables remain required.
- Baseline: existing decoder and motor-startup executables passed on the current source in Ubuntu before implementation.
- RED: task-only polling adapter failed test_cycles_between_task_reads at expected left.count==4000 assertion; logs/2026-11-06-capture-legacy-red.log. This is a host model of the old acquisition limitation, not an execution of old firmware on the robot.
- GREEN: actual capture service passed the edge-stream tests with sanitizers.
- Fresh read-only review: encoder_capture_review found two Important issues and no minor findings. Acknowledge-before-sample could invent reverse motion; the shared global vector table made any core-1 GPIO event owner conflict.
- Final: fixed late-edge race — test_edges_after_ack_do_not_invent_reverse_motion failed before correction (logs/2026-11-06-capture-race-red.log), then passed after pre-ack sampling and a pending-latch consistency check. A second test covers edges arriving during GPIO sampling. Ambiguous batches are flagged and resynchronised without guessed deltas.
- Final: fixed shared-vector ownership — real-backend host startup test failed before correction (logs/2026-11-06-capture-ownership-red.log), then passed after refusing all enabled/forced GPIO events on either core.
- Final suite: all 10 compile/execution/build steps in logs/2026-11-06-v0_2-final-build-result.json have exit 0 and no reported compiler warnings. Four host executables use ASan/UBSan; normal and TEST firmware use SMP=1 with Wi-Fi driver active. Both UF2 hashes were independently verified after the build.
- Final: Ruling: maximum edge rate, physical polarity, stationary electrical noise and on-board v0.2 acceptance remain user-run validation, as the reviewer identified. The no-drive image is supplied for that validation; the cost of inadequate IRQ latency is a failed trial requiring further driver/hardware investigation, not a claimed speed-feedback pass.
- Task 1: software complete. Existing aai05/bring-up branch is retained; no merge to main is performed. Hardware acceptance remains pending. Final artifact SHA256: normal c662dc2f4159267287bba2170baf5244ff7241fb743a3fa59ab1824c25d81c51; TEST b332da2a6a76c683dc59268c8bb09a7e54cea1ccceadfc53012d5378459e1c35.
