# PIO encoder capture for the powered qualification case

The preserved forward run establishes physical and raw encoder signs, but the
GPIO capture drops ambiguous batches: over0/0 ->32/30 while ON. No waveform or
latency measurement establishes whether delays or electrical transitions cause
each batch. This change removes CPU GPIO-service latency from counting; it
does not claim to cure electrical noise or demonstrate calibrated distance.

The user authorized the next development case and previously chose continuous
implementation in the existing bring-up checkout. These are engineering
decisions, not new professor requirements or attributed team agreements.

## Approaches and decision

1. Instrument/tune CPU GPIO IRQ capture. Smallest resource change, but cannot
   recover event order already lost by GPIO latches and remains sensitive to
   interrupt-disabled intervals. Preserve this backend for comparison.
2. PIO hardware count, selected. Two state machines continuously decode the
   existing adjacent GP4/5 and GP16/17 pairs. The CPU reads absolute counters,
   so delayed task/IRQ service does not discard movement history. This costs
   a PIO block and requires explicit resource, freshness and wrap handling.
3. Edge FIFO/DMA stream. Retains individual samples but adds bounded-buffer
   overflow, DMA ownership and stream-framing complexity unnecessary here.

## One bounded change

- Opt-in `ENCODER_PIO=1`, named v0.9 REMOTE candidate. Default GPIO backend and
  all existing images remain available. Keep C, micro T-Kernel, Pico SDK2.3.0,
  existing pin map, raw-duty MQTT contract and motor STOP/lease behavior.
- Adapt Raspberry Pi's BSD-3-Clause quadrature counter example. Origin0,
  26 instructions, nonblocking count snapshots; add a sticky per-SM diagonal
  transition flag. At a 1 MHz SM clock the longest decode path is 10 cycles.
  This is a design-rate bound, not a measured motor-speed/PPR specification.
- Reserve all four SM claims in one otherwise unused PIO block; use SM0/1 for
  the wheels. Respect SDK program/SM allocation and release partial claims on
  failure. No whole-PIO reset or shared radio/kernel edits. The other block
  remains available to Wi-Fi; fail visibly if a whole block is unavailable.
- Do not change input pull configuration. GPIO functions/pindirs touched only
  for the four encoder pins, never motor outputs. Seed prior AB before enable
  so a stationary nonzero startup phase does not create a phantom count.
- Absolute hardware uint32 counters wrap; accumulate unsigned modular deltas
  through int64 arithmetic into the existing signed32 saturating count view.
  Reject an exactly half-range delta. No signed overflow or direct uint32-to-
  int32 out-of-range cast. Startup-only reset and Motion-only capture access.
- Read FIFO depth plus one snapshot with bounded time/iteration limits. FIFO
  full drops snapshots, never stalls counting; do not present FIFO fullness as
  lost wheel edges. Disabled SM, timeout, failed initialization or impossible
  delta produces a latched capture error and prevents further motion.
- Preserve diagonal warnings as sticky PIO invalid flags. Their semantics are
  flags, not exact transition counts. PIO serial reports `ENC_CAPTURE=PIO` and
  `PIO_BAD=L/R`, omitting inapplicable GPIO irq/over counters. Existing `bad`
  and `over` fields remain available only in the GPIO serial profile.
- No PID, automatic forward-sign remapping, IR/calibration changes or MQTT
  protocol extension. Absolute counts remain the established raw signs.

## Evidence required

Before implementation, a test must demonstrate that the old GPIO backend loses
history when CPU service is deferred. Then test the actual PIO instruction
words with an independent instruction interpreter: all transition states,
directions/reversals, nonzero startup phases, dual wheels and long CPU pauses
with a full snapshot FIFO, diagonal flags and modular wrap. Validate the
portable accumulator's signed rails/invalid deltas and platform partial-claim,
fresh snapshot, bounded timeout, disabled SM and timer-wrap paths. Keep the
existing Motion/MQTT/IR/colour host suites and legacy GPIO checks green; build
normal/TEST PIO candidates and retain earlier UF2 hashes.

Physical acceptance remains pending: one forward run with Wi-Fi/MQTT active,
PIO ready/error0, no new PIO invalid flags or saturation, output OFF afterward,
followed by an independent marked-revolution count reference. Zero flags alone
does not prove no aliasing at arbitrarily high edge rates or real accuracy.

Reference: https://github.com/raspberrypi/pico-examples/tree/master/pio/quadrature_encoder
