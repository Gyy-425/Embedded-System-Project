# User-run v0.2 manual encoder results, 6 November 2026

The coordinator supplied two console attachments after the v0.2 retest instructions. They identify the first as rotation of the physical left wheel and the second as rotation of both wheels, and subsequently confirmed deliberate forward and backward rotation. Raw attachments are preserved unchanged beside this file. These are user-run observations, not agent-performed physical tests. Rotation speed, exact rotation angle and reversal timestamps were not labelled.

| Log | Complete records | Duration | L count range | R count range | IRQ first -> last |
|---|---:|---:|---:|---:|---:|
| 2026-11-06-v0_2-left-manual.log | 116 | 29900 ms | -44..88 | 0..0 | 0 -> 429 |
| 2026-11-06-v0_2-both-manual.log | 95 | 24440 ms | -30..131 | -140..80 | 1388 -> 2294 |

All 211 supplied records have ENC_READY=1, capture err=0, bad=0/0, over=0/0, sat=0, DRIVE=OFF, IR valid=1/err=0 and RADIO_READY=1. Records are spaced 260 ms apart. Counts and IRQ values contain stable plateaus as well as periods of activity. The first log ends L=-25/R=0; the second ends L=0/R=0 after both counters varied. A nonzero starting IRQ count is normal for a cumulative counter; the second trial adds 906 IRQ batches.

In the left-only log, all four left A/B states appear while right count and A/B remain 0. This supports left-channel identity/isolation under the supplied action. All four A/B states on both channels appear in the second log, supporting both input responses. The second action is labelled both-wheels, so it is not a separately labelled physical right-only isolation trial.

The largest adjacent displayed changes are 62 left edges in the first log and 165 right edges in the second. These are net changes over a 260 ms report interval, not one physical edge per report or a measured maximum capture rate. Counts are signed net edges: deliberate reverse rotation reduces or reverses the count trend and can return the net count to zero. The coordinator confirmed intentional forward/backward movements, so the increasing/decreasing segments and final zero counts in the second log are reasonable. Exact physical forward-sign convention remains to be established with labelled motor direction tests.

Assessment: PASS for the basic manual encoder functional check under the supplied conditions, with no detected invalid/coalesced batches in either trial. v0.1's separate trial reached bad=66/60; the trials are not speed-matched, so do not claim a quantified error-rate improvement. Zero detected errors alone does not prove no missed full cycles. Counts/revolution, physical forward-sign convention, calibrated distance, maximum reliable edge rate, powered motor performance and load/power stability remain unverified. DRIVE=OFF is software-reported mode; no automatic physical motor-motion observation is inferred from it. RADIO_READY does not establish association/MQTT.
