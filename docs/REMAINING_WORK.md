# Remaining project work

Status snapshot: 8 November 2026. Follow the professor-confirmed revised scope: two IR AO/ADC sensors; report successive tape colours while moving; receive a target colour over Wi-Fi and follow its path; five selected colours plus black/white; readable command-prompt output. Existing software/submission requirements remain applicable. This is a work checklist, not evidence that pending behaviour has passed.

Focused motion coding: forward/steering C adapter and equivalent CLI `move`
are implemented; strict/sanitized C checks,71 Python/Tcl tests and four
compatibility builds passed after the independent review's input-narrowing
finding was corrected and regression-tested.
No on-car IR controller is active yet. The user reports three neutral bank-only
untethered runs travelling straight under power, with a small post-OFF left
movement. Keep turn0; move to the two intentional curve checks. Fresh client
status/OFF exchanges currently work; earlier transient connection/ACK failures
remain unexplained. Precise stopping/heading performance and calibrated encoder
feedback remain open.
[Latest ground result and next check](../testing/integration/logs/2026-11-08-neutral-ground-result.md).
[Motion API and ground guide](CHASSIS_MOTION.md).

Latest prepared development: v0.9 PIO capture replaces CPU-serviced edge latches
in an opt-in image. Host checks, independent review and all four PIO/GPIO
normal/TEST builds passed. The basic powered trial below has now passed;
independent count accuracy/scaling remains pending.
[Reproduction guide](PIO_ENCODER_V0_9.md),
[preparation evidence](../testing/integration/logs/2026-11-08-pio-v0_9-preparation.md).

Latest physical observation: **v0.9 basic powered PIO capture trial passed**.
The power/USB-reconnected run starts clean, counts in the expected directions,
retains PIO_BAD0/0/UNCERTAIN0/errors0/sat0 and ends with fresh output OFF and
stable counts-10652/+11113. No repeat of this test is needed. The earlier
flag's cause is unproven; independently establish count scaling before using
calibrated feedback. [Clean rerun](../testing/integration/logs/2026-11-08-v0_9-pio-clean-rerun.md).

Earlier GPIO case: [paired motor/encoder qualification](MOTOR_ENCODER_QUALIFICATION.md).
The laptop broker is now 192.168.1.206; a separately named working v0.8 image
has been built for that address, with no motor/encoder source changes. The
first paired physical test produced both-wheel movement and a saved fresh
output-off response. Raw positive duty drives left rearward and right forward;
the user subsequently confirmed both forward with `-50/+50`. The supplied
serial records establish raw forward encoder signs left negative/right positive
and requested output OFF, with errors0/bad0/sat0. Powered capture is unqualified:
over rises 0/0 ->32/30 during movement and ->39/36 during spin-down.
Do not replay basic direction tests; investigate capture quality before feedback.
[Forward-run evidence](../testing/integration/logs/2026-11-08-forward-encoder-result.md).
[Observed result and limits](../testing/integration/logs/2026-11-08-paired-motor-qualification-result.md).

Latest actual result: v0.8 bounded MQTT left-wheel movement, ordinary STOP/rearm, GP21 during movement, broker-loss stopping, recovery without automatic movement and fresh START afterward passed within the recorded setup. [Movement result](../testing/integration/logs/2026-11-07-v0_8-pico-motion-result.md), [loss/recovery result](../testing/integration/logs/2026-11-07-v0_8-broker-loss-result.md). The current MQTT movement bring-up case is complete; do not repeat it. The stationary capture/range-overlap tool and configurable C classifier core are prepared and host-verified; no actual tape readings or live recognition results are available yet. [Capture guide](TAPE_CALIBRATION.md).

## Completed foundation

- [x] Neutral untethered ground bring-up: user reports three straight powered runs at forward40/turn0, with a small post-OFF left movement retained as a stopping observation. Current command/status/OFF exchanges are observed; no measured heading/stop-distance or precise speed claim. [Scoped result](../testing/integration/logs/2026-11-08-neutral-ground-result.md).
- [x] v0.9 basic powered PIO capture trial: clean baseline and final sticky flags0/0, expected raw count progression, errors0/sat0, fresh output OFF and stable coast totals. Independent count/revolution accuracy, calibrated speed/distance and ground-load performance remain pending. [Scoped result](../testing/integration/logs/2026-11-08-v0_9-pio-clean-rerun.md).
- [x] Dedicated v0.8 broker-loss stop/recovery/no automatic movement/fresh START case. Increasing uptime and new session/off-state recovery are observed; physical stop and fresh movement are user-confirmed. [Scoped evidence](../testing/integration/logs/2026-11-07-v0_8-broker-loss-result.md).
- [x] v0.8 actual bounded MQTT left-wheel movement, ordinary STOP/rearm and GP21 while moving. Two successful fresh output-off confirmations and user-confirmed physical movement/GP21 stop are recorded. Later communication loss was deliberate; broker-only actuation-loss/recovery is still pending. [Scoped result](../testing/integration/logs/2026-11-07-v0_8-pico-motion-result.md).
- [x] Laptop-network-path interruption:127 saved telemetry samples,11 waiting notices, automatic resumed data/targetunset/newtag and serial resubscription, with continuing Pico uptime. USB reporting correctly continues through a laptop Wi-Fi outage. [Result](../testing/integration/logs/2026-11-07-v0_5-network-path-result.md).
- [x] MQTT session recovery: pre-gap colour1 → post-gap unset, telemetry seq228 →0, increasing Pico uptime, changed connection tag, automatic watch resumption and successful post-recovery commands. Occasional stale IR frames are recorded for later timing checks. [Recovery result](../testing/integration/logs/2026-11-07-v0_5-mqtt-recovery-ui-result.md).
- [x] Actual Pico target selection/status/clearing sequence: unset/seq0 → colour1/seq1 → unset/seq2, all OK, consistent connection tag; user confirms deliberate shutdown. [Target result](../testing/integration/logs/2026-11-07-v0_5-target-clear-ui-result.md).
- [x] First actual Pico MQTT exchange observed in VS Code: broker199/Pico200, ready STATUS ACK and readable valid raw IR telemetry. Coordinator confirmed the ending stop/disconnection was deliberate. Target mutation/recovery remain pending. [UI result](../testing/integration/logs/2026-11-07-v0_5-pico-network-ui-result.md).
- [x] v0.5 host verification:25 stages passed, including8 communication/client tests and both firmware builds; actual Pico communication remains pending. [Evidence](../testing/integration/logs/2026-11-07-v0_5-host-verified.md).
- [x] Personal GitHub repository, course starter, Ubuntu toolchain and normal/TEST baseline builds.
- [x] Reported wiring map checked against component/board references; no pin remap currently needed.
- [x] Both raw IR channels respond; basic manual encoder input/IRQ capture supported by supplied logs.
- [x] Captured motor-start HardFault and three GPIO definitions corrected, with regression/build evidence.
- [x] All four hardware motor buttons reportedly produce clear movement and stopping on release: basic hardware-button response passes.
- [x] v0.6.1 actual software-triggered four-request sequence: reported cutoffs400020/400010/400007/400009us, DONE/cutoff4/stop0, then GP21 STOPPED/stop1/DRIVE OFF. Coordinator reports movement working well. [Scoped result](../testing/integration/logs/2026-11-07-v0_6_1-pico-motor-result.md). Powered encoder accuracy and STOP under load remain unverified.
- [x] v0.7 actual local Motion renewal/expiry and GP21 while left moves, with subsequent GP20 restart prevention. Retained expiry seq100/reason3/error0/DRIVE OFF; GP21 seq37/reason2/error0/DRIVE OFF, confirmed by physical observations. [Result](../testing/integration/logs/2026-11-07-v0_7-pico-motion-result.md). No measured latency, accurate encoder feedback or simultaneous two-wheel qualification is claimed.
- [x] Laptop broker/client runtime and no-drive communication image prepared; actual Pico status/target/raw-IR exchanges and the recorded recovery cases are supported.

## Remaining work in practical order

Current motor development: physical forward signs, scoped MQTT movement/stop/recovery and the basic powered PIO capture trial are supported. Preserve those results. Independent encoder accuracy/scaling, ground-loaded performance and integrated tracking remain pending.

1. [x] **Core communication bring-up.** Actual IP, broker/status/target/raw telemetry, MQTT session recovery and laptop-network-path loss/restoration are supported by recorded runs. This completes the current no-drive communication case. Actual Pico association-loss/rejoin and final demo-network checks remain explicitly deferred to integrated validation; they are not claimed by the laptop-only test.
2. [ ] **Qualify program-controlled movement for tracking.** Software pulses, local Motion STOP/expiry, physical forward signs and the basic powered PIO capture trial passed. A bounded forward/steering C API and matching CLI are implemented and software-verified, including correction/regression of the independent review's input-boundary finding. Next qualify ground-loaded starting, straight movement and both steering directions. Establish independent count/revolution scaling before calibrated feedback if used; add balancing/control as needed for stable tracking. Preserve prior passes rather than replaying diagnostic versions.
3. [x] **Scoped MQTT actuation-loss/recovery bring-up.** The user confirms stopping on broker loss and successful fresh-command movement afterward. Watch shows the pre-gap RUNNING/ON session, waiting interval, new session IDLE/OFF, explicit START, RUNNING/ON and confirmed final STOPPED/OFF. [Evidence](../testing/integration/logs/2026-11-07-v0_8-broker-loss-result.md). Exact latency, all replay/fault injections, actual AP loss, final demo network and combined-load qualification remain later integration checks.
4. [ ] **Actual tapes, track and acceptance conditions.** Obtain/use the actual five chosen colours plus black/white, cardboard/background and fixed sensor mounting. Set track width/curves/start positions and layouts for both required demonstrations. Choose achievable speed, shortest segment and accuracy/lighting conditions from measurements;5cm remains a proposal, not a confirmed requirement. Check seven classes are separable in IR ADC data before committing to a tape set.
5. [ ] **Colour calibration and recognition.** The portable C classifier core is implemented with per-sensor configurable ranges and explicit unknown/ambiguity/freshness guards;16,384 reference comparisons and host/ARM/firmware compile checks pass. [Core API](COLOUR_CLASSIFIER_CORE.md). Runtime model installation, recognition/reporting and real calibration remain pending. Stationary capture/CSV/metadata and offline overlap-report code is prepared;46 affected host tests passed. Use [the guide](TAPE_CALIBRATION.md) when actual tapes are available. Record labelled readings from each sensor at installed height, derive/store calibration and implement seven-class plus unknown/uncertain handling. Validate stationary and moving classification, transitions and short segments; avoid filtering/debounce that erases brief colours. Report actual accuracy against known tape sequences, not guessed RGB colour recognition.
6. [ ] **Colour reporting while moving.** Combine reliable motion and colour recognition to travel the demonstration path and print successive detected colours cleanly. Coordinate sensor placement/line-position estimation so tracking and colour capture work together. Define behaviour at background, gaps, lost line and track end.
7. [ ] **Wi-Fi-selected target-colour following.** Accept a valid selected colour, acquire/follow that path within the chosen layout, and handle bends, lost target/wrong colour and target changes consistently. Define valid starting positions and any acquisition behaviour so no unimplemented junction-search behaviour is assumed.
8. [ ] **Integrated RTOS/controller behaviour.** Replace the diagnostic-only app with normal robot operation combining IR, Motion and Communications. Define ready/run/stopped/fault/finish behaviour as needed; enforce one motor writer, fresh sensor/control data, deterministic STOP, safe link-loss/reconnect handling and safe mode transitions. Measure actual sampling/control timing with Wi-Fi active; verify priorities/stack use for the final task set.
9. [ ] **Minimum-bit representation and Week13 evidence — design during coding.** Specify ranges, signedness, resolution, invalid values, overflow and timestamp-wrap rules. Pack application states/flags/samples/messages safely across tasks/ISRs, with explicit binary MQTT serialization and validated decoding. Maintain equivalent functionality for before/after RAM and message-size comparisons; template-versus-new-app sizes are not valid representation savings. Provide the field/bit table and correctness/timing evidence.
10. [ ] **Final integrated acceptance runs.** Exercise both demonstrations repeatedly at stated speed/segment length/lighting; test run/stop, invalid/stale commands, connection interruption, line/target loss and clean restart. Validate the combined motor/IR/Wi-Fi setup on the intended bank-powered demo arrangement, capture resets/faults and investigate any instability. Save actual console records, metrics and demonstration evidence.
11. [ ] **Submission and reproducibility.** Review application C for BARR-C, document retained shared-source changes, update architecture/pin/protocol/calibration/build/run instructions and required reports/declarations. Complete representation/RAM/message/performance evidence in GitHub, tidy remaining template placeholders, preserve truthful historical results/ownership, and package/tag the final reproducible source and firmware. Keep credentials out of commits and prepare/test the demo network and fallback arrangement.

Networking does not need to wait for every motion qualification test. Calibration/recognition work needs actual tapes; a final movement/recognition pass cannot be established from host tests alone. Diagnostic versions need not be replayed as separate mandatory robot features.

## Outside the confirmed core scope

Barcode decoding/directed junction manoeuvres, ultrasonic/servo obstacle profiling/avoidance and hump-height measurement are omitted under the professor's confirmed revision. IMU is optional and should only be reconsidered after the required colour/motion/communication application works. No dashboard is required.

Current next action after final motion software checks: one short duty-based ground run for starting, straight movement and steering using the installed v0.9 image and new CLI move command. Establish an independent marked-revolution count reference before calibrated encoder feedback if selected; it does not block duty-based testing or colour work. The clean v0.9 powered rerun needs no replay or firmware change. The earlier invalid flag's cause remains unproven; investigate if it recurs while stationary. Actual tape capture can proceed when samples/background are available; then inspect per-sensor overlap and populate/validate the prepared C classifier before runtime integration. Actual AP loss/rejoin, demo network and combined-load freshness/timing remain later validation.
