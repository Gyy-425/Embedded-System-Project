# GP20 no-output isolation test v0.3p

Use **output/firmware/AAI05_Motor_Probe_v0_3p_NO_DRIVE.uf2**, without the _TEST suffix. This is a separate diagnostic image, not a fix for the reported v0.3 freeze. Do not retry the powered v0.3 test yet. No wiring changes or added power source are required.

## What changes

MOTOR_PROBE=1 retains the force-high overrides on both selected motor GPIOs throughout each virtual pulse. GP8/9 and GP10/11 therefore request 11/coast, even while the PWM counter and timer run. It retains the existing controller, locks, PWM register setup, alarm0/IRQ0 and cutoff/stop path. The two writes that normally release coast to drive are omitted. The original powered image is preserved under its original name.

The banner identifies **AAI05 MOTOR PROBE v0.3p: NO MOTOR OUTPUT**. Every regular report contains **MOTOR_MODE=PROBE** and **DRIVE=OFF**, including during phase=PULSE. Here PULSE means a virtual timed request; M/D/pct are the tested parameters, not applied motor drive. us is the software-observed timer span when stop begins, not an electrical measurement.

## One test to run

1. Stop Serial Monitor and unplug the Pico USB. Leave the bank disconnected. Keep both wheels raised and cables clear. Do not use the M1A/M1B/M2A/M2B hardware test buttons.
2. Hold Pico W BOOTSEL, reconnect its USB data cable to the laptop, then release BOOTSEL when RPI-RP2 appears. Copy the exact NO_DRIVE UF2 named above to that drive; let it reboot.
3. Open the actual Pico COM port at 115200, 8N1, no flow control. Use COM3 if it is still assigned to the Pico. Leave GP20/21 released for several seconds. Confirm MOTOR_MODE=PROBE, phase=IDLE, next=1, ENC_READY=1, motor err=0 and DRIVE=OFF. If the startup banner was missed, the repeating PROBE marker identifies this mode. If the marker is absent or an initialization error appears, stop without pressing GP20.
4. Start saving the console output. Press GP20 for about 0.1-0.2 seconds, then release it once. **Neither wheel should be driven.** Observe output for at least three seconds; do not rotate wheels during this test.
5. Send the records from about two seconds before the press through three seconds afterward, or the final records if output stops. State whether reporting continued and whether either wheel moved. If it stops, close the monitor and unplug USB. Do not retry the powered image or add the bank as a workaround.

Expected software progression: IDLE -> PULSE -> COOLDOWN -> IDLE next=2, cutoff increases by one, us near 400000, DRIVE=OFF throughout. Reports are roughly 260 ms apart; missing a brief phase alone is not failure if the final cutoff/elapsed/state records confirm progression. STOPPED or nonzero errors need their exact records. This expected outcome is not a measured result.

## How the result will narrow the investigation

| Observation | Supported inference | Still unresolved |
|---|---|---|
| Probe also stops reporting | Physical motor drive/load is not required to reproduce the symptom | Firmware/peripheral/interrupt path versus USB/host reporting; exact execution point |
| Probe completes while powered v0.3 did not | Releasing the output overrides or resulting motor/encoder activity changes the outcome | Load, noise, encoder IRQ workload and timing-dependent software; this does not prove brownout |
| A wheel is driven in the probe | Behaviour contradicts the intended diagnostic profile | Confirm flashed image and physical controls/wiring before continuing |

## Build and verification

From Ubuntu in this checkout:

```sh
python3 tools/build_wsl.py --sdk "/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0" --motor-probe --label 2026-11-06-v0_3p-rebuild
```

MOTOR_PROBE is included in the Make object-profile signature and image filename. The runner gives probe artifacts distinct names and records the mode/hash. The host pulse test compiles in both modes; probe mode asserts 11/coast after each modeled register write and at an active PWM phase, across all four wheel/polarity combinations and expiry/fault paths. The probe assertion failed before the probe conditional was added. These boundary fakes do not reproduce the on-board freeze or exercise the real SMP lock/IRQ implementation. Hardware execution remains pending until a user log is supplied.

Verified software result on 2026-11-06: all 16 compile/execution/build steps passed with no reported warnings (seven sanitized host programs and two probe firmware variants), recorded in testing/integration/logs/2026-11-06-v0_3p-reviewed-build-result.json. Independent artifact hash checks matched that record. Normal NO_DRIVE UF2: 562688 bytes, SHA256 `ddaca6a73eac1ced8e80ba64c2808c55a3d84b84d9e39b06e370c2cbb4f1d805`. The preserved original powered v0.3 UF2 remains SHA256 `4a1af5d9b09026536aa1239bfb2ce1b7c5d3a6897837919c0c09a48aef4002e0`. No on-board probe pass is claimed.
