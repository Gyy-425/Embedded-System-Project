# Forward and steering interface

## Intent and scope

The user authorized the focused motion-control coding task after basic powered
PIO capture passed. Prepare one logical forward/steering interface for the
future IR controller and an equivalent CLI convenience command for the next
ground run. Work in the existing aai05/bring-up checkout per user preference.
Keep the tested installed v0.9 image, raw drive semantics, MQTT v2 protocol,
STOP/rearm policy, 100ms Motion expiry and independent driver lease unchanged.
No hardware actuation, PID, encoder scaling, automatic course following or
unmeasured motor trim/ramp is included.

## Approaches and decisions

1. Portable C mixer plus existing task-context submit path, with equivalent
   Python CLI conversion into existing raw MQTT commands: chosen engineering
   approach. Small, offline-testable, usable by the later on-robot controller
   and immediately usable with the installed image.
2. New MQTT opcodes/coordinate flag: unnecessary protocol/server change for
   this scope; would expand replay/lease compatibility work.
3. Change existing raw duty meanings: would silently break proven diagnostic
   commands. Retain those and make the logical interface explicit.

These detailed defaults are implementation decisions, not professor-issued
requirements or claimed team agreements. The user requested direct execution
of the focused task; no additional approval handoff is needed.

## Contract

- C11/BARR-C style, integer-only pure conversion, no heap/shared mutable state.
- `motion_chassis_command_t`: issued_ms:uint32_t, sequence:uint16_t,
  forward_pct:uint8_t (0..50), turn_pct:int8_t (-50..50).
- Values are electrical-duty requests, not calibrated speed/turn angle.
- Positive turn means right: increase left logical wheel relative to right.
- Clamp turn magnitude to forward duty so this interface never reverses a
  wheel. At forward0 both outputs are0; it does not pivot in place.
- Form logical L=forward+turn, R=forward-turn. If peak>50, scale both by
  50/peak with integer truncation toward zero. Apply verified physical
  polarities only at the boundary: raw L=-logicalL, raw R=logicalR.
  Examples: (40,0)->(-40,40); (40,10)->(-50,30);
  (40,-10)->(-30,50); (40,20)->(-50,16); (10,50)->(-20,0).
- `motion_chassis_convert(request, raw)` returns MOTION_ACCEPTED or
  MOTION_BAD_COMMAND. Reject null/invalid inputs without changing the caller's
  output. Preserve issued_ms/sequence exactly, including wrapping bit patterns.
- `subsystem2_motion_submit_chassis(request)` converts then calls existing
  `subsystem2_motion_submit`. Same logical producer and shared sequence domain
  as raw submits; not a second controller or new lock/hardware writer.
  Existing unavailable, pending arm, duplicate/conflict, stale, STOP, fault,
  expiry and owner-only output rules remain authoritative.
- Zero conversion is a neutral request, not latched STOP; intentional STOP
  still uses subsystem2_motion_stop / CLI stop. CLI move requires forward>0.
- `drive_cli.py ... move --forward 40 --turn 10 --seconds 2` converts before
  opening the bridge, prints requested logical and mapped raw duties, then
  uses the existing bounded run_drive START/renew/STOP/off-confirmation flow.
  Default forward30, turn0, seconds2. Reject invalid inputs before networking.
- Firmware builds are compile-compatibility evidence only; do not replace
  delivered v0.9 UF2s or ask for a redundant upload. ADC/calibration pending.

## Evidence

Hand-derived vectors and exhaustive valid C/Python parity/invariants; invalid
and null boundaries; timestamp/sequence wrap; real Motion runtime integration
for STOP/rearm/fault/unavailable/expiry with host driver doubles only at the
hardware boundary; invalid CLI inputs rejected before bridge creation and
logical requests serialized through the real run_drive path. Run existing
host regression suite with fatal ASan/UBSan and normal/TEST firmware builds;
preserve all previously delivered UF2 hashes. Physical steering/ground-load
behaviour remains pending until the user's next practical run.
