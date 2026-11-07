# AAI05 revised project scope — approved

Prepared7 November2026. **Approved by the coordinator7 November2026.** This is a consolidated
scope record, not evidence that the final robot behaviours have passed.
The coordinator checked this scope and authorized the configurable C
colour-classifier core as the next focused implementation case.

## 1. Basis and precedence

The main basis is the coordinator's six posted professor-confirmation answers
and subsequent statement that the listed revised scope was confirmed. The
workspace's `docs/aai05/confirmed-scope-2026-11-02.md` records the same core
requirements. The actual sent/replied email was not located as a local file;
this draft uses the confirmation wording supplied in the conversation and
does not claim direct inspection of the professor's email.

Those confirmations replace the earlier barcode/obstacle/hump mission scope.
Original software and submission requirements continue to apply. Historical
Week6/tribunal reports and contribution records remain historical evidence;
they are not retroactively rewritten by this scope review.

## 2. Project objective and required demonstrations

Build a Pico W robot that uses the existing reflective IR sensors' analogue
AO outputs through the ADC to recognise a calibrated, fixed tape set, and
integrates that information with motor control and Wi-Fi/MQTT.

**Demonstration A — successive colour reporting during movement.** The robot
must move along the demonstration track, detect successive tape colours and
report the sequence in clean, readable command-prompt output. Reporting raw
ADC values while stationary is preparation, not completion of this behaviour.
The demonstration must assess transitions and shorter colour segments under
the chosen, stated test conditions.

**Demonstration B — Wi-Fi-selected colour-path following.** The robot must
receive a target colour over Wi-Fi and follow the corresponding colour's path
in the agreed demonstration layout. Merely storing a target label or remotely
requesting left/right PWM is preparation, not target-colour following.

Both behaviours are included. They may use separate demonstration modes or
track areas; the professor's confirmation does not require a specific track
geometry or seven simultaneous route choices.

## 3. Confirmed functional boundaries

| Area | Scope |
|---|---|
| Sensing | Existing IR modules, AO -> ADC. Current agreed build uses the two forward-facing sensors; the side sensor is omitted following the later professor clarification. |
| Colour set | Five selected tape colours plus black and white: seven classes total. Exact five materials/colours await actual samples and measurements. |
| Calibration | Calibrating/training against that fixed set is allowed. The confirmation does not require a particular machine-learning algorithm or general RGB recognition. |
| Movement | Software-controlled driving/steering sufficient to perform both required demonstrations. |
| Wi-Fi | Receive the target-colour command and integrate it with following behaviour. MQTT remains the required messaging technology. |
| Output | Clean command-prompt output is sufficient. Existing USB serial or a readable laptop MQTT client can provide it; no dashboard/onboard display is required. |

The three technical areas remain **IR/calibration/classification**, **Motion/
tracking**, and **Wi-Fi/MQTT/interfaces**. These are software responsibilities,
not a new assessment attribution or claim about team contributions. The current
solo coding workflow requested by the coordinator does not change historical
tribunal contribution records.

## 4. Software, representation and submission requirements retained

- Firmware uses C, the Pico C SDK and micro T-Kernel; follow the supplied BARR-C
  conventions. Python laptop capture/test tools supplement the C firmware.
- Use MQTT for the project's network messaging.
- Meet the issued Minimum-Bit Data Representation requirement: define ranges,
  signedness, resolution, invalid values, overflow/wrap and concurrent-update
  rules; serialize binary messages explicitly rather than sending C padding.
- Retain Week13 RAM/message-size comparisons and correctness/performance
  evidence, using equivalent functionality. Adding features to a template is
  not proof of representation savings.
- Retain applicable original reports, source/demonstration/test evidence and
  submission/declaration rules. Do not invent measurements, contributions or
  discussion agreements to complete them.

## 5. Removed and optional work

The confirmed revision permits omitting barcode decoding, barcode-directed
junction manoeuvres, ultrasonic/servo obstacle profiling and avoidance, and
hump-height measurement. Their earlier mission architecture is not the current
implementation target.

The IMU is optional. A grade increase is not guaranteed by the supplied wording;
it should not displace either required demonstration. A third IR/barcode sensor,
dashboard and onboard display are not required for the agreed build.

Encoder feedback, speed balancing/PID and diagnostic motor exercises are
implementation/support choices, not additional professor-confirmed end
demonstrations. Use them where needed for stable movement; do not use unreliable
encoder readings as qualified feedback or block the portable classifier core
on encoder qualification.

## 6. Engineering choices still to settle or validate

These are not fixed professor-issued acceptance targets:

- Track geometry, tape width, bends/branches, starting positions, valid target
  routes and track-end behaviour. The final layout must meaningfully demonstrate
  both required behaviours; a particular earlier mock diagram is not binding.
- Shortest segment length.5cm remains a candidate, not an accepted measured
  capability or mandatory minimum.
- Robot speed, recognition accuracy target, transition-error limits, lighting,
  sensor mounting/height and background. Define these from achievable measured
  conditions; do not substitute an undefined "normal scenario".
- Calibration/model selection, filtering and how the sensors contribute to
  tracking. Avoid filtering/reporting that hides the short transitions being
  demonstrated. Left/right sensors may see different surfaces while tracking.
- Behaviour for unknown/ambiguous readings, missing/lost target, stale sensor
  data, STOP, faults, connection loss and explicit restart. These are necessary
  design decisions for an integrated demonstration, not new quoted professor
  requirements. Existing tested STOP/freshness protections should be preserved.

Earlier working values such as0.10m/s or95% recognition in the old scope record
remain proposals; this draft does not adopt them as requirements or results.

## 7. Present progress and the remaining gap

| Area | Established within recorded scope | Still required for the final demonstrations |
|---|---|---|
| IR | Two responsive raw ADC channels; stationary capture/range-report tool prepared and host-tested. | Actual seven-tape/background data; per-sensor calibration and C classification; moving colour/transition validation. |
| Motion | Basic software motor requests; local renewal/expiry; MQTT left-wheel movement, ordinary rearm, GP21 stopping, broker-loss stop/recovery and fresh START observed. | Coordinated tracking, physical forward signs/combined-wheel qualification and any feedback quality needed by the selected controller. |
| Communications | Target/status/raw-IR messaging and scoped MQTT actuation/recovery checks observed. | Integrate recognised-colour events and target-path behaviour; final demo-network and combined-load validation. |
| Assessment evidence | Builds, host checks and scoped physical observations are recorded. | Final integrated demonstrations and comparable representation/RAM/message/performance evidence. |

Actual tape samples/background are not yet available. No real seven-class
calibration, recognition accuracy, shortest-segment performance or autonomous
colour following is claimed. Completed diagnostic checks need not be replayed
as separate mandatory robot features.

## 8. Next coding case after scope review

The proposed next case is a **portable C colour-classifier core** that can be
host-tested while tapes are unavailable. It accepts validated calibration data
and each sensor's ADC reading, returns one of the seven known class IDs or an
explicit unknown/ambiguous result, and validates bad/missing/overlapping model
data. This handling is an engineering proposal, not a separate professor-issued
algorithm requirement.

Use synthetic fixtures only for code correctness tests. Do not hardcode
imaginary real-tape thresholds or claim physical recognition from those tests.
Actual captures will populate/validate the model later. Tracking, motor feedback,
MQTT redesign and optional IMU work are separate cases, preserving the requested
one-case-at-a-time workflow.

## References used for current progress

- [Current work checklist](REMAINING_WORK.md).
- [Observed MQTT movement/GP21](../testing/integration/logs/2026-11-07-v0_8-pico-motion-result.md).
- [Observed broker-loss/recovery/fresh START](../testing/integration/logs/2026-11-07-v0_8-broker-loss-result.md).
- [Prepared tape capture tool](TAPE_CALIBRATION.md).
- [Capture-tool host evidence](../testing/integration/logs/2026-11-07-calibration-preparation.md).

Scope review complete. The authorized configurable C core is implemented and
verified within its portable-core scope; see [the API](COLOUR_CLASSIFIER_CORE.md).
Actual tape calibration and integrated demonstrations remain pending.
