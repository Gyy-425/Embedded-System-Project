# Distance measurement: software ready, ground integration pending

The route has successive colour sections of as-yet-unmeasured lengths. This
package builds/tests the measurement functions before the physical route is
available. Expected section lengths are not configuration inputs or stopping
targets. Real encoder calibration remains unset. The existing powered firmware
still uses its tested one-second/single-target following policy.

## Run the actual C replay

From PowerShell in `C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware`:

```powershell
$distanceCheckLabel = "distance-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
wsl.exe -d Ubuntu -- python3 '/mnt/c/Users/Guo YouYan/Desktop/INF2004-main/AAI05_Firmware/tools/check_route_measurement.py' --label $distanceCheckLabel
```

Ubuntu uses the project's installed GCC/ARM toolchain. The command builds the
actual C source, runs strict ASan/UBSan behavior tests, checks the
calibration CLI and compiles an RP2040 ARM static library. It does not use the
robot, serial port, Wi-Fi, broker or network secrets. Choose a new label.

The runner reports test PASS/FAIL, not lengths for the user's track. Numerical
synthetic vectors remain inside unit tests to verify conversion and segmentation;
none are loaded into a car or offered as measured calibration. Boundary ranges
are timing/position uncertainty estimates, not statistical confidence intervals,
and exclude calibration/slip/classification error.

Software deliverables under `output/software/<label>`: C source/header, Linux
test executable and `libroute_measurement.a` compiled for Cortex-M0+.
Evidence is retained in `testing/integration/logs/<label>-*`. These are not UF2
files and should not be copied to RPI-RP2.

## How distance is calculated

Each wheel has its own measured reference distance/count ratio and a physically
confirmed forward sign. Convert small count changes with remainder carry so
integer rounding does not discard sub-micrometre increments at every update.
Average signed wheel travel, then accumulate forward axle-midpoint travel.
Balanced opposite wheel travel produces zero translation for a pivot. This
first route implementation faults on reversing instead of counting retraced
tape as a new forward section. Wheel slip and sensor/axle geometric offsets
remain physical limitations, especially at corners.

No real calibration is supplied or silently inferred. Zero reference distance
or counts, an invalid sign or range returns CONFIG. Internal results use checked
uint32 micrometres (maximum about4.295km), with checked int64 intermediate math.
One-micrometre representation resolution does not imply that physical accuracy.
Existing minimum-bit wire protocols are unchanged; representation-comparison
evidence is a separate project task.

## Colour boundaries and short sections

Two distinct ordered fresh acquisitions confirm a new colour. Repeated reads
of one sequence do not count twice. On confirmation, close the previous section
at the candidate boundary estimate, not at the later confirmation position.
The boundary interval spans the last confirmed old observation's earliest
position and the first new candidate's latest position. Keep the interval on
each section. A brief UNKNOWN/mixed boundary is buffered and marked uncertain;
sustained unknown observations, stale data or insufficient history end the
measurement with an explicit fault and unresolved distance.

An unconfirmed colour blip marks uncertainty instead of inventing a confirmed
section. Start/end flags distinguish interrupted/partially observed sections
from sections with both boundaries. Totals are estimated sums of closed
sections. While running, `unassigned_um` also includes the currently open,
uncommitted section; after finish/fault it describes unresolved travel.
Missing acquisition sequence entries mark the affected section uncertain,
including gaps while establishing the first colour. A balanced pivot preserves
fractional distance history and contributes exactly zero translation in the model.

BLUE uses reserved input ID5 in unit tests only. The current TCS34725
classifier has calibrated RED/YELLOW/WHITE/BLACK/BACKGROUND, not BLUE. Blue
needs real samples and validation before runtime may publish BLUE observations.

Two-sample confirmation cannot recover a section the sensor never resolved.
At an illustrative100mm/s, a50mm section lasts500ms; field of view, roughly100ms
prior RGB cadence and50.4ms integration reduce clean observations. Actual
speed/visibility must be measured. If samples are insufficient, expose the
uncertainty/unassigned distance rather than claim the nominal50mm was measured.

## Calibration utility for later measured inputs

Record real straight ground travel with the same chassis reference at start
and finish, along with each signed encoder delta; include coasting to the final
resting position. Use the actual distance, not motor duty multiplied by time.
No guessed wheel diameter, CPR or gearing is needed. Recheck the conversion
on an independent measured distance using the same surface and load.

The command shape is:

```powershell
python .\tools\calibrate_distance.py --distance-mm <actual-distance> --left-counts <signed-left-delta> --right-counts <signed-right-delta> --output .\config\distance_calibration.json
```

Replace the angle-bracket placeholders with measurements before running. The
utility prints/writes rational calibration and signs. It rejects invalid or
zero inputs and refuses to overwrite an existing output. Generated values are
user inputs, not automatically verified calibration. They are not loaded into
the car merely by creating the JSON file.

## Runtime integration contract

`route_measurement.h` defines a statically allocated, single-owner context.
No function touches motors or makes a kernel/network call. Future integration:

1. Call `route_measurement_init` with validated calibration, accepted interior
   colour mask and a coherent encoder origin. Use an epoch that changes on
   encoder resets/restarts. Confirm initial colour while stationary before
   starting if a complete first section is required.
2. Feed `route_measurement_motion` with coherent signed counts and acquisition
   timestamps each Motion cycle. Caller must combine encoder capture errors,
   PIO invalid flags and saturation into the validity field. Set realistic
   per-update count/time limits; epoch changes and jumps fault explicitly.
3. Feed each new classified colour with its actual bounded acquisition window.
   Current `rgb_snapshot_t.sampled_ms` is the freeze/read timestamp, not a
   precise optical boundary. A runtime adapter must expose/record conversion
   timing and align its window to encoder history; do not substitute USB or
   MQTT arrival time. This adapter is not implemented by this core package.
4. The core stores64 timestamped positions. Missing/expired history faults;
   it never extrapolates a colour position. Drain the16-record FIFO under the
   same owner and publish private record copies through a kernel object to
   Report/MQTT. Report must not concurrently read or modify this context.
5. On a marked finish or stop after final motion samples, call
   `route_measurement_finish`. Unknown start/finish positions remain partial.
   Preserve queued records and status after faults; reinitialization starts
   a new run. A full queue faults instead of silently losing measurements.

The later powered route mode must accept calibrated successive interior colours
without the current single-target mismatch STOP, while retaining fresh IR,
bounded boundary handling, GP21, watchdog/expiry and one motor writer. It needs
a mission bound beyond the current one-second exercise, plus a start/finish
rule. No such motor-policy changes are included in this standalone package.
