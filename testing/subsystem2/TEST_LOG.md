# Motion diagnostic test log

Original template cases S2-U01 and S2-U02 referred to removed producer/pool examples and are retired, not reused. The original YYYY-MM-DD/abc1234 row was an unfilled template, not a real result.

| ID | Check | Method | Expected | Status |
|---|---|---|---|---|
| S2-U101 | Forward quadrature cycle | TEST=1 portable on-target assertion | Count +4 | PENDING target execution |
| S2-U102 | Diagonal transition | TEST=1 portable on-target assertion | Invalid counter increments, movement count unchanged | PENDING target execution |
| S2-U103 | Signed overflow | TEST=1 portable on-target assertion | Saturation flag set, no overflow | PENDING target execution |
| S2-M101 | Left/right encoder routing | Gently turn one raised wheel slowly at a time | Correct counter/A-B states change; reverse changes sign | PENDING hardware |
| S2-M102 | No drive at startup | Observe raised wheels during normal boot | No commanded wheel movement | PENDING hardware |

The host counterparts cover five groups: forward/reverse cycles, bounce/no-change, diagonal rejection/resynchronisation, signed/invalid-counter saturation and independent wheels. They have been executed on Ubuntu with ASan/UBSan; see testing/integration/logs/2026-11-05-host-encoder.log and the build-result JSON. Host passes are not physical encoder or target passes. Slow polling is not qualified for motor-speed feedback.

Additional host regression: testing/host/test_motor_startup.c checks that every motor-startup operation stays non-driving, preserves an unrelated GPIO and finishes in coast. It failed against the original per-pin enable sequence and passed after the masked-enable correction. The final run is logged at testing/integration/logs/2026-11-05-host-motor-startup.log. Physical startup observation S2-M102 remains pending.
