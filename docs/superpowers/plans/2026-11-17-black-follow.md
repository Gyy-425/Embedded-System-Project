# Bounded black-follow implementation plan

Spec: ../specs/2026-11-17-black-follow-design.md
Execution: authorised solo inline checkout; no commit/push or automatic flash.

## Interfaces and tasks
- [x] 1. `black_follow.h/.c`: portable input/config/state/output, fresh-button
  gate, steering and safety policy. Write tests, observe RED, strict GREEN.
- [x] 2. `black_follow_owner.h/.c`: Motion-owner-only cached observation adapter,
  existing motion_control/chassis/driver integration, explicit rearm and cutoff
  policy. Test real policy with substituted acquisition/driver boundaries.
- [x] 3. `follow_snapshot.h/.c`: FOLLOW-only interrupt-preserving copied Motion
  status; test pthread concurrency. Integrate profile/owner/startup/reporting,
  reuse RGB acquisition and PIO, preserve legacy profiles.
- [x] 4. Build verification helper, replay actual positions, full regression,
  distinct normal/TEST and NO_DRIVE artifacts, independent review, test guide.

Global constraints: C11/BARR-C conventions, one motor writer, raw100ms hardware
lease, no kernel/network changes, no false live-test or measured-speed claims.
Review focus: held-button boot/restart; RGB/IR busy versus invalid/stale; motor
cutoff before renewal; ADC overflow/ambiguous both-high; no-drive build guards.

Pre-flight: existing Motion app has its own GP20 exercise, so FOLLOW must be a
separate mutually exclusive owner branch. Current RGB_DIAG requires no-drive;
FOLLOW enables the shared RGB owner without weakening that diagnostic guard.
Existing PIO make restriction needs explicit FOLLOW allowance. Current report
couples IR output to successful Motion polling; new report must be independent.

Ruling: implement this already-authorised focused milestone without another
permission round. Local1000ms prototype precedes later Wi-Fi following. Preserve
existing dirty work, calibration fixtures and prior firmware; no worktree move.

Task1: portable core RED (header absent), then strict/sanitized GREEN. Position
replay97 rows confirms correct neutral/right/left outputs; no measured motion.
Task2: owner RED (missing module), GREEN after realistic periodic-renewal test
correction. Review required final lease <= remaining run duration; implemented
and tested10ms final lease at age990. Added sensor-freshness lease cap after
observing RED at IR age60; GREEN verifies40ms IR and20ms RGB remaining leases.
Task3: snapshot module RED (missing module), pthread concurrency GREEN. Initial
target build found kernel/libc size_t conflict from stddef after kernel header;
removed redundant include. Current local profile does not alter legacy reader.
Ruling:1000ms is a kernel-clock target; leases cap run/data remaining time but
hardware timing, quantization, WCET and stopping distance remain unmeasured.

Review P1: relative remaining-time leases could be extended by preemption
before driver application. Fixed with a separate absolute-deadline driver API:
start/renew constrain against actual hardware clock under the driver lock.
Owner anchors hardware clock before kernel time, subtracts10ms quantization
margin, and caps to run/IR/RGB/command deadlines. Delayed first/renewal and
timer-wrap tests added; old relative API preserved. Re-review found no remaining
Critical/Important issue. Test guide must describe nominal, possibly early stop.

Final startup audit: FOLLOW must complete RGB start before Motion can read its
availability flag. Actual app_main ordering test observed RED; reordered only
FOLLOW startup to IR/RGB/Motion and halt on RGB resource/start failure. Legacy
startup sequence unchanged. Test proves a failed RGB start never starts Motion.

All tasks complete for this bounded local milestone. Final23-stage release,
69-stage prior regression,17-stage RGB regression pass;89 earlier UF2s
unchanged. Normal/NO_DRIVE copies and manifest saved. Independent final review
clean after absolute-deadline fix. Hardware NOT RUN; Wi-Fi following deferred.
