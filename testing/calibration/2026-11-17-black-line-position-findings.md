# Actual black-strip position check — 17 November 2026

User confirms ROBOT moved left/right relative to a fixed35mm strip. No firmware or wiring edited.

| Robot position | Printed valid IR count | Left range (mean) | Right range (mean) | Correction |
|---|---:|---|---|---|
| centred | 39 | 250–287 (268.15) | 289–323 (304.36) | Neutral |
| robot-left | 18 | 213–249 (235.50) | 1428–1456 (1442.44) | Right |
| robot-right | 40 | 1341–1425 (1366.47) | 217–334 (239.47) | Left |

All97 printed IR snapshots valid/err0, age<=20ms. All124 RGB reports healthy and BLACK/MATCH, modelBWY_20261117. This supports the scoped live black-recognition check and a preliminary steering signal; it is not powered following acceptance.

Left/right IR gain/offset differ. Calibrate each independently; do not use equal raw values as the centred condition. Off-centre changes exceed1000counts and separate clearly from measured variation. Keep current geometry for the first controller prototype.

Both IR readings near the light response can mean a centred straddled strip OR bare background. A simple both-low rule cannot establish line presence. The central fresh BLACK RGB observation can provide a guard; invalid/stale/UNKNOWN data must not silently authorize continued motion. Physical end/loss tests remain required.

The robot-left capture contains27 extra DIAG reports with IR=0 motion=-50. In include/tk/errno.h, -50 is E_TMOUT/polling failure; subsystem2_get_snapshot uses tk_loc_mtx(TMO_POL). The aggregate reporter suppresses its normal IR line whenever either snapshot read fails. These27 reports are not valid Motion snapshots and not automatically evidence of sensor failure or motor stall. Underlying repeated acquisition contention/scheduling needs investigation in integration. RGB reporting continues independently.

Next focused implementation: bounded plain-black-stripe controller (right sensor more black -> right correction, left sensor more black -> left), single Motion writer and STOP/lease protections, RGB freshness/presence guard, explicit snapshot failure handling. Verify output sign with raised wheels, then a short low-speed run. Current v0.12.1 stays read-only.

Existing prior cardboard data are retained as a reference; no new large sample collection is required before beginning controller work.
