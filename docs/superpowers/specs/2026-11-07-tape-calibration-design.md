# Stationary tape calibration capture

The user authorized the next focused coding case after MQTT actuation-loss
verification. Preserve passed images/tests and existing C firmware. User prefers
autonomous inline implementation and one case at a time; no new design gate.

Build a Python3.10+ standard-library laptop tool using existing v2 Bridge,
codec and STATUS synchronization. It collects labelled left/right raw12-bit
IR readings from the current firmware while stopped, saves durable CSV plus
metadata and reports per-sensor range overlap. This enables the first real
seven-tape experiment; no classifier/thresholds, synthetic physical results or
path controller is delivered in this case. Project firmware remains C.

Choices: manual copying is error-prone and loses provenance; changing firmware
before obtaining data adds an unnecessary flash/profile. Reuse validated MQTT
telemetry. This is the implementer's choice, not a new professor requirement.

`calibrate_cli.py capture` requires class black/white/colour1..5 (IDs0..6),
human tape name and setup description; optional measured height is recorded,
never assumed. Both sensors must cover the same actual tape patch at installed
height. No concurrent drive process. Only STATUS may be published; no motor
command, STOP, ARM or renewal. Initial and every fresh frame must be ready,
non-active, remote IDLE/STOPPED, Motion READY/STOPPED and zero duty/lease.
Session changes, movement/fault or2s with no fresh telemetry abort.
Compare host/device elapsed time from initial STATUS and abort divergence over1s;
recheck deadline/gap after receive before accepting. This is a relative backlog
guard, not measured absolute sample age. File cleanup can finish after the
sample-acceptance deadline.

Defaults:40 valid fresh samples,30s total deadline,500ms settling; sample count
10..1000, timeout1..300s. These are capture-tool defaults, not project NFRs.
Exclude invalid/stale IR, duplicate/backward telemetry seq/time and samples
before settling. Unsigned16/32-bit wrap is supported. Zero ADC is valid.

Each exclusive new output directory contains `samples.csv` flushed per sample
and `metadata.json`, initially IN_PROGRESS, finally COMPLETE/TIMED_OUT/ABORTED/
INTERRUPTED. Preserve partial captures on failure. Existing paths are never
overwritten. Metadata records schema1, label/name/setup/optional height/device,
session/requested count/state/counters and device timestamps. Capture times are
telemetry times, not every ADC conversion. Process termination can leave
IN_PROGRESS with partial CSV; analysis must not treat it as complete.

Offline report reads immediate capture subdirectories only. Validate complete
files/row counts/ranges/order; exclude and list incomplete captures. Bound to
256 captures and1000 rows each. Require consistent setup/height and one name per
class. Summarize count,min,max,median and exact ADC-rail hits for each sensor.
List overlapping class intervals independently for left/right. Missing any of
seven classes -> INCOMPLETE; overlap -> OVERLAP; otherwise CANDIDATE_RANGES.
No classification accuracy, guaranteed separability or deployed model is claimed.
Reports also use exclusive file creation. Invalid files fail clearly.

Tests use deterministic packets/clocks/temp files, never network/robot access.
Cover persistence/failure, exclusive paths, unsigned wrap/replay, validity and
movement guards, report consistency/overlap/missing classes/malformed input.
One final fresh code review, material fixes RED->GREEN, affected Python checks;
no firmware rebuild because no firmware source/build changes.
