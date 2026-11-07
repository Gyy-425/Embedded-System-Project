# v0.7 local Motion check

Use `output/firmware/AAI05_Motion_v0_7_LOCAL.uf2`, without `_TEST`.
One GP20 press renews left D1/50% commands for two seconds; right coasts.
These two cases cover renewal/expiry and STOP during motion. No repeat of the
old four-pulse, basic IR, hardware-button or network checks is needed.

## Upload once

1. Support both wheels clear; keep the switch reachable. Close Serial Monitor,
   switch OFF and unplug bank power and Pico USB. Keep existing wiring.
2. Hold BOOTSEL while connecting Pico USB. Copy the normal LOCAL UF2 above onto
   RPI-RP2. After flashing, switch OFF, connect the existing bank VIN lead,
   switch ON and retain Pico USB for serial.
3. Open the Pico port (previously COM3),115200. Confirm banner
   `AAI05 MOTION v0.7: LOCAL OPEN-LOOP EXERCISE` and
   `MOTION[state=READY L=0 R=0 ... reason=0 ... err=0] DRIVE=OFF`.
   Initially release both buttons. A boot-held GP20 must not start.

## A: renewal and automatic expiry

Press/release GP20 once. Left should run for approximately two seconds, then
stop; right stays stationary. Producer renewal ends at two seconds; the last
command expires within its remaining100ms lease. Nominal total duration is
about2.0..2.1s, subject to the observed clock/IRQ latency.

```text
MOTION[state=READY L=0 R=0 ... reason=0 ...] DRIVE=OFF
MOTION[state=RUNNING L=50 R=0 lease=... seq=... reason=0 ...] DRIVE=ON
MOTION[state=STOPPED L=0 R=0 lease=0 ... reason=3 ...] DRIVE=OFF
```

Sequence increases while RUNNING. Report count/lease depend on scheduling.
`cutoff` can increment if IRQ stops output, or remain unchanged if the owner
detects expiry first. A timeout discovered during rearm can leave err=-50 with
reason3; retain that log rather than claiming measured timing. Encoder warning
`ENC_UNCERTAIN=1` is still unqualified feedback and must not cause a restart.

## B: GP21 during the longer run

Reset once without reflashing: close the monitor, switch OFF and disconnect
both bank power and Pico USB until lights go out. Reconnect the known bank
VIN/Pico USB arrangement and open the monitor. Confirm READY again.

Press/release GP20, then press GP21 while left is turning, within the first
second. Expect prompt stopping and
`MOTION[state=STOPPED L=0 R=0 lease=0 ... reason=2 ...] DRIVE=OFF`.
Later GP20 must not restart. If reason3 appeared before GP21, the result only
establishes expiry, not stopping during motion.

New MOTION reason codes differ from the older pulse format:

| Reason | Meaning |
|---|---|
|0 | No stop cause |
|1 | API STOP request |
|2 | GP21 button |
|3 | Command or timer-lease expiry |
|4 | Hardware/readiness failure; state FAULT |

Switch OFF if the wrong wheel moves, motion continues or a real fault appears.
Send short A/B logs and physical observations; switch OFF afterward. No new
wiring photo is required. Simultaneous two-wheel load, accurate encoder control
and ground navigation remain later validation.

Rebuild from Ubuntu in this checkout:

```sh
python3 tools/build_wsl.py --sdk "/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0" --motion-app --motor-focused --label UNIQUE_LABEL
```

The command builds/checks only; it never flashes, connects MQTT or starts motors.
