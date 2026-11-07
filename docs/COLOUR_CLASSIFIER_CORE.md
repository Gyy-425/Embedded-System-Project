# Configurable C colour-classifier core

This implements the next case authorized after scope review. The module is
`app_program/subsystem1/colour_classifier.[ch]`: portable C with no RTOS, I/O,
heap, floating point, global mutable state or temporal filtering. Existing
firmware compiles it, but IR/Motion/MQTT do not call it yet. The current car
image and `detected=unknown` telemetry remain intentional until integration.

No real tape thresholds are supplied. The host test ranges are explicitly
synthetic and never installed as robot calibration.

## Configure, validate, classify

1. Create a private `colour_calibration_t`. Call
   `colour_calibration_init(&model, max_age_ms)` with a nonzero1..65535ms age
   limit. Initialization clears all class-presence bits: nothing is calibrated.
   The age setting is an application choice, not a measured deadline.
2. For each sensor0 left/1 right and class0..6, call
   `colour_calibration_set_range(&model, sensor, class_id, lower, upper)` using
   actual measured/calculated bounds. Both endpoints are inclusive0..4095.
   Invalid arguments, reversed bounds, bad config or ADC overflow reject
   without modifying the destination. One range is not a complete model.
3. Call `colour_calibration_validate(&model, sensor, &overlap_mask)` for each
   sensor. OK means structurally complete/disjoint; MISSING means fewer than
   seven configured classes. BAD_ARGUMENT/BAD_CONFIG/BAD_RANGE reject invalid
   state. OVERLAP reports all classes whose inclusive intervals intersect;
   touching endpoints are ambiguous. The output mask may be NULL and must not
   alias the model. Validation does not prove real recognition accuracy.
4. Pass a `colour_sample_t` with timestamp, raw ADC and validity to
   `colour_classify(&model, sensor, &sample, now_ms)`. Decode the return using
   `colour_result_class()` and `colour_result_reason()`.

Class IDs remain0 black,1 white,2..6 colour1..5;7 unknown. Sensors have separate
calibration and can legitimately see different surfaces during tracking.
All seven classes must be configured for the selected sensor before a known
class can be returned. Zero and4095 are valid raw values if calibrated there.

| Reason | ID | Returned class / meaning |
|---|---:|---|
| MATCH |0|Exactly one interval matches; class0..6|
| UNCALIBRATED |1|7; selected sensor lacks a complete model|
| NO_MATCH |2|7; sample lies outside all configured intervals|
| AMBIGUOUS |3|7; more than one interval matches|
| INVALID_SAMPLE |4|7; invalid flag or raw value above4095|
| STALE_SAMPLE |5|7; unsigned age is at least max_age_ms, including future timestamp|
| INVALID_MODEL |6|7; zero age setting, reserved presence bit or reversed decoded bounds|
| BAD_ARGUMENT |7|7; NULL model/sample or invalid sensor index|

Precedence: bad arguments, invalid sample, model validity/completeness, sample
age, then interval matching. An all-zero model never silently selects black;
it is invalid until initialized and remains uncalibrated until populated.
Time uses uint32 unsigned subtraction across wrap; ages must represent the
short configured lifetime, not arbitrary multi-wrap replay protection.

A structurally valid overlap model can return a unique class outside its
overlap; it returns AMBIGUOUS within it. This supports diagnostics without
guessing a first/nearest class. Deployment must explicitly inspect OVERLAP and
qualify/revise the model; the core does not certify such calibration for path
following. No history/filtering means immediate consecutive transitions are
not intentionally suppressed. Actual moving accuracy remains untested.

## Ownership and integration boundary

Setters/init are not concurrent publication APIs. Build/validate private copies,
then later publish an immutable complete snapshot using the IR owner's lock or
equivalent ownership transfer. Readers must not observe in-place updates of
three-byte bounds. The module is reentrant with immutable independent models;
it does not provide a thread-safe live configuration mechanism.

Classification does not drive motors, change a Wi-Fi target or emit MQTT events.
Actual model import/storage, synchronized runtime installation, reporting and
tracking are later integration work. It can be tested before tapes arrive;
the actual capture tool will provide measurements for model selection later.

## Representation and measured comparison

Each endpoint needs12 bits. Fourteen lower/upper pairs use42 explicit bytes:
byte0 lower11..4; byte1 lower3..0 in its high nibble and upper11..8 in its low
nibble; byte2 upper7..0. A16-bit age and two7-bit presence masks bring the
field-wise allocation to366 bits, fitting46 whole bytes. Presence bit7 in both bytes is
reserved0. No compiler bitfields or packed-struct pragmas are used.

The result uses class bits2..0 and reason bits5..3 in one byte; bits7..6 zero.
Non-match reasons always carry unknown7. Accessors consume core-produced
results; this is not an untrusted network decoder. The model's native age field
also means the model struct is not a wire/file format.
Class/reason dependencies also permit a4-bit categorical result code; that
still occupies one addressable byte for a standalone return. This explicit
field layout does not claim globally minimal dependent-state/batch encoding.

The equivalent test reference stores the same14 bound pairs as uint16 fields,
plus identical age/masks. Its model is60 bytes versus46 packed:14 bytes saved
(23.3%) per model. Its separate uint8 class/reason result is2 bytes versus1:
one byte saved per result. Host and Cortex-M0+ layout assertions verify these
sizes; all4096 ADC values on both channels are compared against the reference
for disjoint and overlapping fixtures (16,384 comparisons).

These are scoped equivalent-function storage results. They do not establish
total firmware RAM savings, network-message savings, actual tape accuracy,
runtime WCET or complete Week13 evidence. The native input-sample object is
an8-byte API view on the tested ABI and is not claimed as packed persistent
storage. Model validation has at most7 ranges/21 pair checks; measure runtime
cost with Wi-Fi active when integrating.

## Reproduce without the robot

From the firmware directory in Ubuntu/WSL:

```sh
python3 tools/check_colour_core.py --label NEW_UNIQUE_LABEL
```

For full compatibility builds, add:

```sh
python3 tools/check_colour_core.py --label NEW_UNIQUE_LABEL --firmware --sdk '/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0'
```

The tool checks that a controlled sanitizer failure is fatal, then runs strict
sanitized host tests and Cortex-M0+ source/layout compilation,
and optionally both existing REMOTE build variants using distinct EXE_FILE
names. Compile-only candidates go to a new output subdirectory; old UF2 hashes
are checked unchanged. It does not flash, start a broker or contact the robot.
Keep the previously tested car image installed; this core needs no car retest.
