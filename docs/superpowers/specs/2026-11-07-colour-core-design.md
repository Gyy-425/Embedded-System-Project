# Configurable C colour-classifier core

Coordinator approved PROJECT_SCOPE_REVIEW.md and explicitly authorized this
case. Existing checkout aai05/bring-up; user wants autonomous inline execution,
one case, BARR-C conventions and no redundant hardware tests. No tapes available.

Create portable C11 `app_program/subsystem1/colour_classifier.[ch]`. No hardware,
RTOS, allocation, floating point, I/O, global state or temporal filtering.
It is compiled with the firmware but not activated in IR/Motion/MQTT yet.
Keep raw telemetry/STOP/network behavior and tested images intact. No fake
physical calibration, threshold defaults, deployed model or accuracy claim.

Two channels independently hold all seven inclusive ADC ranges, IDs0 black,
1 white,2..6 colour1..5;7 unknown. Raw and bounds0..4095, resolution1 count.
Configuration:42 explicit bytes for14 pairs of12-bit bounds (three bytes per
pair), uint16 max_age_ms1..65535, two uint8 present masks bits0..6; bit7 reserved0.
Expected size46 bytes on supported host/Pico ABI, versus60 for an equivalent
uint16-range model. Model is not a wire format; native max_age_ms is not sent.

`colour_calibration_init(model, max_age_ms)` clears all masks/ranges on valid
arguments; rejects invalid arguments without changing the destination.
`colour_calibration_set_range(model, sensor, id, lower, upper)` validates all
arguments before updating one range/presence bit; reject reversed/out-of-range
bounds without modifying any state. Does not invent missing classes.
`colour_calibration_validate(model, sensor, overlap_mask)` returns OK, MISSING,
BAD_ARGUMENT, BAD_RANGE, BAD_CONFIG or OVERLAP. Check time/reserved masks,
require all seven classes for that sensor, and validate decoded lower<=upper.
Optional overlap output is cleared first and identifies all intersecting class
bits. Inclusive touching endpoints count as overlap. No priority/nearest guess.

`colour_classify(model,sensor,sample,now_ms)` returns a packed uint8 result:
class bits2..0; reason bits5..3; reserved bits7..6 zero. Reasons0 MATCH,
1 UNCALIBRATED,2 NO_MATCH,3 AMBIGUOUS,4 INVALID_SAMPLE,5 STALE_SAMPLE,
6 INVALID_MODEL,7 BAD_ARGUMENT. All non-MATCH results use class7. Explicit
accessors return class/reason; this internal result is not added to MQTT yet.
Input sample is uint32 sampled_ms,uint16 raw,bool is_valid. Null/invalid sensor
-> BAD_ARGUMENT; invalid flag/raw -> INVALID_SAMPLE; bad/missing model next;
then unsigned sample age >= configured max_age_ms -> STALE_SAMPLE (including
future timestamps, modulo32). With valid structure, OVERLAP models can classify
a uniquely matched region; matching multiple ranges returns AMBIGUOUS. Gaps
return NO_MATCH. Applications must inspect the model overlap status before
using it for deployment; this core never claims overlap models are qualified.

Model validation is bounded to7 ranges and21 pair comparisons per call. No WCET
claim before measurement. Caller owns models; setter/init operate on private
copies. Models must be immutable during classify/validate. Future runtime model
publication needs an IR-owner lock/snapshot; no thread-safe update claim here.
Left/right need not have identical ranges or agree on the observed surface.

Alternatives: nearest-centre guesses hide out-of-set readings; reject every
overlap globally loses useful unambiguous diagnostic regions. Selected range
matching exposes explicit ambiguity and validates deployment overlap separately.
No smoothing/hysteresis until actual moving tape data informs that decision.

Verify strict sanitized host C tests, independent byte/edge vectors, full4096
ADC-code comparison on each channel against a byte-aligned reference (disjoint
and overlapping fixtures), failure atomicity, missing/corrupt model, rails,
expiry/future/wrap, no temporal state. Measure sizeof both equivalent models/
results, not total application savings. Compile Cortex-M0+ and both normal/TEST
firmware variants into separately named local candidates; preserve old UF2s.
One fresh review, material fixes RED->GREEN, no robot/network actions or push.
