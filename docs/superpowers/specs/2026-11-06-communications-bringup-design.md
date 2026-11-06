# Motor isolation and communication-first bring-up

The coordinator requests the next motor/encoder isolation check and prioritises working Wi-Fi/MQTT before completing later motion tests. A broker should run on this laptop. This continues the authorised inline solo workflow; earlier contribution reports remain historical.

## Approach and scope

Use the preserved normal v0.3 image once with all four motor winding leads disconnected from the carrier terminals. Keep encoder power/A/B and IR wiring intact. Unlike the passed coast-only probe, this exercises normal output release while excluding connected motor windings. No new motor firmware is needed and no original connected-motor retry is authorised by this procedure.

Prioritise a separate motor-inactive network milestone using the template's existing NET=lwip/MQTT=1 path. First show association/IP, broker connection, heartbeat TX/RX and laptop-originated text received on the Pico. Then add validated project commands and actual IR/encoder telemetry through the service-owned network path. A demo heartbeat alone is not completion of application messaging.

For the laptop, use official Windows Mosquitto binaries unpacked under tools/mqtt/.runtime, without a Windows service or Docker dependency. Initial verification binds only 127.0.0.1 and is an actual MQTT 3.1.1 publish/subscribe check. A later listener binds the selected laptop LAN IPv4 address; reachability/firewall and campus network access must be checked with the actual Pico. No firewall-wide disable, router configuration change or Wi-Fi password retrieval is part of setup.

Alternatives considered by the assistant: Docker (installed but daemon unavailable), WSL broker (additional NAT/port-forwarding work), and workspace-local native broker (chosen for direct laptop networking and removal with the workspace). This is an implementation choice, not a claimed team agreement.

## Observed constraints

- The template currently uses WPA2 mixed PSK authentication and a fixed IPv4 MQTT broker address, port1883, no username/password or TLS integration.
- The laptop scan identifies SINGTEL-A9C0 on 2.4GHz/channel1, reported WPA3-Personal. WPA2 transition compatibility and Pico association are not established by that scan.
- The coordinator will enter the Wi-Fi password locally in ignored config/wifi_secrets.h. Do not print it or commit it. Current laptop LAN address may change when switching networks.
- The current shared MQTT example prints received data and publishes heartbeat text; it exposes no app-level telemetry-submit or command-callback interface. Use it unchanged for initial transport evidence, then define a narrow service-owned app interface. Do not call lwIP/MQTT from application tasks.
- Motors remain inactive during initial communication work. Colour calibration and physical direction/encoder-under-drive validation remain pending.

## Acceptance

Motor isolation: user supplies continuous output around one first GP20 request, exact image, disconnected winding confirmation, cutoff and final state; either outcome is evidence, not a predefined pass of the old freeze.

Laptop broker: real MQTT3.1.1 publish/subscriber round trip on a dedicated bring-up topic, bounded timeouts, owned test processes cleaned up, repeatable launch instructions and preserved log. No claim of Pico or LAN connectivity from localhost success.

Network milestone: actual Pico association/IP and broker connection; matching heartbeat TX/RX; laptop message appears in Pico RX; IR reports remain healthy; no motor activation; network/broker restart recovery observed. Application command validation, target-state handling, compact telemetry and later motor integration have separate acceptance tests.
