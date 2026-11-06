# v0.5 communication-only preparation

User instruction: develop without the robot/tapes for testing tomorrow; then narrow to one case, quality first, preserving coding conventions. This change implements communication only. No motor/PWM source, physical wiring, classifier, calibration or path-following behaviour was changed.

## Implemented scope

- Binary GET_STATUS, SET_TARGET and CLEAR_TARGET with explicit field validation, connection tag and wrap-aware mutating sequence.
- ACKs and raw12-bit IR telemetry; detected class UNKNOWN and motion/classification capabilities absent.
- Network-owner application callbacks, bounded command reassembly and subscription/connection retry readiness.
- Readable stdlib Python client using existing Mosquitto tools, bounded retries and owned subprocess cleanup.
- Distinct no-drive build/profile/artifacts. Host boundary/cross-language checks are prepared for the next session.

Source base: `9a9aeb9cb466e55b82dbdf073a6ecc219ba72154`, with the reviewed working changes. Exact working-file hashes are in `2026-11-07-v0_5-source-manifest.json`. Source binaries containing private Wi-Fi configuration remain ignored/local; no credentials are included in this record.

## Actual compile/static results

Final invocation: `python3 tools/build_wsl.py --sdk "/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0" --comms-app --build-only --label 2026-11-07-v0_5-final` in existing Ubuntu WSL. Result JSON and compiler logs share that label.

| Check | Observed result |
|---|---|
| Pure packet C99 compile: Wall/Wextra/Werror/Wconversion/Wshadow/Wstrict-prototypes | exit0, no warnings |
| Prepared C boundary-test source compile, same diagnostics | exit0, no warnings; not linked/executed |
| Python codec/client/prepared-test syntax compile | exit0 |
| Normal firmware `TEST=0` | exit0, no warnings |
| Test firmware `TEST=1` | exit0, no warnings; not executed |
| Later Python syntax compile including build runner | exit0 |
| New application/test C tabs/80-column scan | no violations |
| Git diff whitespace check on source/docs/JSON | no violations; raw compiler logs retain make's whitespace |
| Private header and firmware ignore checks | both ignored |

The first compile attempt stopped on inherited kernel FP/FUNCP strict-prototype declarations. A scoped diagnostic exception surrounds only the adapter's kernel header include; kernel types were not changed. Final strict checks on new code remain enabled. Intermediate compile logs are preserved in the parent workspace's `tmp/aai05_comm_2026-11-07/intermediate-logs`.

Normal UF2: `output/firmware/AAI05_Comms_v0_5_NO_DRIVE.uf2`, SHA256 `67e97ef38a396a087b382852037a648274e8aa1a4c0fc987d9a667494af024f0`.

TEST UF2: `output/firmware/AAI05_Comms_v0_5_NO_DRIVE_TEST.uf2`, SHA256 `2191644fa13d43d01001152f8d092c908bdaeb160f663e889c8e93dfad3e2766`.

Linker totals from `arm-none-eabi-size`: normal text337544/data100/bss82376; TEST text338100/data100/bss82376 bytes. These are build totals, not measured runtime high-water marks or equivalent-functionality representation savings. Native structure padding and MQTT/TCP overhead remain outside the8/9/16-byte application payload lengths.

Old normal v0.3.2, TEST v0.3.2 and normal v0.4 UF2s were rehashed and match their previous recorded SHA256 values. They were not overwritten. No new device upload occurred.

## Static review

A fresh-context read-only reviewer inspected C/Python encoding, CRC, serial ordering/wrap/idempotency, fragment handling, retry callback lifetime, single network-task ownership, no-drive safeguards and prepared tests. No material correctness findings were reported. Private application names were aligned with module prefixes; documentation clarifies that TX-drop is only the last synchronous enqueue outcome. Runtime delivery/PUBACK tracking is not claimed.

## Explicitly pending

All executable host checks, C→Python vector verification, broker exchanges, Pico association/subscription, target ACK/telemetry reception, fragmentation/reconnect behavior and on-target timing are **NOT RUN** in this development task. Build runner entries show DEFERRED rather than a passing exit code. No sensor data, tape accuracy, motion result or team contribution was invented. Static review and compilation cannot establish runtime correctness.

Next procedure: [COMMS_TEST_V0_5.md](../../../docs/COMMS_TEST_V0_5.md). Protocol and representation limits: [COMM_PROTOCOL_V1.md](../../../docs/COMM_PROTOCOL_V1.md). Complete this communication validation before broadening the coding scope.
