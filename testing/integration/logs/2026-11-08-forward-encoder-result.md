# Forward encoder run: signs established, capture quality not qualified

Source: the user's pasted BEFORE/DURING/AFTER records and their prior physical
confirmation that both wheels move forward with raw duty left=-50, right=+50.
[Preserved serial records](2026-11-08-forward-encoder-user-serial.txt).
The supplied text is sufficient for this assessment; no new VS Code inspection
or robot command was needed.

## Established observations

| Item | Observation | Conclusion |
|---|---|---|
| Motor forward polarity | Raw left -50, right +50; user confirms both physically forward | Keep left motor sign -1 and right +1 for a future logical-forward interface. |
| Raw encoder signs | Left monotonically decreases, right increases in every supplied moving interval | Forward encoder deltas have left negative and right positive. Normalize at a defined feedback boundary, with overflow-safe signed arithmetic. Do not rewire A/B. |
| Command renewal | RUNNING seq3 through seq94 in epoch1, lease100, err0 | Movement snapshots show accepted renewed output in this case. |
| Output stop | By t187200, STOPPED, seq100, epoch1, reason1, zero duties/lease, DRIVE OFF, err0; remains OFF | Requested software output stop is observed. Exact actuation-stop latency and physical rest time are not measured. |
| Readiness | ENC_READY1, capture err0, Motion err0, bad0/0, sat0 throughout | No reported initialization/capture API/driver error, invalid transition or saturation in these samples. This does not establish count accuracy. |
| Capture quality | over0/0 at t185120, over32/30 at t186940 while ON; over39/36 by t187720 | New ambiguous capture batches occur within the same powered epoch. Feedback accuracy is not qualified. |
| Reporting | Increasing uptime and 260 ms report spacing through the supplied moving/stopped interval | No reset or report freeze is visible in that interval. No power-rail or worst-case timing claim. |

## Exact comparisons and limits

BEFORE ends at t53040 with zero counts and warning counters. The first DURING
sample is t185120: a 132080 ms gap, not an immediately adjacent pre-start
snapshot. The gap does not prevent identifying new warnings in the continuous
moving interval itself: over rises from 0/0 to 32/30 while DRIVE is ON in epoch1.
It does prevent claiming an uninterrupted complete recording from baseline.

Between t185120 and t186940 (1820 ms), raw counts change from -58/+58 to
-9203/+9204: deltas -9145/+9146. These are accepted decoder counts, not an
independent reference for real wheel revolutions, speed or lost-edge percentage.
Their close magnitudes do not demonstrate balanced physical speeds.

After output is OFF, counts continue from -10453/+10479 at t187200 to
-11248/+11772 at t187980. This is consistent with the current driver's intended
coast stop (`motor_pulse_stop_locked` in motor_pulse.c); output OFF is not a
mechanical-rest detector. The last two records have right 11736 ->11772, so
the last supplied right count is not a demonstrated fully settled total.
Do not infer a wheel-speed mismatch from these end values.

The newly found saved CLI artifact `encoder-forward-20261108-153002.txt` contains
a fresh OFF response with seq7, tag a376afe1. The pasted run stops at Motion
seq100. Do not silently identify that CLI file as the matching acknowledgement
for this serial run. Preserve it as a separate artifact with an unverified run
association; the serial data itself provides the output-OFF evidence here.

## Code trace and next development step

`encoder_capture_irq` reads owned GPIO pending bits, samples AB and rechecks
the pending bits. Multiple latched edge bits for a wheel, or pending bits
changing around the sample, mark a batch ambiguous. `capture_wheel` increments
the saturating overrun counter, resynchronizes AB and deliberately skips the
count delta. GPIO edge latches do not preserve the event order/multiplicity
needed to recover it. `bad=0` is therefore compatible with missing uncertain
counts: ambiguous batches bypass `encoder_update`'s invalid-transition check.

This code trace establishes the warning's meaning, not the physical cause.
Interrupt-service latency/interrupt-masked regions and electrical transitions
remain possible contributors. The logs contain no measured latency or waveform
that distinguishes them. Do not label the wiring faulty, blame Wi-Fi alone,
hide the warnings, guess replacement counts or claim calibrated speed/PPR.

Motor bring-up and both forward sign mappings now have sufficient evidence.
No further basic pulse/direction/MQTT replay is needed. The next development
case should address powered capture quality with Wi-Fi active: inspect IRQ
latency and critical sections, then choose a justified capture change (including
PIO if CPU-serviced latches cannot meet the rate). Use the preserved trace as
the failing hardware acceptance case. No production capture code was changed
in this assessment, and no new firmware build is necessary for this record.
Before encoder feedback is accepted, require the revised implementation to
pass a focused capture run plus an independent count/revolution reference.
