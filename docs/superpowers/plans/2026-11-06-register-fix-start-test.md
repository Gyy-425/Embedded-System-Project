# Register repair and motor-start test implementation plan

> **For agentic workers:** Use superpowers:executing-plans for inline execution. The user explicitly authorised correcting the three reviewed definitions and proceeding to the next diagnostic image; no further design decision is missing.

**Goal:** Repair the inherited GPIO definitions and produce a distinctly named50%/400ms button-triggered normal motor diagnostic.

**Architecture:** Keep the existing driver/controller, pin map, timer cutoff and STOP path. Update GPIO_CTRL/GPIO argument parentheses and GPIO_OUT offset; increase the normal diagnostic's maximum/requested duty to50%. Retain all existing binaries and issue v0.3.2 artifacts instead of overwriting v0.3.1.

**Tech stack:** C, course micro T-Kernel RP2040 BSP, Pico SDK2.3.0, existing Ubuntu build runner and sanitized host fixtures.

**Spec:** docs/PIN_MAP_AUDIT_2026-11-06.md recommendations and coordinator's latest explicit instruction; docs/MOTOR_RETEST_V0_3_1.md bounds.

## Constraints and review focus

- Existing firmware checkout/feature branch aai05/bring-up is clean and dedicated to this task; preserve the user's established hardware/artifact paths and main branch.
- Fixed map: GP8/9 M1, GP10/11 M2; GP26/27 ADC; encoder4/5 and16/17; onboard20 NEXT/21 STOP.
- Exactly one wheel,50% requested duty,400ms timer cutoff, no boot motion/automatic restart/duty escalation, same STOP/cooldown/fault handling.
- No hardware request or automatic upload; output is a reviewable local UF2 plus instructions.
- Independent register constants must catch wrong SIO offset and conditional/arithmetic GPIO macro expansion.
- All four wheel/direction combinations at50% must obey polarity, keep the other wheel in coast, preserve ADC muxes and cut off without a task running.
-51% and401ms rejected; wrap/reset/expiry/fault behavior remains covered.
- UART stays disabled; radio initialized, not a Wi-Fi/MQTT connection claim.
- No credentials/raw target dumps or built binaries committed.

## Task1: shared register definitions

- [x] Add testing/host/test_rp2040_registers.c importing real sysdef.h, checking independently documented addresses and conditional/arithmetic arguments; run on current code and save expected failure.
- [x] Apply the three exact replacements from the audit; rerun to green and recheck all66 addresses using the existing SDK oracle.
- [x] Integrate the register regression into tools/build_wsl.py.

## Task2: next bounded image

- [x] Extend the real pulse fixture to exercise50% in all four combinations, independently sample the period to verify50% drive/50% coast, require rejection above50%; show expected failure on the existing25% cap.
- [x] Set MOTOR_PULSE_MAX_DUTY to50; update normal banner/default runner version and artifact basename to0.3.2. Keep existing400ms limit and controls unchanged. No generic profile/configuration feature is needed for this one diagnostic step.
- [x] Run the full existing runner (all host tests, normal and TEST firmware), Python helper suite, artifact hashes/old-image preservation and review diffs.
- [x] Independent whole-change review; no material findings, user procedure published. Save this completed record with the implementation on the existing feature branch; verify the remote hash after pushing. Physical pulse/motor success remains pending user evidence.

## Progress / rulings

- Ruling:50% is a diagnostic setting selected by the assistant and stated to the user, not a measured motor rating or proven cure; retain400ms and one-wheel limits.
- Ruling: update the single current normal diagnostic rather than add a general duty-profile feature; preserve old25% UF2/hash and Git history for comparison. The coast-only host branch is also tested at the new cap but its old released UF2 is not rebuilt/overwritten.
- Task1 complete: expected seven address/expansion failures captured before repair, then register regression and independent SDK oracle green66/66.
- Task2 implementation/verification/review complete:50% acceptance failed before cap update; all19 runner steps and10 Python helpers pass; normal/TEST artifacts/hash verified, no Critical/Important review issue. Reviewer could not independently run WSL; primary fresh execution/evidence is preserved.
- Software completion contract met; the implementation and this record are saved together. User procedure is docs/MOTOR_START_TEST_V0_3_2.md; hardware upload/motion remain pending coordinator action, with no assistant upload or motion performed.
