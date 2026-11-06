# One motor-button check with the application stopped

## Why this check is now needed

The normal25% and50% software runs complete their reported400ms cutoffs, but no clear wheel rotation is observed. At50%, each selected motor/polarity produces just one encoder change. Pin/address correctness and host tests do not prove the actual bridge/motor power path. Stop further GP20 requests and firmware-duty increases until a different boundary is checked.

Cytron explicitly documents M1A/M1B/M2A/M2B buttons as a way to test the driver/motor connections without application code. These buttons run a motor at full speed. Earlier instructions avoided them during software-timed tests; this is now a dedicated, brief hardware-only isolation check. The motor voltage/current rating remains unidentified, so this is a short observation at the existing5V input, not a prolonged run or stall test.

The application must be stopped for a meaningful button test. Its normal idle commands both bridge inputs high/coast; do not assume a hardware button will override every actively driven GPIO state. Cold BOOTSEL mode avoids running that application and resets the motor GPIO configuration. Simply pressing BOOTSEL while the firmware is running does not enter this mode.

## Procedure: M1A only, one short tap

1. Close Serial Monitor. Switch OFF and unplug both laptop Pico USB and bank power; wait for the lights to go out. Leave all motor/encoder/IR leads connected. Support the chassis securely with both wheels clear and fingers/cables away; keep the power switch reachable.
2. Keep bank disconnected. Hold Pico W BOOTSEL while connecting its USB data cable and switching the carrier ON. Release only after Windows shows the RPI-RP2 drive. Do NOT copy a UF2: the installed v0.3.2 stays intact in flash, but the application is not running. No COM3 serial output is expected in this mode.
3. With RPI-RP2 still present, connect the existing bank5V lead to the existing VIN path, leaving Pico USB connected. Do not change voltage or wiring. Confirm RPI-RP2 remains present; if it disappears/reboots into normal firmware, stop and report that instead of pressing motor buttons. Simultaneous USB/VIN is documented for Robo Pico; this is not proof of which source carries motor current.
4. With wheels still raised, give the actual M1A motor-test button beside the motor controls ONE quick tap, about0.1second, and release immediately. This is not GP20. Do not hold it down, press another motor button simultaneously, or try to help the wheel by hand.
5. Watch whether the left wheel clearly turns, and whether the M1A red motor-status LED lights. This button is a full-speed hardware request: there is NO software400ms cutoff while in BOOTSEL. Release immediately; switch OFF and unplug both sources if motion is unexpected or does not stop receiving drive. A wheel can coast after drive is removed.
6. Stop after this one tap, switch OFF, and report: actual wheel, clear rotation/twitch/sound only, M1A LED behaviour and whether RPI-RP2 stayed present. No serial log is needed for this hardware-only test. Do not press M1B/M2A/M2B or resume GP20 testing yet.

After the result, a normal power-on without BOOTSEL returns to the installed image; no reflash is required just to leave this mode.

## Interpreting the result

- Clear M1 motion supports the left driver's basic full-supply path, winding connection and mechanical ability to rotate under this setup. It does not by itself prove PWM generation is wrong, nor qualify load current/rail stability. The next check can focus on PWM/control/startup conditions with actual output evidence.
- No M1 motion does not prove a dead motor. Power delivery, driver state/output, winding connection and mechanics still need isolation/measurement before further firmware changes. Record LED behaviour, then use appropriate electrical measurements or supervised bench help; another duty increase is not a diagnosis.

No new hardware result, motor rating or voltage measurement is claimed in this procedure.

Sources: [Cytron DC motor tutorial](https://my.cytron.io/tutorial/get-started-robo-pico-circuitpython-dc-motor), connection test instructions; [Cytron-authored Robo Pico datasheet](https://www.farnell.com/datasheets/4248509.pdf), motor buttons/full-speed behaviour, power options and driver truth table.
