# Three-colour tape feasibility screening

Received 14 November 2026. User confirmed the same heights, lighting and power, with both sensors over the same tape, for all six samples. Numeric heights and exact lighting details were not supplied for this session. Raw inputs are retained separately, byte-for-byte.

| Sample | Count | Left range | Left mean | Left SD | Right range | Right mean | Right SD |
|---|---:|---|---:|---:|---|---:|---:|
| black | 85 | 2523–2562 | 2545.46 | 12.51 | 2864–2910 | 2884.80 | 13.39 |
| blue | 108 | 170–229 | 203.08 | 12.84 | 190–238 | 211.84 | 12.34 |
| yellow | 110 | 173–219 | 194.95 | 14.06 | 180–225 | 201.65 | 13.72 |
| whitetape1 | 147 | 162–203 | 181.90 | 12.72 | 135–249 | 189.91 | 15.96 |
| whitetape2 | 140 | 165–208 | 186.68 | 13.02 | 165–214 | 187.56 | 14.74 |
| whitetape3 | 141 | 161–205 | 183.85 | 13.44 | 171–216 | 190.90 | 14.04 |

All 731 records have valid, error-free IR data, age<=100ms and DRIVE OFF; no duplicate timestamps within blocks. Black contains 85 readings rather than 100.

Black is clearly separated. Blue has larger average separation from every white than yellow. White tape 1 gives the largest left-channel separation from blue; white tape 2 the largest right-channel separation. Every white/blue and white/yellow pair overlaps in range on each channel. White1 and blue have no exact shared paired value in this finite dataset; that does not establish reliable classification.

An exploratory paired-reading nearest-centroid comparison (first/second half and reverse, from the same captures) gives:

| White | Third colour | Overall sample accuracy | White/third-colour samples only |
|---|---|---:|---:|
| whitetape1 | blue | 88.8% | 85.1% |
| whitetape1 | yellow | 76.6% | 68.9% |
| whitetape2 | blue | 87.7% | 83.5% |
| whitetape2 | yellow | 78.5% | 71.2% |
| whitetape3 | blue | 88.3% | 84.3% |
| whitetape3 | yellow | 78.3% | 70.9% |

These are exploratory within-capture split results, not independent validation or a firmware performance claim. Black inflates the overall score. Candidate selection also used these data. Full method and confusion counts are in the JSON.

Recommendation: provisionally investigate black + blue + white tape 1; tape 2 is a backup. This combination is plausible for further controlled testing, but not cleanly separable using the existing full-range interval classifier. Do not install overlapping threshold ranges as if they establish reliable recognition. Validate with fresh repositioned and moving captures, including background, and limit smoothing delay to the chosen shortest segment/speed. A paired model cannot be assumed valid when left/right sensors see different surfaces while steering. No firmware or calibration changed.
