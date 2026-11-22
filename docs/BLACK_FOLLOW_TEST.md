# Black follow v0.13.3 D40/T8 R2: bounded local prototype

Prepared software, not yet hardware-validated. Keep the35mm plain black stripe
and current mounting. This follower uses GP20/GP21 locally; it does not accept
MQTT commands or follow coloured corridors. No wiring changes are needed.

## Images

- `output/firmware/AAI05_Black_Follow_v0_13_3_D40_T8_NO_DRIVE.uf2`: sensing/control
  calculations with physical motor output inhibited. Use first.
- `output/firmware/AAI05_Black_Follow_v0_13_3_D40_T8.uf2`: powered version; no drive on
  boot, fresh GP20 press permits one nominal one-second run.
- `_TEST` and compatibility images are build evidence, not the user test image.

## 1. Check requests without movement

1. Close Serial Monitor. Switch off and disconnect bank/USB. Hold BOOTSEL while
   reconnecting laptop USB; copy NO_DRIVE UF2 to RPI-RP2 and allow reboot.
2. Use laptop USB only for this stationary check. Open serial at115200 and
   leave GP20 released. Do not use the four hardware motor-test buttons, which
   operate independently of the software controller.
3. Centre the robot on the strip at its usual height. Check RGB_CLASS BLACK,
   modelBWY_20261117_R2, fresh/valid IR, FOLLOW ready=1, inhibit=1, DRIVE=OFF.
   Encoder/driver startup must be healthy.
4. Press GP20 for about0.15s, release, watch for about2s. Expect RUNNING then
   STOPPED reason=COMPLETE. DRIVE stays OFF: nonzero L/R are inhibited requests.
5. Move the ROBOT slightly left, repeat a fresh press; repeat slightly right.
   Keep RGB over black, as in your position captures.

| Pose | Expected turn | Raw duty requests |
|---|---|---|
| Centred, both IR near cardboard baseline |0|L approximately−40, R+40|
| Robot left, right IR high |positive/right|Left faster: about−40/+28|
| Robot right, left IR high |negative/left|Right faster: about−28/+40|

Release GP20 for at least0.1s before another press. Holding it does not extend
the run. Recovery never starts motion automatically. If ready=0, inspect the
sensor/driver fields; do not hold the button waiting for startup.

## 2. Powered wheels-raised check

After requests match, flash the powered normal UF2 with the same power-off/
BOOTSEL process. Use bank VIN for motors and laptop USB for serial as in the
passed motor setup. Support chassis with both drive wheels clear. Raise the
sample cardboard beneath the heads to preserve ORIGINAL sensor gaps, outside
the wheel path. Lifting the car far from the tape will fail the presence guard.

Check inhibit=0 and ready=1. One fresh centred GP20 press should move both
wheels physically forward for about one second, then stop. On a second fresh
run press GP21 while moving: expect STOPPED/BUTTON and output OFF. Release
both GP20 and GP21 before another fresh start. If direction/stop is wrong or
reporting freezes, switch off, save the short log and do not proceed to ground.

## 3. Short straight ground run

After wheels-raised success, switch off, remove the USB tether and use bank
VIN only. Centre on a straight black section with clear runway beyond it and
GP21/power reachable. Leave GP20 released at boot, allow about5s for sensing,
then press once. Judge correction, not just wheel rotation. Try a small initial
offset after a stable centred straight run. Curves come afterward.

Reconnect USB afterward if needed for retained STOPPED status; this is not a
complete history logger. Save a few FOLLOW/IR/RGB_CLASS lines and describe the
actual movement and stop. No old per-wheel/MQTT or100-reading tests are needed.

## Policy and limits

- Baselines L268/R304 and spans1100/1140 come from static captures. Both low
  IR readings require fresh RGB BLACK to distinguish centred from background.
- Maximum wheel40% is electrical duty, not measured speed; nominal turn capped8%, no reverse
  or automatic search. Gains and rolling performance require car testing.
- IR older than100ms, invalid data, RGB not fresh BLACK (max200ms), both
  normalized IR>=600, hardware fault or GP21 stops the run.
- Busy reads may reuse only last coherent data until ORIGINAL timestamps
  expire. Newly observed invalid data is not hidden by the cache.
- Absolute motor deadlines are checked at actual start/renew under driver
  lock, capped by run/data/command expiry with10ms conservative clock margin.
  The1000ms duration is nominal and may end early; actual timing/WCET unmeasured.
- Independent hardware cutoff cannot silently restart. Fresh GP20 start can
  explicitly rearm stopped/coasted outputs; STOP recovery alone never drives.
- STOP is coast. Stop distance and drift are unmeasured. RGB sits13.5mm behind
  IR and conversion takes time, so precise end-of-tape stopping is not claimed.
- FOLLOW Motion snapshots use a copy-only SMP spinlock; IR report availability
  is independent. RGB DRIVE=OWNER refers to the actual FOLLOW DRIVE field.
- PIO encoders are monitored, not used as wheel-speed feedback. No new control
  task or network interface; existing RGB acquisition task is reused.
- Software checks and recorded-position replay do not establish physical
  following, curve accuracy, stop latency or speed acceptance.
