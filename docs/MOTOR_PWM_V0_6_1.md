# v0.6.1: complete the bounded motor check

Status: the actual four-request sequence and unloaded GP21 input/latch have
passed within the recorded setup. [Result and limits](../testing/integration/logs/2026-11-07-v0_6_1-pico-motor-result.md).
Keep this procedure for reproduction; no unchanged repeat is needed now.

Use `output/firmware/AAI05_Motor_PWM_v0_6_1.uf2` without `_TEST`.
The preceding v0.6 image produced observed left-wheel rotation but stopped
early on a capture-quality warning. Its result is recorded separately.

This image keeps50% duty,10kHz PWM and the independent400ms cutoff.
Encoder ambiguity/invalid-transition counters remain visible and set the
sticky `ENC_UNCERTAIN=1` flag. They no longer abort this open-loop test, which
does not use encoder feedback to drive the motor. Initialization/readiness
faults, count saturation, GP21 STOP and driver faults still latch STOPPED.
Encoder capture itself is unchanged; uncertain counts are not qualified
speed/distance feedback. MQTT is disabled in this motor image.

## One session

1. Support the chassis with both drive wheels clear; keep the switch reachable.
   Close Serial Monitor, switch OFF and unplug bank power and Pico USB.
   Keep the existing wiring connected.
2. Hold BOOTSEL while connecting Pico USB to the laptop. Release when RPI-RP2
   appears and copy the normal v0.6.1 UF2 above onto it. After flashing, switch
   the board OFF, connect the existing bank-to-VIN lead and switch ON, retaining
   Pico USB for serial. No wiring change or repeat hardware-button test.
3. Open the Pico COM port (previously COM3) at115200. Confirm
   `IDLE next=1`, `ENC_READY=1`, motor `err=0`, `stop=0`, `DRIVE=OFF`.
   Startup banner: `AAI05 MOTOR PWM v0.6.1: FIXED-LOW INPUT`.
4. Press/release GP20 once. The left wheel should turn briefly, then stop
   automatically. Wait for `IDLE next=2`, `cutoff=1`, `us` near400000 and
   `stop=0`. `ENC_UNCERTAIN=1` is a retained quality warning and should not
   prevent completion. Record rotation and counters without treating them
   as an accurate speed/distance measurement.
5. If the request completes correctly, finish the three remaining requests,
   waiting for IDLE before each fresh GP20 press:

| Request | Wheel | Polarity | After normal completion |
|---|---|---|---|
| 1 | Left/M1 | D1 | IDLE next2, cutoff1 |
| 2 | Left/M1 | D2, opposite rotation | IDLE next3, cutoff2 |
| 3 | Right/M2 | D1 | IDLE next4, cutoff3 |
| 4 | Right/M2 | D2, opposite rotation | DONE next0, cutoff4 |

6. After DONE, press GP21 once. Expect `STOPPED stop=1 DRIVE=OFF`; a subsequent
   GP20 press must leave it stopped. This checks the button input and latch
   without chasing a400ms timing window. Interruption under load remains a
   later controlled-run check. GP21 can always be used earlier if needed;
   it intentionally ends the session and requires reset before restarting.

If motion continues, the wrong wheel moves, a real fault appears or the
sequence cannot proceed, switch OFF and send the surrounding lines and
physical observation. Do not increase duty or repeat unchanged attempts.
Send the short session log and observed directions, then switch OFF after
testing. A completed diagnostic does not qualify sustained closed-loop motion.

Rebuild using the existing `--motor-focused` command in the v0.6 guide; it now
also runs the affected controller regression. It never uploads or starts motors.
