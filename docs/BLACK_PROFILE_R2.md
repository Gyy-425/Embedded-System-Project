# Black-profile coverage revision R2

Use `AAI05_Black_Follow_v0_13_3_D40_T8.uf2` for the current powered comparison,
or its `_NO_DRIVE` counterpart. Check model=`BWY_20261117_R2`, limit=40 and
turn_max=8. This updates recognition coverage only; it does not change the
gentler steering, speed setting, GP20/GP21, run duration or safety deadlines.

## Exact failure in the latest capture

22 valid/fresh, unsaturated RGB observations at4x/50400us:

- 16 have normalized red1351, just below the old lower limit1352.
- 2 have normalized blue1320, just above old upper limit1319.
- 4 match, explaining ready=1 only intermittently.

The car is IDLE/DRIVE OFF throughout. Guarding starts on fresh RGB BLACK
correctly prevents a run on UNKNOWN; the acquisition driver reports no errors.
PIO_BAD=1 is a separate sticky encoder-quality warning, not this NO_MATCH cause.
The earlier one-sided eight-unit chromatic allowance was too small.

## Refit coverage with the accumulated labelled data

Use177 known-black observations:104 original first/repositioned,51 actual-track
and22 current samples. Normalized q4095 extrema are [1351,1398,1287] to
[1400,1429,1320]. Applying the original engineering margin32 consistently gives:

| Channel | Lower | Upper |
|---|---:|---:|
|R|1319|1432|
|G|1366|1461|
|B|1255|1352|

Retain clear495–828 and every white/yellow/background bound. Ambiguous multiple
matches, missing/faulted/saturated/exposure-mismatched or stale data still
reject. Do not force UNKNOWN to BLACK in the follower. RGB colour matching
and IR steering thresholds remain separate.

Every584 recorded replay case passes, including all22 new samples, previous
other colours, unsupported red and explicit real-profile overlap rejection.
BLACK/BACKGROUND envelopes overlap in some hypothetical inputs, which must
continue to return UNKNOWN/AMBIGUOUS. Known data remain correctly separated.
These177 observations inform calibration; the51 original validation-black rows
are now adaptation, not independent held-out accuracy. New-lighting, different
geometry, moving recognition and the R2 hardware result remain unqualified.

## Quick confirmation

Flash the new normal image using the usual power-off/BOOTSEL procedure.
Keep car stationary on the same black strip, GP20 untouched. Verify R2 tag and
BLACK/MATCH across5–10 lines with ready=1. No100-reading capture or rewiring is
needed. The motor stays off until an explicit fresh GP20 press.

Then resume the single centred D40/T8 movement comparison described in
FOLLOW_STEERING_TUNING.md. If UNKNOWN persists, save the raw RGB and paired
RGB_CLASS fields; do not broaden ranges by hand or bypass presence protection.
