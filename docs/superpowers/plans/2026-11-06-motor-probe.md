# GP20 stall: coast-only diagnostic

## Objective and hypothesis

Record the user's actual first-press stall and distinguish a start/timer-path symptom that also occurs without motor drive from one dependent on output release or consequent motor/encoder activity. This is evidence gathering under systematic debugging, not a speculative repair. Original powered v0.3 remains paused and preserved.

## Implementation

- [x] Preserve supplied observations and a verbatim log excerpt in testing/integration/logs/2026-11-06-v0_3-gp20-freeze.md. Exact flashed filename remains unknown.
- [x] Add a host probe policy that requires 11/coast after every operation, including a modeled PWM active phase; reproduce failure on original driver with MOTOR_TEST_PROBE=1 (2026-11-06-v0_3p-no-output-red.log). This red test demonstrates the absence of the new diagnostic policy, not a reproduction of the hardware stall.
- [x] Add MOTOR_PROBE=1 profile and distinct image/profile signature. Keep normal mode default zero and preserve old delivered UF2.
- [x] Retain force-high GPIO overrides in probe mode; leave PWM/timer/IRQ, spinlocks and controller paths shared.
- [x] Label banner and reports PROBE, DRIVE=OFF, describing PULSE as virtual activity.
- [x] Review once using a fresh read-only reviewer: no blocking defect or confirmed freeze cause found. Address its minor missing probe marker in snapshot-error reports.
- [x] Build/hash checks: all 16 steps pass without reported warnings; both UF2 hashes match JSON and original powered UF2 is unchanged. Reviewed no-output image is prepared locally; feature branch save follows.
- [x] Coordinator supplied a continuous M1 D2 virtual request/cutoff/cooldown log and confirmed both wheels stayed still. PASS this captured request; original powered cause remains unresolved. Evidence: testing/integration/logs/2026-11-06-v0_3p-user-results.md.

Build evidence: testing/integration/logs/2026-11-06-v0_3p-reviewed-build-result.json. Seven host programs include original powered-policy and probe-policy pulse fixtures, plus decoder, startup, capture, capture-startup and controller. Two probe firmware variants retain SMP and the radio driver. No shared kernel/BSP/radio source was altered. Host tests use boundary fakes and cannot qualify real SMP lock/interrupt delivery or establish the on-board cause.

Follow-up depends on observed results, not presumed brownout, IRQ deadlock or a COM-port label. Procedure and interpretation matrix: docs/MOTOR_PROBE_V0_3P.md.
