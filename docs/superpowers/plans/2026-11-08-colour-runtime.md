# Colour Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Connect the existing IR classifier to acquisition, serial and read-only MQTT reporting without invented tape calibration.

**Architecture:** IR task owns a private immutable boot model and bounded transition ring behind its existing mutex. Network owner drains reports after motion service; existing v2 controls stay compatible.

**Tech Stack:** C11, micro T-Kernel, Pico SDK, Python3, WSL GCC/ASan/UBSan, Mosquitto client tools.

**Spec:** docs/superpowers/specs/2026-11-08-colour-runtime-design.md

## Global Constraints

- In-place aai05/bring-up; no upload/actuation/push/merge.
- Default100ms age; missing calibration only, never synthetic deployment bounds.
-16 queued transitions; uint32 wrapping sequence/time; saturating uint32 drops.
- Independent left/right packed results; no debounce or automatic motion.
- Separate23-byte colour v1 frame; existing motion v2 bytes unchanged.

## Review Focus

- A brief colour between equal neighbours must survive a slow reporting poll.
- Cached valid samples must become stale; wrap must not reject recent samples.
- Busy locks/publication failures must not block the motion owner or lose data silently.
- Reconnect must not label old queued events with the new session.
- Corrupt/partial/overlap models and malformed result encodings must never become a known colour.

### Task1: Runtime state and IR ownership

Files: create subsystem1/colour_runtime.[ch], config/colour_calibration_config.h;
modify subsystem1/subsystem1.[ch]; add testing/host/test_colour_runtime.c and
test_ir_runtime.c. Runtime API init/update/latest/pop/reset is caller-serialized.
Export subsystem1_colour_latest/pop/begin/take via subsystem1.h, using TMO_POL.

- [x] Write host assertions for the five review risks applicable to runtime,
      including all seven synthetic classes, one-sample transition and overflow.
- [x] Observe RED against minimal failing runtime stubs.
- [x] Implement core, immutable boot config and IR locked publication.
- [x] Run strict sanitized runtime and real-adapter host tests GREEN.

### Task2: Binary report and read-only watch

Files: create subsystem3/colour_protocol.[ch], colour_service.[ch],
colour_platform_rp2040.c; tools/mqtt/colour_codec.py, colour_cli.py; tests
test_colour_protocol.c, test_colour_service.c, test_colour_codec.py.
Hook link/poll into mqtt_drive_app.c after existing motion service;
serial reads latest safely. Service accesses platform bool begin/take,
publish and now functions; no motion APIs.

- [x] Write failing C golden-vector and service retry/reset/heartbeat tests,
      Python valid/malformed frame and read-only watch tests; observe RED.
- [x] Implement exact23-byte encode/decode, bounded reporting and client.
- [x] Verify C/Python vectors, session resets, retries and idle heartbeats GREEN.
- [x] Verify existing drive service still passes with colour hooks substituted.

### Task3: Build evidence and handoff

Files: build_make/mtkernel_3/app_program/subdir.mk strict-source rule; tools/check_encoder_pio.py;
docs/COLOUR_RUNTIME.md, README.md, REMAINING_WORK.md and core/capture guides.

- [x] Add new checks to the canonical full host/compatibility runner and build
      distinctly named colour v0.10 candidates in a new ignored directory.
- [x] Run full host suite and four firmware builds; read results, zero warnings,
      preserve every prior UF2 hash. Do not flash.
- [x] Review complete diff independently; fix important findings with RED/GREEN.
- [x] Record actual evidence/remaining tape and hardware work; commit locally.

## Progress ledger

- Design/plan: direct focused execution selected by user; no new permission
  handoff. No model values, tape names, measurements or agreements invented.
- Task1: runtime stub failed the missing-model assertion; real IR adapter test
  failed on the absent integration APIs. Implemented and strict ASan/UBSan
  checks pass, including every-sample intermediate class, wrap and overflow.
- Task2: protocol/service stubs failed expected assertions; Python decoder
  rejected the valid fixture before implementation. All new C/Python tests,
  cross-language golden vector and old drive-service checks now pass in the
  canonical runner.70 WSL Python tests plus 6 Windows Tcl tests passed.
- Ruling: begin and event-or-snapshot take are atomic IR-mutex operations,
  rather than separate reset/read calls, to prevent reconnect/snapshot races.
  Cost if wrong: reordered observations; actual scheduling still needs measurement.
- Task3: canonical runner PASS,73 stages,76 Python/Tcl tests, zero compiler
  warnings; all four PIO/GPIO normal/TEST builds pass,46 earlier UF2s unchanged.
- Final review: independent colour_runtime_review found no Critical/Important
  defect; no product fix pass required. Delayed snapshots retain copy-time
  results/sample timestamps as explicitly documented, not current control data.
- Final: minor (deferred): read-only watch resolves unused mosquitto_pub through
  Bridge; subscriber-only installs fail. Established laptop bundle has both
  tools. Lazy publisher resolution is a later usability change.
- Branch outcome: keep aai05/bring-up locally under the user's established
  preference, no push/merge/upload; no worktree cleanup is applicable.
