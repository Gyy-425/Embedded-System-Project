# Motor starting test v0.3.2:50% duty,400ms maximum

**Latest user result:** the50% run completed all four cutoffs with continuous reporting, but no clear wheel rotation; one encoder edge appeared per selected motor/polarity. Physical operation is NOT PASSED. Pause this GP20 procedure; follow [the dedicated hardware-button check](HARDWARE_MOTOR_BUTTON_CHECK.md). That check intentionally uses one full-speed hardware-button tap with the application stopped in cold BOOTSEL, rather than changing this firmware or its duty again. Original preparation/upload instructions below remain historical evidence.

## Purpose and verified preparation

The coordinator authorised the three register-macro corrections and the next motor diagnostic. The earlier25% run completed all four software cutoffs without freezing, but showed no encoder edges or clear wheel movement.50% is an assistant-selected diagnostic setting to test insufficient starting torque as one hypothesis; it is not a measured motor rating or a demonstrated cure.

The three inherited definitions are corrected: GPIO_CTRL(n)/GPIO(n) now parenthesise n; GPIO_OUT is SIO_BASE+0x010. New independent-address and conditional/arithmetic regressions failed on the old definitions and pass after repair. All66 audited register addresses now match official SDK2.3.0 values. The real pulse-driver fixture failed at50% before the old25% cap was raised, then passed all four wheel/direction cases,50% drive/coast sampling, rejection of51%, ADC/other-wheel preservation and task-independent cutoff.

Normal and TEST SMP firmware builds and all19 runner steps passed without reported compiler warnings. All10 Python helpers passed outside the restricted Windows sandbox; the initial run's temporary-directory permission errors are preserved separately. Independent read-only review found no Critical/Important issue; the reviewer checked hashes/diff/evidence but could not rerun WSL under its own permissions. No v0.3.2 hardware upload or pulse has been performed by the assistant.

| Artifact | Use | SHA256 |
|---|---|---|
| output/firmware/AAI05_Motor_Test_v0_3_2.uf2 | Normal image to use now | 72c224beb09b5f388e99c64e6e5c8e8cc3dceb27b2eac23187267beda3d0caef |
| output/firmware/AAI05_Motor_Test_v0_3_2_TEST.uf2 | Additional target assertions; not the requested upload | 6fa36cc2d339193fb09bde2919009b3bbe00bfaa24350055965ce6ddc079139f |

The original v0.3.1 normal/TEST UF2 files retain their hashes. The v0.3p/v0.4 no-drive artifacts are also retained, not overwritten. Only the three reviewed BSP definitions change in shared code. The diagnostic duty cap/request is now50%; pulse400ms,10kHz nominal PWM, one-wheel operation, pins, fresh GP20 trigger, GP21 STOP latch, cooldown, expiry/fault behaviour and no drive at boot remain as before. The host-only no-drive fixture is exercised at the new cap as well; no new no-drive firmware artifact was issued.

## Upload using the Pico USB cable

1. Close Serial Monitor. Switch the car OFF, unplug Pico USB and power-bank input, and wait for the lights to go out. Leave motor/encoder/IR wiring unchanged. Raise both drive wheels with cables/fingers clear and keep the power switch reachable. The Debug Probe is not needed for this USB upload.
2. Keep bank power disconnected. Hold the Pico W BOOTSEL button while connecting its USB data cable to the laptop; the carrier must be ON for the Pico to power up. Release BOOTSEL when RPI-RP2 appears. Do not press GP20 or the M1A/M1B/M2A/M2B hardware buttons.
3. Copy the normal AAI05_Motor_Test_v0_3_2.uf2 onto RPI-RP2. It should reboot. Do not copy the older v0.3/v0.3.1 file or the _TEST variant for this run.
4. After upload, turn the car OFF. Keep Pico USB attached; reconnect the existing bank5V VIN supply and turn ON with GP20/21 released. This retains the already documented supported USB/VIN arrangement and does not prove which source carries motor current.
5. Open the actual Pico serial port (COM3 if still assigned) at115200/8N1/no flow control. The banner, if captured, is AAI05 MOTOR TEST v0.3.2:50 PERCENT START CHECK. Wait for continuous reports with ENC_READY=1, capture err=0, MOTOR phase=IDLE next=1, motor err=0, stop=0, cutoff=0 and DRIVE=OFF. pct=0 before the first request is normal. Stop before motion if startup differs.

## One request only, then send the result

With wheels still raised, press GP20 once for0.1-0.2seconds and release. Do not hand-start a powered wheel. This first request selects M1/left, D1,50% for a requested400ms maximum.

Expected: only the left wheel visibly moves, its encoder responds, reporting continues through COOLDOWN to IDLE next=2, and the completed record shows M=1 D=1 pct=50, cutoff=1, us near400000, stop=0, err=0 and DRIVE=OFF. bad/over must not grow. A missing short PULSE line alone is not failure because reports are about260ms apart. The wheel can coast after drive removal; this is not a command to brake at exactly400ms.

If reporting freezes, motion is unexpected or drive removal is doubtful, switch OFF immediately and disconnect both USB and bank. GP21 is the responsive-firmware STOP, not a failsafe after a stall. If the pulse completes but the wheel stays still, only twitches or hums, stop after this one request and report it. Do not repeat, increase duty/voltage, use full-speed motor buttons or continue the other three requests without checking the first result.

Send5-10 idle and surrounding pulse/completion records, plus actual wheel, physical direction and whether it clearly moved. Confirm both wheels were raised and bank VIN/Pico USB connected. The next three directions remain the existing M1D2/M2D1/M2D2 sequence, conditional on the first result passing; they are not requested yet.

If STOP has not been checked on the current setup, it can be tested separately while idle: GP21 must latch STOPPED and a subsequent GP20 press must not start a pulse; reset restores idle. Do not mix that intentional STOP latch with the first pulse observation.

## Rebuild and evidence

From this checkout in Ubuntu:

```sh
python3 tools/build_wsl.py --sdk "/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0" --label 2026-11-06-v0_3_2-rebuild
```

Use a fresh label for later evidence. The runner neither flashes nor starts motion. Saved final evidence is testing/integration/logs/2026-11-06-v0_3_2-final-build-result.json, the register-map-green JSON, red regressions, Python suite and review record. Host/build success does not establish powered motor/encoder operation, load integrity or colour tracking. Actual Wi-Fi/MQTT transport remains the following independent milestone.
