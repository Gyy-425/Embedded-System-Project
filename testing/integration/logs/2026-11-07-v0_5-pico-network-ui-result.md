# First observed Pico communication result

Source: direct read-only inspection of the coordinator's two existing VS Code integrated-terminal results and Serial Monitor on7 November2026. This is a transcription/assessment of visible retained output, not a complete saved raw serial or broker capture. No MQTT test command, motor request or firmware upload was issued during inspection.

## Observed address roles

| Address | Role | Observed evidence |
|---|---|---|
| 192.168.1.199:1883 | Laptop MQTT broker | Broker startup identifies this listener; laptop watch command uses this host |
| 192.168.1.200 | Pico W's own Wi-Fi interface | Serial `WIFI_IP=192.168.1.200`; broker reports incoming Pico connection from this address |

These addresses should differ because the laptop and Pico are separate devices. The reporter obtains WIFI_IP from the radio status snapshot; MQTT_BROKER_IP is separately passed to the MQTT client's destination parser. There is no evidence here of a broker-address mismatch requiring another rebuild.

## What the displayed output establishes

- Broker terminal: Mosquitto2.1.2 running on the selected listener; Pico client `aai05-picow-01` connects from192.168.1.200; a command-topic subscription request and repeated16-byte telemetry publications on `aai05/v1/robot01/telemetry` are displayed.
- Watch terminal: command targets `--host 192.168.1.199 watch`. Initial response shows `ACK result=OK target=unset seq=0 tag=9e5d3851 ready=1 motion-capability=none`. The client only prints this decoded ACK after its packet checks; together with the firmware's readiness gate, this supports the actual STATUS request/ACK round trip and command subscription readiness.
- Decoded IR telemetry is shown as valid, target unset, detected unknown, drop0 and motion-capability none. Two normalized visible samples: uptime159010ms, left926/right1858, seq305; uptime159510ms, left929/right1843, seq306. These adjacent samples are500ms apart. Later visible output reaches uptime184510ms/seq356. This is not a measured whole-session loss rate or timing-distribution result; drop0 describes synchronous enqueue status, not guaranteed network delivery.
- Serial Monitor: COM3 retained output begins at uptime47320ms, after networking is already up. Repeated reports show `NETWORK_MODE=COMMS_APP`, `WIFI_LINK=3`, `WIFI_IP=192.168.1.200`, `MOTOR_MODE=PROBE`, IDLE/pct0 and `DRIVE=OFF`, with responding IR fields. Local CYW43 headers define link3 as connected with an IP address. Physical wheel behavior was not observed through this screen inspection.

Assessment: basic actual Pico association/IP, MQTT connection/subscription, read-only status round trip and delivered readable raw IR telemetry are supported. This advances beyond the previous host-only results. Actual tape classification is not implemented or demonstrated; unknown is expected in this communication-only profile.

## End of retained session and pending work

The watch output ends with `Stopped.` and a PowerShell prompt. The serial tail shows `comm: link unavailable; target unset; motor output disabled`, `mqtt: connection status 256, retrying in 3s`, and `Closed the serial port COM3`. The local lwIP header defines status256 as MQTT_CONNECT_DISCONNECTED. The serial UI showed no selected port. The coordinator subsequently confirmed: `Yes—I stopped/disconnected them`. Record this as a deliberately ended session, not evidence of an unexpected firmware fault. The confirmation does not establish the exact disconnect ordering or a successful reconnect/recovery test.

No SET_TARGET/CLEAR_TARGET result or deliberate broker/network recovery cycle was observed. These remain pending, along with sustained-load/timing, motion and tape-dependent acceptance. Next communication checks are the selection/status/clear sequence and a controlled recovery test in [COMMS_TEST_V0_5.md](../../../docs/COMMS_TEST_V0_5.md).

An extra empty terminal split was inadvertently opened while selecting a result tab and was closed again; the two original terminals and their output were retained. No project source or private configuration was changed by the UI inspection.
