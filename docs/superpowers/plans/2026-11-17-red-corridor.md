# Red corridor and35% duty implementation plan

> Implement inline with test-driven development and one fresh final code review.

User requests red as the second black-bordered lane colour, reports the yellow
trial smoother than the black stripe trial, and requests35% motor duty. The
third interior colour is not yet selected. Black remains a recognised border
and a retained historical stripe test, not the intended final interior colour.

## Design and constraints

Reuse the bounded corridor controller and single motor owner; a compile-time
RED target is separate from YELLOW and plain BLACK. Two mutually exclusive
colour switches avoid accidental polarity changes from noisy RGB reports.
Keep the yellow-normalized IR scale and turn limit8 because the three red
positions exhibit the same response; prove every measured position by replay.
At35% duty the straight request is -35/+35; maximum corrections -35/+25 and
-25/+35. These are electrical duties, not calibrated speed.

Add RED class4 to the existing local3-bit class encoding, preserving IDs0..3
and UNKNOWN7. Grow native model to five profiles and validate mask0..4. Do not
change the existing MQTT/IR protocol. Calibrate red from the142 supplied rows,
32/4095 chromatic margin and25% clear margin; retain every R3 bound. Old
unsupported red samples remain raw historical evidence with their original
expected UNKNOWN, unless actual new-profile overlap requires explicit review.
No forced match, live polarity switching, search or automatic restart.

New follow defaults are duty35. Older UF2s must remain unchanged. Keep the
one-second trial, GP21, RGB/IR freshness and absolute cutoff. All supplied red
captures contain sticky PIO_BAD1; a fresh stationary check precedes motion.

## Tasks

- [x] Archive raw file and its three labelled sections; add142 RGB/IR fixture
  rows and provenance. Add tests for red classification, direction, wrong
  target,35% output, startup identity and mask validity; observe RED.
- [x] Add red profile/class, explicit selected target/geometry, modelR4,
  defaults35 and startup/report identity. Keep yellow/black tests green.
- [x] Extend reproducible --track red build and invalid-combination checks.
  Run all835 RGB rows, red/yellow/black position replay, owner STOP/freshness/
  deadline tests; build separate NO_DRIVE, powered, TEST and compatibility
  images. Check prior artifacts unchanged.
- [x] Fresh review; resolve concrete findings, publish red35 and yellow35
  images and practical test guide. No flash, commit, push or claim of red motion.

Files: config/follow_config.h and rgb_calibration_config.h; subsystem1 RGB
classifier/calibration and startup/report; subsystem2 shared policy/owner;
build_make/pico_rp2040.mk; tools/check_black_follow.py and check_rgb_diag.py;
testing/host replay/owner/startup tests and fixture CSV/provenance; docs.

Work in the existing bring-up checkout with its necessary untracked sources;
do not clean, move, stage or commit unrelated work. User has explicitly asked
to develop now; reuse of the existing controller is an implementation choice,
not a new asserted team agreement. Hardware acceptance remains separate.


## Evidence ledger

- RED: redrgb-20261117-red rejects the new literal red sample; redfollow-20261117-red fails requested right correction for left-border contact.
- GREEN: redrgb-20261117-green passes17 stages/all835 labelled RGB rows. Redfollow-20261117-green passes43 host/target stages including142 new position replays and35% owner outputs.
- Redfollow-20261117-regression passes69 legacy/driver/protocol stages.
- Fresh review found no substantive extension defects. Independently checked142 data rows and all256 model masks, red-only target acceptance and35% owner safety.
- Final firmware run passes62 stages/seven firmware builds, no warnings;131 prior UF2s unchanged. Delivered red/NO_DRIVE and yellow35 ELF strings identify the correct target andR4. Root UF2 hashes match candidates.
- No flash/commit/push; red and new yellow35 hardware remain pending. Earlier yellow success recorded as user report, not measured performance.
