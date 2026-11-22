# Red-strip edge check — 16 November 2026

Order assumed from requested sequence: centred, shifted left, shifted right, bare cardboard. User has not explicitly labelled positions in the attachments.

| Position | Count | Left range (mean) | Right range (mean) | L−R range (mean) |
|---|---:|---|---|---|
| centred (assumed order) | 21 | 185–221 (203.00) | 196–232 (209.95) | -45–24 (-6.95) |
| shifted left (assumed order) | 20 | 191–226 (207.75) | 192–227 (209.15) | -32–29 (-1.40) |
| shifted right (assumed order) | 20 | 187–227 (206.60) | 200–235 (217.55) | -44–27 (-10.95) |
| cardboard (assumed order) | 23 | 191–228 (207.26) | 198–426 (224.87) | -222–22 (-17.61) |

All 84 IR records have valid=1, err=0 and age at most 30 ms. All 84 accompanying RGB records have valid=1, sat=0, status=0, errors=0 and age at most 200 ms. No records removed. One right-IR reading of 426 occurs in the fourth capture; preserve it rather than silently filtering it.

Conclusion: these captures do not qualify reliable red-strip tracking with an IR left/right error. Position ranges overlap, and left/right difference changes sign within every capture. This is not proof that all tracking strategies are impossible. No firmware fault is established.

RGB clear means: centred 643.71; left 625.95; right 626.05; cardboard 706.83. The red/background difference supports additional colour-recognition work, while left versus right is not resolved by this central RGB measurement. No red calibration model was fitted.

The new cardboard RGB capture differs from the earlier calibration (clear 778–821 versus 706–707 here). Exposure fields match but matching sample placement, height and illumination cannot be established from logs. Keep it as fresh validation evidence; do not assume earlier raw-brightness thresholds generalise.

Next: proceed with RGB recognition separately; verify a high-contrast strip edge response before implementing or claiming autonomous following. A black-strip trial can qualify the steering controller, but does not by itself demonstrate red/white/yellow path following. No code or wiring changes made.
