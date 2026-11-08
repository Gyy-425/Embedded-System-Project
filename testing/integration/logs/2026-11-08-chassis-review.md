# Independent motion-interface review and correction

One read-only review by /root/chassis_motion_review covered c3312fe..a895e3c,
the design/plan, C mixer/runtime boundary, Python CLI, build integration,
tests and practical guide. No reviewer edits, actuation or recursive review.

## Finding and actual correction

Important: the original uint8/int8 request fields could narrow a controller's
computed value before validation. Forward296 could become40 and be accepted.
The byte-domain test did not establish correct handling of wide computations.
No Critical or Minor findings were reported.

Regression `test_wide_inputs_rejected_before_narrowing` assigns computed296,
negative values and mixed INT32_MIN/MAX values. With the old byte fields,
the assertion that296 must be rejected fails (review-red mixer log). The
temporary test-only conversion-warning exception used to demonstrate that
old boundary was removed after the fix; production warnings remain strict.
The local verification helper's review-red label selected its green expectation,
so it exited1 while preserving the real intended C assertion failure; do not
interpret the helper wrapper error as a different product failure.

The final request uses signed32-bit inputs. Both bounds are checked before
any mixer arithmetic or narrowing. Invalid requests leave the raw output
unchanged; existing raw/stored int8 duties, Motion ownership and MQTT v2 remain
unchanged. Python boundary cases include296 and signed32 extremes too.
Strict sanitized mixer/LOCAL/REMOTE tests and freestanding ARM compile pass
after the fix. Full post-fix regression/build run uses label
`2026-11-08-chassis-reviewed`; its result JSON records the final outcome.

## Decisions and cost

- Revise initial spec's narrow input fields to signed32 computation values.
  This prevents invalid computed commands from becoming valid duties before
  checking. ARM layout measured with arm-none-eabi-gcc and nm: temporary
  request16 bytes, validated raw command8 bytes. No new global slot or wire
  format is introduced. This is a validation tradeoff, not representation
  savings. Compiler measurement output:

```text
00000000 00000010 B chassis_request_bytes
00000010 00000008 B raw_command_bytes
```

- Reviewer declined to judge loaded starting duty, physical steering and
  stopping distance. Keep them pending for the user-operated ground trial;
  guessing could leave a car that hums, steers poorly or coasts farther than
  expected. Host tests establish command intent/limits, not that response.

The reviewer found the original CLI ready for the documented operator-driven
test on installed v0.9 because Python validates before generating raw duties.
The future C boundary is now corrected and verified separately. There is no
second review; the material finding is closed by RED-to-GREEN regression and
the whole-suite/build gate. Final gate PASS:65stages, zero warnings,71 Python/
Tcl methods, four firmware configurations and42 existing images unchanged.
Ready for the documented ground trial; no physical ground test occurred in
this development task.
