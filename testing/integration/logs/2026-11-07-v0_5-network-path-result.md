# Laptop Wi-Fi/network-path interruption result

Sources: coordinator-generated `network-path-20261107-125830.log`, direct VS Code terminal/Serial Monitor inspection, and local source confirming that USB reporting and radio/MQTT transport are independent. The original log is preserved unchanged; a UTF-8 copy is provided for review. File hashes, sample counts and boundary data are in `2026-11-07-v0_5-network-path-result.json`. No test command, power/network change, upload or source fix was performed by the agent during this inspection.

## Observed result

- Initial watch ACK: OK, targetcolour1, mutating seq1, tag9c5338b8, ready1, motion-capability none.
- Raw log contains127 telemetry samples:126 valid,1 explicitly stale,11 waiting-for-reconnection notices and no samples with a nonzero enqueue-drop flag. Drop0 is not an end-to-end packet-loss measurement.
- Last pre-gap sample: uptime94420ms, left878/right1890, valid, targetcolour1, telemetry seq111.
- First resumed sample: uptime188060ms, left868/right1898, valid, targetunset, telemetry seq0. The same running watch resumes without a logged exit/relaunch. Uptime increases across the gap.
- The93.64s sample-to-sample gap includes the interruption/operator downtime, not an isolated reconnect latency. All other recorded adjacent telemetry intervals are500ms.
- Later samples reach uptime211560ms/seq47, targetunset. The final STATUS visible in the terminal is OK, targetunset, mutating seq0, tag375b55c0, ready1, motion-capability none. This tag differs from9c5338b8. The final STATUS is outside the watch-only raw log.
- Serial Monitor retains continuous IR/encoder/status reports. Around reports at187980..188240ms it displays the following MQTT transition:

```text
comm: link unavailable; target unset; motor output disabled
mqtt: connected to 192.168.1.199
comm: command subscription ready; target unset; motor output disabled
mqtt: command subscription ready
```

The unavailable line is part of the connection-callback transition; it is not independently timestamped as the original outage onset. Earlier timeout/status-code output was not separately captured in this inspection. These lines establish restored broker connection and subscription readiness.

Serial reports show WIFI_LINK3/IP192.168.1.200 and PROBE/IDLE/pct0/DRIVE OFF during the inspected periods. Their continuation is expected: USB serial does not use the laptop Wi-Fi connection, and WIFI_LINK describes the Pico-to-router interface rather than broker reachability. A serial freeze is not a desired outage indication.

The single stale sample is at uptime82420ms/seq87, raw0/0; later samples are valid. Its cause is not established, and the earlier combined-load freshness/timing investigation remains relevant.

## Assessment and limits

**Network-path loss/restoration passes within the observed scope:** a selected target existed before loss, watch detected missing packets, the Pico remained running, MQTT reconnected/resubscribed, the target was cleared, a new tag was returned and telemetry resumed automatically. No firmware change or repeat of this successful case is required before progressing with motor development.

This does not establish actual Pico Wi-Fi association loss/rejoin; the Pico remained connected to the router in the inspected reports. That coverage remains for a later controlled access-point/demo-network validation. Physical motor behavior, tape recognition, precise failure-detection latency and full-load timing are not passed by this communication test.
