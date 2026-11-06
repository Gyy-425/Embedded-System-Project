# Connected-motor test with USB and power-bank VIN

**Paused after user-run repeat stall on 2026-11-06:** reporting stopped again and no wheel motion was visible. The user switched off power. Do not repeat this procedure unchanged. Follow DEBUG_PROBE_SETUP.md for connection-only preparation; no fault cause or motor pass is established.

The coordinator requests connected-motor testing and proposes adding the existing power bank while retaining laptop USB. This replaces the winding-disconnected check as the next requested procedure; that alternative remains available if needed. Keep all motor/encoder wiring connected for this test. The earlier USB-only serial stall remains unresolved, and no powered motor pass is claimed yet.

## Verified power arrangement

Cytron's [Robo Pico datasheet](https://www.farnell.com/datasheets/4248509.pdf), specifications on printed page5, explicitly lists USB and VIN connected together. VIN accepts3.6-6V. Use the bank's labelled **5V/2.1A USB output**, through the existing red+/black- lead into the carrier's green VIN terminal, while laptop USB connects to Pico W. Do not change the bank voltage or the GPIO/encoder supplies.

This board selects its power source automatically. Two attached inputs do not provide separate isolated logic/motor supplies or prove that the bank alone carries motor current. The datasheet describes motor-rail behaviour as depending on relative USB/VIN voltage. This is a controlled change in the connected supply arrangement, not a demonstrated solution to a power fault.

## Preparation and upload

1. Stop Serial Monitor. Switch Robo Pico OFF and disconnect both USB and the bank before handling wires. Confirm M1A/M1B/M2A/M2B leads are held firmly and no exposed ends touch adjacent terminals. If previously removed, restore left contact6->M1A, left5->M1B, right6->M2A, right5->M2B. Leave encoder power/A/B and IR connections unchanged.
2. Support the chassis steadily with **both drive wheels clear of the surface**, cables clear of the tyres, and the board power switch accessible. Do not hold a wheel or try the test on the floor.
3. With bank still disconnected, enter BOOTSEL over laptop USB as before (board switch ON while flashing), and copy the normal **output/firmware/AAI05_Motor_Test_v0_3.uf2**. This is the motion-enabled original image, not either NO_DRIVE probe or _TEST. SHA256 reverified: `4a1af5d9b09026536aa1239bfb2ce1b7c5d3a6897837919c0c09a48aef4002e0`.
4. After flashing, close any monitor and turn the board OFF. Keep laptop USB attached; plug the existing VIN lead into the bank's5V/2.1A output and activate the bank if needed. Turn Robo Pico ON with GP20/21 released.
5. Open the actual Pico COM port (COM3 if still assigned) at115200/8N1/no flow control. Expect the v0.3 MOTOR TEST banner, IDLE next=1, ENC_READY=1, motor err=0 and DRIVE=OFF. No wheel should be driven at startup. If there is an error/unexpected movement, switch OFF and stop here.

## First pulse and stop conditions

Press and release **GP20 once**, holding it about0.1-0.2seconds. It requests M1 D1 at25% for400ms. Observe the physical wheel and record the surrounding serial output. Keep a hand ready at the board power switch, clear of wheels/wiring.

Expected: intended left wheel alone turns briefly, its encoder changes, then COOLDOWN and IDLE next=2; cutoff=1 and us near400000. bad/over stay unchanged and errors remain0. The report interval is about260ms, so use post-pulse cutoff/elapsed/counts as well as any brief PULSE record. A wheel may coast after drive is removed. The timer span is software-reported, not an oscilloscope measurement.

If serial output stops again, unexpected motion occurs, or there is doubt that drive was removed, **switch OFF immediately and disconnect both laptop USB and bank**. Do not repeatedly press NEXT or depend on GP21 after a firmware stall. When firmware is responsive, GP21 is the normal latched STOP control. The software cutoff is not a guarantee under a stall or hardware failure.

If the wheel does not turn, record whether it twitches/hums and whether the log/cutoff continues. Stop rather than increasing duty or using M1A/M1B/M2A/M2B hardware test buttons, which Cytron documents as full-speed controls. No movement at this setting does not by itself establish a defective motor.

## Remaining pulses, only after the first passes

Proceed only if reporting continues, the selected wheel visibly moves and stops receiving drive, the expected encoder responds, and no capture/driver fault appears. Wait for IDLE after each cooldown, then use one fresh GP20 press for each row.

| Press | Channel/polarity | Record |
|---|---|---|
| 1 | M1 D1 | Actual wheel, physical direction, L/R encoder delta, cutoff1 |
| 2 | M1 D2 | Same wheel, opposite direction, opposite encoder trend, cutoff2 |
| 3 | M2 D1 | Intended right wheel only, direction/right encoder delta, cutoff3 |
| 4 | M2 D2 | Same wheel, opposite direction, opposite encoder trend, cutoff4 |

D1/D2 describe electrical polarity, not verified physical forward. After the fourth pulse/cooldown expect DONE next=0. Stop on a wrong-wheel response, missing encoder response, bad/over growth, nonzero error, reset or reporting stall; do not automatically continue the sequence.

Send the serial records and observations, including that bank VIN plus laptop USB were connected. A passing run supports basic loaded output/routing/direction/cutoff under those conditions. It does not prove source current sharing, rail stability, encoder counts/revolution, maximum rate or the original failure's cause. Wi-Fi/MQTT remains the next priority after this result; no other basic IR/manual encoder test is required.
