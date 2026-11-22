# Gentler steering comparison after recorded D40 weaving

Evidence: f818ef1d capture archived as2026-11-17-ground-weaving-D40-serial.txt.
Two runs,8 RUNNING snapshots at260ms report intervals: turns[-20,+20,+6,-20]
and[+20,+20,-20,-20]. Both finishCOMPLETE; valid IR, RGB BLACK during RUNNING,
PIO_BAD0. Max requests−16/+40 versus−40/+16 show strong alternating steering.
Sampling cannot verify the user's exact count of about5 turns per run.

Hypothesis: proportional strength/early normalization saturation overcorrects.
Black response is thousands of counts above cardboard; moving a black/white
threshold is not supported. Static spans reach full scale by~1400 whereas
moving black reaches~3000; keep spans fixed for this single-variable test.

Change only nominal max proportional turn20→8; retain D40, baselines, deadband,
snapshot handling, duration and all absolute freshness/deadline protections.
At maximum correction D40 should request40/28 instead of40/16. New T8 images
must be distinctly named and show turn_max=8. No physical fix claim before test.

- [x] Add literal8/20 steering/owner fixtures; observe RED for requested8.
- [x] Add bounded turn config/build flag, replace fixed20 and visible status.
- [x] Verify all duty presets plus oldT20, measured replay, regressions/builds.
- [x] Review and publish D40_T8 with short centred comparison instructions.

Existing solo checkout, no flash/push, all earlier images retained. True cause
may include geometry/delay/motor nonlinearity; do not label it fully resolved.

Software trial prepared:39-stage final,69-stage regression and17-stage RGB
checks pass;103 earlier images preserved. Review clean. D40_T8 images and
manifest published locally; no flash/push and no physical fix claim.
