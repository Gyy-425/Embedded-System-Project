# System — test log

Tests of the complete product against its requirements. Run the full set before every demo and before hand-in. See `testing/README.md` for how to run tests and record results.

**Requirements basis:** the approved revised scope (`docs/PROJECT_SCOPE_REVIEW.md`) and Change Requirement 1 (minimum-bit data representation). The Tribunal 2 requirement ("respond safely and predictably to failures or invalid data") is included as REQ-SAFE.

| REQ | Requirement |
|---|---|
| REQ-A | Report successive tape colours while moving, in clean command-prompt output |
| REQ-B | Receive a target colour over Wi-Fi and follow that colour's path |
| REQ-F | Follow a colour lane between black borders using the IR sensors |
| REQ-D | Report encoder-based distance per colour section |
| REQ-M | MQTT start, renewal and stop of a run |
| REQ-SAFE | Stop safely and predictably on stale/invalid sensor data, lost commands or faults |
| REQ-CR1 | Minimum-bit data representation, with RAM and message-size comparison |

## Test cases

Test IDs never change.

| ID | What is tested | Requirement | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| SYS-01 | Host suites for the integrated MQTT follow image | REQ-F, REQ-M, REQ-SAFE, REQ-D | auto | `tools/check_encoder_pio.py`, `tools/check_rgb_diag.py --home-track`, `tools/check_black_follow.py` with the v0.87 flag set (see `testing/integration/logs/*-result.json`) | Every stage PASS, 0 warnings |
| SYS-02 | Firmware build reproduces the demo image | all | auto | `tools/build_follow_mqtt.py --label <new>`, then compare with the demo UF2 | Build PASS with 0 warnings; same source gives an identical image |
| SYS-03 | Whole-route colour follow on the home track | REQ-F, REQ-A | manual | MQTT menu `f` on the start lane; save serial and client output | Car completes the route between the borders; client run report lists each colour section in order |
| SYS-04 | Target-lane follow selected over Wi-Fi | REQ-B | manual + auto | Host: `follow-target-owner` stage and `test_follow_selects_one_supported_lane`. Car: place on blue, type `fb`; repeat `fr`/`fy` on red/yellow; also type `fr` while on blue | Car follows only the chosen lane and stops with `TARGET_END` at the next lane; start off-lane is refused with `TARGET_MISMATCH`, motors never on |
| SYS-05 | MQTT run renewal over the demo network | REQ-M, REQ-SAFE | manual | Run `f` for 30 s or more on the demo hotspot; save serial (`NET[...]`) and client output | No `MQTT_STOP` without a user STOP; if `ack_fail` > 0 then `ack_retry_ok` keeps pace |
| SYS-06 | Distance accuracy per colour | REQ-D | manual | `l` straight runs over a tape-measured length (3 or more runs); then `f` runs and read the `r` report | Reported length within the stated tolerance of the measured length; record the error % |
| SYS-07 | Safe stop on STOP, GP21, broker loss and stale sensors | REQ-SAFE | manual + auto | Host: lease, expiry and stale-data stages in SYS-01. Car: press GP21; stop the broker mid-run; cover the RGB sensor | Motors stop within the lease (≤ 100 ms motor permission, ≤ 750 ms drive lease); no automatic restart |
| SYS-08 | Minimum-bit representation evidence | REQ-CR1 | auto | `docs/DATA_REPRESENTATION.md`: ARM struct sizes before/after, image RAM, message sizes | Tables complete; host suites still PASS |

## Results

Add a new row for every run. Never edit or delete old rows.

| Date | Test ID | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| 2026-11-17 to 2026-11-23 | SYS-03 | v0.82–v0.87 images | team | FAIL | `testing/integration/logs/2026-11-24-track-runs-v082-v087/README.md` | v0.87: blue→red join in 3/3 runs, but no run completed the red corner; all ended `JOIN_TIMEOUT` with motors stopped. Derived extracts only: the raw logs were overwritten |
| Demo day (date not recorded) | SYS-05 | `3137627` (demo image, byte-identical build) | team | FAIL | `testing/integration/logs/2026-11-24-demo-mqtt-stop-analysis.md` | Repeated stops; missing ACKs followed by `MQTT_STOP`. No log saved; cause not proven |
| 2026-11-24 | SYS-02 | `3137627` | Claude (review) | PASS | `output/firmware/review_follow_mqtt_20261124/build.log` (not tracked) | 0 warnings; UF2 byte-identical to the demo image |
| 2026-11-24 | SYS-01 | `4f12c22` | Claude | PASS | `testing/integration/logs/req-20261124-suite-result.json`, `req-20261124-rgb-result.json`, `req-20261124-follow-result.json` | 77 suite stages, RGB stages, full follow suite incl. `follow-target-owner`; 107 Python tests OK (5 skipped). Host only |
| 2026-11-24 | SYS-02 | `4f12c22` | Claude | PASS | `output/firmware/follow_mqtt_v2_target_20261124/build.log` (not tracked) | New image builds with 0 warnings; bss 84 596 B (was 84 608 B). Not yet flashed |
| 2026-11-24 | SYS-04 | `4f12c22` | Claude | PASS (host) | `follow-target-owner` stage; `test_follow_selects_one_supported_lane`; client lane tests | Refused start, lane end after 2 samples, stray frame ignored, whole route unchanged. **Car run pending** |
| 2026-11-24 | SYS-08 | `4f12c22` | Claude | PASS | `docs/DATA_REPRESENTATION.md` | Follow status 92→68 B, snapshot 324→300 B; binary messages 6.7–11.4× smaller than equivalent JSON |
