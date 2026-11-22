# Yellow corridor v0.14: first movement check

The user now reports this yellow-lane trial passed with smoother movement.
This guide describes the earlier40%/modelR3 images. For the new35% yellow
image and current modelR4, see `RED_FOLLOW_TEST.md`. Longer routes remain untested.

This is a separate local GP20/GP21 trial, not yet MQTT target following. Keep
the existing wiring and sensor mounting. Use the yellow lane with black
borders used in the three captures, not the plain black strip. Duty40 is an
electrical limit, not a measured physical speed. The nominal run is one second.

## What changes

Track is fixed to YELLOW_CORRIDOR and the RGB target is YELLOW. Left IR rising
steers right; right IR rising steers left. Centre baselines are L255/R236,
with gradual correction reaching its limit near raw500 on either channel.
There is no abrupt turn at500. Both channels at500 or above cause AMBIGUOUS
STOP. A fresh invalid/stale IR or non-YELLOW RGB result stops movement.

The R3 model adapts yellow chromatic coverage from210 labelled samples,
including109 new assembled-lane observations. Every clear bound and the
black/white/background bounds remain unchanged. This is adaptation/replay
evidence, not independent measured moving accuracy. GPIO20/21, absolute
cutoff, release/rearm and one-writer motor ownership remain in effect.

## 1. Short request-only check

1. Close Serial Monitor, switch the car off and disconnect bank and Pico USB.
2. Hold BOOTSEL while connecting Pico USB, release when RPI-RP2 appears, and
   copy `output/firmware/AAI05_Yellow_Follow_v0_14_D40_T8_NO_DRIVE.uf2` onto it.
3. Open the resulting Pico serial port at115200. Confirm:
   - `track=YELLOW_CORRIDOR target=YELLOW limit=40 turn_max=8 inhibit=1`.
   - `model=BWY_20261117_R3 class=YELLOW reason=MATCH` with healthy fresh data.
   - `PIO_BAD=0` after reboot. All supplied stationary captures had a sticky2;
     if it persists before movement after reboot, stop and save those lines.
4. Rest the car at its normal height, with RGB over yellow and both IR heads
   in their centre positions. Release GP20, then press/release once.
   Centre should request approximately `L=-40 R=+40`, `turn` near0.
5. Reposition the robot slightly LEFT, release GP20, then press/release once:
   LEFT IR should rise and `turn` should be positive (right correction).
   At full correction the request is approximately `L=-40 R=+28`.
6. Reposition slightly RIGHT and repeat: RIGHT IR rises, negative `turn`,
   approximately `L=-28 R=+40` at full correction.

Rearm requires a fresh release/press; a held GP20 does not restart. Each
request ends COMPLETE after the bounded run. Inhibit1 keeps DRIVE=OFF even
when RUNNING requests are printed. No large sample collection is needed.

## 2. One powered straight-lane run

1. Power off and flash `output/firmware/AAI05_Yellow_Follow_v0_14_D40_T8.uf2`
   using the same BOOTSEL procedure. This image has `inhibit=0`.
2. For a quick stationary status check, confirm the yellow track/target and
   modelR3, `PIO_BAD=0`, `ready=1` and RGB YELLOW/MATCH. It starts with no drive.
3. Close the monitor, disconnect Pico USB and use the existing bank VIN power
   arrangement for the ground trial so the cable cannot pull the car.
4. Place it centred on a straight clear section, with room beyond the trial
   for coasting. Press/release GP20 once. It should move forward, correct away
   from either border, and stop after the nominal one-second bound. GP21 must
   remain reachable to stop earlier.
5. Report whether the correction is smooth and inward. If it stops early,
   capture FOLLOW's reason plus nearby IR/RGB lines on a follow-up monitored
   check; do not adjust multiple thresholds or broaden the RGB model blindly.

All stops coast; stopping distance and curve tracking remain unmeasured.
This firmware does not search for a lane, choose branches, reverse, or resume
automatically. Finish this straight trial before extending duration or adding
Wi-Fi mission integration. Earlier black UF2 files remain usable and unchanged.

## Rebuild

From PowerShell, with Ubuntu/SDK available:

```powershell
wsl.exe -d Ubuntu -- python3 '/mnt/c/Users/Guo YouYan/Desktop/INF2004-main/AAI05_Firmware/tools/check_black_follow.py' --label yellow-new-label --track yellow --duty 40 --firmware --sdk '/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0'
```

Use a new label. The checker creates a distinct candidate folder; it never
flashes the car or replaces previous images.
