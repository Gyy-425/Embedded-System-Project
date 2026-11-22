# Black and background RGB track captures — 17 November 2026

| Sample | Count | Raw R | Raw G | Raw B | Clear | Normalized red | v0.12 output |
|---|---:|---|---|---|---|---|---|
| Initial centred black |26|202|211|193|528–529|1365|26 UNKNOWN|
| Repositioned black |25|199|208–209|191|523|1360–1363|25 UNKNOWN|
| Cardboard |22|389–390|359|325|837–838|1485–1487|7 BACKGROUND /15 UNKNOWN|

Every record is valid, unsaturated, status0/errors0, exposure4x/50400us and
age20–100ms. All rows retained. User labels establish the known surfaces;
logs alone do not establish the physical reason for changed optical response.

In v0.12 black's normalized red lower bound1367 rejects the new black values;
background upper bound1486 rejects the1487 readings. All other normalized
channels and clear intensity remain within their corresponding profiles.

v0.12.1 changes only BLACK red lower to1352 and BACKGROUND red upper to1495,
with provisional allowance8/4095 beyond the newly observed extrema. These
small one-sided changes preserve exposure, all other bounds, UNKNOWN,
ambiguity and freshness/acquisition checks. Physical robustness is not proven.

Keep original489 observations and the new73 adaptation observations separate.
Strict C replay passes all562, including old white/yellow and unsupported red.
Since the new data inform the correction, their success is regression evidence,
not an independent validation or a statistical recognition accuracy estimate.

Next live check: same mounting/illumination, few labelled black/cardboard RGB
pairs with BWY_20261117 visible. This remains read-only; continue separate
four-position IR tests for following after the recognition check.
