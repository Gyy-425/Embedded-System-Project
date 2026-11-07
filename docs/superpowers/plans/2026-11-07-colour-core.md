# C Colour Classifier Core Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline; one final fresh review. Existing user authorization covers implementation; no new worktree/approval cycle.

**Goal:** Add a configurable, portable and explicitly packed C classifier core.
**Architecture:** Private per-channel calibration -> validated bounded range matching -> one-byte class/reason result. No live task/protocol changes.
**Tech Stack:** C11, GCC/ASAN/UBSAN, Cortex-M0+ compiler, existing micro T-Kernel build.
**Spec:** docs/superpowers/specs/2026-11-07-colour-core-design.md.

## Global Constraints

- Seven classes plus unknown; ADC0..4095; all non-match outputs class7.
- Inclusive ranges; ambiguity never picks a first class. Model immutable while reading.
-42 packed-bound bytes +2 age bytes +2 masks; one-byte result with explicit masks/shifts.
- Existing binary/client/hardware behavior unchanged; no physical calibration or MCU action.

## Review Focus

- Missing/corrupt/overlapping calibration must not silently select black or first class.
- Range setters must preserve previous state on rejection and preserve adjacent packed fields.
- Future/expired/wrapped time and invalid readings cannot become valid colour matches.
- Packed layout/savings must retain equivalent behavior and not imply complete Week13 savings.
- Build verification must not overwrite tested firmware or activate synthetic calibration.

### Task1: portable core and verification

**Files:** new subsystem1/colour_classifier.[ch], testing/host/test_colour_classifier.c,
tools/check_colour_core.py, docs/COLOUR_CLASSIFIER_CORE.md, build strict flags,
scope/status/evidence updates.

**Interfaces:** init/set/validate/classify and colour_result_class/reason exactly
as spec. No existing runtime producer/consumer interface changes.

- [x] Write host tests and public header; observe functional RED against safe stubs.
- [x] Implement packed model, rejection/overlap/freshness rules and independent reference checks.
- [x] Strict host compile/sanitized run and target compile; Expected: all assertions PASS, no warnings.
- [x] Build existing REMOTE normal/TEST with distinct EXE_FILE/candidate names; Expected: exit0/no warnings, old hashes unchanged.
- [x] One fresh review and one material RED->GREEN fix pass if needed.
- [x] Record API/representation/concurrency limits, final evidence, approved scope and local commit; no push.

Ledger: base9d25648. Pre-flight: new portable API only; existing runtime unchanged.
Ruling: caller-owned immutable configuration rather than runtime installation —
keeps this case portable and avoids claiming synchronized live updates before
an integration adapter exists. Actual calibration data will be supplied later.


Completion ledger,7 November2026:
- Safe unimplemented baseline compiled; first functional init assertion RED.
-7 assertion groups/16,384 reference comparisons GREEN; sizes46/60 and1/2.
- Full label2026-11-07-colour-core-verify:6 stages pass, zero warnings,
  normal/TEST candidate hashes identical to tested v0.8.26 older UF2s preserved.
- One fresh reviewer found Important: UBSan recovery could report errors with
  exit0. Actual shared host flags were exercised with a controlled overflow;
  regression RED (exit0) -> GREEN after -fno-sanitize-recover=all.
- Final label2026-11-07-colour-core-final:5 affected stages PASS, zero warnings,
  including fatal-sanitizer gate, full C suite and target source/layout checks.
  All28 then-existing UF2s preserved. No core/firmware behavior change in the
  runner-only fix; passed firmware builds did not need repeating.
- No additional findings or deferred minors; no re-review. Python helper syntax
  and whitespace checked. No car/network/upload/push action.
- Ruling: models are caller-owned immutable snapshots, not a synchronized live
  update API. Cost: integration must add proper IR ownership/publication.
- Ruling: overlap status is exposed; unique diagnostic regions can classify,
  ambiguous regions never guess. Cost: deployment still must qualify/reject
  overlap models. No physical accuracy or complete Week13 savings claimed.
