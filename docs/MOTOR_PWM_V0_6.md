# v0.6 practical motor check

Historical procedure: v0.6 produced left-wheel motion but an encoder capture
warning latched an early STOP. Use [v0.6.1](MOTOR_PWM_V0_6_1.md) for the next
session; the original procedure below records the previous image.

Use **output/firmware/AAI05_Motor_PWM_v0_6.uf2**, the normal image without `_TEST`. This is the next focused motor case. No wiring change, repeat IR/manual-encoder check or network retest is needed.

## What changed

The selected motor input now receives direct50% PWM and the other input staysLOW, matching Cytron's example. Frequency10kHz and maximum pulse400ms are unchanged. The running off-state is00/brake; boot, STOP and cutoff still force11/coast. Start/stop ordering prevents an opposite-polarity transient. Host tests and compilation support this code path; physical motor movement and the cause of the previous no-motion result remain unverified.

This motor image uses USB diagnostics and radio background work, with MQTT disabled. The separately verified v0.5 communication image is preserved. Selecting a colour over MQTT is not how this motor check starts.

## One setup, one conditional session

1. Put a stable support beneath the chassis so both wheels are clear. Keep the power switch reachable. Close Serial Monitor, switch OFF and disconnect bank power and Pico USB for upload. Keep the existing motor/encoder/IR wiring connected.
2. Hold BOOTSEL while reconnecting Pico USB, release when RPI-RP2 appears, and copy the **normal v0.6 UF2** above onto it.
3. After flashing, switch the board OFF, connect the existing bank-to-VIN power lead, then switch ON. Keep Pico USB connected for serial data, using the same bank+USB arrangement already used for the motor checks. Do not alter any wiring while powered.
4. Open the actual Pico COM port (previously COM3),115200. The new startup banner is `AAI05 MOTOR PWM v0.6: FIXED-LOW INPUT`. If the initial banner scrolled out, use the exact flashed filename above rather than rebooting just to recapture it. Confirm healthy IDLE/next1, ENC_READY1, motor err0, bad0/over0 and DRIVEOFF. Do not press the M1/M2 hardware-test buttons; they have already been checked.
5. **Press and release GP20 once.** Observe the left wheel. It should visibly turn briefly and stop automatically. Record which way it turns and the corresponding encoder-count change. A beep or one encoder edge alone is not a demonstrated motion pass.
6. Wait for IDLE/next2. Record the cutoff count and reported elapsed microseconds (nominal400000), together with the physical observation. If there is no visible rotation, an error, an unexpected wheel, or continued motion, switch OFF and send the before/after serial lines. Do not repeat identical pulses, run all four blindly, or increase duty.
7. **Only if the first pulse works**, finish the remaining directions in the same session, waiting for IDLE before each fresh GP20 press:

| Press | Expected selected wheel | Requested polarity |
|---|---|---|
| 1, already completed | Left / M1 | D1 |
| 2 | Left / M1 | D2, opposite to its first turn |
| 3 | Right / M2 | D1 |
| 4 | Right / M2 | D2, opposite to its first turn |

Record actual wheel direction and encoder changes; electrical D1 is not yet a calibrated physical-forward convention. The unselected wheel should remain visually undriven. Do not reset/reflash between these four requests.

8. For a practical STOP observation, GP21 can be pressed during a later pulse with the second finger ready. It should stop the request and latch STOPPED/stop1/DRIVEOFF; later GP20 presses must not restart it. If the400ms cutoff happens before GP21, record that this only checked the stop latch, not interruption under load. Do not repeat the entire session to chase a short timing window; STOP-under-load can be confirmed during the later controlled-run stage. Host tests already cover STOP transitions in both PWM phases.

## What to send

Provide the firmware filename, which requests you tried, which physical wheels moved/directions, and serial lines covering the requests/cutoffs or STOP. Keeping Serial Monitor open from the first pulse helps retain the context. No long stationary IR log or repeat manual wheel/button exercise is needed.

Passing this session supports software-controlled polarity and bounded motion in this setup. Speed calibration, sustained control, full-load power/timing, tracking and minimum-bit savings still need their own relevant evidence. If the changed waveform still does not move the first wheel, the next step is actual motor-output/power evidence, not another unchanged firmware pulse trial.

## Rebuild, if source changes

In Ubuntu from the firmware checkout:

```sh
python3 tools/build_wsl.py --sdk "/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0" --motor-focused --label UNIQUE_MOTOR_LABEL
```

This runs only the two affected pulse/probe host executables plus normal/TEST builds. Unchanged suites are explicitly skipped. It never uploads or starts a motor.
