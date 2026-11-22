# RGB calibration: black, white tape 1, yellow and cardboard

Recorded setup: RGB optical gap25.37mm, IR gaps about15.8mm, IR centre spacing34mm; RGB chip13.5mm behind IR row. Indoor lighting, illumination LED ON, laptop USB only, mounting unchanged and background matching the intended track. Third tape is YELLOW, not blue. Source has an obsolete PENDING heading; preserve it as captured, not as evidence of an empty file.

## Data and quality

| Surface | Calibration count | Validation count | Total | R mean | G mean | B mean | Clear range | Clear mean |
|---|---:|---:|---:|---:|---:|---:|---|---:|
| BLACK | 53 | 51 | 104 | 266.01 | 267.46 | 245.84 | 605–662 | 637.00 |
| WHITE TAPE 1 | 52 | 48 | 100 | 567.54 | 579.68 | 545.45 | 1428–1435 | 1431.40 |
| YELLOW | 50 | 51 | 101 | 449.37 | 412.11 | 322.09 | 981–984 | 982.86 |
| CARDBOARD | 50 | 50 | 100 | 351.65 | 336.97 | 305.60 | 778–821 | 815.17 |

All405 RGB records pass ready/ID/has/valid/saturation/age/status/error/DRIVE checks, at gain4x and integration50400us. No malformed RGB lines, duplicate timestamps or repeated sequence IDs. Every class has at least100 total samples; white validation has48 rather than50, which is reported rather than padded. No readings discarded.

## Separation and exploratory prediction

Across both passes, clear-channel ranges are disjoint: black605–662, cardboard778–821, yellow981–984, white1428–1435. This is strong stationary separation for these fixed samples. Yellow has a lower blue-channel proportion than white/cardboard; do not rely on brightness alone for future variable conditions.

A simple raw-RGBC nearest-centroid baseline fitted on the205 calibration readings correctly classifies all200 validation readings: black51/51, white48/48, yellow51/51, cardboard50/50. No validation-specific fitting, temporal smoothing or rejection is used. The classifier family was selected after inspecting these data; this is exploratory validation, not a blinded final assessment or200 independent repositioned trials. Background is explicitly represented. Always-predict-nearest is unsuitable as the final UNKNOWN policy.

Do not deploy literal training min/max boxes: black calibration clear661–662 shifts to605–639 after repositioning; white1428–1429 shifts to1434–1435. Mean-profile/normalised-colour matching with measured tolerances and confidence rejection should be investigated, then tested on fresh data.

## Recommendation and geometry

Proceed to a C RGB classifier for BLACK/WHITE/YELLOW plus background/unknown handling, replaying these records in host tests. Preserve raw RGBC and exposure metadata. Recognition remains stationary-only until mixed-edge and moving tests pass. No more large stationary capture is needed now.

For a first black-line geometry trial,34–35mm tape width is a provisional engineering starting point with34mm IR spacing, to put heads near opposite edges. It is not a validated final width: sweep the strip sideways to establish actual edge response before making a full board. The44.63mm combined assembly width does not determine tape width. For white/yellow following, assess their IR edge/background contrast separately; an RGB centre sensor does not supply left/right position by itself.

Keep the current RGB height/exposure for the next stationary software milestone. A25.37mm gap cannot establish the optical field of view or shortest segment capability by itself. The13.5mm longitudinal offset creates detection timing differences during motion. Do not specify a speed/segment limit without a moving trial. Changes to mounting or exposure require revalidation.

No firmware, live calibration or wiring was edited by this analysis. Raw copy and full JSON retain all records and diagnostic statistics.
