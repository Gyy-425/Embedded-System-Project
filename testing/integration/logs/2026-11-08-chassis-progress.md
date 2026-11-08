# SDD ledger — plan: docs/superpowers/plans/2026-11-08-chassis-motion.md

Execution: inline in existing aai05/bring-up checkout per persistent user preference; base c3312fe. No push/merge/upload/actuation.
Pre-flight: Task1 defines mixer contract consumed by Task2 CLI; exact ranges, clipping, common integer scaling and raw signs agree with spec. Shared sequence/STOP domain uses existing Motion submission.
Baseline: 2026-11-08-chassis-baseline-result.json PASS, 57 stages, 38 older UF2s unchanged, Python/Tcl regression suite included. No hardware tests.
Task 1: in progress.
Task 1: complete (commits c3312fe..3f0129a, tests: python3 .superpowers/sdd/2026-11-08-chassis-motion/verify-task1.py final → ARM strict freestanding compile PASS)

Task 2: in progress. CLI literal/parity/parser/actual run_drive flow tests RED-to-GREEN; eight final methods now include subprocess entrypoint validation. 65-stage canonical check and four firmware builds PASS, zero warnings, 38 old images unchanged.
Environment observation: additional Windows-wide unittest discovery hit 47 sandbox temp access errors (network_config/tape_calibration/sanitizer gate); no production change justified. Canonical WSL suite and Windows Tcl split remain authoritative. Preserve failed log and rerun final Python suite in that established environment after the added test.
Task 2: complete (commits 3f0129a..a895e3c, tests: python3 .superpowers/sdd/2026-11-08-chassis-motion/verify-task2.py 2026-11-08-chassis-ledger-final → PASS: final Python/Tcl regression suite; 65 recorded host/build stages; 38 old UF2s unchanged)

Final review: /root/chassis_motion_review, read-only c3312fe..a895e3c. Important: narrow C input fields can wrap computed296 into40 before validation. No Critical/Minor findings. CLI ground test supported; future C controller boundary requires fix.
Final: Ruling: revise initial spec's input fields from uint8/int8 to int32/int32 — reviewer identified pre-validation wrap; retain raw/stored int8 duties, reject signed limits before arithmetic — cost: transient request16 rather than8 bytes on ARM, no wire/global slot growth; avoids accepting invalid computed commands.
Final: Ruling: reviewer declined loaded ground response — preserve as pending user-operated hardware validation in guide, rather than infer steering/minimum duty/stopping distance from host tests — cost if untested: intended curve or starting duty may not work on the loaded car.
Final fix RED: test_wide_inputs_rejected_before_narrowing rejected296 assertion fails with old byte fields; review-red mixer log retained. Local task1 helper treats nonexact red label as green, so its outer verification exits1 while preserving the real desired assertion failure. No production conclusion inferred from helper exit alone.
Final: fixed pre-validation C narrowing — wide rejection regression RED-to-GREEN, strict sanitized C and ARM PASS; entire post-fix suite/build gate 2026-11-08-chassis-reviewed PASS65stages/71PythonTclmethods/fourbuilds/zero warnings,42oldUF2s unchanged. No further code changes after gate. No deferred minors. Ready for operator ground test, not claimed ground qualified.
