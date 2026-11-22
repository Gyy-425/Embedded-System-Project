# Adjustable follower duty: controlled ground-weaving experiment

User reports v0.13 moves forward but weaves on the ground and asks for speed
control. No moving FOLLOW logs yet; cause is not established. Hypothesis:
lower travel per control cycle can reduce overshoot. Do not change steering
gain/deadband and speed simultaneously or claim a hunting fix without car data.

Scope: compile-time duty presets30/35/40/45/50, default40. Scale both converted
wheel duties by limit/50 AFTER the existing chassis conversion, preserving turn
ratio up to integer rounding and limiting every wheel. Keep pure steering law,
GP20/GP21, one-second limit, absolute deadlines and freshness policy unchanged.
This is open-loop PWM limiting, not encoder-based physical speed regulation.
No live USB command parsing: existing USB input is discarded by the port;
no shared-driver or MQTT changes for this focused milestone.

- [x] Regression: literal35/40/50 straight and correction duties; observe RED.
- [x] Implement config/build flag, scaling and visible limit in boot/status.
- [x] Verify host/sanitizers, prior regressions, guarded builds and review.
- [x] Publish distinct D40 normal/NO_DRIVE images with clear car comparison.

Existing solo checkout and old images preserved. Latest moving logs needed
before steering-specific changes. New physical behavior remains untested.

Complete: observed RED then strict/sanitized GREEN for all5 duty presets;
32-stage build check,69-stage regression and17-stage RGB check PASS.
96 earlier firmware images preserved; reviewed D40 files published locally.
Ground comparison NOT RUN; weaving diagnosis still pending moving evidence.
