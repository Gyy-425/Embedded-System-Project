# Diagnostic integration test log

Template cases INT-01 and INT-02 for producer/consumer/monitor examples are retired, not reused. The old YYYY-MM-DD/abc1234 row was an unfilled placeholder.

| ID | Check | Method | Result/evidence |
|---|---|---|---|
| INT-101 | Normal and TEST=1 firmware compile/link | tools/build_wsl.py | Actual per-command exit status, compiler output and UF2 hashes in logs/2026-11-05-build-result.json |
| INT-102 | USB boot and repeating reports | Upload named normal diagnostic UF2, open actual Pico COM port | PENDING hardware; no flash performed by agent |
| INT-103 | Combined slow encoder and IR activity | Observe real console while moving a surface and one wheel | PENDING hardware; save real console log |

Host encoder test results are in logs/2026-11-05-host-encoder.log. A TEST=1 build pass means the target test image compiled; it does not mean the on-target assertions ran. Build memory sizes are static sections, not measured whole-system RAM consumption or compact-representation savings.
