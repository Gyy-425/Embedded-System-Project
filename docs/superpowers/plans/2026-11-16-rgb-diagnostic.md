# RGB Diagnostic Implementation Plan

> Execute inline using superpowers:executing-plans; the user authorised the
> focused read-only milestone. No new approval or hardware actuation needed.

**Goal:** Build and document a no-drive TCS34725 diagnostic with bounded I2C.
**Architecture:** Portable state machine, SDK adapter, RGB owner task, report reader.
**Tech Stack:** C11, Pico SDK2.3.0, micro T-Kernel, Ubuntu host/cross compilers.
**Spec:** ../specs/2026-11-16-rgb-diagnostic-design.md

## Global constraints

No secrets changes, upload, motor actuation or pushes; existing checkout and
interfaces retained. SDK I2C1 GP2/3 alone; inherited kernel I2C disabled.

## Review focus

Faults must invalidate cached data; wrapping time must retain waits/freshness;
partial bursts must not mix observations; absent sensor must not block IR;
RGB profile must reject powered/remote configurations.

## Tasks

1. [x] Add portable driver header and fake-bus host tests; observe RED, then
   implement state machine and observe GREEN with strict/sanitized compilation.
2. [x] Add SDK adapter tests and implementation; integrate task/report/profile.
   Verify shared deadline, correct pins, profile guards and no-drive build.
3. [x] Run regression tests and normal/TEST RGB plus remote PIO builds. Inspect
   warnings and old UF2 hashes; fresh review, fix findings, rerun affected checks.
4. [x] Write upload and bounded stationary test guide, record exact artifacts,
   evidence and remaining hardware checks. No commit of unrelated capture files.

## Execution ledger

- Planning: SDK and compilers already installed. WSL requires sandbox escalation;
  elevated read-only discovery succeeds. No dependency installation needed.
- Ruling: user explicitly authorised this exact focused case after previous
  discussion; record choices here and proceed without a redundant approval turn.
- Task1 complete: missing source RED, then sanitized host core GREEN. Additional
  RED/GREEN regressions for freeze-before-burst, delayed freeze age, and10ms
  quantisation phase. Final core also checks every setup transfer and readback.
- Task2 complete: adapter missing-source RED, then GREEN; short-write RED/GREEN.
  Real task adapter resource-failure RED/GREEN. Default RGB flag remains off;
  no-drive profile rejects powered, remote and UART configurations.
- Ruling: portable bus lengths are uint8_t rather than size_t to avoid the
  inherited kernel's incompatible size_t typedef. SDK sizes stay inside adapter.
- Ruling: SDK header warnings are scoped out around SDK includes only; new
  application code retains strict warnings-as-errors. No inherited SDK modified.
- Review: fresh reviewer found cross-conversion and resource-startup issues;
  both fixed with regression tests. Adopted short-write hardening and maximum
  conversion-time guard. Reviewed kernel tick=10ms; oscillator13ms/conversion67ms
  guards include quantisation margin. Quantised-clock adversarial host test passes.
- Task3 complete: rgb-20261116-release-result.json records all host checks,
  normal/TEST RGB and remote PIO builds passing, no compiler warnings,66 old
  UF2s unchanged. Expanded final host cases also pass in rgb-20261116-finalhost.
- Task4 complete: docs/RGB_DIAGNOSTIC_TEST.md and normal image at
  output/firmware/AAI05_RGB_Diagnostic_v0_11.uf2. No upload/actuation/network or
  private credential changes. All physical sensor and optical acceptance pending.
