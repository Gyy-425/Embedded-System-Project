# First user-run diagnostic results, 6 November 2026

Firmware: AAI05_Diagnostics_v0_1.uf2. The coordinator supplied console records and confirmed the physical right wheel was rotated first, then the left. This is user-run hardware evidence, not an agent-observed physical test. Raw wheel attachment is preserved unchanged as 2026-11-06-manual-wheel-v0_1.log.

## Established observations

- Firmware USB console is operating. Five initial stationary records have IR valid=1, err=0, age=10; encoder L/R=0/0, bad=0/0, sat=0; DRIVE=OFF and RADIO_READY=1.
- Initial IR ranges in those five records: L=3492..3544 and R=3516..3539.
- Ten supplied IR test records show L=307..3950 and R=224..3945, all valid=1 and err=0. Both analogue channels respond. In a subsequent reply the coordinator confirmed checking both IR sensors successfully after receiving the individual-sensor test instructions. Record the basic per-sensor functional check as user-confirmed. Surface-labelled numerical results, colour polarity, calibration repeatability and seven-tape separability are not yet established.
- Wheel attachment: 143 complete records spanning 36920 ms, at 260 ms record spacing. All have valid IR reads, DRIVE=OFF and RADIO_READY=1.
- Right encoder count varies -3..19; left varies -15..1. The channel responding first is R, then L, consistent with the coordinator's physical actions. Both packed A/B states vary. This supports encoder channel identity and response, not accurate edge totals or calibrated distance.
- Invalid-transition counters rise from 0/0 to 66/60 (left/right). Count saturation remains clear. Accurate quadrature counting is not passed.
- Latest encoder clarification: faster manual rotation can leave the displayed count unchanged or give inconsistent changes; gentle slow rotation gives steady changes but requires careful movement. This supersedes the earlier interpretation of 'skewed' as a mechanical problem. Do not record motor misalignment, stiffness or a hardware repair as established by this clarification. Precise hand rotation is not an acceptance requirement for the eventual encoder driver.

## Software timing finding

The motion task calls tk_dly_tsk(1), but config/config.h sets CNF_TIMER_PERIOD=10 ms. kernel/tkernel/timer.c adds TIMER_PERIOD to relative waits and releases events on timer ticks. From a tick-aligned request, 1+10 ms is serviced on the next eligible 20 ms boundary. The nominal encoder polling interval is therefore approximately 20 ms, potentially longer under scheduling load; it was not measured with a logic analyser. The earlier description '1 ms polling' described the requested delay and was inaccurate as a sampling-rate claim.

The report's requested 250 ms delay gives 260 ms under the same timer rule, consistent with the supplied records. Missed encoder transitions are plausible at the much slower actual polling rate. The records do not establish polling as the sole cause or exclude electrical noise/contact issues. Do not change wiring or the kernel tick speculatively.

## Remaining work

1. Replace or otherwise qualify encoder acquisition independently of coarse task-delay polling before treating counts as reliable speed/distance feedback; evaluate a compatible shared interrupt or PIO approach against the existing port/radio implementation.
2. Retest individual wheel motion and stationary stability with the improved acquisition and record invalid-counter deltas. Direction signs remain a convention to establish, not an A/B rewiring requirement.
3. Basic individual IR response is user-confirmed; do not request the same check again without a new reason. Use the actual seven tape samples at fixed height to obtain labelled calibration distributions and assess separability later.
4. Controlled motor-drive direction, speed feedback, motor-load power behaviour, Wi-Fi association/MQTT and colour following remain untested. RADIO_READY is driver readiness only, and DRIVE=OFF is firmware-reported mode.
