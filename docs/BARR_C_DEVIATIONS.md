# BARR-C:2018 conformance and recorded deviations

The application C under `app_program/` and the host tests under `testing/host/` follow the supplied *Embedded C Coding Standard* (BARR-C:2018). This file records where the code does not follow it, why, and what limits the risk. It was reviewed on 24 Nov 2026 against the v0.87 frozen follower and the MQTT follow image.

## Followed

| Rule | Practice in this code |
| --- | --- |
| 1.3 Braces | Every `if`/`else`/loop body is braced, with braces on their own lines. The remaining same-line braces in `app_program/` (49, mostly in template-derived files) were moved onto their own lines on 24 Nov 2026. The change is whitespace only: the token streams are identical to the previous commit. |
| 5.2 Fixed-width integers | Fixed-width types (`uint16_t`, `int8_t`, `uint32_t`) throughout; unsigned literals carry `U`. |
| 6.2d–e | Each public function has a prototype in its module header; private functions are `static`. |
| 7.1h–l | Lower-case names with underscores; pointers start `p_`, globals start `g_`. |
| 8.1a | No comma operator in declarations. |
| No magic numbers (8.4a) | IR sensor-model values in `black_follow.c` (`IR_RAW_MAX`, `IR_NORM_FULL`, `IR_NORM_CLEAR`, `IR_BORDER_FULL_RAW`, `IR_AMBIGUOUS_RAW`, `IR_BLUE_EDGE_RAW` and the lane centres) are named constants. GP20/GP21 use `MOTOR_NEXT_PIN`/`MOTOR_STOP_PIN`, and the network snapshot age uses `DRIVE_SNAPSHOT_FRESH_MS`. Timing and tolerances come from `config/follow_config.h` (`FOLLOW_IR_FRESH_MS`, `FOLLOW_JOIN_MAX_MS`, …). |
| Compiler warnings | Host tests build with `-std=c11 -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes`, and the firmware builds with 0 warnings. |

## Deviations

### D1 — Rule 7.1m: Boolean names do not start with `b_`

- **Scope:** all `bool` variables and fields in `app_program/` (454 declarations on 24 Nov 2026).
- **Reason:** the project consistently uses C99 `bool` names phrased as states (`ambiguous`, `ir_ok`, `rgb_target_matched`). The status fields are also field names in the serial and MQTT telemetry and the host decoders (`tools/mqtt/`, `tools/check_*.py`). Renaming would change the reported field names and touch every module just before the demo.
- **Mitigation:** the type is always `bool` (never an integer flag), so `-Wconversion` catches integer/boolean mixing. New code should use `b_` names once the demo is over.

### D2 — Rule 6.2a: functions over 100 lines

| Function | Lines | Reason it is long |
| --- | --- | --- |
| `black_follow_step` (`subsystem2/black_follow.c`) | 611 | Follower state machine for the frozen v0.87 track profile. Each bend cue sits under its own `FOLLOW_*` compile-time flag. |
| `black_follow_owner_step` (`subsystem2/black_follow_owner.c`) | 602 | Owner loop: freshness, colour join, lease and per-phase overrides, likewise flag-gated. |
| `report_task` (`subsystem3/subsystem3.c`) | 351 | Formats all serial report lines in one task body. |
| `red_exit_lead`, `motion_task`, `motion_runtime_step`, `rgb_sensor_step`, `report_bend` | 101–166 | Single-purpose routines slightly over the limit. |

- **Reason:** `black_follow_step` and `black_follow_owner_step` hold the frozen v0.87 behaviour, which was tuned on the track run by run (`testing/integration/logs/2026-11-24-track-runs-v082-v087/`). Splitting them now would change the code that was track-tested without a track session to re-check it.
- **Mitigation:**
  - Host tests exercise the flag-gated branches. The follow checker builds and runs 446 stages across flag combinations, and checks that the firmware builds.
  - Each branch carries a comment naming the run that motivated it.
- **Planned fix:** extract each `FOLLOW_*` cue into a `static` helper returning an override. Do it after the demo, with a track re-run.

### D3 — Rule 8.4a: tuning literals inside bend cues

- **Scope:** duty percentages (for example `output.forward_pct = 30`), per-cue normalised IR thresholds (120, 240, 320, 600…) and cue timings (for example `>= 500U` ms) inside the flag-gated cues of `black_follow.c` and `black_follow_owner.c`.
- **Reason:** each value belongs to one cue, and the value was tuned against the specific logged run named in the comment beside it. Turning each into a separate global constant would separate the value from that reasoning.
- **Mitigation:**
  - The shared sensor model (raw ceiling, full scale, clear level, border level, lane centres, blue edge level) is named; see "Followed".
  - The host tests assert the resulting duty values at these thresholds, so a changed literal fails a test.

## Tests

`testing/host/test_black_follow_owner.c` used to keep all scenarios in one 2,400-line `main()`. It is now split as follows:

- **Profile functions:** one per track profile (`check_home_track_profile`, `check_clipped_loop_profile`, …). `main()` only selects one.
- **Scenario functions:** one per flag-gated scenario that starts from a fresh owner (`check_red_exit_lead`, `check_blue_confirm_grace`, …). A failing assertion therefore points to a named scenario.
- **Shared fixtures:** shared RGB readings are now file-scope `static const` fixtures, kept under the same compile-time conditions.

Two functions are still over 100 lines:

- `check_clipped_loop_profile` (536 lines);
- `check_red_bend_after_handoff` (508 lines).

They step one owner through a continuous track sequence, and each step depends on the previous state, so they cannot be split without changing what is tested.
