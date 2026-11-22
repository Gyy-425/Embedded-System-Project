# Ground weaving: isolated gentler-steering trial

Current comparison image: `AAI05_Black_Follow_v0_13_3_D40_T8.uf2`, model R2.
The v0.13.2 trial exposed black-profile boundary flicker; see BLACK_PROFILE_R2.md.
The steering comparison below is unchanged; confirm recognition first.
Ground trial now recorded: see
`../testing/calibration/2026-11-17-black-follow-r2-ground-findings.md`.
Three observed runs completed their one-second bound; the user reports much
straighter motion with a small end turn. One captured early AMBIGUOUS stop
still needs its triggering input to establish the physical cause. This is
limited prototype evidence, not sustained track-following acceptance.

## What the moving capture shows

Two one-second runs at duty limit40, with eight RUNNING reports:

| First run elapsed | Turn | Electrical L/R |
|---:|---:|---|
|100ms|−20|−16/+40|
|360ms|+20|−40/+16|
|620ms|+6|−40/+31|
|880ms|−20|−16/+40|

Seven of the eight reports across both runs reach maximum steering magnitude.
Both finish COMPLETE; PIO_BAD0 and valid/fresh IR. RGB is BLACK in the RUNNING
reports. Raw black responses exceed2000 while cardboard is around200, so
there is good reflectance contrast. This does not look like uncertain RGB
recognition or merely noise around a black/cardboard decision threshold.

The logs support strong alternating corrections consistent with hunting.
They do not prove a sole physical cause. Reporting every260ms cannot measure
the user's exact count of about five swings. IR reports and FOLLOW normalized
values are separate copied observations, so they need not correspond exactly.

Control normalization reaches1000 by raw L1368/R1444, based on the earlier
offset samples. Moving black can exceed3000 and therefore saturates that
position scale. Maximum correction can be reached well before the full black
response. This is an engineering control scale, not the RGB classifier limit.
Keep normalization and deadband unchanged for this isolated gain experiment.

## One change

Retain duty40, duration, sensor calibration, deadband and all safety checks.
Reduce nominal proportional turn strength20→8:

| Condition | Old D40/T20 L/R | New D40/T8 L/R |
|---|---|---|
|Centred|−40/+40|−40/+40|
|Maximum right correction|−40/+16|−40/+28|
|Maximum left correction|−16/+40|−28/+40|

This changes differential correction, not selected maximum duty. Actual motor
response and average movement under correction are not measured. It may help
overshoot or inside-wheel stalling; no smoother-following claim is made yet.
Do not broaden RGB profiles or adjust IR thresholds at the same time.

## Short car comparison

1. Flash the D40_T8 powered normal image with the usual power-off/BOOTSEL steps.
2. Check FOLLOW shows `limit=40 turn_max=8`, fresh RGB BLACK and ready=1.
3. Start centred: both norms approximately0/0. The supplied capture started
   with right norm1000; starting centred makes this comparison easier to judge.
4. Use one GP20 run on the same35mm straight strip, with no cable pulling it.
   GP21 and the nominal one-second limit remain. Keep clear runway.
5. Report whether the swings reduce and whether wheels roll smoothly. Save
   a few RUNNING/STOP FOLLOW lines if possible. There is no need for another
   large calibration capture or old per-wheel/network tests.

This local image has no live MQTT tuning commands. Supported build/flash
turn presets are8 and20; invalid presets are rejected. `--turn20` reproduces
the earlier steering strength for a controlled comparison, not a recommended
default. Actual stop distance, WCET, speed and sustained curve following remain
unqualified. The NO_DRIVE counterpart remains available for request-only checks.
