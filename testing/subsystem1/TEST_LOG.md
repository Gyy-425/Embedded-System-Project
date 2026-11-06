# IR diagnostic test log

Original template cases S1-U01, S1-U02 and S1-U03 referred to the removed GP19 LED example and are retired, not reused. Its YYYY-MM-DD/abc1234 row was an unfilled example, not a real result.

| ID | Check | Method | Expected | Status |
|---|---|---|---|---|
| S1-M101 | Left/right AO routing | Move light/dark material under each sensor individually | Correct raw channel responds; record actual values | PENDING hardware |
| S1-M102 | Complete recent pair | Inspect real console over multiple seconds | valid=1, err=0 and recent age for successful pairs | PENDING hardware |

No physical IR results or seven-colour accuracy measurements have been recorded. Raw ADC responsiveness alone is not colour classification.
