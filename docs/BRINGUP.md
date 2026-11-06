# AAI05 diagnostic firmware bring-up

## Current milestone

The application now has three modules: IR, Motion and Communications. The coordinator handles all three as a solo development workflow. The course kernel/library implementation is retained.

Version 0.1 prints raw GP26/GP27 ADC readings, manual encoder counts/A-B levels, sample ages/errors and radio-driver readiness. It never commands motor drive. Coast (both inputs high) permits manual wheel checks. The onboard motor-test buttons bypass the program and run motors at full speed; do not use them for this diagnostic.

The first encoder implementation is nominal 1 ms polling for slow manual wiring checks, not a qualified high-speed encoder driver. No wheel circumference, gear ratio, counts/revolution or colour calibration is assumed. Colour following, target commands and MQTT remain later milestones.

## Baseline evidence

The untouched course commit c3b09544cb1fd6dac6ad4b8f523be0af59b44482 built successfully in Ubuntu with TEST=0 and TEST=1, SDK 2.3.0, GCC Arm 13.2.1 and Make 4.3. The source was clean before and after those builds. Windows/WSL Git line-ending handling was aligned using the checkout-local core.autocrlf=true setting; application content was not altered for the baseline.

Full baseline evidence is in the parent workspace at tmp/aai05_baseline_2026-11-05/baseline-result.json and its two logs. Preserved baseline ELF/UF2/map files are in the parent output/firmware/baseline directory. These are build checks only; no template image was flashed. Their memory figures are not proof of compact-data savings.

## Rebuild in Ubuntu

From the firmware repository in Ubuntu:

```sh
python3 tools/build_wsl.py --sdk "/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0"
```

The script uses a temporary SDK symlink to avoid spaces in Make's SDK include paths. It does not modify the SDK, write credentials, connect a broker, or flash the car. It runs host encoder tests, builds normal/test images and saves actual command results in testing/integration/logs/2026-11-05-build-result.json.

First-upload image: output/firmware/AAI05_Diagnostics_v0_1.uf2. The _TEST variant additionally runs portable decoder assertions on the target. Compiling that variant is not the same as running it on the Pico.

## First upload and console

1. Disconnect the power bank before changing cables. Support the car with drive wheels clear of the surface for initial firmware checks.
2. Use a USB DATA cable directly from the computer to the Pico W USB socket. The Debug Probe is not required for UF2 upload. Use this one power source for the first upload.
3. Set the Robo Pico switch ON. Hold Pico W BOOTSEL while connecting USB, then release BOOTSEL. Windows should show the RPI-RP2 drive.
4. Copy AAI05_Diagnostics_v0_1.uf2 to RPI-RP2. Automatic drive disappearance/reboot is expected after the copy.
5. Open the Pico's new COM port using VS Code Serial Monitor or PuTTY at 115200, 8 data bits, no parity, 1 stop bit, no flow control. The device must use the Pico USB connection, not the Debug Probe UART port.
6. Look for the AAI05 DIAGNOSTICS v0.1 banner and repeating DRIVE=OFF records. If the banner was sent before the terminal opened, press the carrier RESET button once; repeating records should still appear.

Output format only, not measured results:

```text
t=... IR[L=... R=... valid=... err=... age=...] ENC[L=... R=... AB=.../... bad=.../... sat=... age=...] DRIVE=OFF RADIO_READY=...
```

## Manual checks and interpretation

- IR: move light/dark material beneath one sensor at a time and identify which raw channel responds. valid=1 requires a complete, recent read; err=0 alone is insufficient. A stuck 0/4095 or identical channels needs investigation, not a claimed colour result.
- Encoders: gently rotate only one raised wheel slowly. Its counter and A/B level should change; reversing rotation should reverse count direction. Counts are edge counts, not millimetres. If the wheel cannot turn freely, do not force the gearbox.
- bad counts indicate diagonal transitions, which can result from missed samples or wiring/noise. Even bad=0 does not prove that fast cycles were not missed.
- Sample ages are milliseconds. IR data older than 100 ms is displayed invalid. Radio readiness is initialization state, not a connected Wi-Fi or MQTT session.
- DRIVE=OFF reports this firmware's intended mode. Confirm physically that it does not drive the wheels. Unexpected motion requires disconnecting power and investigating the flashed image and wiring.

Save 10-20 real console lines and record the surface/wheel action used. No on-car result has been recorded by the development agent. The professor's reported approval to proceed without a multimeter is retained; no rail voltage or electrical specification is represented as measured.

## Startup regression check

Review identified that enabling GP8-GP11 individually could briefly select drive state 10. A host regression reproduced this from the original operation sequence. The local motor driver now clears all four output enables, preloads all four latches high, configures every mux/pad while output remains disabled, then enables all four through one masked SIO write. The regression checks every operation for brake/coast-only states and verifies another GPIO is preserved. It passed after the fix, including repeated initialization. This is a software/model result, not a measured wheel or oscilloscope result.
