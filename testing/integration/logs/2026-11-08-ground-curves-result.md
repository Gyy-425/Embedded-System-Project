# Basic ground motion bring-up: both requested curve directions confirmed

After the neutral straight check, the user reports the curves work, with the
same small stopping movement as the straight command. The phrase "right curve"
was ambiguous, so the assistant asked whether it meant correct directions or
both commands turning right. The user explicitly confirmed:
`+10 right; -10 left—both correct`.

The preceding procedure requests forward40/turn+10/seconds1 for the right
curve, then forward40/turn-10/seconds1 for the left, using bank VIN and no
laptop-to-Pico USB. Expected raw duties are -50/+30 and -30/+50 respectively.
This result records the user's physical confirmation following that procedure;
new CLI/serial records were not inspected for these particular curve trials.
No trial count beyond the two requested directions, measured angle/radius,
stopping distance, capture diagnostics or timing is invented.

## Scoped completion

Basic motion bring-up is complete within the recorded setup:

- Software/raw motor operation, physical forward polarities and bounded
  Motion/MQTT control were established in the earlier scoped records.
- The powered PIO capture trial passed within its recorded setup; calibrated
  count/revolution and maximum-rate accuracy were not claimed by it.
- Three neutral untethered runs were reported straight while powered, with
  a small leftward settling movement after output OFF.
- The user now confirms both intentional curve directions and stopping,
  retaining the same small residual stopping movement.

Keep turn0 as the straight baseline and retain the current firmware/wiring.
No permanent steering trim, PID, brake-mode change or new test image is added.
These basic straight/curve cases need no replay merely to advance the project.
Exact stop/heading performance, combined-load timing/rail measurements and
track-end behaviour remain later integrated acceptance work. Earlier transient
connection/ACK failures remain unexplained; current successful communication
does not establish their cause or a software fix. If calibrated encoder
speed/distance feedback is selected, establish its scaling/reference first.

## Next project case

Proceed to IR colour calibration and runtime classifier/reporting integration.
The portable classifier and stationary capture tool already exist. Actual
black/white/five-colour tape samples and the intended background are still
needed to establish separability, thresholds and recognition accuracy; their
availability has not been newly confirmed. Synthetic host ranges remain test
fixtures, not a robot calibration. Live colour reporting and target-colour
following have not passed yet.

No production-code edits, firmware upload, automatic robot command or new
physical test was performed in recording this result.
