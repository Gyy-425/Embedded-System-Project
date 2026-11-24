# Encoder distance calibration — straight run, 22 Nov 2026

## Files

| File | Content |
| --- | --- |
| `2026-11-22-distance-straight-340mm.json` | Calibration used by the firmware |
| `2026-11-22-distance-straight-350mm-provisional.json` | Earlier provisional value, kept for traceability |

Both files are copied unchanged from `output/firmware/distance_straight_20261122/`. That folder also holds the build logs and the `AAI05_Distance_Straight_2s_D25[_R2].uf2` images used for the run.

## Values

| Profile | Wheel | Reference distance | Encoder counts | Forward sign | Scale (µm/count) |
| --- | --- | --- | --- | --- | --- |
| 340 mm (in use) | Left | 340 000 µm | 4455 | −1 | 76.3 |
| 340 mm (in use) | Right | 340 000 µm | 4280 | +1 | 79.4 |
| 350 mm (provisional) | Left | 350 000 µm | 4494 | −1 | 77.9 |
| 350 mm (provisional) | Right | 350 000 µm | 4307 | +1 | 81.3 |

The scale column is reference_um ÷ reference_counts. It is derived here and is not stored in the JSON.

## How it is used

The 340 mm values are compiled into `app_program/subsystem2/follow_route.c` as:

```c
.left = {340000U, 4455U, -1},
.right = {340000U, 4280U, 1},
```

Per-colour section distances in the MQTT follow image come from these values. The `motion-runtime-distance` stage of `tools/check_encoder_pio.py` exercises that path on the host.

## Limits (stated honestly)

- **Not physically validated.** Both JSON files carry `"status": "USER_INPUTS_NOT_PHYSICALLY_VALIDATED"`. The reference distance and counts were entered from one straight 2 s run at D25. No repeat runs or tape-measure checks are recorded, so repeatability and error are unknown.
- **Uneven wheels.** The left wheel counts about 4 % more than the right over the same distance. The firmware keeps one scale per wheel; it does not average them.
- **Next validation step:** do three or more straight runs over a measured length. Record the reported distance against the measured distance, and the % error, in `testing/system/TEST_LOG.md`.
