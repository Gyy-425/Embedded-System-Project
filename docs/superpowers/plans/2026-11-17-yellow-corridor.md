# Yellow corridor implementation plan

> Implement inline using superpowers:executing-plans, then request one fresh
> code review. Spec: ../specs/2026-11-17-yellow-corridor-design.md.

Goal: separate yellow-border trial while preserving black behaviour and all
existing motor expiry protections. Stack: C11, Pico SDK, micro T-Kernel, Python
evidence tools. Work in the existing bring-up checkout: the current working
firmware includes uncommitted and untracked sources needed by this task; do
not commit, move or clean unrelated work. Build in distinct candidate paths.

## Review focus

- Mirrored border/stripe steering and per-channel normalization.
- Incorrect target or UNKNOWN cannot grant movement.
- Both-high, stale data, held buttons and late writes remain bounded.
- RGB adaptation must preserve other classes and ambiguity rejection.
- Cache/profile identity and artifacts must identify yellow versus black.

## Tasks

- [x] Archive three raw captures; make labelled CSV/provenance and paired
  vectors. Add yellow-policy tests and run RED against current code.
- [x] Add compile-time geometry selection, target gate, yellow normalization,
  modelR3 and reporting. Keep default black behaviour unchanged. Run GREEN
  core, owner, new captures and old RGB replay, including STOP/freshness/wrap.
- [x] Extend reproducible check/build helper with --track yellow, mode rejection
  and separate names/profile key. Build NO_DRIVE, powered and TEST yellow,
  plus black/RGB/remote compatibility; verify all previous UF2s unchanged.
- [x] Fresh review, fix substantiated issues and rerun affected checks. Publish
  uniquely named artifacts and a concise two-stage test guide. Hardware NOT RUN.

Ledger: User explicitly asks to build now, continuing their preference for
practical implementation without repeated design confirmation. Technical
choices above are implementation decisions, not claimed team agreements.


## Evidence ledger

- RED: yellow-20261117-red failed the requested right correction for left
  border; yellowrgb-20261117-red rejected29 labelled yellow records.
- GREEN: yellowrgb-20261117-green1 passes17 stages and693 labelled RGB rows;
  yellow-20261117-green1 passes pure yellow policy and108 paired positions.
- yellow-20261117-regression passes69 legacy/driver/protocol stages.
- Review independently checked raw hashes, fixture fidelity and R3 extrema.
  It found the old black startup banner in yellow builds. Added actual-output
  startup assertions; yellow-banner-20261117-red fails the old banner, then
  conditionalized the banner/target. Final build includes both startup tests.
- Powered hardware and repeatable PIO health remain NOT RUN for yellow.

Final: yellow-20261117-final passes50 stages, six firmware builds, no compiler warnings;123 previous UF2s unchanged. Correct mode/version/target/model verified in both yellow ELF files; root UF2 copies hash-match final candidates. Review follow-up found no remaining substantive yellow-only issues. No car flash or powered validation performed.
