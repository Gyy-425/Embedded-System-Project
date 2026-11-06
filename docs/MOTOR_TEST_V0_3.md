# Raised-wheel motor direction test v0.3

**Historical image, superseded:** use [MOTOR_RETEST_V0_3_1.md](MOTOR_RETEST_V0_3_1.md). The original v0.3 GPIO_CTRL argument bug was captured by SWD and corrected in v0.3.1. Do not reload this original image for motor testing.

**Testing paused on 2026-11-06:** the coordinator reports serial output stopping at the first GP20 press, with no visible wheel movement. Use [the v0.3p no-output probe](MOTOR_PROBE_V0_3P.md) before retrying this powered procedure. The original procedure below is retained for reference; motor operation has not passed.

**Subsequent steering:** the coast-only probe completed, and the coordinator now requests connected-motor testing with bank VIN plus laptop USB. Follow [that current controlled procedure](CONNECTED_MOTOR_TEST_V0_3.md), including immediate physical power-off on another reporting stall. No fault correction or connected-motor pass has been established.

This image can drive one wheel only after an explicit onboard GP20 press. It is different from no-drive v0.2. It requests 25% PWM for 400 ms, then removes drive and coasts. A wheel may keep rolling briefly after drive is removed. No floor driving, line following, automatic duty increase or MQTT command exists in this image.

## Controls and sequence

Use the programmable buttons labelled GP20 and GP21 on the purple Robo Pico board. Do not use M1A/M1B/M2A/M2B quick-test buttons: Cytron documents those as full-speed hardware controls outside this program.

| Control | Behaviour |
|---|---|
| GP20 | Press and release for the next single pulse; allow at least 0.1 second for a press |
| GP21 | Remove drive and latch STOPPED until board reset; releasing it does not rearm |
| RST | Restart the four-step test; startup remains idle until a fresh release/press |

| Press number | Electrical channel/polarity | Expected physical observation |
|---|---|---|
| 1 | M1 D1: A high, B pulsed low | Intended left wheel only; record actual direction and encoder sign |
| 2 | M1 D2: B high, A pulsed low | Same wheel, opposite direction |
| 3 | M2 D1: A high, B pulsed low | Intended right wheel only; record actual direction and encoder sign |
| 4 | M2 D2: B high, A pulsed low | Same wheel, opposite direction |

D1/D2 are electrical polarities, not a claim about physical forward. Define physical forward as the rotation that would move the car toward its intended front. Current motor winding contact 5 is wired to M1B/M2B and contact 6 to M1A/M2A; direction still needs this observation.

## Upload and idle check

1. Keep the power bank disconnected. Support the chassis with both wheels clear of the table and wires clear of the wheels. Use the laptop USB connection as the single source for this initial short, single-wheel test; Cytron documents Pico USB as a board-and-motor power option. A successful small pulse does not qualify the supply under full load.
2. Stop Serial Monitor and unplug USB. Hold Pico W BOOTSEL while reconnecting its USB DATA cable. Release when RPI-RP2 appears.
3. Copy output/firmware/AAI05_Motor_Test_v0_3.uf2 onto RPI-RP2. Use the normal image, not _TEST. The Pico should reboot.
4. Open the actual Pico COM port in Windows Serial Monitor at 115200, 8N1, no flow control.
5. Confirm the AAI05 MOTOR TEST v0.3 banner, ENC_READY=1, MOTOR phase=IDLE, next=1, motor err=0 and DRIVE=OFF. Leave GP20/21 released for several seconds. Wheels should not be driven on startup. If initialization fails, save the error and do not press NEXT.

## Verify STOP before motion

1. With the car idle, press GP21. Confirm phase=STOPPED, stop=1 and DRIVE=OFF.
2. Release GP21, then press GP20. No pulse should start while STOPPED.
3. Press the board's RST button, leave GP20/21 released, and reconnect Serial Monitor if its COM session closes. Wait for phase=IDLE next=1 before continuing.
4. Check a fresh power-on too: stop Serial Monitor, unplug USB with the bank still disconnected, wait for the board lights to go out (about five seconds), then reconnect normally without BOOTSEL. Leave GP20/21 released. Reopen the monitor and confirm IDLE, ENC_READY=1, motor err=0 and DRIVE=OFF before any pulse. If this differs from post-upload startup, send the log and stop the test.

## Run the four pulses

1. Record starting encoder L/R, bad, over and cutoff values.
2. Press and release GP20 once. Observe which wheel moves and its physical direction. A normal drive request lasts 400 ms. The program then shows COOLDOWN and waits at least one second; holding GP20 does not start another pulse.
3. Record the changed encoder channel/sign and the first post-pulse MOTOR record with DRIVE=OFF. Wait until phase=IDLE before the next fresh press.
4. Repeat for all four rows in the sequence table. After the fourth pulse and cooldown, phase=DONE next=0; further NEXT presses do nothing until reset.
5. GP21 can stop/latch the test at any point. If the wrong wheel moves, both wheels are driven, a reset occurs, or a wire/motor behaves unexpectedly, use GP21 or remove power and send the observations. If it does not turn at 25%, record whether it twitches/hums and send the log; do not hold it stalled or increase duty automatically.

The cutoff is a timer interrupt independent of the reporting task. On a normal completed pulse cutoff should increase by one and us should be near 400000. us is a software observation of the timer span when the stop function begins, not an oscilloscope measurement of the motor waveform. GP21/error stops can give shorter spans and no cutoff increment. A 500 ms task-level fallback latches a driver fault if hardware capture still reports active; neither mechanism is a hard safety guarantee under CPU/interrupt/hardware failure. Both startup-held buttons and fresh power-on paths are covered by software tests; their physical behaviour still needs this run.

## Interpreting the log

| Field | Meaning |
|---|---|
| phase | IDLE, PULSE, COOLDOWN, DONE or STOPPED |
| next | Pending/current step 1..4; 0 after completion |
| M / D / pct | Last selected motor, electrical polarity and requested PWM percentage |
| us | Last observed pulse elapsed microseconds; zero before completion |
| cutoff | Cumulative automatic timer cutoffs, not a motor-rotation count |
| stop=0/1/2/3 | No latch / GP21 / dependency fault (capture, driver readiness or buttons) / driver start or timeout fault |
| motor err | Initialization/start failure, separate from IR and encoder errors |
| DRIVE | Last published snapshot of requested drive state, not a live rail measurement |

Keep bad and over unchanged during an accepted pulse. Capture faults latch STOPPED; do not treat that as a wiring diagnosis by itself. Encoder count deltas are edges, not distance or RPM. A report appears about every 260 ms, so it may show only one or two active records per pulse. Missing the PULSE line alone is not proof of no activation; use physical observation, encoder deltas, cutoff and post-pulse us together. Main t now uses monotonic kernel operating time rather than the old unset wall clock.

Send 5-10 idle/STOP-check records, then labelled records around each pulse, with a short table: actual wheel, physical forward/backward, L/R count change, bad/over change, cutoff change and final us. No motor-current, power-rail, stall or maximum-speed result is claimed before your measurements.

## Implementation and evidence

The initial task puts bridges in coast, then establishes/validates the 1 MHz watchdog-derived timer tick before yielding to USB/radio service tasks. It never resets the shared timer counter. Pulse initialization deasserts only PWM reset and checks RESET_DONE with a bounded failure path, reserves PWM4/5 and timer alarm0, and runs on core 0. Other kernel/BSP/radio sources are unchanged. PWM5 aliases GP26/27 only when those GPIOs select PWM; this driver leaves their analogue muxes untouched.

The final dated build-result JSON records six sanitized host executables and normal/TEST SMP firmware builds. Host models include cold-reset PWM behaviour, unprepared/paused/stopped timebase rejection, timestamp wrap, expiry during start, operation-by-operation coast/polarity checks, held/bouncing buttons, STOP/fault precedence and all four steps. They do not measure actual IRQ latency, motor ratings or powered encoder rate. TEST firmware compiles additional portable decoder assertions; compilation is not target assertion execution.

References: [Cytron board datasheet](https://docs.google.com/document/d/1X67yKga7m5pugBcogww6pyR2YHXwRJL79_nNDLTYcKU/edit), [Cytron button/motor example](https://github.com/CytronTechnologies/Cytron-ROBO-PICO/blob/main/Demo_Code/circuitpython/code.py), and [board power options](https://www.cytron.io/p-robo-pico).
