# Motor/encoder qualification readiness, 8 November 2026

The coordinator requested motor/encoder qualification after the C classifier
case. The current firmware already supports paired signed duties and serial
encoder diagnostics. No motor, encoder, IRQ or protocol source change is needed
for the first focused paired-motion check.

Read-only host network inspection outside the command sandbox found the laptop
connected to SINGTEL-A9C0_MLO at 192.168.1.206. The private header's public broker
definition was 192.168.1.199. Updated only that address to 206, preserving all
other private bytes. The credential header remains Git-ignored; no password is
printed or committed. No OS/router setting was changed.

Reused the existing checked build tool with a new evidence label:

```text
python3 tools/check_colour_core.py --label 2026-11-08-motor-qual-broker206 --firmware --sdk <existing SDK2.3.0>
```

Seven stages passed with zero warnings: fatal-sanitizer gate, strict sanitized
C core compile/run, Cortex-M0+ source/layout checks and existing normal/TEST
REMOTE firmware builds. These are software build checks, not new motor hardware
passes. Build result and command details:
`2026-11-08-motor-qual-broker206-result.json`.
All 28 pre-existing UF2s matched their initial hashes. The distinct candidate
build was copied to explicit qualification filenames without overwriting any
old artifact. The ELF broker literal 206 is present; old 199 is absent.

| Variant | File | SHA256 |
|---|---|---|
| Normal, next user upload | `output/firmware/AAI05_Motor_Qualification_v0_8_BROKER206.uf2` | `d34dd070d9ed2723eacefb24f45b4039f6363130b6a093e04c5971b5b3208a29` |
| TEST, retained compile variant | `output/firmware/AAI05_Motor_Qualification_v0_8_BROKER206_TEST.uf2` | `cf84fce3e261bd612e157216a83e76660819d422e63a0f8de68d30cbf0e5241b` |

[Readiness manifest](2026-11-08-motor-qualification-readiness.json).
The explicit normal file is the working v0.8 REMOTE behavior configured for
today's broker. Classifier code remains unused/garbage-collected as before;
this does not activate synthetic calibration. Images/ELF/maps remain ignored.

## First new physical case, still pending

Use [the focused guide](../../../docs/MOTOR_ENCODER_QUALIFICATION.md). Supported
raised wheels, existing bank VIN/Pico USB arrangement, serial baseline, then
one paired +50/+50 two-second request. Record each physical wheel's direction
relative to the front, before/during/after signed counts and bad/over/saturation,
and both-wheel stopping. Positive electrical signs are not assumed physical
forward. Counters are cumulative; compare deltas. `over` marks ambiguous batches,
not the exact number of missed edges. Sticky uncertainty is not by itself a
new-run failure. Zero new warnings is not proof of counts-per-revolution.

The assistant did not start a broker, upload or actuate the car. Actual paired
motion, encoder-quality/accuracy, forward signs, loaded operation and power
stability remain unverified. Next choose a reference-turn measurement or capture
fix based on the single recorded result; do not repeat passed basic tests.
