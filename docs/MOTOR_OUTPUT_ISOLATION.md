# Normal-output check with motor windings disconnected

The coordinator subsequently chose [connected testing with bank VIN and laptop USB](CONNECTED_MOTOR_TEST_V0_3.md). This unloaded method remains an alternative, not a required step before networking or that connected test.

Purpose: the v0.3p coast-only request completed. Now separate normal GPIO/PWM output release from connected motor load and rotation-triggered encoder activity. Use the preserved normal v0.3 image only with all four winding leads disconnected below. This is not a retry of the original connected-motor test and does not prove a particular cause in advance.

## Disconnect with all power removed

Stop Serial Monitor. Unplug Pico USB and keep the bank unplugged; wait for lights to go out. Label the four winding leads, then loosen the carrier screw terminals to remove them. Leave the wires insulated/separated so exposed ends cannot touch the board or each other.

| Empty terminal for this test | Lead to remove and label for later reconnection |
|---|---|
| M1A | Left motor connector contact6 |
| M1B | Left motor connector contact5 |
| M2A | Right motor connector contact6 |
| M2B | Right motor connector contact5 |

These are the four motor **winding** leads at the black M1/M2 screw terminals. Keep the six-pin harnesses on the motors and leave encoder contact1/4 (GND/3V3), A/B GP4/5 and GP16/17, and both IR AO/supply connections unchanged. Do not alter green VIN +/- or remove encoder 3V3 as a substitute. If the four winding wires cannot be identified by their terminal labels, stop before rewiring.

## One request

1. With both winding pairs disconnected, support the wheels clear of the table and reconnect the laptop USB only, holding BOOTSEL until RPI-RP2 appears.
2. Copy **output/firmware/AAI05_Motor_Test_v0_3.uf2**, normal without _TEST. This is deliberately the original normal-output image, not NO_DRIVE. Verified SHA256: `4a1af5d9b09026536aa1239bfb2ce1b7c5d3a6897837919c0c09a48aef4002e0`.
3. Open the actual Pico COM port at115200, 8N1, no flow control. Release GP20/21 and wait for healthy IDLE next=1, ENC_READY=1, motor err=0 and DRIVE=OFF. The banner should identify AAI05 MOTOR TEST v0.3; no MOTOR_MODE=PROBE marker is expected. Keep output recording from before the request.
4. Press GP20 for approximately0.1-0.2 seconds and release **once**. Do not rotate wheels manually or press hardware M1/M2 test buttons. Neither wheel can be driven through the disconnected winding leads.
5. Watch for at least3 seconds. Expected progression: PULSE (possibly brief) -> COOLDOWN -> IDLE next=2, cutoff=1, us near400000. DRIVE=ON during PULSE is the logical output request; it does not mean a disconnected wheel moved. Encoder counts should stay unchanged if wheels remain still.
6. Save two seconds before through three seconds after, or final records if reporting stops. State all four terminal winding wires were disconnected and whether reporting continued. If output stops, close monitor and unplug USB. If either wheel moves, disconnect power and recheck the winding disconnection.
7. Stop after this first request. Leave windings disconnected for initial network work. Only reconnect them later with **all power removed**, using the table; do not reconnect while this image is powered.

## Interpretation

- If this check still stops reporting, connected motor windings/rotation are not required to reproduce the symptom. Investigate output switching/peripheral/software/reporting interaction; the board bridge still switches even unloaded, so this is not proof of a software-only cause.
- If this check completes but the earlier connected-motor test did not, output release alone is insufficient to reproduce that earlier observation. Connected load/noise, encoder interrupt activity or changed timing remain candidates. Do not declare brownout proven or reconnect/increase duty automatically.

The car is not cleared for floor driving or closed-loop control by this check. The previous basic IR/manual encoder checks need not be repeated. Wi-Fi/MQTT bring-up can proceed with windings disconnected while the powered-motion investigation remains open.
