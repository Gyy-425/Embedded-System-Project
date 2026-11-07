# v0.5 host verification result

The coordinator supplied the complete PowerShell runner output for label `comms-host-20261107-104543`. The corresponding local result JSON and individual logs were inspected directly. This is a review of that actual run; no redundant test run was performed.

- Source commit: `48497af135abe7179ec9d3ec4bc3463ee4f1565e`; runner recorded `source_dirty=false`.
- Profile: application0.5, NO_DRIVE_PROBE, BINARY_COMMS_APP, functional tests HOST_ONLY, hardware tests NOT RUN.
- Runner exit code0 in supplied transcript. All25 recorded stages have exit code0; no deferred stages or reported compiler warnings.
- C protocol boundary assertions completed and printed `Communication protocol checks passed.`
- Python communication/client suite:8 tests, OK, no skipped tests. The runner supplied captured C vectors for cross-language decoding.
- Private network-configuration suite:4 tests, OK. No credentials were printed in the inspected evidence.
- Existing encoder/register/motor-logic/capture/control/pulse/probe host checks completed. These use host models and do not establish physical motor operation.
- Both normal and TEST communication firmware compiled. Compiling TEST is not on-device execution.

The C test emitted synthetic ACK `4e980000010203047e` and telemetry `9793ffff01020304ffffffffabc123c2`; these are protocol test vectors, not measurements from the car.

Normal UF2 recorded SHA256: `67e97ef38a396a087b382852037a648274e8aa1a4c0fc987d9a667494af024f0`.

TEST UF2 recorded SHA256: `2191644fa13d43d01001152f8d092c908bdaeb160f663e889c8e93dfad3e2766`.

Both current local artifacts were rehashed during review and matched the recorded values and original prepared v0.5 hashes. No application source change or firmware upload was needed.

Step1 is passed at host scope. Actual Pico association/IP, MQTT connection/subscription, delivered raw IR telemetry, target selection/clear ACKs and broker/network recovery remain pending. Continue at step2 of [COMMS_TEST_V0_5.md](../../../docs/COMMS_TEST_V0_5.md). Do not repeat the completed host stage unless source/configuration changes or a later failure justifies it.
