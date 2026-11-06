# Motor-inactive Wi-Fi/MQTT transport probe v0.4

The v0.5 target-selection/ACK/raw-IR application supersedes this v0.4 text-heartbeat procedure for the next session. Follow [COMMS_TEST_V0_5.md](COMMS_TEST_V0_5.md); retain this guide as transport-preparation history. No actual Pico Wi-Fi/MQTT pass has been recorded.

This milestone is prioritised before later colour following. The coordinator now selects CONNECTED_MOTOR_TEST_V0_3.md as the next motor check; the winding-disconnected method is an alternative. Motor winding wires may remain connected for networking, using only the v0.4 NO_DRIVE profile, which retains both force-high motor overrides in software. It uses the existing course network/MQTT implementation unchanged.

## What this stage demonstrates

1. Pico associates and obtains a LAN IP.
2. Pico connects/subscribes to the laptop MQTT broker.
3. Heartbeats published by the Pico are visible on the laptop and received back by the Pico.
4. A non-retained laptop text message appears in Pico serial RX.

It does not yet implement validated robot commands, target-colour state, IR MQTT telemetry, colour classification or movement. Those are the next application step after transport works. A compiled image, RADIO_READY=1 or laptop loopback check alone does not pass this stage.

## Network settings

Local ignored config/wifi_secrets.h contains the coordinator's Wi-Fi password, a unique Pico client ID, the selected broker IPv4 and topic `aai05/bringup/heartbeat`. The password was entered locally, not in chat. Neither this header nor credential-containing compiled images should be uploaded to GitHub. The runner checks configuration format without echoing secrets before invoking the compiler. Use normal quoted strings; escape embedded backslashes and quotes.

Scan on 2026-11-06 confirmed the selected SINGTEL-A9C0 network advertises 2.4GHz/channel1, reported WPA3-Personal. The existing firmware requests WPA2 mixed PSK; transition compatibility is not established by the scan. Try association with these settings first. If the log reports authentication failure, use a known 2.4GHz WPA2-compatible test hotspot or an appropriate router SSID; do not change security settings automatically. The laptop may remain on another band if both devices can communicate on the same LAN.

The current laptop WLAN IPv4 was 192.168.1.196/24. Recheck it whenever networks change and update MQTT_BROKER_IP/rebuild if it changes. For the campus demo, verify actual authentication and device-to-laptop access in advance; campus access is not established by a home test. A browser-login/enterprise network or client isolation may need an approved IoT/test network or hotspot.

## Start the laptop broker

From a PowerShell terminal in this checkout:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\mqtt\Start-Broker.ps1 -BindAddress 192.168.1.196
```

Leave it open; Ctrl+C stops the broker. This binds only that chosen laptop address. The local check script's temporary loopback broker is already stopped. If a process already uses port1883, investigate it rather than stopping an unknown process.

If Windows blocks the actual Pico connection, add only the required program/port/local-subnet inbound rule for this broker; do not disable the firewall. No such rule is presumed present from localhost success. Use this anonymous plaintext bring-up listener only on the selected test LAN, close it after testing, and do not forward it through the router.

Open another PowerShell terminal and subscribe:

```powershell
& '.\tools\mqtt\.runtime\mosquitto-2.1.2\mosquitto_sub.exe' -h 192.168.1.196 -t 'aai05/bringup/heartbeat' -v
```

## Flash and inspect

1. Keep bank disconnected for upload, wheels raised and motor wires secure. Winding leads may remain connected. Close Serial Monitor; switch OFF and unplug the Pico USB.
2. Hold BOOTSEL while reconnecting the laptop USB data cable, then release when RPI-RP2 appears. Copy **output/firmware/AAI05_Network_Probe_v0_4_NO_DRIVE.uf2**, normal without _TEST, onto the drive.
3. Open the actual Pico COM port at115200, 8N1, no flow control. Do not press GP20; no motor action is required for this network test.
4. Confirm the AAI05 NETWORK PROBE v0.4 banner, MOTOR_MODE=PROBE, NETWORK_MODE=TRANSPORT_PROBE and DRIVE=OFF. Look for `wifi: link up, ip=...`, then `mqtt: connected`, `mqtt: subscribed` and matching heartbeat TX/RX every approximately5 seconds. The repeating WIFI_LINK/WIFI_IP fields are snapshots; use actual link and MQTT event logs together.
5. Check that IR reads remain valid and there is no new encoder/capture error while network activity runs. Neither wheel should be driven.
6. Send a laptop message in another terminal:

```powershell
& '.\tools\mqtt\.runtime\mosquitto-2.1.2\mosquitto_pub.exe' -h 192.168.1.196 -t 'aai05/bringup/heartbeat' -m 'hello from laptop'
```

Expect that exact printable text in the Pico's `mqtt: RX` line. This is transport only, not a motor command. Do not send control text to USB Serial Monitor; this port's USB RX is discarded.

Save startup through several heartbeat cycles and the laptop-message RX. If association/broker connection fails, send the event/error lines without posting the private password/header. Do not report a connection pass from repeated RADIO_READY alone.

After the initial successful run, stop/restart only the owned broker and observe reconnect/subscription/heartbeat restoration. A later network interruption test is also required. Do not mark these recovery tests passed before actual records exist.

## Rebuild

In Ubuntu from this checkout:

```sh
python3 tools/build_wsl.py --sdk "/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0" --network-probe --label UNIQUE_LABEL
```

The profile is SMP1/USB/radio, NET=lwip, MQTT=1, MOTOR_PROBE=1. It has distinct artifacts and runs motor/encoder host regressions plus private-config validation tests. The shared BSP/kernel/network code is unchanged. Actual maximum interrupt latency, power stability, seven-tape recognition and minimum-bit comparisons remain separate validation work.

Verified preparation on 2026-11-06: all17 build/host steps passed without reported warnings, including seven sanitized C executables, private-config unit checks and both normal/TEST network firmware builds. Normal UF2 SHA256 `e9427de33eb385c6b9d49a86647022126bd9be49bfdf30ee507737ee735abe99`, 671744 bytes; build evidence in testing/integration/logs/2026-11-06-v0_4-network-final-build-result.json. A subsequent host-only review correction rejects JSON-only escapes that are not valid C escapes; all four config test groups and the actual private-file preflight passed in 2026-11-06-network-config-reviewed-green.log. No target C changed after the built image. The broker's real localhost round trip passed in 2026-11-06-broker-local-verified.json, with its owned process stopped. These are software/host results, not a Pico connection pass.
