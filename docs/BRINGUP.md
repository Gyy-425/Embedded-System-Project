# AAI05 diagnostic firmware bring-up

## Current milestone

Current deliverable is **AAI05_Motor_Test_v0_3.uf2**, a motion-enabled, button-triggered raised-wheel test. Follow docs/MOTOR_TEST_V0_3.md. GP20 requests one fixed 25%/400 ms pulse; GP21 latches STOP. The six host executables and normal/TEST builds pass in testing/integration/logs/2026-11-06-v0_3-final-build-result.json; actual motor direction, powered encoder behaviour, power stability and observed pulse duration remain pending.

The v0.2 procedure below is preserved for the no-drive diagnostic and its user-run results; it is not the v0.3 motor-test procedure.

The application now has three modules: IR, Motion and Communications. The coordinator handles all three as a solo development workflow. The course kernel/library implementation is retained.

Version 0.2 prints raw GP26/GP27 ADC readings, interrupt-captured encoder counts/A-B levels, capture readiness/IRQ/overrun diagnostics, sample ages/errors and radio-driver readiness. It never commands motor drive. Coast (both inputs high) permits manual wheel checks. The onboard motor-test buttons bypass the program and run motors at full speed; do not use them for this diagnostic. Follow docs/ENCODER_RETEST_V0_2.md for the new image and retest.

The preserved v0.1 image requested tk_dly_tsk(1) between samples, resulting in nominal approximately 20 ms polling under this port's 10 ms tick and relative-wait rounding. v0.2 moves acquisition to one core-0 GPIO dispatcher; the Motion task only publishes snapshots. New pending flags during sampling or multiple latched edges are flagged as ambiguous and resynchronised without guessed counts. Actual maximum reliable rate remains unmeasured. No wheel circumference, gear ratio, counts/revolution or colour calibration is assumed. Colour following, target commands and MQTT remain later milestones.

## Baseline evidence

The untouched course commit c3b09544cb1fd6dac6ad4b8f523be0af59b44482 built successfully in Ubuntu with TEST=0 and TEST=1, SDK 2.3.0, GCC Arm 13.2.1 and Make 4.3. The source was clean before and after those builds. Windows/WSL Git line-ending handling was aligned using the checkout-local core.autocrlf=true setting; application content was not altered for the baseline.

Full baseline evidence is in the parent workspace at tmp/aai05_baseline_2026-11-05/baseline-result.json and its two logs. Preserved baseline ELF/UF2/map files are in the parent output/firmware/baseline directory. These are build checks only; no template image was flashed. Their memory figures are not proof of compact-data savings.

## Rebuild in Ubuntu

From the firmware repository in Ubuntu:

```sh
python3 tools/build_wsl.py --sdk "/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0" --label 2026-11-06-v0_2-rebuild
```

The script uses a temporary SDK symlink to avoid spaces in Make's SDK include paths. It does not modify the SDK, write credentials, connect a broker, or flash the car. It runs four host executables and builds normal/test images. Use a fresh --label for separate evidence. Final v0.2 results are in testing/integration/logs/2026-11-06-v0_2-final-build-result.json; v0.1 logs and artifacts remain preserved.

First-upload image: output/firmware/AAI05_Diagnostics_v0_2.uf2. The _TEST variant additionally runs portable decoder assertions on the target. Compiling that variant is not the same as running it on the Pico.

## First upload and console

1. Disconnect the power bank before changing cables. Support the car with drive wheels clear of the surface for initial firmware checks.
2. Use a USB DATA cable directly from the computer to the Pico W USB socket. The Debug Probe is not required for UF2 upload. Use this one power source for the first upload.
3. Set the Robo Pico switch ON. Hold Pico W BOOTSEL while connecting USB, then release BOOTSEL. Windows should show the RPI-RP2 drive.
4. Copy AAI05_Diagnostics_v0_2.uf2 to RPI-RP2. Automatic drive disappearance/reboot is expected after the copy.
5. Open the Pico's new COM port using VS Code Serial Monitor or PuTTY at 115200, 8 data bits, no parity, 1 stop bit, no flow control. The device must use the Pico USB connection, not the Debug Probe UART port.
6. Look for the AAI05 DIAGNOSTICS v0.2 banner and repeating DRIVE=OFF records. If the banner was sent before the terminal opened, repeating records still identify the version through new ENC_READY/irq/over fields.

Output format only, not measured results:

```text
t=... IR[L=... R=... valid=... err=... age=...] ENC[L=... R=... AB=.../... bad=.../... sat=... age=...] ENC_READY=... err=... irq=... over=.../... DRIVE=OFF RADIO_READY=...
```

## Manual checks and interpretation

- IR: move light/dark material beneath one sensor at a time and identify which raw channel responds. valid=1 requires a complete, recent read; err=0 alone is insufficient. A stuck 0/4095 or identical channels needs investigation, not a claimed colour result.
- Encoders: gently rotate only one raised wheel slowly. Its counter and A/B level should change; reversing rotation should reverse count direction. Counts are edge counts, not millimetres. If the wheel cannot turn freely, do not force the gearbox.
- bad counts indicate diagonal transitions, which can result from missed samples or wiring/noise. over counts ambiguous edge batches which were discarded and resynchronised. Even bad=0 and over=0 do not prove that unlimited fast cycles can be captured.
- Sample ages are milliseconds. IR data older than 100 ms is displayed invalid. Radio readiness is initialization state, not a connected Wi-Fi or MQTT session.
- DRIVE=OFF reports this firmware's intended mode. Confirm physically that it does not drive the wheels. Unexpected motion requires disconnecting power and investigating the flashed image and wiring.

Save 10-20 real console lines and record the surface/wheel action used. On 6 November the coordinator supplied real initial, IR and manual-wheel records: valid responsive raw IR channels and both encoder input responses were observed in those records. Physical right-wheel then left-wheel actions match the reported R then L channels. The coordinator subsequently confirmed both individual IR sensors work, so that basic check need not be repeated. Surface-labelled numerical calibration, colour polarity and seven-tape classification remain pending.

Invalid transitions accumulated to bad=66/60, so accurate encoder counting is not passed. The wheel attachment contains 143 records over 36.92 seconds at 260 ms record spacing, consistent with the report task's requested 250 ms delay plus the port's timer rule. Latest wording clarifies that faster manual rotation can produce frozen/inconsistent counts, while careful slow rotation produces steady changes. This supersedes the earlier mechanical interpretation of 'skewed'; no misalignment, stiffness or repair is established by that wording. Improve capture before relying on encoder feedback; unusually precise hand rotation is not an eventual driver requirement. No powered motor-drive test or agent-observed physical test is claimed. The professor's reported approval to proceed without a multimeter is retained; no rail voltage or electrical specification is represented as measured.

## Startup regression check

Latest user-run v0.2 outcome: two logs contain 211 complete reports with capture ready, no capture error, bad=0/0 and over=0/0 throughout. The left-only test leaves R=0; both channels respond in the second test. Intentional forward/backward movement was confirmed, so increasing/decreasing net counts are expected. The basic manual functional check passes under these conditions; powered motor operation, counts/revolution and maximum reliable edge rate remain unverified. Evidence: testing/integration/logs/2026-11-06-v0_2-hardware-results.md. No further firmware correction is indicated by these records alone; the next stage is a controlled raised-wheel motor-direction test using a separately prepared motion-enabled image.

Review identified that enabling GP8-GP11 individually could briefly select drive state 10. A host regression reproduced this from the original operation sequence. The local motor driver now clears all four output enables, preloads all four latches high, configures every mux/pad while output remains disabled, then enables all four through one masked SIO write. The regression checks every operation for brake/coast-only states and verifies another GPIO is preserved. It passed after the fix, including repeated initialization. This is a software/model result, not a measured wheel or oscilloscope result.
