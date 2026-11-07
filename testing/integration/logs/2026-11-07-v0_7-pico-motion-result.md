# v0.7 actual Pico Motion result

7 November2026. The coordinator reports left-wheel movement after GP20,
automatic stopping, GP21 stopping the moving wheel with reason2, and subsequent
GP20 presses producing no restart. The assistant inspected retained VS Code
COM3/115200 output. Serial was already closed and no port selected at inspection.
No upload, reconnection, power action, button press or command was performed.

Selected UI fields below are transcribed from the retained announcement and
visible rows, not a full raw-log export. The startup banner was not captured.

## A: automatic expiry

| Observed t (ms) | State | L/R duty | Sequence | Reason | Cutoff | Error | DRIVE |
|---:|---|---|---:|---:|---:|---:|---|
|135200 | READY |0/0 |0 |0 |0 |0 | OFF |
|137280 | RUNNING |50/0 |2 |0 |0 |0 | ON |
|139360 | STOPPED |0/0 |100 |3 |0 |0 | OFF |
|140140 | STOPPED |0/0 |100 |3 |0 |0 | OFF |

During RUNNING, the retained intermediate records show sequence15,28,41,54,67
and80, each reporting lease100 and DRIVE ON. At the stopped records lease0.
Sequence100 is consistent with the finite two-second producer renewing every
nominal20ms. These report timestamps do not measure exact motion duration,
expiry latency or WCET.

At first observed expiry: ENC L10397/R0, irq10405, over8/0,
bad0/0, saturation0, capture ready1/error0, ENC_UNCERTAIN1.
At t140140 counts L11698/R0 and irq11706; warnings remain8/0.
DRIVE OFF is the software output status, not an independent current measurement.
Counts can continue after coasting and are not calibrated distance evidence.

Reason3 with cutoff0/error0 supports command expiry handled by Motion before
an active timer IRQ cutoff was required. It is not a failure of the timer,
and does not physically exercise the100ms task-stall fallback.

## B: GP21 during motion and restart prevention

The final retained session has a lower device uptime, consistent with reset.
At t12480 through13780 the visible rows repeatedly show:

```text
ENC[L=4817 R=0 AB=2/3 bad=0/0 sat=0 age=0]
ENC_READY=1 err=0 irq=4820 over=3/0 ENC_UNCERTAIN=1
MOTION[state=STOPPED L=0 R=0 lease=0 seq=37 reason=2 cutoff=0 err=0]
DRIVE=OFF RADIO_READY=1
```

The user explicitly confirms GP21 was pressed while the left wheel was still
moving and stopped it. This supports the GP21 stop-under-motion path together
with the reason2 record. The user also confirms repressing GP20 does nothing;
the repeated STOPPED records support the intended restart-prevention latch.
This is a scoped functional observation, not a measured stop-distance or
response-time result. The coordinator mentions trying the automatic case
twice; the assistant independently inspected one retained expiry sequence,
and does not count a second raw capture that was not recovered.

## Result and limits

PASS within this setup: sustained local command renewal, automatic expiry,
GP21 stopping during left-wheel motion, and GP20 restart prevention until
reset. No repeat of these same cases is needed now. Both expiry and GP21
deliberately latch STOPPED; inability to request another run is expected.

Still pending: powered encoder accuracy (ambiguity warnings persist), measured
timing/STOP distance,100ms task-stall fallback on target, simultaneous two-wheel
load, MQTT-controlled movement and integrated colour tracking. The previous
Wi-Fi/MQTT communication and four-pulse passes remain valid within their scope.
Next development case is connecting Motion to MQTT with explicit fresh/session
commands and stop/restart rules; current v0.5 packets still cannot drive motors.
