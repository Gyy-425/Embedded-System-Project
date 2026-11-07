# MQTT Motion Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline; one final
> fresh whole-feature review. User requests autonomous practical development.

**Goal:** Connect MQTT commands to Motion with explicit fresh START/STOP and
safe lease/session handling, preserving prior profiles.
**Architecture:** Portable v2 codec/control -> network-owner adapter -> typed
Motion ARM/submit/STOP -> sole driver owner. CLI uses the same binary contract.
**Tech Stack:** C, micro T-Kernel, SDK2.3.0 lwIP, Python/Mosquitto tools.
**Spec:** docs/superpowers/specs/2026-11-07-mqtt-motion-design.md.

## Constraints / review focus

-750ms remote lease,1000ms single-use grant,100ms internal lease,20ms feed,
 200ms CLI renewal; duplicates never extend a lease.
-14/23/26-byte formats, CRC and explicit signed/big-endian validation; v1
 selection cannot drive the REMOTE image.
- Retained metadata is captured once per publish and survives fragmentation;
 SDK parser hash guard protects the private-header assumption.
- Owner-only rearm requires coast, healthy hardware and released GP21. No
 command/STOP/ARM race or reconnect may cause automatic restart.
- Report ACK acceptance separately from actual Motion/DRIVE; no false successes
 on ACK loss, stale grant, link/session changes or unavailable status.

## Task1: portable protocol and remote controller

Create drive_protocol.[ch], drive_control.[ch], strict host tests and C vectors.
Interfaces: decode command, encode status; reset/receive/poll/arm-result and
submission-result transitions, with explicit ARM/STOP/SUBMIT actions.

- [x] Failing codec tests -> strict validation/serialization -> green.
- [x] Failing lifecycle/replay/expiry tests -> state policy -> green.

## Task2: Motion/transport integration

Extend Motion task API/status with asynchronous ARM epoch and owner-side
revalidation; driver explicit rearm only in coast. Add REMOTE profile gate,
disable LOCAL producer, and implement mqtt_drive_app.c on the network task.
Extend mqtt_utk for the common app hooks and retained-aware remote hook.

- [x] Reproduce missing/unsafe rearm cases, implement guards, pass old/new tests.
- [x] Add adapter and fragment tests including retained replays, failed ARM,
  concurrent STOP and status loss; inspect private parser and pin its hash.

## Task3: CLI, profiles and delivery

Create drive_codec.py/drive_cli.py, parameterize the existing Bridge defaults
without changing v1 behavior. Add --remote-app build/profile/version v0.8,
network config/parser guard, new host tests and C/Python vectors.

- [x] CLI tests for session/grant/ACK matching, bounded renewal and exit STOP.
- [x] Run affected motion/port/protocol/client checks and both builds once,
  preserve existing UF2 hashes; repeat only on changes/failures.
- [x] One fresh review, material findings RED->GREEN, final verification and
  concise two-case network hardware guide. Save local commits, no push/merge.

Ruling: session/grants provide replay/liveness checks on the current trusted
LAN, not authentication. Retained delivery detection follows MQTT3.1.1 and
does not claim visibility of the original publisher flag on live forwarding.
Cost: hostile publishers are outside this lab control model; no security claim.
Prior completed evidence checkpoint0660fdd is local and preserves v0.7 results.


Completed7 November2026: one fresh whole-feature review found two material
issues. Added RED regressions for actual network-owner servicing (REMOTE and
observed DOWN) and START-to-STOP duration budgeting; fixed both, then all31
final affected stages passed with zero warnings. Final logs use
`2026-11-07-v0_8-remote-final`. All22 prior UF2 hashes match. Practical hardware
validation remains pending, not part of the software-complete claim. Local
feature commit only; no push, merge, upload, broker startup or actuation.
