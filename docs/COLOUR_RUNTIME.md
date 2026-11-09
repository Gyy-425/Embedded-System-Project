# IR colour runtime and reporting

This case connects the configurable classifier to the IR owner and reporting.
No tape readings have been captured. The shipped boot model deliberately has
no ranges, so valid readings report unknown/UNCALIBRATED. Host fixtures are
synthetic and are not linked as robot calibration. Target following is not
implemented by this case.

## Ownership and behaviour

`subsystem1` acquires both ADC channels, classifies each acquisition and
publishes raw/latest colour state under its existing mutex. The default task
delay is 10 ms; this is not a measured 10 ms sampling guarantee. Each sensor has
its own result; different colours under left/right are allowed. There is no
vote, nearest-colour guess or temporal debounce.

The first pair of results and each subsequent change in either class/reason
enters a 16-element ring. An intermediate colour observed in just one acquired
sample is retained for reporting. This cannot detect a segment missed between
samples. The ring discards the oldest on overflow, retains the newest and
increments a saturating uint32 drop total. A wrapping uint32 transition
sequence makes gaps visible. Ordinary unchanged raw readings update latest
without creating an event. No loss-free arbitrary-rate/downtime claim is made.

The IR mutex protects model/readings/queue across cores. Models are copied at
boot and never edited while tasks run. Network/serial readers use nonblocking
locks and copy data; no MQTT call occurs while holding the IR mutex. A new
network session atomically clears pre-session events and takes an initial
snapshot. The network task then drains at most one event per service poll,
retains one pending report if publication is rejected and emits idle snapshots
every 1000 ms. A link change discards its pending report. Colour work follows
existing motion/lease processing and cannot submit motor commands.

Latest snapshots recheck sample freshness when copied. Queued transitions are
historical, with the result and timestamp at acquisition. Delivery/retries can
be delayed; receipt time is not sample time or proof that the sensor is live.
The network is QoS0/non-retained; the counter measures IR ring overflow, not
all possible network/client loss. The watch also identifies sequence gaps.

## Boot calibration

`config/colour_calibration_config.h` contains the public, currently absent
model. It is separate from ignored Wi-Fi credentials. Keep it absent until
labelled readings from both installed sensors justify all 14 ranges. Use
[stationary capture](TAPE_CALIBRATION.md), inspect separability and independent
validation, then encode each measured inclusive range with the documented
[classifier format](COLOUR_CLASSIFIER_CORE.md). Both complete disjoint sensor
models are required for deployment; partial, overlapping, reserved-bit,
reversed-range or zero-age configurations report INVALID_MODEL rather than
guessing. An entirely absent model with a valid age reports UNCALIBRATED.
Changing the model requires rebuild/reboot. Live calibration import and flash
persistence are not part of this case.

Default max_age_ms is 100 ms, an engineering setting. It uses uint32 subtraction
across clock wrap and is not multi-wrap replay protection. Invalid acquisitions
produce unknown/INVALID_SAMPLE; unexpected raw values above 4095 are replaced
by a zero placeholder and both channels marked invalid. A returned zero with
MATCH remains legitimate calibrated data. See the core guide for reason IDs
and precedence; unavailable model/sample states do not establish freshness.

## Command-prompt colour watch

9 November update: laptop WLAN changed to192.168.1.210. The configuration-only
normal PIO rebuild is output/firmware/AAI05_Colour_Runtime_v0_10_BROKER210.uf2.
Use [the stationary car procedure](COLOUR_RUNTIME_CAR_CHECK.md) for upload,
broker setup, expected output and evidence collection. Older206 candidates
remain preserved for their recorded network configuration.

After a deliberately scheduled upload of the new colour-runtime candidate,
with the normal laptop broker already running, from PowerShell:

```powershell
Set-Location -LiteralPath 'C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware'
python .\tools\mqtt\colour_cli.py --host 192.168.1.210 watch
```

Use the laptop's current broker address and the matching firmware network
configuration. The Pico has its own address. The watch uses existing Mosquitto
tools, sends no robot commands and cannot move/stop the robot. Ctrl+C closes
only this subscriber. Without tapes/model, expected output includes:

```text
SNAPSHOT sample_t=...ms seq=... L=unknown/UNCALIBRATED AO=... R=unknown/UNCALIBRATED AO=... dropped=... session=...
```

`CHANGE` means a new pair of class/reason results; `SNAPSHOT` is a current-state
copy and need not be a new colour. IDs0/1 mean black/white; colour1..5 are
project class labels, not established visible names. The watch announces new
sessions, missing sequence entries, IR queue overflow and waiting for reports.
A recovered session starts a new continuity interval; it cannot reconstruct
the disconnected route. Repeated snapshots do not count as colour changes.

Serial output adds `COLOUR[read=1 L=class/reason R=class/reason t=... drop=...]`.
`read=0` means no available colour copy; ignore that bracket's placeholder
values. Its sample timestamp can differ from the separately copied raw IR
snapshot. Existing v2 status/drive/move/capture tools retain their wire format;
the old raw v2 watch still prints its own `detected=unknown`. Use the separate
colour watch for classifier results.

## Binary report v1

Topic: `aai05/colour/v1/robot01/telemetry`, no inbound command topic.

| Byte(s) | Field | Range/encoding |
|---|---|---|
|0|Kind/version|D0 transition, D1 snapshot; others rejected|
|1..4|MQTT connection session|uint32 big-endian; zero rejected|
|5..8|Transition sequence|uint32 modulo;0 valid after wrap|
|9..12|Sample time,ms|uint32 modulo uptime|
|13..16|IR ring discarded events|uint32 saturating at0xFFFFFFFF|
|17..19|Left/right ADC|two unsigned12-bit values; L high12 then R low12|
|20|Left result|class bits2..0; reason bits5..3; bits7..6 zero|
|21|Right result|same; MATCH requires class0..6, other reasons require7|
|22|CRC8|existing polynomial0x07, initial0, bytes0..21|

Frame length 23 bytes. No compiler struct/bitfield is transmitted. Strict
decoding rejects wrong lengths, CRC, kind, session and contradictory results.
Native structs may contain padding; the ring/state added here consumes RAM.
This new diagnostic packet is not evidence of network-message savings or
globally minimum representation. Equivalent-function Week13 comparisons and
measured final timing remain separate work.

## Offline verification / next physical work

```sh
python3 tools/check_encoder_pio.py --label NEW_UNIQUE_LABEL --sdk '/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0' --debug-python '/mnt/c/Users/Guo YouYan/AppData/Local/Programs/Python/Python312/python.exe' --firmware --colour-runtime
```

This runs strict/sanitized C, real IR-adapter/service host tests, C/Python wire
vectors, the full host suite and PIO/GPIO normal/TEST builds in a new candidate
directory. Earlier UF2 hashes are verified unchanged; it never flashes or
contacts the car. Use the PIO normal candidate for the later colour runtime
check, not a GPIO compatibility or TEST image. Keep the tested installed
v0.9 image until that upload/check is requested.

Actual seven-class separability, short-segment moving detection, recognition
accuracy and lighting/height sensitivity require real tape/background data.
Combined IR/motor/Wi-Fi timing, mutex hold times, stack use and final stopping
behaviour remain measured integration work. Do not replay passed motor
direction/straight/curve tests just to use the capture tool.
