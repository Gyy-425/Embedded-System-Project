# v0.6.1 motor diagnostic preparation

7 November2026. Source base9ff4d4b36acd7aa16b97d873b1d1abaeaebc9a8c,
with the saved working-tree changes listed by the accompanying plan.
No upload, power action or motor start was performed by the assistant.

## Verified change and scope

Extracted the original readiness predicate into `motor_test_inputs_ready`.
The new real-controller regression failed against that unchanged policy:
exit134 at the expected ACTIVE-versus-STOP assertion after one left capture
warning, matching the observed early-stop fields. See the warning-red log.
Removing count-quality warnings from diagnostic readiness makes the regression
pass while preserving null, initialization/readiness, driver, button and
saturation fault guards. It also checks GP21 during a warning-bearing pulse
and confirms GP20 cannot resume a latched stop.

The GPIO capture backend, PWM waveform,50% duty,10kHz frequency,400ms timer
and GP21 state-machine behavior are unchanged. Serial output retains all
counters and adds `ENC_UNCERTAIN`. This is a bounded open-loop test policy,
not an encoder fix or permission to control speed from uncertain counts.

## Fresh verification

`tools/build_wsl.py --motor-focused --label 2026-11-07-v0_6_1-warning`
with SDK2.3.0 completed eight executed stages, all exit0 and no compiler
warnings: controller compile/run, pulse compile/run, no-drive probe compile/run,
normal firmware build and TEST firmware build. Eleven unrelated stages were
explicitly skipped, not counted as passes. Build-result JSON records commands,
scope, sizes and artifact hashes. Source/doc whitespace check passed.

All18 pre-existing UF2 hashes matched the saved pre-build hashes. Inspection
of the new normal ELF confirmed the v0.6.1 banner and uncertainty field.

| Artifact | SHA256 |
|---|---|
| AAI05_Motor_PWM_v0_6_1.uf2 | 018a41bc4f56955218c0d227ac367771cee20a38790792316b13244c2da71b09 |
| AAI05_Motor_PWM_v0_6_1_TEST.uf2 | 5b33b5fdfc9b363a070644329efd34787367ab6776f21d40bdf6a6e5b8d8943b |

## Review and pending evidence

One fresh read-only reviewer checked Motion integration, stop priority, cutoff,
counter reporting and successful build records. It identified stale v0.6
handoff links, subsequently corrected in README, remaining-work status and the
historical v0.6 guide. The review session ended at a service usage limit before
a final full-review report; it is not represented as completed independent
approval. A separate author pass inspected the complete source/build diff,
format arguments, test relevance and final guide.

New-image physical testing is pending. Use docs/MOTOR_PWM_V0_6_1.md for one
raised-wheel session covering four requests and an unloaded GP21 input/latch
check. Actual encoder accuracy, cause of ambiguity, GP21 interruption under
load and sustained closed-loop motion remain unverified.
