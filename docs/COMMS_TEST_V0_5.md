# v0.5 communication test guide

Current progress7 November: step1 host checks passed (25 stages,8 communication/client tests, both builds). Step2 broker address is now updated to the observed laptop address192.168.1.199, and both firmware variants rebuilt successfully. [Current image hashes and verification](../testing/integration/logs/2026-11-07-v0_5-broker199-rebuild.md). Continue at step3, then flash the normal image in step4. Recheck the address if changing networks. Earlier preparation-time deferral statements below are historical.

Prepared for the next session. No car, tape, broker or protocol functional test was performed while developing this version. The normal and TEST firmware are compile variants; a compiled TEST image is not an executed test. Use the normal **AAI05_Comms_v0_5_NO_DRIVE.uf2** for this procedure. The prior v0.3.2 motor investigation is separate and remains unresolved.

## 1. Run the prepared host checks first

This step needs only the laptop. It checks packet boundaries, CRC, duplicate/conflicting/stale commands, sequence wrap, tag mismatch, explicit ADC packing, C→Python decoding and CLI response matching. It does not prove network or motor operation.

Open Ubuntu in the firmware repository:

```sh
cd "/mnt/c/Users/Guo YouYan/Desktop/INF2004-main/AAI05_Firmware"
python3 tools/build_wsl.py --sdk "/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0" --comms-app --label UNIQUE_COMMS_TEST_LABEL
```

Replace the label with a new descriptive label. Omit `--build-only` to execute the host checks. The runner captures the C packet vectors and sets `COMM_VECTOR_FILE` for the Python cross-language check. Confirm each host check and both compile variants pass; if one fails, fix it before relying on this firmware. Save the generated JSON/logs. Today's build-only result deliberately lists functional tests as DEFERRED.

## 2. Confirm the laptop broker address

Keep laptop and Pico on a mutually reachable LAN. The Pico W requires2.4GHz Wi-Fi; the saved starter authentication configuration must also work on that network. A laptop on another band is fine if the router bridges both clients. Campus authentication/client isolation still requires checking before the demonstration.

In PowerShell, inspect the connected Wi-Fi adapter:

```powershell
Get-NetIPConfiguration
```

Use its current LAN IPv4 below. Old addresses in the v0.4 guide are historical examples. Locally check `config\wifi_secrets.h`: `MQTT_BROKER_IP` must match this address, and `MQTT_BROKER_PORT` must match1883. Keep the Wi-Fi password in that ignored file; do not paste the file into chat or commit it. If the address/network configuration changes, rebuild using step1 before flashing.

## 3. Start the broker

In PowerShell at `C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware`, replace the example address:

```powershell
$brokerIP = 'REPLACE_WITH_LAPTOP_LAN_IPV4'
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\mqtt\Start-Broker.ps1 -BindAddress $brokerIP
```

Leave that terminal open. Ctrl+C stops the owned broker. If port1883 is already in use, identify its owner rather than terminating an unrelated process. If Windows prompts for this broker, permit the selected private test network as appropriate; if traffic is blocked, diagnose a scoped broker rule rather than disabling the firewall. The existing listener is anonymous/plain MQTT for the selected test LAN; close it afterwards and do not expose it through router port forwarding.

## 4. Flash the normal communication image

1. Close Serial Monitor. Leave the bank disconnected for upload, turn the Robo Pico OFF and unplug Pico USB. Keep both drive wheels clear on a stable support; no motor buttons are needed.
2. Hold BOOTSEL while plugging the Pico W data USB into the laptop. Release after `RPI-RP2` appears.
3. Copy `output\firmware\AAI05_Comms_v0_5_NO_DRIVE.uf2` onto that drive. It will disappear as the Pico restarts.
4. Open the actual Pico COM port (previously COM3, recheck) at115200,8N1, no flow control. If the board switch is needed for the connected peripherals, switch it ON. No bank or powered motion is required for this communication test.
5. Confirm the v0.5 COMMS/no-motor-output banner, `MOTOR_MODE=PROBE`, `NETWORK_MODE=COMMS_APP` and `DRIVE=OFF`. Look for actual `wifi: link up, ip=...`, `mqtt: connected` and `mqtt: command subscription ready`. `RADIO_READY=1` alone does not establish any of these.

Do not press GP20 or the hardware motor buttons. No servo/IMU/barcode connection is needed. Commands below use MQTT through a separate laptop terminal; USB Serial Monitor input is discarded by this port.

## 5. Receive readable raw IR data

Open another PowerShell terminal at the firmware repository:

```powershell
$brokerIP = 'REPLACE_WITH_LAPTOP_LAN_IPV4'
python .\tools\mqtt\robot_cli.py --host $brokerIP watch
```

The client uses the existing local Mosquitto executables and Python3.10+; it installs no Python package. Expect an actual ready STATUS ACK, then nominal500ms raw IR lines. Readings should be0..4095 with quality flags, target `unset`, detected `unknown` and `motion-capability=none`. These are expected results to check, not measurements already obtained. Move paper beneath each sensor if useful; tape is unnecessary for raw telemetry. Changing IR values must not be reported as successful seven-colour recognition.

If no ready ACK arrives, capture the serial association/MQTT lines and broker error. A running laptop broker without a Pico ACK is not a communication pass. Stop `watch` with Ctrl+C; its own subscriber child is cleaned up.

## 6. Select and clear a target

Keep `watch` open if desired. In one additional controlling terminal, use the same broker address:

```powershell
$brokerIP = 'REPLACE_WITH_LAPTOP_LAN_IPV4'
python .\tools\mqtt\robot_cli.py --host $brokerIP status
python .\tools\mqtt\robot_cli.py --host $brokerIP target colour1
python .\tools\mqtt\robot_cli.py --host $brokerIP status
python .\tools\mqtt\robot_cli.py --host $brokerIP clear
```

Check that SET ACK confirms target `colour1` (ID2), the next STATUS/telemetry agrees, and CLEAR restores `unset`. Only one controlling command at a time is supported. Names `colour1`..`colour5` are placeholders for tapes not yet calibrated. The robot must stay undriven; selecting a colour stores data only.

The CLI first learns the device tag and last mutating sequence. It retries an identical mutation if a response is missing, accepts OK/DUPLICATE only with matching sequence/target, and reports negative/mismatched-session responses as errors. Verify bad input such as `target unknown` is rejected locally. Firmware malformed-packet and sequence checks are covered by the prepared host cases; fragmented MQTT delivery and duplicate delivery should be observed in later integration tests before claiming those paths passed on the Pico.

## 7. Recovery and saved evidence

After the first successful exchange, stop and restart this broker while leaving `watch` running. Observe serial disconnect/retry, a new successful subscription, restored telemetry and an unset target. Select again only after readiness. Then briefly interrupt/restore the chosen test network and check recovery. Disconnection detection can be delayed by TCP/keepalive; measure actual timing rather than assuming an instant reset. If watch exits because the broker client terminated, rerun it and record that outcome.

Save a short record containing firmware filename/SHA, host-check result, actual Pico IP, ready ACK, several telemetry lines, SET/STATUS/CLEAR results and recovery observations. Record any sensor errors, resets, unexpected motor movement or missing messages as failures to investigate. Do not post passwords. No tape classification, line following, powered PWM validation, whole-project memory saving or final acceptance result is established by this test.

Detailed wire fields and scope limits: [COMM_PROTOCOL_V1.md](COMM_PROTOCOL_V1.md).
