# TCS34725 read-only diagnostic

The user selected and authorised the previously proposed focused driver and
read-only diagnostic firmware. Work stays in the existing bring-up checkout;
no upload, motion, Git push, dependency install or private credential changes.

## Approach and scope

Add a portable state machine with injected I2C/time callbacks, a Pico SDK I2C1
adapter and a separate module-1 RGB owner task. This keeps bounded bus operations
out of IR acquisition and mutex critical sections. Extending the IR task would
delay ADC acquisition on a missing device; enabling the inherited kernel I2C
driver would claim default motor/encoder pins. Both alternatives are rejected.
Existing IR classification and MQTT payloads remain intact. No colour labels,
calibration, following or RGB MQTT publication in this milestone.

## Contract

- C11/BARR-C conventions, integer samples, no allocation. Raw RGBC preserved as
  uint16_t; no unmeasured lossy compression. GP2 SDA / GP3 SCL, I2C1, address 0x29.
- Accept only ID 0x44 (TCS34725). Disable ADC/interrupts, configure ATIME 0xEB
  (50.4 ms nominal), gain code 1 (4x), PON then a 13ms clock guard (3ms minimum
  plus the kernel's10ms quantisation margin). Require CNF_TIMER_PERIOD=10.
- Single-cycle scheduling: enable AEN, wait at least 67ms in the kernel clock
  (57ms integration/init bound plus10ms quantisation margin), check AVALID,
  disable AEN to freeze the
  registers, then one auto-increment 8-byte read from 0x14. Commit only after
  both freeze and read succeed, then begin another cycle.
  Do not publish repeated polls of the same conversion as new samples.
- Fixed exposure; saturation limit 21*1024=21504 counts. Saturated readings
  remain visible but valid=false. Clear=0 is a raw dark reading, not an I2C fault.
- One combined transfer uses a shared SDK deadline of 5000us. Handle short
  transfers, NACK and timeout. Recover I2C1 on failed transfers and retry full
  sensor configuration after 1000ms. Missing AVALID fails after 200ms.
- Preserve last successful raw observation/time/sequence on I2C failure;
  invalidate it immediately and report error. Timestamp the freeze, not delayed
  retrieval. Read freshness uses unsigned
  subtraction; valid requires age<=200ms. Counters saturate or wrap explicitly.
- RGB owner priority 15 (IR14, Report16), requested 10ms delay. Kernel radio
  remains priority4. Snapshot copied under inheritance mutex; readers TMO_POL.
  No bus call or console operation under that mutex. Timing is configured,
  not measured WCET. RGB initialisation/missing sensor must not halt IR/report.
- RGB_DIAG defaults off. RGB_DIAG=1 requires USB console, WIFI=cyw43 (existing
  SDK GPIO/time/clock compatibility), NET=none, MQTT=0, MOTOR_PROBE=1 and no
  motion/remote/comms app. No network credentials required. Existing kernel
  I2C driver remains disabled. Build names and stale-object profile include RGB.
- New diagnostic serial line includes ready, device ID, RGBC, sample sequence,
  age, valid, saturation, exposure and errors. Existing telemetry wire unchanged.

## Verification

Host fakes assert register order, ID rejection, warm-up, conversion wait,
little-endian burst decoding, partial/error reads, missing AVALID, saturation,
dark samples, retry, stale handling and timestamp wrap. Adapter tests cover
pins, shared transfer deadline, NACK/timeout/partial recovery. Build diagnostic
normal/TEST and existing remote PIO profile, preserving prior UF2 hashes.
Actual wiring, illumination, readings, timing and power remain user-run tests.

Source: ams TCS3472 DS000390 v1-04, registers and timing tables:
https://look.ams-osram.com/m/7ec5bcc3e40679be/original/TCS3472-DS000390.pdf
