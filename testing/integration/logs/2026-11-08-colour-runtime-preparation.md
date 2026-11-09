# Colour runtime/reporting preparation

Started 8 November, completed 9 November 2026 (Singapore). User has no actual
seven tape samples/background yet and selected 'prepare runtime integration'.
This is offline preparation, not a physical recognition result.

## Implemented

- IR owner classifies every acquisition using a privately copied boot model;
  both per-sensor results remain distinct. No synthetic deployment ranges.
- 16-entry transition ring, sample times/raw values, wrapping transition
  sequence and saturating overflow total; latest and queue use the IR mutex.
- Nonblocking readers; atomic reconnect-begin and event-or-snapshot selection.
  Network owner retains one rejected publication and bounds work per poll.
- Separate23-byte colour v1 stream and read-only watch. Existing motion v2
  commands, raw telemetry, motor writer and STOP/expiry/rearm paths retained.
- Serial class/reason results, explicit uncalibrated default and deployment
  rejection of partial/overlapping/invalid models. No autonomous path following.

## Actual verification

Canonical command, from the firmware directory in Ubuntu/WSL:

```sh
python3 tools/check_encoder_pio.py --label 2026-11-08-colour-runtime --sdk '/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0' --debug-python '/mnt/c/Users/Guo YouYan/AppData/Local/Programs/Python/Python312/python.exe' --firmware --colour-runtime
```

[Machine-readable result](2026-11-08-colour-runtime-result.json): **PASS,
73 stages, zero compiler warnings**. 70 WSL Python tests plus 6 Windows Tcl tests
passed (76 total); C stages include strict ASan/UBSan core, runtime, actual IR
adapter, reporting service and wire tests. The C-produced golden colour frame
matches Python decoding. Existing PIO/GPIO, motion, MQTT protocol/service,
ingress, CLI, capture and other host regressions passed. All four compatibility
firmware builds passed and 46 earlier UF2 hashes are unchanged.

RED was observed before implementations: the runtime stub failed the expected
missing-model assertion; the adapter test failed on absent APIs; wire/service
stubs failed the expected frame/begin assertions; Python valid-frame tests
failed against the unimplemented decoder. After implementation all are GREEN.
No failed hardware run or real calibration is inferred from these fixtures.

| Candidate | text | data | bss | SHA256 |
|---|---:|---:|---:|---|
|PIO normal|343628|100|82980|d333263c3e1e6ab56bb0744e97e630ec521fb6467e524f75626f9eab6a7d7291|
|PIO TEST|344320|100|82980|abcd06caba0653917d837f2247342d0adada062434779489aabddadefdc5b0a7|
|GPIO normal|342872|100|82952|f2a06b48976a7b35ce630f3975f73c2d59e7d5e466b124729ee677ec5d90ceaa|
|GPIO TEST|343428|100|82952|7a53caabe80d6173ce974c1448dc0ecf9fa4f3d16e4fb23b1b93cc212413dfb1|

Candidates are under output/firmware/encoder_pio_2026-11-08-colour-runtime/.
The future car image is AAI05_Colour_Runtime_v0_10_PIO_REMOTE.uf2.
No firmware was uploaded and no broker, network or hardware test was started.
Keep the tested installed v0.9 image until the next explicit upload/check.
PIO normal bss increased 364 bytes versus the previous reviewed candidate;
these new runtime/reporting facilities are not evidence of minimum-bit savings.

## Independent review

Read-only reviewer colour_runtime_review found **no Critical or Important
defect**, inspecting actual code/tests, ownership, queue loss, freshness/wrap,
reconnect/retry ordering, wire validation, read-only watch and motion hook
ordering. The reviewer did not duplicate the running suite or touch the car.

Deferred minor: colour watch reuses Bridge, which resolves both mosquitto_sub
and mosquitto_pub even though this watch never publishes. A subscriber-only
installation would fail unnecessarily. The established laptop bundle contains
both tools; lazy publisher resolution is a future usability improvement.

Explicit limitation: delayed snapshot retries retain results from copy time,
with the original sample timestamp. Receipt is not proof of current sensor
freshness. These reports are diagnostic observations, never Motion inputs;
actual combined sampling/reporting latency remains to be measured.

## Remaining

Real tape/background calibration and separability; on-car v0.10 report check;
moving sequence/short-segment validation; target-following controller;
Wi-Fi-active scheduling/WCET/mutex/stack qualification and final acceptance.
No guessed colours, thresholds, accuracy, segment length or stopping metrics.
