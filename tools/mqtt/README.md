# Laptop MQTT broker

The v0.8 movement client is `drive_cli.py`: status, watch, target, clear,
stop and bounded drive. See [the car procedure](../../docs/MQTT_MOTION_V0_8.md)
and [v2 protocol](../../docs/MQTT_MOTION_PROTOCOL_V2.md). Host checks and both
firmware builds passed. The first car START was rejected at grant expiry;
the Python correction is tested and ready for the same step3 retest.

`robot_cli.py` remains the v0.5 no-drive target/status/raw-IR client. Its actual
recorded connection/recovery passes remain valid. Both clients use Python3.10+
and the existing Mosquitto tools, with no additional Python dependencies. The
text heartbeat examples below are historical v0.4 transport examples; use the
v0.8 procedure for the current test, not heartbeat messages.

Official Eclipse Mosquitto 2.1.2 Windows files are unpacked locally in .runtime/mosquitto-2.1.2. They were not installed as a Windows service. No Docker daemon is needed. The runtime, installer and active machine settings are ignored by Git; they remain inside this workspace.

From a PowerShell terminal in this firmware checkout:

```powershell
# Local laptop-only listener; leave the terminal open. Ctrl+C stops it.
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\mqtt\Start-Broker.ps1

# For the actual Pico test, stop the local instance first, then bind ONE
# current Wi-Fi LAN IPv4 address of this laptop (replace the example):
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\mqtt\Start-Broker.ps1 -BindAddress 192.168.1.199
```

The script changes neither Wi-Fi nor firewall settings. A LAN listener requires actual reachability from the Pico; add only a scoped broker/port/local-subnet firewall rule if Windows blocks it. Do not disable the firewall. The bring-up listener uses anonymous plain MQTT because the unmodified template has no authentication/TLS configuration: use it on the selected test LAN, close it when done and do not expose/port-forward it to the Internet. Campus authentication and client isolation must be verified before the demo.

In another terminal, subscribe or publish a non-retained test message:

```powershell
& '.\tools\mqtt\.runtime\mosquitto-2.1.2\mosquitto_sub.exe' -h 192.168.1.199 -t 'aai05/bringup/heartbeat' -v
& '.\tools\mqtt\.runtime\mosquitto-2.1.2\mosquitto_pub.exe' -h 192.168.1.199 -t 'aai05/bringup/heartbeat' -m 'hello from laptop'
```

These addresses are examples from the current laptop network, not permanent configuration. The initial Pico image must subscribe to that topic before a laptop message appears on its serial console. Printing received text is transport evidence, not a validated robot command.

## Local integration check

Stop any broker on 127.0.0.1:1883 first. Run:

```powershell
python .\tools\mqtt\test_broker.py --label UNIQUE_LABEL
```

The check starts its own loopback-only broker, publishes a unique retained test payload with MQTT3.1.1/QoS1, receives it using a separate subscriber, clears the retained test payload and stops its owned process in finally. It refuses an existing listener or evidence label, uses bounded timeouts and leaves logs/JSON in testing/integration/logs. Success proves localhost broker/client operation only.

## Provenance

- [Official Mosquitto download](https://mosquitto.org/download/): mosquitto-2.1.2-install-windows-x64.exe; SHA256 `58008ad7a22ada0b4073afa415801746e027c5f583e4fa52d0f4e9193b98d6aa`.
- Extracted with the official 7-Zip26.05 Linux tool after Ubuntu's older23.01 extractor reported zero files. The installer itself was not executed. Runtime licence/notice files remain alongside the binaries.
- To recover after deleting runtime, download that pinned official installer and unpack it with a current NSIS-capable 7-Zip into .runtime/mosquitto-2.1.2. Check that mosquitto.exe, mosquitto_pub.exe and mosquitto_sub.exe are present and version2.1.2 runs. Do not commit binaries or credentials.

The earlier v0.5 Pico exchanges passed. v0.8 movement and final demo-network operation remain separate checks.
