# Route distance measurement core

User requests software first for one route with successive sections of unknown
length, without the track currently available. Example lengths were withdrawn.
Implement inline with TDD; request one fresh review at the end.

## Scope/design

Deliver a portable single-owner C module, host behavior tests and calibration
utility. It accepts timestamped coherent encoder counts and classified colour
observations; it has no motor, kernel or MQTT calls. Physical integration with
the powered route mode follows ground calibration. Preserve tested UF2s and
all current follow logic. Blue ID5 is only a reserved unit-test input here;
the RGB classifier has no calibrated blue profile yet.

Per-wheel rational um/count conversion, sign normalization and remainder
carry; average signed wheel distances, zero translation for a balanced pivot.
This first route mode requires forward progress; reversing/reset/invalid
capture, excessive count/time jump and overflow fault rather than invent
distance. Calibration defaults are absent, never synthetic on hardware.

Keep64 odometry points for bounded timestamp interpolation. Colour inputs
provide an acquisition-time interval, not serial print time or an assumed
exact exposure midpoint. Match its interval to history; use last old window
start and first new window end to bound a boundary. Two distinct fresh new
colour samples confirm; backdate to that first candidate, not confirmation.
First/last sections are partial unless caller explicitly marks start/finish
at a physical boundary. Records expose boundary bounds and flags; totals are
estimates. Brief UNKNOWN gaps are buffered and flagged; sustained gaps fault
and leave unresolved travel unassigned. A single unconfirmed colour cannot
silently become a confidently measured section.

Use bounded16-record FIFO, no allocation, immutable configuration copy and
checked uint32 um results. Use int64 only for checked arithmetic. Queue full,
history expiry and sensor/order faults are explicit; caller drains records.
Single owner only: future runtime must copy/publish records outside its motor
critical section, not concurrently access this context.

## Steps

- [x] Write behavior tests and executable replay; observe failure with stub.
- [x] Implement checked odometry/history/segmentation, run strict C11 and
  ASan/UBSan tests including reset, stale, ambiguity, wrap, pivots and overflow.
- [x] Add calibration CLI using measured distance and signed counts, no guessed
  motor CPR; validate input. ARM compile and link actual new module in a test
  executable without modifying current powered firmware/profile.
- [x] Fresh review, fix evidenced defects, document commands and integration
  contract. Clearly separate simulated results from physical measurements.

Hardware limits: colour acquisition about100ms in prior logs with50.4ms
integration means5cm sections need speed/visibility validation. No guarantee
of short-section detection or accuracy. A third colour's calibration and
full-route motor mode are separate outstanding work. No commit/push/flash.


Final ledger: Example user dimensions withdrawn. Public length demo removed;
only private synthetic behavior tests retain numerical vectors. No real route
length/calibration is set. Release passes5 stages, no warnings,141 prior UF2s
unchanged. Fresh review clean after pivot and missing-sequence regressions were
reproduced and fixed. Core, ARM library, host tests and calibration utility
ready; actual RGB-window/motor-route integration and physical checks pending.
