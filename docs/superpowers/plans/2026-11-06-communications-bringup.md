# Communication-first Bring-up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Isolate the powered-output stall, then prioritise visible two-way Wi-Fi/MQTT communication with motors inactive.

**Architecture:** Preserve existing motor images and shared network implementation. Use a workspace-local native laptop broker, first proving localhost operation; use the course heartbeat/receive example for the first actual Pico network test before adding an app interface.

**Tech Stack:** Pico W, micro T-Kernel, C SDK/C, lwIP MQTT3.1.1, Windows Mosquitto, Python/PowerShell host helpers.

**Spec:** docs/superpowers/specs/2026-11-06-communications-bringup-design.md

## Global Constraints

- Motors remain inactive during initial communication work.
- Do not print or commit the Wi-Fi password or assume scan results prove authentication compatibility.
- Only the existing core-0 network service owns cyw43/lwIP/MQTT calls.
- Localhost, real Pico connectivity and application messaging are distinct acceptance results.
- Preserve previous motor results and artifacts; no new connected-motor retry in the isolation procedure.

## Review Focus

- Rewiring only when all power is removed; disconnect four winding leads, not the six-pin encoder harnesses.
- Temporary broker processes must be owned and cleaned up, and bind only loopback during the host check.
- Changing Wi-Fi can change broker IPv4 and introduce device isolation or incompatible authentication.
- Heartbeat/received-print evidence must not be described as completed robot command handling or live sensor telemetry.
- Initial network firmware must retain a no-output motor policy and accurate network-mode/banner reporting.

## Task 1: Motor-load isolation instructions

**Files:** Create docs/MOTOR_OUTPUT_ISOLATION.md; append S2-M108/INT-107 in test logs.

**Interfaces:** Uses the preserved normal v0.3 UF2 (SHA256 4a1af5d9b09026536aa1239bfb2ce1b7c5d3a6897837919c0c09a48aef4002e0). Consumes a user log and physical disconnection confirmation; produces a scoped diagnostic result only.

- [x] Verify old UF2 hash, write exact terminal/contact table and one-request procedure, including distinct physical versus requested DRIVE states.
- [ ] Record the coordinator's actual result before attributing the connected-motor freeze to any cause.

## Task 2: Laptop broker and real host round trip

**Files:** tools/mqtt/Start-Broker.ps1; tools/mqtt/test_broker.py; tools/mqtt/README.md; ignored tools/mqtt/.runtime; .gitignore; testing/integration/TEST_LOG.md.

**Interfaces:** Start-Broker.ps1 [-BindAddress IPv4] [-Port 1883], default127.0.0.1. test_broker.py --label UNIQUE, real MQTT3.1.1 round trip on a dedicated retained test topic; JSON/log evidence, no retained robot command.

- [x] Unpack official broker under the ignored runtime directory; retain provenance/hash locally.
- [x] Add launcher with loopback default and explicit single-address binding.
- [x] Execute real publish/subscribe check with bounded timeouts and finally cleanup; record results. Check script parsing and Git ignore/whitespace.
- [x] Prepare only helper source, instructions and non-secret evidence for the feature-branch save; no service, installer/runtime binaries or credentials.

## Task 3: Actual Pico transport milestone

**Files:** tools/build_wsl.py, tools/network_config.py, testing/host/test_network_config.py, app_program/app_main.c and subsystem3/subsystem3.c; ignored config/wifi_secrets.h; docs/NETWORK_BRINGUP.md.

**Interfaces:** Existing cyw43_utk service, NET=lwip/MQTT=1, configured SSID/broker IPv4, unique client ID, aai05/bringup/heartbeat topic. No direct application lwIP calls or claim of command handling.

- [x] Coordinator supplies password by editing the ignored local header; compatible authentication and actual LAN access remain required.
- [x] Build distinct normal/TEST network images with truthful banners and no-drive output policy; verify hashes and existing host safety regressions.
- [ ] Bind broker to selected LAN IPv4 and configure only the necessary scoped inbound access if required; verify actual Pico association/IP and MQTT TX/RX.
- [ ] Send a non-retained laptop text message on the subscribed bring-up topic, observe actual Pico RX, then test broker/network restart recovery without motion. Save user-run logs separately from build results.

## Task 4: Project commands and telemetry, then motion integration

- [ ] Define a narrow service-owned app interface, validated STOP/STATUS/SET_TARGET and real sensor telemetry; do not call the existing printable RX handler a command parser.
- [ ] Unit-check malformed/oversized/unknown commands, target range, duplicate/retained/expired motion intent and bounded concurrent updates before testing actual transport.
- [ ] Calibrate actual seven tapes, then qualify powered wheel polarity/encoder signs and integrate target-colour tracking. Check Wi-Fi loss/STOP, timing with networking active, and equivalent minimum-bit versus baseline correctness/size evidence.

No dates or invented measured targets are assigned to these remaining implementation steps.

## Execution ledger

Ruling: continue on the existing clean aai05/bring-up feature checkout under the coordinator's established inline workflow, without another worktree/approval menu. Task1 physical execution and Task3 association/transport require user-operated hardware; do all independent host preparation first.

Ruling: the broker launcher is a small reversible configuration wrapper. Use real broker/client integration and syntax checks rather than unit tests mirroring its configuration. Add targeted tests for private-header validation because malformed strings must be rejected before compiler diagnostics.

Task1 instructions complete, hardware pending. Task2 real localhost MQTT3.1.1 publish/receive/clear PASS; test broker stopped. Initial Windows restricted tests hit socket/temp-directory sandbox permissions; repeat in the appropriate execution context passed. Old Ubuntu7zip extracted zero files from the new NSIS package; official7-Zip26.05 extracted101 files and Mosquitto2.1.2 ran. No Windows installer/service was executed.

Task3 software preparation complete: seven C host programs, config checks and normal/TEST firmware are17 successful steps, no reported warnings; both UF2 hashes independently matched. A fresh read-only review found no blocking issue. Its JSON/C-escape edge was reproduced synthetically (three unexpected acceptances), corrected in host tooling and all four groups passed; no actual secret leakage was established. The actual local header also passed private preflight without outputting values. Hardware association, actual LAN reachability, MQTT round trip and recovery remain pending. No shared kernel/BSP/network library source changed.
