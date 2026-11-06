# IR diagnostic test log

Original template cases S1-U01, S1-U02 and S1-U03 referred to the removed GP19 LED example and are retired, not reused. Its YYYY-MM-DD/abc1234 row was an unfilled example, not a real result.

| ID | Check | Method | Expected | Status |
|---|---|---|---|---|
| S1-M101 | Left/right AO routing | Move light/dark material under each sensor individually | Correct raw channel responds; record actual values | 2026-11-06: coordinator confirmed both individual sensors work after receiving this procedure; detailed surface-labelled values not supplied |
| S1-M102 | Complete recent pair | Inspect real console over multiple seconds | valid=1, err=0 and recent age for successful pairs | PASS in supplied 2026-11-06 initial/IR/wheel console records: valid=1, err=0, age=10 |

The coordinator supplied real raw IR records and confirmed the individual-sensor functional check. These are user-run results, not agent-observed tests. Initial readings were L=3492..3544/R=3516..3539; the supplied IR test records span L=307..3950/R=224..3945 without surface labels. Raw ADC responsiveness alone is not colour classification. Seven-colour calibration and accuracy measurements remain pending.
