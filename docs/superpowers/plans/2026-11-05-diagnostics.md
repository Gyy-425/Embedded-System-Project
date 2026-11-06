# AAI05 initial diagnostics implementation plan

Goal: produce the first three-module diagnostic firmware after verifying the untouched course baseline.

Architecture: subsystem1 owns IR ADC reads, subsystem2 owns motors and encoders, subsystem3 owns application reporting. Development is handled as a solo effort per the coordinator's instruction. The built-in radio task continues to own the radio/network stack. No autonomous movement is introduced in this milestone.

Stack: Pico W, Robo Pico, C, micro T-Kernel, Pico SDK 2.3.0, Ubuntu GCC Arm 13.2.1 and GNU Make 4.3.

Spec: ../../../../docs/aai05/confirmed-scope-2026-11-02.md and ../../../../docs/aai05/io-mapping-2026-11-05.md.

## Constraints and interfaces

- Two IR AO inputs only: left GP26/ADC0 and right GP27/ADC1. Preserve all 12 ADC bits in uint16_t; validity is separate.
- Left encoder A/B on GP4/GP5; right on GP16/GP17. Counts are quadrature edges, not distance, until wheel scale is measured.
- Winding contact 5 to M1B/M2B and contact 6 to M1A/M2A. No motor drive or PWM commands in this milestone; keep the bridge in coast (11) so manual wheel checks are possible.
- The initial encoder diagnostic requests a 1 ms delay between samples for slow manual wheel checks. Follow-up on 6 November found that the current 10 ms kernel tick and relative-wait rounding yield a nominal approximately 20 ms polling interval. User-supplied wheel records show invalid transitions reaching 66/60; accurate counting is not passed. It is not a qualified full-speed decoder and must be replaced/qualified before motor speed control.
- Module snapshots use priority-inheritance mutexes. No report prints while holding a module mutex. A failed sample is explicitly invalid.
- Keep default SMP=1, CONSOLE=usb_cdc, WIFI=cyw43, NET=none, MQTT=0, CRASH_REPORT=0. This tests the built-in radio driver, not Wi-Fi association or MQTT.
- No fabricated on-target tests, tape calibration, classifications or contribution evidence. The template baseline is not the Week 13 functionally equivalent representation comparison.

### Task 1: preserve untouched baseline

- [x] Build original commit c3b09544cb1fd6dac6ad4b8f523be0af59b44482 with TEST=0 and TEST=1.
- [x] Preserve logs, ELF/UF2/map files, sizes and hashes outside the firmware checkout.
- [x] Verify source remains clean. Both builds passed; neither was flashed.

Evidence: tmp/aai05_baseline_2026-11-05/baseline-result.json in the parent INF2004-main workspace.

### Task 2: three-module diagnostic application

Files: app_program/app_main.c, app.h, subsystem1/{subsystem1.c,h,drv_ir.c,h}, subsystem2/{subsystem2.c,h,encoder_logic.c,h}, subsystem3/{subsystem3.c,h}, TEAM.md, testing and docs/BRINGUP.md.

Interfaces: subsystem1_get_snapshot(ir_snapshot_t *) and subsystem2_get_snapshot(motion_snapshot_t *) return a coherent copied status or a kernel error; subsystem3 consumes these snapshots. encoder_init/encoder_update are portable functions used by the manual diagnostic and host tests.

- [x] Write and run host tests for forward/reverse quadrature sequences, bounce, diagonal transitions and count saturation; initial compilation failed because the decoder did not yet exist.
- [x] Implement the decoder and observe GREEN with host GCC warnings treated as errors and ASan/UBSan enabled.
- [x] Replace example application code, start only three modules, and remove the unused subsystem4/5 application files and old cross-module examples.
- [x] Use the existing adca device from the IR owner task; configure only GP26/GP27 as analogue. The kernel ADC driver remains the peripheral implementation.
- [x] Report raw ADC, validity/error, sample age, A/B levels and signed counts. Keep unknown calibration and scales explicit.
- [x] Update logical ownership, startup and manual test instructions. Preserve historical reports outside this repository.

### Task 3: verify and hand off

- [x] Run host decoder tests and build TEST=0 and TEST=1; inspect compiler diagnostics and generated ELF/UF2 files. All saved final command exit codes are zero and warning lists are empty.
- [x] Review startup, bounds, SMP snapshots, absence of movement commands and removal of old example references. Independent review found one important startup issue; the regression reproduced bridge drive state 10, then passed after output enables were applied together through one masked write.
- [x] Preserve named normal/test diagnostic images and a short Windows flash/USB-console procedure. Artifact hashes were rechecked on 6 November. All physical tests remain pending.

## Checkpoint result

Software milestone complete; no flash or on-car validation performed. See docs/BRINGUP.md and testing/integration/logs/2026-11-05-build-result.json. No calibration, MQTT, navigation or full-speed encoder qualification is claimed. Shared kernel, BSP and network implementations remain unchanged. Hardware execution is the next input needed before advancing the sensing/control milestones.

## Review focus

1. Startup must never request wheel movement, including an init failure.
2. A diagonal or bouncing A/B transition must not invent a valid movement count.
3. Signed counts must not invoke C overflow; invalid counter saturation must also be defined.
4. Readers must not see a partly updated two-channel snapshot across cores.
5. Failed/stale ADC data must be labelled invalid and must not appear as successful colour recognition.
