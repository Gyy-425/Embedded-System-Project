# Tape Calibration Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline. One final fresh review; user authorization for autonomous implementation persists.

**Goal:** Prepare durable stationary tape capture and honest per-sensor overlap reporting.
**Architecture:** Existing read-only v2 STATUS/Bridge -> capture/CSV+metadata -> offline report.
**Tech Stack:** Python3.10+, standard library, unchanged v0.8 C firmware.
**Spec:** docs/superpowers/specs/2026-11-07-tape-calibration-design.md.

## Global Constraints

-40 samples,30s deadline,500ms settling; reject movement/session change/2s stale stream.
- Exclusive files; partial captures preserved; no motor commands; no invented thresholds.
- Existing feature checkout, no push/merge or device/network action by the agent.

## Review Focus

- Queued telemetry/reconnect must not become fresh labelled data.
- Abrupt termination/incomplete captures must not become a complete dataset.
- Different physical setups or class names must not be silently pooled.
- Sensor interval separation must not be presented as measured recognition accuracy.
- Existing files and tested motor/client behavior must remain intact.

### Task1: capture and report tool

**Files:** tools/mqtt/tape_calibration.py (capture/report),
tools/mqtt/calibrate_cli.py (CLI), testing/host/test_tape_calibration.py,
docs/TAPE_CALIBRATION.md, current README/checklist and integration evidence.

**Interfaces:**
`capture_samples(bridge, status, output, *, label, name, setup, device='robot01', height_mm=None, sample_count=40, timeout=30.0, settle_ms=500, clock=time.monotonic) -> dict`;
`report_captures(root: Path) -> dict`; `write_report(report: dict, output: Path)`.
Consumes real DriveAck/DriveTelemetry, Bridge.receive and existing synchronise.

- [x] Write deterministic capture/report tests for spec cases; run and observe RED.
- [x] Implement capture/persistence and validated offline report, then CLI.
- [x] Run new tests plus existing v1/v2 client/codec tests with saved C vectors; Expected: all PASS, no network/device access.
- [x] Run CLI help and subprocess report/exclusive-path checks; Expected: readable output and nonzero error on reuse.
- [x] One fresh review; reproduce material findings RED, fix, rerun affected suite.
- [x] Save evidence, instructions, status/checkpoint and local feature commit.

Ledger: base c8b96b9; pre-flight: existing Bridge/codec consumed unchanged.
Ruling: capture first, not classifier — actual tape measurements are missing;
classification quality cannot be established without them. No physical data is
fabricated. Native C/minimum-bit runtime remains unchanged in this case.


Completion ledger,7 November2026:
- Initial missing-module RED recorded; first host functional run hit sandbox
  default-temp permissions, not a product failure. Verified workspace temp
  directory resolved that infrastructure issue.15 initial feature tests passed.
- Corrupt metadata-state validation reproduced TypeError then passed with a
  clear ValueError. No real tape samples were generated.
- One fresh reviewer found two Important timing issues: ordered backlog could
  renew freshness and a receive pause could evade gap/deadline checks.
- Final fixed: three regression assertions RED->GREEN; enforce post-receive
  deadline/gap and1s absolute relative host/device elapsed divergence fence.
- Final tests:18 calibration +24 existing CLI +4 existing codec =46 passed;
  syntax/help/subprocess offline report/exclusive-output checks passed.
- Ruling: relative elapsed guard can conservatively abort on delayed initial
  STATUS or host scheduling; it cannot prove absolute source age. Cost: recapture
  under stable delivery, rather than accepting misleading labelled data.
- Ruling: dataset files are flushed per sample and partial states retained;
  filesystem/power-failure durability is not guaranteed. No disk-recovery claim.
- No deferred minor findings, no re-review, firmware normal/TEST hashes unchanged.
- Local feature commit only. No broker/device command, firmware upload or actual
  capture. Actual tapes/background unavailable per user; classifier is later.
