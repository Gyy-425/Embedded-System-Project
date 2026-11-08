# PIO Encoder Capture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prepare a PIO capture candidate that retains wheel counts across CPU service gaps without hiding invalid samples or changing motor/network behavior.

**Architecture:** Two hardware state machines share a 26-word origin0 decoder.
A Pico-SDK-only IO unit owns claims and bounded snapshots; the Motion-owned
capture adapter maps errors and uses a portable overflow-safe accumulator.
The GPIO backend is kept as the default profile and comparison path.

**Tech Stack:** C11/BARR-C, RP2040 PIO, Pico SDK2.3.0, micro T-Kernel, GCC/ASan/UBSan, Python unittest and normal/TEST firmware.

**Spec:** ../specs/2026-11-08-pio-encoder-design.md

## Global Constraints

- Keep the existing bring-up checkout, wiring, raw count/duty signs, STOP/expiry and MQTT schema.
- PIO is opt-in and reserves an otherwise unused block without resetting shared hardware.
- Diagonal flags and read faults remain explicit; no guessed lost counts or hardware-pass claims.
- Keep credentials private and older UF2 artifacts unchanged; no automatic upload/actuation.

## Review Focus

- A CPU service pause with full RX FIFO must not stop counting.
- Nonzero initial AB must not create a phantom edge.
- Resource contention/partial initialization must not damage Wi-Fi or motor IO.
- No fresh sample, stopped SM or time wrap must not cause unbounded waiting or silent stale acceptance.
- Counter wrap, INT32 rails and exactly half-range delta must not invoke signed overflow.

## Task 1: PIO capture candidate and focused evidence

**Files:** Create subsystem2 encoder_pio program/logic/IO/capture units and host tests; modify capture/app snapshot interfaces, profile flags, serial/banner and affected readiness guard; create a focused build/test runner and guide.

**Interfaces:** Retain `encoder_capture_start/reset/read/irq`; extend snapshots with capture runtime error and PIO invalid flags. SDK IO returns explicit status plus two raw32 counts and a sticky invalid mask. Portable count accumulation consumes raw32 samples and produces the existing signed32 saturating view.

- [x] Reproduce old GPIO history loss with deferred CPU service; keep regression output.
- [x] Add failing program interpreter and accumulator tests, then implement the program/portable C.
- [x] Add failing SDK-boundary and capture-adapter tests, then implement resource/start/read/error handling.
- [x] Integrate opt-in build selection, runtime failure stop and honest PIO diagnostics.
- [x] Run focused and existing host suites, SDK/target checks and normal/TEST builds; preserve image hashes.
- [x] Request one fresh whole-change review, fix material findings, rerun affected verification.
- [x] Record readiness/limitations and provide a single new raised-wheel capture procedure.

## Execution ledger

Baseline: 60c671c. Existing checkout on aai05/bring-up; user preference is no additional worktree.
Ruling: reserve an entire free PIO block, with two active SMs, to avoid untracked IRQ-flag sharing with other programs. Resource cost is explicit and fails closed if unavailable.
Ruling: retain sticky invalid flags rather than pretending to count every invalid event; generic GPIO counters are not reported in the PIO profile.

Task1 complete (software): label2026-11-08-pio-ready has61 passing stages,63 Python/Tcl tests and all32 older UF2s unchanged. Review found no material issue; clock wording fixed. Hardware acceptance remains pending. WSL Tcl absence is handled by explicitly running the six existing debugger tests on Windows Python. Target-header integration failures and their fixes are recorded in the preparation evidence, not hidden.
