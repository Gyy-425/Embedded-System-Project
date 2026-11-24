# Home-track runs v0.82–v0.87 (room 2) — derived evidence

## Provenance

The raw serial output was pasted into `Downloads/test-records.txt` after each test session. That file was overwritten each time and is no longer present, so **no raw serial log survives**. The v0.80 and v0.81 runs left no saved extract at all.

Each `v0.8x-derived-summary.txt` here is the output of `runs.py`, run on the pasted log of that version at analysis time. The extracts were made during development (17–23 Nov 2026) and copied here unchanged. `runs.py` only filters and re-prints fields:

- the `FOLLOW[...]` stop report (stop time, reason, elapsed time, RGB and IR at the stop, join time, phase flags);
- every `BEND[...]` trace line (IR raw, RGB class, turn, duty, join state, encoder counts).

It does not compute or alter values. Section headers (`===== testN`) are the labels the tester typed into the paste.

## How to read a section

- A section lists every distinct `FOLLOW` stop report it contains. The report persists until the next run, so a section often repeats the previous run's stop first.
- **The run in a section is the stop whose `stop_t` lies inside that section's `t=` range.**
- **Duplicated pastes.** If two sections show the same `t=` range, the same log was pasted twice. That is not a new run.

## Outcomes per version

All runs used the home track: black borders, blue lane, then a right-hand corner into the red lane.

| Version | Run | Stop reason | Elapsed (ms) | Phase flags reached | Note |
| --- | --- | --- | --- | --- | --- |
| v0.82 | test1 | JOIN_TIMEOUT, RGB UNKNOWN | 3740 | blue_exit → red_handoff | — |
| v0.82 | test2 | JOIN_TIMEOUT, RGB UNKNOWN | 2360 | blue_exit only | — |
| v0.82 | test3, test4 | — | — | — | Duplicate pastes of test2 (same t range) |
| v0.83 | test1–4 | — | — | — | All four sections duplicate v0.82 test2; this extract holds no v0.83 run |
| v0.84 | test1 | DRIVER (stopped by operator) | 4540 | all red-corner flags | — |
| v0.84 | test2 | JOIN_TIMEOUT | 3280 | blue_exit, red_slow, red_align, red_handoff | — |
| v0.84 | test3 | JOIN_TIMEOUT | 4040 | incl. red_release, red_clear_pivot | — |
| v0.84 | test4 | JOIN_TIMEOUT | 4340 | incl. red_exit_turn, red_exit_align | — |
| v0.85 | test1 | JOIN_TIMEOUT, RGB RED/MATCH | 3540 | incl. red_grace | — |
| v0.85 | test2 | JOIN_TIMEOUT | 5760 | all red-corner flags | — |
| v0.85 | test3 | JOIN_TIMEOUT | 4420 | all red-corner flags | — |
| v0.85 | test4 | JOIN_TIMEOUT, RGB UNKNOWN | 2840 | blue_exit, red_slow, red_align, red_handoff | — |
| v0.86 | test1 | JOIN_TIMEOUT, RGB UNKNOWN | 3560 | incl. red_release | — |
| v0.86 | test2 | JOIN_TIMEOUT | 3780 | blue_exit, red_exit, red_slow, red_align, red_first, red_handoff | — |
| v0.86 | test3 | JOIN_TIMEOUT | 4640 | blue_exit, red_exit, red_slow, red_first | — |
| v0.86 | test4 | JOIN_TIMEOUT, RGB UNKNOWN | 2360 | blue_exit only | — |
| v0.87 | test1 | JOIN_TIMEOUT | 3200 | blue_exit, red_slow, red_align, red_handoff | — |
| v0.87 | test2 | JOIN_TIMEOUT, RGB UNKNOWN | 4720 | blue_exit, red_exit, red_pivot, red_slow, red_first | — |
| v0.87 | test3 | JOIN_TIMEOUT | 4700 | all red-corner flags | — |

## Findings

1. **Each failure ended safely.** No run completed the red corner. Every run that was not stopped by the operator ended in `JOIN_TIMEOUT`: the colour join window ran out, and the follower stopped the motors instead of driving on with no lane colour. This matches the intended safe-stop behaviour.
2. **The car got further from v0.84 on.** From v0.84, most runs reach the red-corner phases (`red_exit`, `red_pivot`, `red_first`, `red_handoff`) and run 4–5.8 s before stopping. v0.82 runs stopped at 2.4–3.7 s. The later versions get further through the corner, but they still fail at different points from run to run.
3. **v0.82 extension bug.** `RGB UNKNOWN` at the stop in v0.82 led to finding that UNKNOWN frames were compared against the packed classification code (0x17). The red-leading join extension therefore never applied. The fix uses `rgb_classification_class()` and is covered by a host test built on real `rgb_classify()` output.
4. **Frozen baseline.** v0.87 is the frozen baseline (commit 4a2dd75). The red corner is still variable and is a known open issue.

## Not covered by this evidence

- **v0.80, v0.81, v0.83.** No saved extract holds these runs. The v0.83 conclusions from development were drawn from logs pasted into chat that were not saved.
- **Operator observations** (for example "nearly finished the turn", or which side of the border the car ended on). These were reported verbally and are not in the extracts.
- **The MQTT follow image** (`follow_mqtt_*`, IR-700 ambiguity threshold). It was host-tested only (`eval-20261124-*-result.json`); no track run of it is recorded here.
