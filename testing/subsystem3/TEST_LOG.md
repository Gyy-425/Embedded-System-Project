# Communication diagnostic test log

Original consumer/statistics example cases are retired. Module 3 currently reports copied module snapshots and radio-driver readiness only.

| ID | Check | Method | Expected | Status |
|---|---|---|---|---|
| S3-M101 | Readable USB report | Open actual Pico COM port at 115200 | Repeating complete records, no ambiguous measured-colour claims | PENDING hardware |
| S3-M102 | Radio readiness | Observe RADIO_READY and boot diagnostics | Driver state is visible; no assertion of network/MQTT connectivity | PENDING hardware |

MQTT and command validation are later milestones, not claimed results of this diagnostic build.
