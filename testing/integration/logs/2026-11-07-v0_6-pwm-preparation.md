# v0.6 focused motor development evidence

User instruction: progress efficiently with practical testing and less repetition. Scope is one motor PWM change; existing wiring, basic IR/encoder/hardware-button and communication passes are retained. No hardware operation or firmware upload occurred in this development task.

## Source decision

The current Cytron example uses10kHz PWM and the default Adafruit DCMotor mode, which pulses one input and holds the otherLOW. The earlier application held one inputHIGH with complementary PWM. The supplied Robo Pico table defines00 brake and11 coast, so generic library decay labels are not used to describe the carrier.

v0.6 adopts direct duty/fixedLOW at unchanged50%/10kHz/400ms. Start releases the fixed-low input before the PWM input; STOP raises the PWM input before the low input, then hands both bridges to SIOHIGH before disabling PWM. This preserves coast/brake/requested polarity at every modelled register-write boundary, with no reverse or wrong-wheel transient. Boot/no-drive profiles remain11/coast; the probe never releases its force-high overrides. The waveform difference is a concrete comparison, not a proven physical cause of the earlier lack of rotation.

New/touched driver/header use Allman braces,4 spaces, module/private prefixes, fixed-width bit arithmetic and80-column lines. Strict target diagnostics include Wall/Wextra/Werror/Wconversion/Wshadow/Wstrict-prototypes. A scoped header exception preserves the supplied kernel's generic FP/FUNCP declarations without changing kernel types. No PWM/timer pins, clock, duration cap, startup ownership or encoder code changed.

## Actual evidence

- New waveform expectation against the unchanged driver failed at test_motor_pulse.c:250, exit134, asserting first-half requested drive/second-half00. Recorded in `2026-11-07-v0_6-pwm-red.log`.
- First changed-driver run passed. The final focused runner repeated that affected suite alongside the probe suite against final source.
- Normal host suite covers both wheels/polarities, direct waveform, exactly50% sampled drive, explicit STOP in both carrier phases, independent IRQ cutoff, setup expiry, limits/conflicts, clock/resource failures and timestamp wrap. It checks the actual driver against MMIO/kernel boundary models, not physical voltages.
- Probe host suite confirms force-high/coast retention across the same operations.
- Final runner `--motor-focused --label 2026-11-07-v0_6-pwm-final`:6 executed stages all exit0, no warnings. These are2 host compiles,2 sanitized host executions and2 firmware compile variants.13 unchanged stages are SKIPPED_UNCHANGED_SCOPE, not recorded as passes.
- Touched driver/header tabs/80-column scan and source diff whitespace check passed.
- One fresh-context static reviewer reported no material findings after checking transition ordering, polarity, probe guards, deadlines/setup/wrap/resources/SMP and focused build selection. No duplicate suites or hardware tests were run by the reviewer.

Normal UF2 SHA256: `e1062112cfc0860a67b2f062ae56948be7f47fd8f5c29180d93b5e1e3cfb3a9f`, file `output/firmware/AAI05_Motor_PWM_v0_6.uf2`.

TEST UF2 SHA256: `cc4bddf1472e134e36ffa3beb965488a805fe8de67bed4373907b38305caf48e`, file `output/firmware/AAI05_Motor_PWM_v0_6_TEST.uf2`.

Both new hashes were rechecked against actual files. Normal/TEST v0.3.2 motor and v0.5 broker199 communication artifacts were independently rehashed and match their prior records. Private credentials and all binaries remain ignored/local.

Physical motion is **PENDING**. Use [the one-session guide](../../../docs/MOTOR_PWM_V0_6.md). One left-wheel request is the gate: continue remaining directions if it visibly moves and stops; otherwise stop unchanged repetitions and collect output/power evidence. No calibrated physical-forward convention, speed, power stability or integrated-motion pass is inferred from compilation/model tests.

Primary references: [Cytron example](https://github.com/CytronTechnologies/Cytron-ROBO-PICO/blob/main/Getting_Started_Guide/CircuitPython/DC_Motor/code.py), [Adafruit implementation](https://github.com/adafruit/Adafruit_CircuitPython_Motor/blob/main/adafruit_motor/motor.py), [carrier truth table](https://www.farnell.com/datasheets/4248509.pdf). Checked7 November2026.
