# First v0.6 software-controlled motion and early STOP

7 November2026. Coordinator reports pressing GP20 caused the physical left
wheel to turn clockwise. Viewing direction was not specified. The retained
VS Code Serial Monitor tail was inspected; COM3 was closed at the end. This
file records selected visible fields, not an invented complete raw capture.

```text
ENC[L=1856 R=0 AB=3/3 bad=0/0 sat=0 age=10]
ENC_READY=1 err=0 irq=1857 over=1/0
MOTOR[phase=STOPPED next=1 M=1 D=1 pct=50 us=159946 cutoff=0 stop=2 err=0]
DRIVE=OFF RADIO_READY=1
```

The repeated tail is live reporting after a latched stop, not the earlier
processor freeze. `stop=2` is MOTOR_STOP_INPUT_FAULT. In the matching source,
one cumulative left ambiguous/coalesced capture warning makes readiness
false. The test stops at159946us, before its400ms cutoff (`cutoff=0`). The
invalid/saturation/readiness/driver fields shown do not indicate another fault.

`over` is the software capture-ambiguity counter, not a hardware FIFO overflow.
The IRQ handler flags pending-edge ambiguity and resynchronizes without
guessing a count. This result establishes a feedback-quality limitation;
it does not establish whether edge rate, interrupt latency or electrical
noise produced that ambiguity, nor the number of lost edges.

STOPPED intentionally prevents GP20 from restarting. Once latched, the
controller returns without replacing its stop reason; pressing GP21 then
cannot demonstrate that GP21 failed. No GP21-under-load result is established.

Supported: one observed software-driven left-wheel rotation, plus encoder
activity. Pending: a complete400ms request, both polarities/wheels, physical
forward convention, GP21 input/under-load response and trustworthy encoder
counts during powered motion. Next image v0.6.1 separates this quality warning
from fatal faults only for the bounded open-loop diagnostic.
