# Black track RGB rejection — 17 November 2026

26 stationary observations, user-labelled black. All RGB data valid, unsaturated,
status0/errors0, age20–100ms, gain4x/integration50400us. No firmware/model edit.

| Feature | Observed | Current BLACK bounds | Result |
|---|---:|---|---|
| Raw R/G/B |202/211/193|Not compared directly|—|
| Normalized red |1365|1367–1431|Below by2|
| Normalized green |1426|1377–1441|Within|
| Normalized blue |1304|1255–1319|Within|
| Clear |528–529|495–828|Within|

All26 logged UNKNOWN/NO_MATCH results are explained by the red fraction alone.
The normalized red difference is2/4095, about0.049 percentage points. This is a
profile coverage limitation, not evidence of an I2C/acquisition error. The
reduction in raw clear brightness is already inside the configured envelope.

Root physical cause is unconfirmed: narrow35mm geometry/edge mixing, mounting,
illumination, tape differences or other setup changes cannot be resolved from
logs. Asked user to confirm optical centring and setup. IR is asymmetric:
left227–265, right1635–1676. Capture position was not labelled; do not assume
this is a centred steering baseline or diagnose a failed sensor from it.

Next: confirm placement and get a short independently repositioned actual-track
black check plus current bare-cardboard control. If stable, update BLACK profile
minimally, replay all previous classes/unsupported-red cases, and issue a distinct
image. Do not broaden all profiles or replace calibration from one stationary
view. A second100-row collection is not necessary for this check.
