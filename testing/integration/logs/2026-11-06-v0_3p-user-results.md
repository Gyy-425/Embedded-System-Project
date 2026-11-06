# User-run coast-only probe result, 2026-11-06

Source: coordinator's uploaded serial attachment, copied byte-for-byte to `2026-11-06-v0_3p-user-probe.log`, SHA256 `605e95201eb1e1831f139882ced9a9595f64b196cdf1930cac931ddadc4c9576`. The coordinator pressed and released GP20 and separately confirmed **both wheels stayed still**. No agent flashed firmware or observed the hardware.

## Observed records

The file contains 25 reports from t=143260 through t=149500, spanning 6240 ms at uniform 260 ms intervals: 19 IDLE, two PULSE and four COOLDOWN records. All identify MOTOR_MODE=PROBE and DRIVE=OFF, with valid IR, ENC_READY=1, zero capture/motor errors, bad=0/0, over=0/0, sat=0, irq=0, unchanged L/R=0 and stop=0.

| t (ms) | Recorded state | Interpretation |
|---|---|---|
| 143260 | IDLE next=2, M1 D1, us=400012, cutoff=1 | First request had already completed before this excerpt began; its active interval is not captured |
| 145080 | PULSE next=2, M1 D2, us=0, cutoff=1 | Second virtual request active |
| 145340 | PULSE next=2, M1 D2, us=0, cutoff=1 | Reporting continues during the active interval |
| 145600 | COOLDOWN next=3, M1 D2, us=400004, cutoff=2 | Automatic cutoff recorded and controller advanced |
| 146640 | IDLE next=3, us=400004, cutoff=2 | Cooldown completed |
| 149500 | Same healthy IDLE state | Continued reporting after the request |

## Result and limits

**PASS the captured M1 D2 coast-only start/cutoff/cooldown check** (S2-M107 / INT-106) for this user-run trial. The GP20 action was accepted. PULSE is explicitly present twice even though the coordinator did not notice it live. No physical wheel pulse is expected in this mode. The earlier M1 D1 completion is supported by retained fields, without a captured start interval.

The recorded 400004 microseconds is the software timer span at entry to stop, not a measured physical motor waveform or guaranteed IRQ latency. This is not a four-step M1/M2 qualification, powered motor pass, maximum encoder-rate test or proof of the original freeze's cause.

The earlier symptom did not reproduce during this coast-only request: controller progression, PWM/alarm setup, timer interrupt/cutoff and continued reporting operated together in the probe trial. Compared with the reported powered v0.3 stall, releasing the coast overrides and the resulting motor/encoder activity remain the distinguishing area to isolate. Electrical load/noise, encoder IRQ workload and timing-dependent software are still hypotheses; do not label brownout or a software deadlock as established. A normal-output test with motor windings disconnected can separate output switching from physical motor activity; no new firmware fix or powered retry is claimed here.
