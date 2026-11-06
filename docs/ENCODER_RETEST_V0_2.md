# Encoder retest: diagnostic v0.2

The image captures both edges of GP4/5 and GP16/17 through one core-0 GPIO interrupt dispatcher. Motion's approximately 20 ms snapshot publication does not control edge acquisition. Counts are raw decoded edges, not distance. No motor PWM or drive command is implemented.

## Upload

1. Stop Serial Monitor. Disconnect the laptop USB and power bank.
2. Support the chassis with both wheels clear of the table. Leave the power bank disconnected and do not press M1/M2 test buttons.
3. Set the carrier switch ON. Hold Pico W BOOTSEL while plugging its USB DATA cable into the laptop, then release when RPI-RP2 appears.
4. Copy output/firmware/AAI05_Diagnostics_v0_2.uf2 to RPI-RP2. The drive should disappear and the Pico restart. Do not use the _TEST variant for the first retest.
5. Open the Pico's Windows COM port in VS Code Serial Monitor at 115200, 8N1, no flow control. Re-select the port after upload if needed.
6. Confirm the v0.2 banner or the new ENC_READY/irq/over fields in repeating lines. DRIVE=OFF must remain present. Disconnect USB immediately if the wheels drive unexpectedly.

## New fields

| Field | Meaning |
|---|---|
| ENC_READY=1 err=0 | Capture initialization succeeded; the err after ENC_READY is separate from the IR err |
| irq | Cumulative serviced encoder IRQ batches; not an exact count of individual edges |
| bad=L/R | Invalid sampled A/B transitions |
| over=L/R | Ambiguous/coalesced edge batches, including new flags during the GPIO sample; no movement is guessed from these batches |
| ENC age | Age of the published snapshot, not time since the last wheel movement |
| sat=0 | Signed counts have not reached their representable limit |

Error and overrun counters are cumulative until reset. Compare their values before and after each action. A zero count of detected errors does not establish the maximum reliable rate; GPIO edge flags cannot retain an unlimited event history.

## Sequence

1. Stationary for 10 seconds: capture should settle to ENC_READY=1 err=0, with stable wheel counts, bad and over. Save 5-10 lines.
2. Left wheel only: turn gently and slowly in one direction, stop, then reverse. L should change and reverse its trend; R should stay steady. Save labelled records.
3. Left wheel only: repeat at a comfortable faster hand speed. No unusually precise movement should be needed. Avoid violent flicking or forcing the gearbox. Note whether counts keep responding and whether bad/over increase.
4. Repeat the slow/reverse/faster sequence for the right wheel. R should respond while L stays steady.
5. Leave both still for another 10 seconds and check for continued count or error changes.

If desired, mark a wheel and compare count changes for the same full rotation slowly and faster, starting/ending at that mark in the same direction. Record actual deltas; do not assume counts per revolution or demand equal counts for unmatched rotation angles.

Send labelled stationary, left slow/faster/reverse and right slow/faster/reverse output. If ENC_READY remains 0, report the adjacent error and stop the wheel test. If bad or over increases, record the trial and speed description; do not rewire merely to suppress the diagnostic. This retest qualifies only the observed manual conditions, not powered motor speed or wheel odometry.

## Software evidence

testing/integration/logs/2026-11-06-v0_2-final-build-result.json records four host executables (decoder, no-drive startup, capture and register/kernel-boundary startup) plus normal/TEST firmware builds. All passed with no reported compiler warnings. The capture suite covers 4000 edges per wheel between task reads, independent wheels, reverse/bounce, invalid/coalesced events, unrelated event preservation and two late-edge interleavings. The backend startup suite executes the real backend with host boundary fakes; actual SMP interrupt timing remains a board-validation item.

Review found and regressions reproduced an acknowledge-before-sample race and a shared-vector core-1 ownership conflict. Both were corrected before delivery. v0.2 has not yet been run on the robot by the development agent or confirmed by coordinator logs.
