# TCS34725: first read-only hardware check

This image collects raw RGBC values alongside existing IR/encoder diagnostics.
It does not classify colours, follow tape, run MQTT or accept network movement.
Firmware motor output is disabled by MOTOR_PROBE=1. Carrier hardware motor-test
buttons bypass software: leave M1A/M1B/M2A/M2B buttons untouched.

## Wiring, with all power disconnected

For the photographed seven-pin breakout (labels as printed):

| RGB board | Robo Pico |
|---|---|
| VIN | 3V3 |
| GND | GND |
| SDA | GP2 |
| SCL | GP3 |
| 3V3 | Unconnected |
| INT | Unconnected |
| LED | Unconnected for initial bring-up |

Grove5 can supply 3V3/GND, but its GP6/GP26 are not the SDA/SCL pair;
GP26 is already the left IR analogue net. Retain IR AO on GP26/GP27.
Board markings specify VIN3.3–5V; use3.3V here. Do not connect VIN and3V3
together on the breakout. Confirm LED illumination physically; its appearance
and default state do not establish successful I2C communication.

## Flash

Use only the normal `AAI05_RGB_Diagnostic_v0_11.uf2`, not `_TEST` or
`Remote_PIO_Regression`. The normal delivery is
`output/firmware/AAI05_RGB_Diagnostic_v0_11.uf2`, copied without modification from
`output/firmware/rgbdiag_rgb-20261116-release/AAI05_RGB_Diagnostic_v0_11.uf2`.

1. Close Serial Monitor. Disconnect Pico USB and bank VIN; wait for lights out.
2. Hold Pico BOOTSEL while connecting Pico USB to the laptop; release BOOTSEL.
3. Copy the normal RGB diagnostic UF2 to RPI-RP2. Its automatic disappearance
   on reboot is expected. If Windows does not mount it, do not copy another image.
4. Leave the power bank disconnected. Open the Pico COM port at115200 baud.
   The COM number may change; select the Pico's current port.
5. Check the banner `AAI05 RGB DIAGNOSTIC v0.11: NO MOTOR OUTPUT`.
   GP20 is not needed. No broker/IP/password changes needed for this image.

## Observe

The `RGB[...]` line is separate from the existing `t=... IR[...]` line.
Expected healthy fields after startup, values shown as placeholders:

```text
RGB[t=... ready=1 id=0x44 seq=... R=... G=... B=... C=... has=1 valid=1 sat=0 age=... status=0 errors=0 gain=4x integration_us=50400 DRIVE=OFF]
```

- `C` is clear-channel intensity, not a colour class. All four channels are raw
  uint16 values. Different exposures change these values; do not reuse IR ranges.
- Sequence advances per completed cycle; printed lines can skip sequence values
  because the sensor is sampled more often than the console is printed.
- Fixed initial exposure: nominal50.4ms, gain4x. Saturation limit21504 counts,
  not65535. This is a starting engineering configuration, not calibrated optimum.
- `age` is from the freeze of the acquired registers; no sample yet gives
  `has=0`, `valid=0`, age4294967295. Ages are kernel-clock estimates.
- Missing sensor does not stop IR/reporting. A failed transfer invalidates the
  last observation, retains its old raw data/time and retries setup about1s later.

| status | Meaning/action |
|---:|---|
| 0 | Successful acquisition; still require valid=1 and sat=0 |
| 1 | Initial startup; allow a few seconds |
| 3 | I2C error, NACK or short transfer; switch power off before checking wiring |
| 4 | I2C timeout or no completed conversion; save the log |
| 5 | Device ID mismatch; send the displayed ID, do not bypass the check |
| 6 | Configuration readback/argument failure; save the log |
| 7 | Saturated exposure; send values before collecting calibration data |
| 8 | RGB kernel-resource startup failure; IR/report continue |

## One short sample check

Keep car stationary, mounting fixed and optical component face pointing down.
Keep fingers off the sensor window/LED, do not drag wires, and use steady indoor
lighting. Position BOTH the RGB chip and its illumination area fully over a
uniform sample; avoid tape edges and seams.

1. Give startup up to5s. If healthy, record10 complete `RGB[...]` lines over black.
2. Reposition over white; wait1s, record10 lines.
3. Repeat over blue and bare cardboard, recording10 lines each.
4. Save labels, complete RGB lines, actual RGB face-to-tape gap, lighting,
   LED state and power arrangement. Retain some IR lines to check coexistence.
5. Send that small capture. No100-reading calibration or motor test required yet.

We expect changes between surfaces, but no particular R/G/B values or recognition
accuracy are guaranteed. All-zero data across every surface, persistent errors,
or saturation require investigation before calibration. Never change powered
wiring to provoke faults: host tests already exercise failures.

## Reproduce software checks

In Ubuntu/WSL, from the repository, use a new unique evidence label:

```text
python3 tools/check_rgb_diag.py --label NEW_LABEL --firmware --sdk /path/to/pico-sdk
```

This performs host tests and builds distinct normal/TEST RGB and remote PIO
regression artifacts without flashing. Actual sensor readings, electrical levels,
timing, optical interference, colour accuracy and moving behaviour remain untested
until user-run hardware evidence is available.
