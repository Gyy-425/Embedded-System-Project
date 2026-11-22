# Red bordered lane v0.15, duty35

Keep the existing sensor wiring and mounting. Use red tape between two black
borders, with the same geometry as the yellow lane. Black is the border, not
an intended final lane interior. The third interior colour is not yet chosen.
This local image uses GP20/GP21; MQTT target selection is a later integration.

## Prepared behaviour

- `RED FOLLOW v0.15`, `track=RED_CORRIDOR target=RED`.
- `limit=35 turn_max=8`, model `BWYR_20261117_R4`.
- Centred request approximately L-35/R+35. Left IR approaching a border gives
  positive turn/right correction (up to L-35/R+25); right IR gives negative
  turn/left correction (up to L-25/R+35).
- Same per-channel normalization as yellow, gradual correction up to raw500.
  Simultaneous raw500 or higher is AMBIGUOUS and stops.
- Fresh valid RED match required. Other colours, UNKNOWN, invalid/stale data,
  GP21, hardware/driver faults or the nominal one-second bound stop motion.
  No automatic restart, reversal, search or branch selection.

The35% setting caps electrical duty; it is not measured speed. Lower starting
torque may affect movement. All stops coast; stopping distance is unmeasured.

## Short check, then movement

1. Close Serial Monitor and disconnect bank power/Pico USB with the car off.
   Hold BOOTSEL while connecting Pico USB. Copy
   `output/firmware/AAI05_Red_Follow_v0_15_D35_T8_NO_DRIVE.uf2` to RPI-RP2.
2. Open the Pico serial port at115200. Confirm the red identity/model/duty
   above, `inhibit=1`, healthy fresh data and RED/MATCH when centred.
   The supplied red captures show historical PIO_BAD1; check after a fresh
   stationary reboot. If it is immediately nonzero, save the startup lines.
3. At normal sensor height, position centre, slightly left and slightly right.
   RED should remain matched at each captured position. A brief GP20 request
   can confirm inward steering requests. Wheels remain off in this image.
   Release GP20 fully between requests. No new large data capture is required.
4. Power off and flash
   `output/firmware/AAI05_Red_Follow_v0_15_D35_T8.uf2`. Check `inhibit=0`, RED
   target and limit35. Close Serial Monitor and disconnect laptop USB for
   ground testing; use the existing power-bank VIN supply so USB cannot pull.
5. Centre the robot on a straight clear red section, keep GP21 reachable and
   press/release GP20 once. Expected: forward movement with inward corrections,
   then stop after the nominal one-second trial. Record smoothness and any
   early stop reason. If it stalls at35%, save the report rather than changing
   colour thresholds; duty is a separate tuning parameter.

There is no need to repeat the earlier per-wheel/MQTT bring-up. Red powered
following is still pending; the user's successful yellow trial does not
establish red tracking or recognition while moving.

## Yellow at the requested duty

`output/firmware/AAI05_Yellow_Follow_v0_14_D35_T8.uf2` retains YELLOW_CORRIDOR
and YELLOW gating with duty35 and R4 recognition. The previous yellow40 image
is preserved. This new35% yellow build has not itself been tested on the car.
The RGB model still recognises black for borders and diagnostics; the retained
plain-black images are historical tests, not the intended final interior mode.

## Rebuild

```powershell
wsl.exe -d Ubuntu -- python3 '/mnt/c/Users/Guo YouYan/Desktop/INF2004-main/AAI05_Firmware/tools/check_black_follow.py' --label red-new-label --track red --duty 35 --firmware --sdk '/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0'
```

Choose a new label. The script checks and builds distinct candidates; it does
not flash, connect to a broker, replace old images or change network secrets.
