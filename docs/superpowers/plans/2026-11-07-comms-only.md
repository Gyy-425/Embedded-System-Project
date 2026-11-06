# Communication-only implementation plan

User scope: prepare code without the robot/tapes; functional validation tomorrow. Latest steering narrows the work to ONE case, quality first, using the required C conventions. Selected case is MQTT target selection/ACK/raw-IR telemetry and a readable laptop CLI. No motor/PWM/classifier/path-following work in this change.

Implementation method: inline on the established dedicated aai05/bring-up checkout. User authorised implementation; do not interrupt for a second planning approval. Apply BARR-C conventions to new application C (Allman braces,4 spaces, fixed-width unsigned bit operations, static private state, module-prefixed lowercase names, pointer p_ names). Inherited port changes remain narrow and documented. User requested functional validation later: perform build/static checks and review now; do not connect a broker/car or claim functional/hardware success.

## Design and interfaces

- Versioned topics: aai05/v1/robot01/command, /ack, /telemetry. One controlling laptop client; public non-secret device topic identifier. No dashboard/new Python dependency; CLI uses the existing Mosquitto executables for binary publish/hex receive.
- Only GET_STATUS(op0), SET_TARGET(op1), CLEAR_TARGET(op2). Colour IDs:black0,white1,colour1..colour5 IDs2..6,unset/unknown7. Actual five tape colours are not yet selected. No START/drive/STOP actuator command in this communication-only stage.
- Command8 bytes: control byte type00 bits7..6/opcode bits5..4/target bits3..1/reserved0 bit0; sequence16 big-endian; connection tag32 big-endian; CRC-8(poly07,initial00) last byte. GET/CLEAR target must7, SET target0..6. GET accepts tag0/read-only; mutating commands require current tag, forward16-bit serial sequence; duplicate sequence with same opcode/target is idempotent, conflicting or old sequence rejected.
- ACK9 bytes: control type01/opcode/current target/reserved0; result3 bits + capabilities4 bits + ready1; last accepted mutating sequence16; current tag32; CRC. GET supplies the sequence baseline so a restarted CLI can continue without guessing.
- Telemetry16 bytes: control type10/target3/detected3(UNKNOWN); capabilities/status byte; telemetry sequence16; tag32; uptime32; left/right ADC12+12 packed into3 bytes; CRC. Only target-storage/raw-IR capability bits enabled; motion/classification capability bits0. IR-valid/stale, previous-TX-drop and command-ready flags are explicit.
- Connection tag is derived from monotonic clocks and a connection generation; changes on successful command subscription and clears target/sequence state on disconnect. It is a freshness aid, NOT authentication or a cryptographic nonce. All commands are non-actuating. Reject retained/stale actuation before extending this protocol to motor commands in future.
- Pure C module comm_protocol.c/.h: validation/encoding/CRC and a single-owner packed target/sequence state. No malloc, raw struct transmission, SDK/RTOS includes or hardware writes.
- App adapter mqtt_app.c in subsystem3: callbacks on network-service owner only; obtains IR via existing nonblocking snapshot getter, publishes500ms telemetry, prints concise accepted/rejected command information. No shared target state is read/written by another task; no packed RMW race introduced.
- Narrow mqtt_utk hooks enabled only by COMM_APP=1: raw fragmented command reassembly, subscription-ready gating, binary publication and service polling. Preserve old text transport profile. Bounded retries for connection/subscription, no duplicate timeout chains.
- New make/profile/runner --comms-app requires NET=lwip/MQTT=1/MOTOR_PROBE=1. Distinct v0.5 NO_DRIVE artifacts, banner and object-profile key. Old motor/transport artifacts preserved. Build-only option skips executing test programs per user's request; no firmware upload.

## Tasks / progress

- [x] Implement pure C packet/state module and publish protocol document with fields, invalid/reserved values, endianness, CRC, sequence wrap and scope limits.
- [x] Implement network-owner adapter, raw MQTT bridge, retry/subscription readiness and safe distinct build profile.
- [x] Implement standard-library Python codec/CLI using mosquitto_pub/sub binary input/hex output; bounded request retries, GET baseline sync, clean shutdown and readable UNKNOWN/no-motion output.
- [x] Compile application C with strict warnings, compile normal/TEST communication firmware without running tests; Python syntax/static checks. Preserve no-hardware/no-functional-validation labels and old artifact hashes.
- [x] Fresh-context read-only review, fix material findings and write tomorrow's exact procedure. Functional Pico/Wi-Fi/MQTT/packet tests remain PENDING. Prepared C/Python boundary and cross-language tests are unexecuted.
- [ ] Save/push the feature branch after final source/evidence checks.

Do not infer colour calibration, seven-tape separability, real motor output, correct demo network authentication or whole-project minimum-bit compliance from this implementation. This case packs communication payloads; whole-application representation and measured savings remain later work.
