# Diagnostic integration test log

Template cases INT-01 and INT-02 for producer/consumer/monitor examples are retired, not reused. The old YYYY-MM-DD/abc1234 row was an unfilled placeholder.

| ID | Check | Method | Result/evidence |
|---|---|---|---|
| INT-101 | Normal and TEST=1 firmware compile/link | tools/build_wsl.py | v0.1 evidence: logs/2026-11-05-build-result.json; final v0.2: logs/2026-11-06-v0_2-final-build-result.json, both build exits 0 and no reported warnings |
| INT-102 | v0.1 USB boot and repeating reports | Upload named normal diagnostic UF2, open actual Pico COM port | PASS in coordinator-supplied 2026-11-06 v0.1 console records; reports are spaced 260 ms; no flash performed by agent |
| INT-103 | Combined slow encoder and IR activity | Observe real console while moving a surface and one wheel | PARTIAL 2026-11-06 user-run evidence: IR reads stay valid during wheel tests and both encoder channels respond; accumulated bad=66/60 means accurate counting is not passed |
| INT-104 | v0.2 IRQ readiness and wheel response | docs/ENCODER_RETEST_V0_2.md | PENDING coordinator run; no v0.2 hardware result claimed |

Preserved v0.1 user evidence: logs/2026-11-06-manual-wheel-v0_1.log and logs/2026-11-06-v0_1-hardware-evidence.md. Final v0.2 software checks consist of four host executables and two firmware variants; host success is not board acceptance. Review fixes and expected failing regressions are documented in docs/superpowers/plans/2026-11-06-encoder-capture.md.

Host encoder test results are in logs/2026-11-05-host-encoder.log. A TEST=1 build pass means the target test image compiled; it does not mean the on-target assertions ran. Build memory sizes are static sections, not measured whole-system RAM consumption or compact-representation savings.
