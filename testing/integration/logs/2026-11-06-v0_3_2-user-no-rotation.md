# Actual50% run: cutoff continues, no clear wheel rotation

The coordinator supplied60 serial reports at260ms spacing, t=47840..63180, and reports sound but no visible motor movement. Raw attachment is preserved byte-for-byte in the matching .log; parsed results are in the matching .json. The log contains all four50% requests despite the preceding guide requesting only the first before review. Record what actually happened without treating extra requests as qualified passes.

| Request | Reported cutoff us | Selected encoder change | Cumulative irq |
|---|---|---|---|
| M1D1 | 400012 | L:-1->0 | 1->2 |
| M1D2 | 400006 | L:0->-1 | 2->3 |
| M2D1 | 400005 | R:0->1 | 3->4 |
| M2D2 | 400007 | R:1->0 | 4->5 |

The excerpt begins at irq1, so its earlier event is not classified. Four further one-count/one-IRQ changes coincide with the selected motor requests, with opposite signs for opposite polarities. These are not zero-edge results, but do not establish sustained rotation. A small rotor/gear nudge or electrical disturbance is plausible; no cause is measured. Encoder A remains high in the observed AB states, while B changes3<->2 for the selected wheel. No invalid/overrun/error growth, serial pause or reset is shown; the final state is DONE, cutoff4, DRIVE=OFF. Firmware-reported cutoffs are not a measurement of terminal PWM/voltage/current.

Basic motor operation remains NOT PASSED. Do not raise firmware duty, repeat the sequence or change the pin map based on this log alone. Firmware pin/address review, corrected macros and host successes are retained; they do not certify the actual bridge output or supply.

Next is a separate manufacturer-documented motor-button check, with the application stopped by a cold BOOTSEL entry so GPIO coasting commands cannot interfere. See docs/HARDWARE_MOTOR_BUTTON_CHECK.md. This explicitly uses ONE brief full-speed M1A hardware-button tap, not another GP20 request. The firmware400ms cutoff is unavailable in BOOTSEL; release/power switch are the stop controls. No hardware-button result has been supplied yet, and no additional firmware image is prepared.
