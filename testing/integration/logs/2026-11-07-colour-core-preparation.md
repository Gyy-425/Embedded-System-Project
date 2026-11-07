# Configurable C classifier preparation

7 November2026. The coordinator approved the revised scope and explicitly
authorized a configurable C colour-classifier core. Actual tapes remain absent.
This completes that portable core, not live recognition or path following.

## Implementation

New `subsystem1/colour_classifier.[ch]`: per-channel seven-class inclusive ADC
ranges, typed setter/init/validation, unknown and diagnostic reason result.
Bounds/validity/calibration/freshness are checked. No dynamic allocation,
floating point, global mutable state, RTOS/hardware/I/O calls or filtering.
Application C follows the scoped BARR-C style (Allman, fixed widths, prefixes,
short lines) and strict conversion/shadow/prototype diagnostics.

Missing calibration, invalid readings/model, future/expired samples, gaps and
ambiguous intervals do not produce a known class. Overlap is exposed in model
validation; an overlapping model can classify unique diagnostic regions, but
is not thereby approved for deployment. Init/set reject invalid changes without
modifying the destination. Configuration must be private during updates and
immutable during reads; synchronized runtime publication is later work.

No sample thresholds are installed in firmware. Synthetic ranges exist only
in host tests. Existing IR/Motion/MQTT behavior is unchanged; raw telemetry
still reports unknown until real model installation and reporting integration.

## RED, GREEN and final review

`2026-11-07-colour-core-red.log`: safe pre-implementation baseline compiled,
then failed the first initialization assertion. The implemented core passed
seven named assertion groups, including independent literal packed-byte and
result vectors, rails, bounds, missing/corrupt/overlap/touching ranges,
failure atomicity, independent channels/models, validity, future/expiry/wrap,
maximum age and immediate transitions. For both disjoint and overlapping
fixtures, every ADC code0..4095 on both channels is checked against the
equivalent byte-aligned reference:16,384 comparisons.

One fresh reviewer found no functional core defect and one Important runner
issue: default recoverable UBSan diagnostics can exit0. A real controlled
signed-overflow probe using the runner's shared host flags reproduced that
false pass (`2026-11-07-colour-core-sanitizer-red.log`). Adding
`-fno-sanitize-recover=all` made the probe fatal and the regression pass. This
gate now runs before every host suite. No re-review/deferred minor findings.
The review did not execute tests, access the robot or inspect private config.

## Executed verification

- Full label `2026-11-07-colour-core-verify`: six stages exit0, zero warnings —
  sanitized host compile/run, Cortex-M0+ core/layout compiles, normal and TEST
  existing REMOTE firmware builds with new EXE_FILE names.
- Final label `2026-11-07-colour-core-final`: five stages exit0, zero warnings —
  controlled sanitizer gate, fatal-sanitized host compile/run, target core and
  equivalent layout compile. The runner-only sanitizer fix did not change core
  or firmware behavior, so the passed compatibility builds were not repeated.
- Python helper/gate syntax checks and source/doc whitespace checks passed.
- [Full result](2026-11-07-colour-core-verify-result.json),
  [final result](2026-11-07-colour-core-final-result.json).

Both firmware candidates are byte-identical to the tested v0.8 images:
normal SHA256
`d80d95ad4b7c364198e293942cef1bb8a46bf8202fb138b580460092dae5ba0a`;
TEST
`0b002d4b11266aedf3ec4a1fd947e63e20ec38cd465e409b3c9d0999ca0b7728`.
The core object compiled with strict flags; unused classifier functions are
removed by section garbage collection because runtime integration is deferred.
Full run preserved26 older UF2s; final run preserved all28 including those two
new candidates. No tested image was overwritten. Candidate folders are ignored.
No flash, broker start, device command, motor test, network action, push or merge.

## Scoped equivalent-function storage comparison

Host measurements and Cortex-M0+ static layout checks:

| Object | Packed core | Equivalent byte-aligned reference | Difference |
|---|---:|---:|---:|
| Two-sensor calibration,14 bound pairs plus age/masks |46 bytes|60 bytes|14 bytes (23.3%)|
| Class/reason result |1 byte|2 bytes|1 byte (50%)|

Packed calibration has366 allocated field bits in46 bytes:336 bound bits,
16 age bits,14 presence bits. Two reserved presence bits must remain zero.
Result has6 logical bits and two zero reserved bits. No compiler bitfields or
packed-structure pragmas. The model's native age means it is not a wire format.
Input sample ABI size8 bytes is an API view, not claimed as packed storage.
Dependent class/reason states could use a4-bit categorical code, still one byte
per standalone return; no globally optimal batch/dependent-state claim is made.

This is an actual scoped representation comparison with equivalent legal
information and reference classification behavior. It is not total application
RAM savings: runtime has no model installed yet. No network savings, measured
WCET, recognition accuracy or complete Week13 qualification is claimed.

## Next

Use [the API/ownership guide](../../../docs/COLOUR_CLASSIFIER_CORE.md). Keep the
existing car image installed; no hardware retest is needed for this core.
Later obtain real tape/background captures, inspect per-sensor overlap,
populate and qualify the model, then add synchronized IR-owner installation,
recognised-colour reporting and target-path integration as separate cases.
