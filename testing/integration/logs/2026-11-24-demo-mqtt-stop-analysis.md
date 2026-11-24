# Demo failure: repeated stops after missing MQTT acknowledgements

**Status: observed failure; root cause not proven.** No serial or client log from the demo was saved, so this note can only state what was observed, what the code shows, and what has changed since.

## Observation (reported by the team, not logged)

During the live demo of `AAI05_MQTT_Follow_Distance_v1.uf2`, the car stopped repeatedly. Some runs showed a missing MQTT acknowledgement, followed by the route report reason `MQTT_STOP`.

- **Image:** `output/firmware/follow_mqtt_demo_hotspot_ir700_20261123/`.
- **Source:** a fresh build of commit `3137627` is byte-identical to this image.

## What the code shows

The stop chain is consistent with the committed source, which is the client as it was on 24 Nov 2026 before the fix:

1. **The keepalive has a short window.** In menu mode the laptop renews the run with a KEEPALIVE about every 0.2 s, allows 0.5 s for the matching ACK, and resends the same frame every 0.2 s within that window (`tools/mqtt/drive_cli.py`, `request()`).
2. **A single lost ACK is recovered.** If the first ACK is lost, the resend reaches the Pico as a duplicate. The Pico answers DUPLICATE (`app_program/subsystem3/drive_control.c`), and the client accepts that answer.
3. **Repeated losses stop the run.** If no ACK arrives within 0.5 s (about three consecutive losses), the client raises "No matching device ACK", prints "Run renewal ended", and sends STOP itself. That deliberate stop is what the firmware reports as `MQTT_STOP` (`FOLLOW_REMOTE_STOP`, `black_follow_owner.c`).

So `MQTT_STOP` after a missing ACK is the designed safe reaction. The open question is why several ACKs in a row did not arrive.

## Plausible firmware cause (not proven)

- **Each ACK was published once.** On failure, `mqtt_drive_app.c` only set a flag; it did not retry.
- **lwIP ran at its defaults:** 4 requests in flight and a 256-byte output buffer (`lwipopts.h` did not override them).
- **Several publishes compete for those 4 slots** every 250 ms:
  - telemetry and the route report, both QoS 0, each held until TCP is acknowledged;
  - every QoS 1 ACK, held until the broker's PUBACK.
- **A slow link fills them.** With phone-hotspot latency, the 4 slots fill and `mqtt_publish()` returns `ERR_MEM`, so the ACK is lost.

Other causes are not excluded:
- loss between the broker and the laptop;
- laptop-side publish latency: before commit `232aaa3`, the client started a new `mosquitto_pub` process for every publish;
- Wi-Fi power-save latency.

## Changes made (24 Nov 2026, host-tested only)

| Change | Where | Test |
|---|---|---|
| A refused ACK is kept and retried on every network poll, ahead of telemetry. Only the newest ACK is kept. | `app_program/subsystem3/mqtt_drive_app.c` | `test_refused_ack_is_retried_before_telemetry` (`testing/host/test_drive_service.c`) |
| lwIP MQTT queue raised to 8 requests and a 768-byte output buffer | `lib/liblwip/sysdepend/pico_rp2040/lwipopts.h` | Firmware build |
| Publish-failure counters printed in a serial `NET[...]` line | `mqtt_drive_app.c`, `subsystem3.c` | Firmware build |
| The client accepts matching running telemetry as proof that a keepalive was accepted when its ACK is lost | `tools/mqtt/drive_cli.py` | `test_menu_keeps_running_when_telemetry_proves_keepalive_was_accepted`, `test_menu_stops_when_telemetry_cannot_confirm_keepalive` |

Safety is unchanged: the Pico still stops within the 100 ms motor-permission limit and the 750 ms drive lease if renewals really stop.

## What to capture on the next run

1. Save the full USB serial log, including the `NET[ack_fail=… ack_retry_ok=… …]` lines, and the full client output.
2. **If the cause was the lwIP queue,** `NET` should show `ack_fail` > 0 with `ack_retry_ok` keeping pace, and no `MQTT_STOP`.
3. **If `MQTT_STOP` still occurs with `ack_fail=0`,** the loss is outside the Pico's publish path: the broker, the laptop or the Wi-Fi link.
4. Record the result in `testing/system/TEST_LOG.md` (SYS-05).
