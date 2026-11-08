# Forward/steering motion interface: software verification

Implemented after the user authorized the focused motion-control coding task.
Work remains on aai05/bring-up, starting at c3312fe. C adapter commit3f0129a;
CLI/document changes are saved in subsequent local commits. No merge, push,
upload, broker startup, network publication or robot movement was performed.

## Implemented

- Pure C motion_chassis_convert and task-context
  subsystem2_motion_submit_chassis. Forward0..50, turn-50..50, positive right;
  turn is limited to forward, common integer scaling at peak50, then verified
  left-negative/right-positive raw polarity. Zero is neutral, not latched STOP.
- Conversion preserves timestamp and sequence. Submission uses the existing
  Motion readiness/owner lock/sequence/TTL/STOP/fault/explicit rearm path.
  No new hardware writer, controller task, PID, ramp or calibrated speed.
- Python chassis_duties and drive_cli `move --forward --turn --seconds`;
  validation before Bridge creation, existing raw drive/MQTT v2 unchanged,
  bounded START/renew/STOP/fresh off confirmation reused. The tested installed
  v0.9 firmware remains suitable for this CLI conversion.

## Actual checks

Baseline `2026-11-08-chassis-baseline-result.json`: PASS,57 stages,38 old
UF2s unchanged. C no-op stubs fail the new mixer and actual LOCAL/REMOTE runtime
assertions as intended; RED logs retained. GREEN and final task1 logs show
strict C11 warnings-as-errors, fatal ASan/UBSan and freestanding ARM compile.
C checks cover literal mixing/rounding, all65536 byte-coordinate combinations
(reject invalid60385; accept5151), valid-pair direction/bounds/symmetry,
null/output preservation, metadata, actual runtime submission/owner behavior,
pending rearm, raw/logical duplicate/conflict/shared sequence wrap, future/stale
timestamps, GP21/fault latching, zero neutral, timestamp-wrap expiry, and
unavailable behavior in a non-Motion build. Hardware is substituted only at
the host driver/RTOS boundary.

CLI RED log retained; Windows initial seven CLI methods then pass. Final
eight methods include real script entrypoint rejection of forward0 and
in-process invalid-input rejection before Bridge creation, unchanged raw drive,
literal conversions/types/ranges, all5151 C/Python pairs, and actual run_drive
serialization/renewal/STOP/fresh-off confirmation over a controlled transport.
These tests do not start Mosquitto or contact a broker/robot.

`2026-11-08-chassis-ready-result.json`: PASS,65 stages,zero warnings,38 old UF2s
unchanged. This includes the complete C/PIO/legacy/colour/communication suite,
64 Python +6 Windows Tcl tests at that point, and normal/TEST builds with
PIO enabled and GPIO compatibility. After adding the final subprocess
entrypoint test, the final established split is65 Python +6 Tcl =71 methods,
all PASS with no skips (`2026-11-08-chassis-final-{python,tcl}.log`). No
production code changed between the full firmware run and that final Python
verification. A real Windows CLI help invocation also succeeds.

An additional Windows-wide discovery attempt failed with47 temporary-folder
access errors, plus2 absent-vector skips. The affected test groups were the
colour sanitizer gate, network configuration and tape calibration; the raw
log is `2026-11-08-chassis-windows-python-final.log`. It attempted the sandbox
temporary path and failed creating/accessing fixture files. No product fix or
claim of a successful Windows-wide run follows. The same complete tests pass
in the established WSL/Windows-Tcl split above with all C vector fixtures.

## Artifacts and limits

Compile-only candidates/ELF/maps:
`output/firmware/encoder_pio_2026-11-08-chassis-ready/`.
These reuse the runner's v0.9/compatibility names; they are not a new hardware
release or a reason to upload. Original delivered v0.9 normal SHA256 remains
0a5e865299d594d4f4a338d387df4c5754e7047d52a9c7191b26acb8eb17b6eb.
Normal PIO ARM text341452/data100/bss82616, TEST342144/100/82616. These build
sizes are compatibility evidence, not minimum-bit representation savings.

Independent review pending. Ground starting, straightness/steering, minimum
useful loaded duty, combined-load timing/rail behaviour and real IR/colour
following remain unverified. The guide is docs/CHASSIS_MOTION.md. Encoder
scaling is required before calibrated feedback if used; it need not block the
duty-based ground run or colour calibration.
