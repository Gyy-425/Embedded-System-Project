# Black-follow R2 ground trial, 17 November 2026

Source: `2026-11-17-black-follow-r2-ground-serial.txt`; parsed observations and
source SHA256: `2026-11-17-black-follow-r2-ground-analysis.json`.

User reports several trials: two appeared to stop immediately; other trials
went straight with a small turn near the end. The supplied capture contains
90 reports at 260 ms intervals, beginning after one early stop, then three
observed running segments. Do not count repeated STOPPED reports as new stops.

## Recorded outcomes

- The three observed running segments each finish COMPLETE at elapsed 1000 ms.
  All 12 RUNNING reports have RGB BLACK/MATCH; all reports show PIO_BAD=0.
- The capture starts STOPPED/AMBIGUOUS with elapsed=140 ms. In the current
  controller this reason means both normalized IR positions reached at least
  600 during RUNNING (raw thresholds L928/R988). It does not name the RGB
  classifier's ambiguity result. The triggering sample is outside this log;
  the later norm=0/425 and ready=1 represent recovered current inputs while
  the stop reason remains latched. The physical cause is not established.
- RGB is BLACK/MATCH in 89/90 reports. The single UNKNOWN/NO_MATCH at t91000
  occurs after the second run completed, so it did not stop an observed run.
  All printed IR and RGB acquisition-valid flags are 1.
- The other user-reported early stop cannot be diagnosed from this capture.

| Run | RUNNING elapsed ms | Turn samples | Finish |
|---|---|---|---|
| 1 | 100,360,620,880 | +2,0,-8,-8 | COMPLETE 1000 ms |
| 2 | 160,420,680,940 | +1,-4,-8,-8 | COMPLETE 1000 ms |
| 3 | 80,340,600,860 | 0,-7,-8,+2 | COMPLETE 1000 ms |

This supports the user's reported improvement over the previous strong
alternating corrections. It does not establish route accuracy or curve
following. Maximum left correction still occurs while DRIVE=ON; a small end
turn cannot be attributed solely to coasting. Independent IR/FOLLOW snapshots
and 260 ms reporting cannot reconstruct the exact path or every control tick.

## Practical next action

Keep D40/T8 and R2 unchanged. Start with the RGB chip over black, both IR heads
over cardboard, low similar normalized values and ready=1; release GP20
before a fresh press. Do not widen RGB or disable the both-high guard based
on this capture. If an early stop recurs, retain the transition and its reason;
controller trigger snapshots would be more useful than another large colour
calibration capture. No firmware change, flash or hardware acceptance claim
is made by this analysis.
