# Black-profile coverage correction

Latest22 user-labelled black observations are valid/fresh/unsaturated:16 miss
red lower1352 by1;2 miss blue upper1319 by1;4 match. Same profileBWY_20261117,
4x/50400us, no I2C error. PIO_BAD1 is a separate sticky encoder-quality warning,
not the RGB NO_MATCH cause. Car remains IDLE/output OFF in this capture.

Earlier one-sided8-unit allowance was insufficient. Refit black chromatic bounds
consistently from177 labelled black observations:104 original cal/repositioned,
51 actual-track cal/repositioned and22 new. Observed normalized minima
[1351,1398,1287], maxima[1400,1429,1320]; same original engineering margin32
gives lower[1319,1366,1255], upper[1432,1461,1352]. Retain clear495–828,
all other class profiles, unknown/ambiguity, health, exposure and motion guards.
Tolerance remains provisional; no general lighting/colour guarantee is claimed.

- [x] Add observed flicker regressions and real-overlap rejection; observe RED.
- [x] Preserve latest fixture, data hashes and177-row adaptation provenance.
- [x] Apply new black box/model revisionR2; replay all584 source-labelled rows.
- [x] Build distinct v0.13.3 D40/T8 normal/NO_DRIVE with reviewed separation,
  unchanged controller, tests/compatibility and old-image hashes preserved.

No steering/speed change: compare recognition first on the same stationary strip.
New22 and original repositioned black rows used here are calibration/adaptation,
not independently held-out accuracy. Existing solo checkout, no flash/push.

Software correction complete:17-stage RGB/584-row replay,39-stage follower
release and69-stage existing regression PASS.110 earlier UF2s preserved.
Independent review clean; R2 tag verified in powered ELF. Local images and
manifest published; physical R2 confirmation/following still pending.
