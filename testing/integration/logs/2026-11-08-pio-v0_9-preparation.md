# PIO encoder capture v0.9: software prepared, car acceptance pending

The user authorized the next development case after the preserved forward
GPIO run showed over0/0 ->32/30 while ON. This is a capture implementation
change, not a claim of hardware accuracy or a measured diagnosis of noise.

## Change

An opt-in ENCODER_PIO=1 REMOTE profile uses two continuously running PIO
quadrature counters, based on Raspberry Pi's BSD-3-Clause example. It reserves
an otherwise unused PIO block through the SDK claims, activates SM0/1 and
never resets a shared peripheral. Partial claims/program initialization are
rolled back on failure. The existing Wi-Fi-enabled BSP releases PIO0/1 reset.
Wi-Fi/MQTT and motor/GP21/lease mechanisms are retained.

The 26-word program runs at a 1 MHz instruction clock and has at most ten
instructions between samples. A full RX FIFO drops count snapshots while
the hardware count continues. Diagonal transitions raise sticky PIO_BAD flags;
these are not exact event counts. Depth-plus-one bounded reads avoid returning
only an old FIFO value. The read bound is 20 ms per wheel, plus a finite poll
guard; missing data, disabled state machine and impossible modular deltas
latch capture faults. Motion and remote availability reject those faults.

Unsigned modular deltas are accumulated via int64_t into saturating int32_t
counts, preserving the established forward signs left-negative/right-positive.
The serial profile explicitly says ENC_CAPTURE=PIO and omits inapplicable
GPIO IRQ/over counters. No raw MQTT duty reinterpretation or PID is added.
The clock banner was corrected after review to say state-machine clock.

## Verification and failures resolved

- RED: no-op PIO instruction baseline failed the five initial interpreter
  tests; accumulator stub, IO stub and capture adapter stub each failed their
  expected assertions. Readiness without capture.error checking failed.
- The old GPIO backend with 1000 cycles deferred until one CPU service showed
  reference4000/observed0/over1. Its retained characterization test now asserts
  that limitation; the PIO interpreter requires all4000 counts across a full
  FIFO/CPU pause instead of treating a vanished warning counter as a pass.
- Actual SDK pioasm generates the checked-in header reproducibly. The small
  local build uses the existing SDK's assembler sources, without installation.
- The interpreter checks actual generated words, all16 transitions and SM
  relative flags, both directions, startup phases, reversals/wrap, full FIFO,
  sticky invalid flags and twelve-instruction state dwell.
- Strict fatal ASan/UBSan C checks cover modular wrap/rails/half-range, resource
  contention/rollback, input seeding, fresh FIFO reads, disabled SM, frozen
  timer and timer wrap, adapter fault latching and remote availability.
- The first complete Python discovery failed test_debug_capture because WSL
  lacks tkinter. The installed Windows Python has Tcl. The final runner
  explicitly runs those six tests there and all remaining Python tests in WSL;
  none is silently skipped. Final totals: 57 WSL Python plus 6 Windows Tcl.
- Target builds caught the inherited T-Kernel size_t conflict from an unnecessary
  stddef include in the kernel adapter, SDK-only inline warning differences and
  an unavailable pico/time.h include in this port. Removed the unnecessary
  adapter include, scoped warning exclusions to external SDK headers and used
  the existing time_us_64 compatibility hook with its correct declaration.
  Application implementation remains under strict diagnostics.
- Final label2026-11-08-pio-ready: **61 stages PASS, zero warnings**, including
  the existing C suites, 63 Python/Tcl tests, normal/TEST PIO and normal/TEST
  default-GPIO compatibility builds. All32 pre-existing UF2 hashes unchanged.
  [Full commands/results](2026-11-08-pio-ready-result.json).
- One fresh review by /root/pio_capture_review found no Critical/Important
  issue. Its minor clock-wording finding was fixed before the final build.
  Reviewed resource/SDK behavior, PIO semantics, wrap, rollback and faults.

## Delivered files

| Variant | File | SHA256 |
|---|---|---|
| Normal, next car upload | output/firmware/AAI05_Encoder_PIO_v0_9_REMOTE.uf2 | 0a5e865299d594d4f4a338d387df4c5754e7047d52a9c7191b26acb8eb17b6eb |
| TEST compile variant | output/firmware/AAI05_Encoder_PIO_v0_9_REMOTE_TEST.uf2 | e7ba61620019d0eb51f9f5135cf896c9fedfa56465fb840d6ff06c83490fb5b2 |

Images were copied exclusively to new names, with matching ELF/map files. Both
ELFs contain the v0.9 PIO banner and broker192.168.1.206. Credentials and images
remain ignored. [Readiness manifest](2026-11-08-pio-v0_9-readiness.json).
Normal ARM size: text341444, data100, bss82616 bytes; TEST text342136, data100,
bss82616. These are feature-build sizes, not equivalent-function minimum-bit
representation savings or proof that Week13 evidence is complete.

## Pending physical work

Use [the single-run guide](../../../docs/PIO_ENCODER_V0_9.md): upload normal,
wheels raised, existing bank VIN/Pico USB and broker, check ready/error0/PIO_BAD0/0,
then one -50/+50 bounded forward run. Preserve before/moving/fully-coasted
records and CLI output. No assistant upload, broker start or actuation occurred.
Actual Wi-Fi-active PIO capture quality and independent count/revolution
accuracy remain pending. Zero sticky flags cannot prove arbitrary-rate capture,
electrical noise immunity, calibrated speed or power-rail stability.
