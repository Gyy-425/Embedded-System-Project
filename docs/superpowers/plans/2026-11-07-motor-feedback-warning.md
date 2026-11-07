# Bounded motor diagnostic: encoder quality warning

Continue the existing feature checkout and practical motor bring-up. No PWM,
duty, wiring, capture-backend or mission-controller change is included.

## Evidence and decision

The retained VS Code serial tail shows M1/D1 at50%, STOPPED/stop2,
elapsed159946us, cutoff0, left count1856, irq1857, bad0/0,
over1/0, saturation0, capture ready1/error0 and motor error0. The user
observed the left wheel turning clockwise; viewing direction was not specified.
The lifetime capture-warning check aborts the open-loop pulse and latches STOP.
Later GP21 cannot replace the original stop reason; this does not test its input.

Ruling: encoder ambiguity/invalid-transition counters are nonfatal only in this
fixed-duration open-loop diagnostic, which does not control motion from counts.
Keep every counter and label uncertain feedback explicitly. Preserve fatal
capture/driver/button readiness checks, saturation, latched GP21 STOP and the
independent400ms cutoff. Cost: this can qualify motor polarity and bounded
motion, but cannot qualify encoder accuracy or closed-loop control.

## Execution

- [x] Extract unchanged readiness policy; reproduce the warning-triggered abort
  with the real portable controller, then make warnings nonfatal and preserve
  real faults/GP21. Use BARR-C conventions for new C.
- [x] Publish an encoder-uncertainty flag, version the image v0.6.1 and update
  the practical procedure. Preserve existing images.
- [x] Run affected controller/pulse/probe host checks and both firmware builds:
  eight executed stages passed without warnings; eleven unrelated stages
  explicitly skipped. All18 older UF2 hashes preserved.
- [x] Attempt one fresh read-only review and inspect the complete diff as author.
  Reviewer checked core integration and found stale handoff links, now corrected;
  service usage limit interrupted its final report. Do not claim completed
  independent approval. Evidence: warning-preparation.md in integration logs.

The warning's physical/timing cause is unresolved. No physical result for the
new image may be claimed. No automatic flashing or actuation.
