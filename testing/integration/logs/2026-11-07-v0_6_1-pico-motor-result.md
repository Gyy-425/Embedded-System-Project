# v0.6.1 actual Pico bounded-motor result

7 November2026. The coordinator reports the test is working well. The assistant
inspected retained COM3/115200 output in VS Code, including earlier records,
the DONE state and the subsequent STOP. No upload, button press, command,
power change or reconnection was performed by the assistant. The monitor
was already closed when inspected.

This is a transcription of selected visible fields, not a complete raw-log
export. Wrapped UI rows were joined only where their fields were visible.
The log contains the v0.6.1 `ENC_UNCERTAIN` field and the expected sequence;
the startup banner was outside the retained visible start.

## Supported results

Initial t56940: IDLE/next1, counts0/0, irq0, over0/0, ENC_UNCERTAIN0,
capture ready1/error0, motor error0 and DRIVE OFF.

| Observed record t (ms) | Completed request | Reported duration (us) | Cutoff count | State | Stop reason | Motor error |
|---|---|---:|---:|---|---:|---:|
| 59540 | Left/M1 D1 | 400020 | 1 | COOLDOWN next2 | 0 | 0 |
| 64480 | Left/M1 D2 | 400010 | 2 | IDLE next3 | 0 | 0 |
| 67340 | Right/M2 D1 | 400007 | 3 | IDLE next4 | 0 | 0 |
| 70980 | Right/M2 D2 | 400009 | 4 | DONE next0 | 0 | 0 |

All selected completion records show DRIVE OFF. These are firmware-reported
elapsed times, not independent oscilloscope measurements. Each request reached
its cutoff; count-quality warnings did not block the following request.

At t71760 the state remains DONE/cutoff4/stop0. At t72020 it becomes
STOPPED/cutoff4/stop1/error0/DRIVE OFF, and remains stopped through t74360.
Stop reason1 is the GP21 button path. This supports the unloaded GP21 input
and stop latch after DONE. There is no evidence here of interrupting an active
pulse or of a particular additional GP20 press after STOPPED.

Final feedback fields: L=-85, R=-75, AB3/2, bad0/0, saturation0, irq13521,
over3/2, ENC_UNCERTAIN1, capture ready1/error0. Left feedback changes during
M1 requests; right feedback changes during M2 requests. Opposite requests
produce opposite count trends; this is qualitative activity, not calibrated
odometry. The capture-ambiguity counters still preclude claiming accurate
edge totals. No capture-backend fix is established by this run.

## Scope and next work

The bounded software-output sequence and unloaded STOP input/latch pass within
this setup, supported by the coordinator's general movement confirmation.
Exact physical CW/CCW observations for all four requests were not supplied;
do not invent a physical-forward convention. Earlier M1/D1 CW observation had
no specified viewpoint.

Do not repeat this same basic four-pulse test. Continue with the normal Motion
interface and controlled running/STOP/expiry integration. Before encoder-based
speed or distance control, address powered capture quality and calibrate its
sign/scale. STOP under load, sustained movement, tracking and combined Wi-Fi
load remain relevant later validation; the existing communication passes stand.
