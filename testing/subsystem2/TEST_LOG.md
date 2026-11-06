# Motion diagnostic test log

Original template cases S2-U01 and S2-U02 referred to removed producer/pool examples and are retired, not reused. The original YYYY-MM-DD/abc1234 row was an unfilled template, not a real result.

| ID | Check | Method | Expected | Status |
|---|---|---|---|---|
| S2-U101 | Forward quadrature cycle | TEST=1 portable on-target assertion | Count +4 | PENDING target execution |
| S2-U102 | Diagonal transition | TEST=1 portable on-target assertion | Invalid counter increments, movement count unchanged | PENDING target execution |
| S2-U103 | Signed overflow | TEST=1 portable on-target assertion | Saturation flag set, no overflow | PENDING target execution |
| S2-M101 | Left/right encoder routing | Gently turn one raised wheel slowly at a time | Correct counter/A-B states change; reverse changes sign | 2026-11-06: right-first/left-second channel response confirmed by supplied records and coordinator; labelled direction-sign convention remains pending |
| S2-M102 | No drive at startup | Observe raised wheels during normal boot | No commanded wheel movement | PENDING hardware |
| S2-M104 | v0.2 stationary/slow/faster IRQ capture | docs/ENCODER_RETEST_V0_2.md | Ready=1, correct wheel responds, stable stationary counts, no growth in bad/over during accepted trial | PENDING v0.2 coordinator run |

The host counterparts cover five groups: forward/reverse cycles, bounce/no-change, diagonal rejection/resynchronisation, signed/invalid-counter saturation and independent wheels. They have been executed on Ubuntu with ASan/UBSan; see testing/integration/logs/2026-11-05-host-encoder.log and the build-result JSON. Host passes are not physical encoder or target passes. Slow polling is not qualified for motor-speed feedback.

User-run v0.1 wheel records on 2026-11-06 contain 143 reports over 36.92 seconds and end at bad=66/60. Latest clarification: fast hand rotation can leave counts apparently stationary or inconsistent; careful slow rotation gives steady changes. Accurate edge counting is not passed. The requested 1 ms task delay yields nominal approximately 20 ms polling under the current 10 ms tick and wait rounding, so missed transitions are plausible; electrical causes are not excluded. Do not record mechanical misalignment or stiffness as confirmed from the earlier word 'skewed'.

Additional host regression: testing/host/test_motor_startup.c checks that every motor-startup operation stays non-driving, preserves an unrelated GPIO and finishes in coast. It failed against the original per-pin enable sequence and passed after the masked-enable correction. The final run is logged at testing/integration/logs/2026-11-05-host-motor-startup.log. Physical startup observation S2-M102 remains pending.

v0.2 adds testing/host/test_encoder_capture.c (7 groups, including 4000 edges per wheel between snapshots, coalescing, unrelated event preservation and late-edge interleavings) and test_encoder_startup.c (real backend with boundary fakes, both-core GPIO ownership conflicts, definition failure and owned-pin setup). Both pass with strict compiler warnings and ASan/UBSan in logs/2026-11-06-v0_2-final-build-result.json. The startup host harness uses the non-SMP conditional; actual SMP lock implementation/core affinity were reviewed and the SMP=1 firmware compiled, but hardware latency is not measured. Legacy polling and both review defects have expected failing assertion logs dated 2026-11-06; none are claimed as on-board executions.
