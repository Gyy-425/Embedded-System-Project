# Selected tapes and background — repositioned capture findings

User confirms white1/white2 are two captures of the SAME selected white tape 1. They are not different tape types. Height, power and lighting values are not written in this file.

| Surface | Records | Left range (mean) | Right range (mean) |
|---|---:|---|---|
| black | 45 | 2608–2685 (2646.49) | 2814–2886 (2852.33) |
| white | 40 | 161–206 (181.57) | 170–213 (188.68) |
| blue | 41 | 184–226 (205.56) | 194–234 (213.83) |
| card | 40 | 180–226 (200.43) | 189–232 (208.53) |

All 166 records are valid, error-free, age<=100ms and DRIVE OFF; no malformed lines or duplicate timestamps. Unequal block sizes are retained rather than padded or selectively trimmed.

Black is clearly separated. White versus cardboard has mean gaps 18.85 left and 19.85 right but overlapping ranges. Blue versus cardboard has mean gaps only 5.14 left and 5.30 right, compared with sample SD about 12–15 counts. Thus blue/background distinction is the main concern for following on this cardboard.

Repeat 1 -> repeat 2, exploratory four-class paired nearest-centroid check: 10/21 blue samples classified as cardboard, and 10/21 cardboard samples classified as blue. Black 21/21 correct; white 14/19 correct. Reverse-direction counts and full method are in the JSON. These diagnostic results are not deployed firmware performance and do not prove that every possible classifier or temporal method must fail.

Black repeat means shift by +39.80 left and -29.29 right; black remains clearly separated, but the shift shows repositioning sensitivity. White right shifts +6.13; blue right shifts -5.70; cardboard means shift +5.32 left and +2.30 right.

Conclusion: the three-tape screening recommendation did not establish path-following feasibility. Blue + current cardboard is not yet a reliable steering signal. Do not install full-range thresholds or build the entire three-colour track on the assumption of reliable following. Scope-consistent options are to screen a different background or third tape against all selected surfaces, or investigate a bounded temporal classifier and validate it on fresh repositioned and moving data. Different backgrounds/materials are not guaranteed improvements. Raw colour reporting and target-path following require separate validation. No firmware, thresholds or previous captures modified.
