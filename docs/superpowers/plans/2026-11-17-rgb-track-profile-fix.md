# Actual-track RGB profile correction

Scope: existing solo checkout; fix two profile coverage limits using labelled
user captures. No classifier algorithm, driver, motion or MQTT changes.

Diagnosis:26 initial known-black samples fail only red fraction1365<1367.
Repositioned25 black samples have red1360–1363;22 cardboard samples have
red1485–1487, causing7 BACKGROUND and15 UNKNOWN at upper bound1486. All
samples valid/fresh/unsaturated with unchanged reported4x/50400us exposure.

Correction: lower BLACK red bound to1352 (new observed minimum1360 minus8),
raise BACKGROUND red bound to1495 (new maximum1487 plus8). This is a small
one-sided engineering allowance, not a statistically measured confidence
interval. All other bounds/exposure/health checks and rejection stay unchanged.
The gap between black and background cannot be assessed from red alone: all
three fractions and clear still must match; ambiguity continues to reject.

- [x] Preserve new raw sources and distinct adaptation fixture/provenance.
- [x] Add observed black/cardboard regression cases; observe RED on v0.12.
- [x] Apply two-bound correction, replay original489 plus new73 observations.
- [x] Verify strict/sanitized/ARM tests and distinct normal/TEST v0.12.1 firmware
  with prior images preserved; review, document and deliver normal image.

New73 samples are adaptation evidence, not an independent accuracy assessment.
Live v0.12.1 recognition and moving/steering behaviour remain untested.

RED: rgbtrack-20261117-red failed the new known-black assertion before the
model change. GREEN: rgbtrack-20261117-green passed strict/sanitized host and
ARM compile tests; all562 replay records match their expected outcomes.

Release complete:23-stage final check and69-stage regression PASS,77 earlier
UF2s unchanged. Independent review found no Critical/Important issue. Normal
image and manifest published locally; live v0.12.1 check pending.
