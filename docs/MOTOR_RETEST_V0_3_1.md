# Raised-wheel retest after the captured HardFault correction

## Verified state

Normal v0.3.1 is already installed through SWD. No UF2 copy is needed for this retest. Image: output/firmware/AAI05_Motor_Test_v0_3_1.uf2, SHA256 `30abbae451e37d54b6734c802a2ada3c38902ec979d5d3816ab15b01f288c9a1`. Original v0.3 is preserved for fault evidence; do not reload it.

All ELF flash load bytes matched independent readback. The last 96 UF2 padding bytes are zero while ELF programming leaves them erased FF; these bytes are beyond the ELF load image. OpenOCD reported a reset-init `CX` argument error during upload despite successful programming/verification. After the coordinator physically removed/reconnected bank power, SWD confirmed motor ready=1/error=0/inactive, encoder interrupt masks enabled, GPIO8..11 SIO/high and PWM4/5 disabled. The incomplete debugger-reset startup is recorded separately from this successful cold boot.

No post-fix physical motor pass is claimed. The corrected register writes pass the actual-board-macro host regression and normal/TEST firmware builds; real movement, polarity, encoder behaviour and cutoff need the steps below.

## Return to USB Serial Monitor

1. Leave GP20 untouched. Switch the car OFF, unplug bank power, and wait for all board lights to go out. Disconnect probe USB and D-to-DEBUG while the target is unpowered. Leave motor, encoder and IR wiring connected.
2. Connect the Pico W USB data cable to the laptop normally, without BOOTSEL. Reconnect the existing 5 V bank VIN supply with the board switch OFF, then turn ON with GP20/21 released. Cytron documents simultaneous USB/VIN; source selection is automatic, so this does not establish isolated logic/motor power or bank-only motor current.
3. Open the actual Pico COM port (COM3 if still assigned) at 115200, 8N1, no flow control. Expect the v0.3.1 banner if captured, continued reports, `ENC_READY=1`, `phase=IDLE next=1`, `err=0`, `DRIVE=OFF`, `cutoff=0` and unchanged bad/over counters. Stop before requesting motion if startup differs.
4. Before a pulse, hold/support the solid chassis steadily with both drive wheels clear, fingers and wires away from tyres, and the power switch reachable. Resting is fine during idle; do not run these pulses on the floor.

## First request

Press GP20 once for about 0.1-0.2 seconds, then release. Expect M1 D1: only the intended left wheel briefly turns at 25% for a requested 400 ms, then drive is removed. Reporting should continue through COOLDOWN to `IDLE next=2`, `cutoff=1`, `us` near 400000 and `DRIVE=OFF`. The left encoder should respond, with bad/over unchanged and no error. The report interval is about 260 ms, so missing the short PULSE line alone is not failure; use the completion fields and physical observation together. A wheel can coast after drive is removed. Reported elapsed time is not an instrumented measurement of output timing.

If reporting stops, motion is unexpected, or drive removal is doubtful, switch OFF immediately and disconnect both power sources. Do not depend on GP21 after a firmware stall. If output/cutoff continues but the wheel stays still or only twitches, stop and record it; do not increase duty or use the full-speed M1A/M1B/M2A/M2B hardware buttons.

Send the idle and first-request serial records plus which wheel moved, its physical direction and the observed stop. The next three requests are conditional on that first result being healthy.

## Remaining requests after the first passes

Wait for IDLE before each fresh GP20 press. Keep wheels raised throughout.

| Press | Request | Required observation |
|---|---|---|
| 2 | M1 D2 | Left wheel reverses relative to D1; left encoder trend reverses; cutoff=2 |
| 3 | M2 D1 | Right wheel only; record physical direction/right encoder delta; cutoff=3 |
| 4 | M2 D2 | Right wheel reverses; right encoder trend reverses; cutoff=4, then DONE next=0 |

D1/D2 are electrical polarities; actual forward direction is assigned from observation. Stop on wrong-wheel response, missing encoder response, bad/over growth, driver/capture errors or reporting loss. GP21 is the responsive-firmware STOP and latches until reset; a deliberate STOP test must also show that releasing it and pressing GP20 does not rearm. Perform that test separately with raised wheels after the four-step observations; reboot restores idle.

A passing run establishes only basic raised-wheel output, routing, reversal and reported cutoff under this setup. Full-load power integrity, maximum encoder rate, wheel distance calibration, colour tracking and network operation remain separate work. Wi-Fi/MQTT is next; basic IR/manual encoder checks need not be repeated.
