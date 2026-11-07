# Stationary calibration tool preparation

7 November2026. User requested the next focused code case and confirmed actual
seven tapes/background are not yet available. This is prepared laptop tooling,
not actual sensor calibration or a deployed classifier.

New standard-library Python files: `tools/mqtt/tape_calibration.py` and
`tools/mqtt/calibrate_cli.py`. Capture uses existing v2 STATUS/telemetry and
publishes no motion command. It requires stopped/ready state, fresh sequence
and uptime in the same session, valid IR and500ms settling. Default40 samples,
30s sample-acceptance deadline,2s maximum stream gap,1s relative host/device
elapsed divergence fence. These are tool settings, not measured project NFRs.

Exclusive output directories contain per-sample flushed CSV and metadata with
complete/incomplete status. Offline reports reject corrupt/inconsistent data,
exclude incomplete attempts and list missing classes/overlap independently for
left/right. Disjoint observed stationary ranges are labelled CANDIDATE_RANGES,
not recognition accuracy, guaranteed separability or firmware thresholds.

## Verification and review

One fresh whole-feature reviewer found two Important issues. Deterministic
regressions reproduced: ordered telemetry advancing250ms every1.5s could be
accepted as fresh; a pause inside receive could evade the2s gap or permit a
final sample after the deadline. Three assertions failed in
`2026-11-07-calibration-review-red.log`, then passed after one fix pass adding
post-receive checks and a relative elapsed-time fence. No re-review or deferred
minor findings. Physical/radio timing, real tape quality and old firmware
behaviour were outside that read-only review; those remain separately scoped.

Final executed tests:

| Group | Count | Evidence |
|---|---:|---|
| Capture/persistence/report/CLI |18|`2026-11-07-calibration-reviewed-final.log`|
| Existing v1/v2 clients |24|`2026-11-07-calibration-reviewed-cli.log`|
| Existing v2 codec/C vectors |4|`2026-11-07-calibration-reviewed-codec.log`|

All46 passed. Existing C vector files supplied through DRIVE_VECTOR_FILE and
COMM_VECTOR_FILE; no vector-dependent skip. Python syntax compilation and CLI
help succeeded. New tests exercise actual temporary CSV/JSON and an offline
report subprocess, including exclusive-file rejection. Test packets are
synthetic, not physical measurements. The collector fixture has receive only;
there is no publish/motor interface available to it.

Initial RED is missing implementation import, not a functional capture failure.
The first subsequent run hit sandbox default-temp permissions; test temp was
redirected to the verified workspace tmp directory. That infrastructure error
is not claimed as product RED. A malformed metadata-state error was separately
reproduced and corrected. Logs retain these distinctions.

Reproduce in the firmware directory, with TEMP/TMP pointing to a writable test
temporary directory:

```powershell
python -m unittest discover -s testing/host -p test_tape_calibration.py -v
python -m unittest discover -s testing/host -p 'test_*cli.py' -v
python -m unittest discover -s testing/host -p test_drive_codec.py -v
```

For vector tests, set DRIVE_VECTOR_FILE and COMM_VECTOR_FILE to the recorded
v0.8 final C-vector log paths as in the completed run. No broker/robot is used.

## Limits and next step

The elapsed guard is relative to initial STATUS, not an absolute measured
sample-age guarantee. Delayed STATUS/host scheduling may conservatively abort;
recapture under stable conditions. CSV/metadata are flushed, not guaranteed
against filesystem/power failure; partial/corrupt output is not a complete
dataset. No equivalent-function RAM savings or grading/accuracy claim.

Firmware source/build and existing clients are unchanged. No firmware rebuild,
upload, broker command, motor actuation or real tape capture occurred. Verified
normal REMOTE SHA256 remains
`d80d95ad4b7c364198e293942cef1bb8a46bf8202fb138b580460092dae5ba0a`;
TEST remains
`0b002d4b11266aedf3ec4a1fd947e63e20ec38cd465e409b3c9d0999ca0b7728`.

The separate broker-loss/recovery result is now recorded as passed within its
scope. Next use [the capture guide](../../../docs/TAPE_CALIBRATION.md) with real
tapes, then inspect range overlap before designing the C classifier. No further
basic MQTT movement tests or component photos are needed for this case.
