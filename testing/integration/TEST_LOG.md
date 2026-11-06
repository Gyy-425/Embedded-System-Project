# Diagnostic integration test log

Template cases INT-01 and INT-02 for producer/consumer/monitor examples are retired, not reused. The old YYYY-MM-DD/abc1234 row was an unfilled placeholder.

| ID | Check | Method | Result/evidence |
|---|---|---|---|
| INT-101 | Normal and TEST=1 firmware compile/link | tools/build_wsl.py | v0.1 evidence: logs/2026-11-05-build-result.json; final v0.2 and v0.3: logs/2026-11-06-v0_2-final-build-result.json and logs/2026-11-06-v0_3-final-build-result.json, build exits 0 with no reported warnings |
| INT-102 | v0.1 USB boot and repeating reports | Upload named normal diagnostic UF2, open actual Pico COM port | PASS in coordinator-supplied 2026-11-06 v0.1 console records; reports are spaced 260 ms; no flash performed by agent |
| INT-103 | Combined slow encoder and IR activity | Observe real console while moving a surface and one wheel | PARTIAL 2026-11-06 user-run evidence: IR reads stay valid during wheel tests and both encoder channels respond; accumulated bad=66/60 means accurate counting is not passed |
| INT-104 | v0.2 IRQ readiness and wheel response | docs/ENCODER_RETEST_V0_2.md | PASS for basic manual response in two coordinator-supplied logs: 211 records, ready=1/err=0, bad=0/0 and over=0/0 throughout; intentional forward/backward rotation confirmed. Physical forward-sign convention, speed/angle-labelled qualification remain pending |
| INT-105 | v0.3 idle/STOP latch and four bounded motor pulses | docs/MOTOR_TEST_V0_3.md | PENDING coordinator run; no powered movement or physical timing pass claimed |
| INT-106 | v0.3p GP20 start/cutoff with motor outputs held in coast | docs/MOTOR_PROBE_V0_3P.md | PENDING coordinator run; distinct no-drive diagnostic, not a demonstrated freeze correction |

2026-11-06 update to INT-105: coordinator reports idle/STOP checks worked, but output stopped on first GP20 without visible wheel movement under laptop USB only. The full pulse sequence and motor activation acceptance are NOT PASSED; powered testing paused. Preserved supplied evidence and limitations: logs/2026-11-06-v0_3-gp20-freeze.md. The old pending row remains as history.

v0.3p software evidence: logs/2026-11-06-v0_3p-reviewed-build-result.json records 16 successful steps, seven sanitized host programs and two probe firmware builds, without reported warnings. Both hashes were independently checked; the old powered image is unchanged. A fresh read-only review found no blocking defect or established freeze cause; its minor missing marker in the snapshot-error branch was corrected before the reviewed build. INT-106 remains pending physical execution.

v0.3 software evidence: six host executables (existing decoder/startup/capture/startup plus motor controller and real pulse-driver boundary model), ASan/UBSan and strict warnings; both SMP firmware variants retain the radio driver. Fourteen compile/execution/build steps passed. Expected-failure logs include initial no-drive adapters, premature task deadline, expired setup, PWM held in reset and an unprepared timer clock; see the motor-pulse plan for interpretation.

Preserved v0.1 user evidence: logs/2026-11-06-manual-wheel-v0_1.log and logs/2026-11-06-v0_1-hardware-evidence.md. Final v0.2 software checks consist of four host executables and two firmware variants; host success is not board acceptance. Review fixes and expected failing regressions are documented in docs/superpowers/plans/2026-11-06-encoder-capture.md.

v0.2 user-run evidence and limits: logs/2026-11-06-v0_2-hardware-results.md, with unchanged left-only and both-wheels raw logs beside it. Basic input response is supported; do not infer calibrated edge totals, powered-speed qualification or direction correctness without labelled rotation actions.

Host encoder test results are in logs/2026-11-05-host-encoder.log. A TEST=1 build pass means the target test image compiled; it does not mean the on-target assertions ran. Build memory sizes are static sections, not measured whole-system RAM consumption or compact-representation savings.
