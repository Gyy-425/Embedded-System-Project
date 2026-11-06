# Communication-only protocol v1

This v0.5 application stores a selected target and reports raw IR readings over MQTT. It has no motion command, classifier or calibrated tape names. Detected colour remains `unknown`. It is preparation for the revised project, not completion of target-colour following. Functional and hardware checks are pending.

## Ownership and transport

The existing core0 radio/lwIP service task owns all MQTT calls and communication state. Its receive callbacks and service poll call `mqtt_app.c`; the reporting task does not call lwIP. IR is copied through the existing nonblocking snapshot getter. No other task/ISR reads or modifies the packed communication control byte. A future Motion consumer needs a defined, synchronized interface; directly exposing this byte would break the ownership rule.

MQTT3.1.1 topics, configured publicly in `config/comm_config.h`:

| Topic | Producer → consumer | QoS | Retain |
|---|---|---|---|
| `aai05/v1/robot01/command` | Laptop → robot | 1 | false |
| `aai05/v1/robot01/ack` | Robot → laptop | 1 | false |
| `aai05/v1/robot01/telemetry` | Robot → laptop | 0 | false |

Command-ready becomes true after successful command SUBACK. Failed subscription attempts retry through one timeout chain; failed connection attempts are also delayed. On an MQTT connection callback the application clears the target and sequence state; successful subscription creates a new nonzero connection tag. A TCP/MQTT disconnect can lag physical Wi-Fi loss. This is not a tested link-loss motor safety mechanism.

Only one controlling laptop process is supported. A passive `watch` client may coexist; its STATUS requests do not advance mutating sequences. All publishes copy an explicitly encoded byte array; no native structure, C enum, pointer, padding or host byte order goes on the wire.

## Values and operations

| Field | Meaning |
|---|---|
| Colour0 | black |
| Colour1 | white |
| Colours2..6 | `colour1`..`colour5`; assign actual tape labels after selection/calibration |
| Colour7 | unset target / unknown detected class |
| Opcode0 | GET_STATUS, read-only |
| Opcode1 | SET_TARGET, stores ID0..6 only |
| Opcode2 | CLEAR_TARGET, stores unset7 |
| Opcode3 | invalid |

STATUS and CLEAR require target7. SET_TARGET rejects7. There is no START, run, motor STOP, mode-change or calibration opcode in this version. The no-drive build does not turn a selection into movement.

All fields are unsigned integers. Multi-byte integers are big-endian. Bit0 is the least significant bit. Every packet ends with CRC-8/SMBUS over all preceding bytes: polynomial0x07, initial0, final XOR0, no reflection. The reference string `123456789` has CRC0xF4. MQTT already provides transport framing; the CRC is a small application format check, not authentication.

### Command: 8 bytes

| Bytes | Encoding |
|---|---|
| 0 | bits7..6=`00`; opcode bits5..4; target bits3..1; reserved bit0=0 |
| 1..2 | mutating sequence16; ignored for STATUS |
| 3..6 | current connection tag32; ignored for STATUS (CLI sends0) |
| 7 | CRC |

Reject wrong length, packet type, reserved bit, CRC, opcode or target. The receive bridge preserves binary bytes and reassembles fragments into its existing128-byte buffer, rejects oversized/truncated frames, checks total received length and requires the exact command topic. Application decoding still requires exactly8 bytes.

### ACK: 9 bytes

| Bytes | Encoding |
|---|---|
| 0 | bits7..6=`01`; opcode bits5..4; current target bits3..1; bit0=0 |
| 1 | result bits2..0; capabilities bits6..3; command-ready bit7 |
| 2..3 | last accepted mutating sequence16, initially0 |
| 4..7 | current connection tag32 |
| 8 | CRC |

Results:0 OK,1 DUPLICATE,2 BAD_FRAME,3 BAD_TARGET,4 SESSION_MISMATCH,5 STALE_SEQUENCE,6 SEQUENCE_CONFLICT,7 NOT_READY. ACK target is the current stored state, not a rejected requested target. If decoding fails, the adapter uses opcode STATUS in the error ACK; only a result0 ready STATUS ACK can synchronise the CLI.

The ACK sequence is not an echo of a STATUS request. STATUS supplies the baseline needed by a restarted CLI. For mutations, a valid connection tag is required first. If no mutation has been accepted on this connection, any initial16-bit sequence is allowed. Afterwards `delta = (new - previous) modulo65536` is interpreted as follows:

- Delta1..32767: accept and update target, last opcode and sequence.
- Delta0 with the same opcode and target: duplicate; return current state without another update.
- Delta0 with a different opcode/target: reject conflict.
- Delta32768..65535: reject stale/ambiguous sequence.

Thus65535→0 is a valid forward step. The CLI learns the baseline, sends `(baseline+1) modulo65536` and retries identical bytes on an unconfirmed request. A lost response can therefore receive DUPLICATE. Reconnection creates a new tag and clears the selection; the user must select again.

The tag mixes the local millisecond/microsecond clocks and connection generation. It differs from the immediately previous tag within the running application. It is neither cryptographic nor guaranteed globally unique across power cycles. There is no wall-clock command-expiry field. The current lwIP callback does not expose the retained flag; firmware cannot claim to reject all retained publications. The supplied CLI publishes non-retained commands and ignores retained responses. These limits are acceptable only for this non-actuating stage; define and validate retained-command, expiry and authorization behaviour before adding movement commands.

### Telemetry: 16 bytes

| Bytes | Encoding |
|---|---|
| 0 | bits7..6=`10`; selected target bits5..3; detected class bits2..0 |
| 1 | capability bits3..0; IR-valid bit4; IR-stale bit5; previous enqueue failure bit6; command-ready bit7 |
| 2..3 | telemetry sequence16 |
| 4..7 | connection tag32 |
| 8..11 | uptime32 in milliseconds |
| 12 | left ADC bits11..4 |
| 13 | left ADC bits3..0 in high nibble; right ADC bits11..8 in low nibble |
| 14 | right ADC bits7..0 |
| 15 | CRC |

Capabilities: bit0 target storage, bit1 raw IR, bit2 motion, bit3 colour classification. This image advertises only bits0/1. Detected class is always7. It never presents a chosen target as a measured colour.

Raw ADC values range0..4095, resolution one ADC count; values above4095 reject encoding. Zero is a valid raw value and is not an invalid sentinel. If the snapshot is unavailable, raw fields are zero with validity false/stale true. A snapshot older than100ms is stale. An available snapshot with sensor error can be invalid without being stale. Valid and stale are never asserted together. The laptop rejects that contradictory combination.

Nominal telemetry interval is500ms; actual scheduling and delivery rate require measurement. Unsigned subtraction handles uptime wrap at2^32ms (about49.7 days), provided interval checks occur within one wrap. Uptime is device-relative, not synchronized wall time. Telemetry sequence wraps65535→0 and resets on link-state changes. It advances on successful queue admission; QoS0 delivery is not guaranteed. The enqueue-failure flag describes the last synchronous application publication attempt, including ACK attempts. It is not an end-to-end delivery/PUBACK indicator; asynchronous errors are logged by the port.

## Minimum-bit reasoning and limits

Seven colours plus unknown fit3 bits. Three opcodes fit2 bits; eight result values fit3 bits; four capability flags use4 bits; the raw12-bit ADC pair uses exactly3 bytes. A16-bit serial number supports bounded forward ordering and explicit wrap. A32-bit tag/uptime has the scope limits above. Reserved/type bits define packet validation and framing rather than silently accepting arbitrary padding.

Persistent target, sequence-present, ready, last-opcode and previous-enqueue-failure flags share one control byte. This storage is updated by one task, so it introduces no concurrent read/modify/write. The in-memory structures still have ABI padding; their `sizeof` is not their wire size. No compiler bit-field layout or packed unaligned structure is relied on.

Payload lengths are8/9/16 bytes by construction. Compared with two16-bit ADC fields, the pair alone uses one fewer payload byte. This is a field-width calculation, not measured whole-project RAM, network bandwidth or execution savings. MQTT topic/control/TCP overhead is additional. Week13 still needs an equivalent-functionality before/after implementation, real `sizeof`/linker measurements, packet captures and correctness/performance results. Those comparisons are not complete here.

## Conventions and verification status

New application C uses fixed-width fields, unsigned bit operations, Allman braces,4-space indentation, bounded buffers, module prefixes, static private state and explicit serialization. The pure codec is compiled as C99 with `-Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes`. The adapter receives strict warning checks while narrowly suppressing inherited µT-Kernel generic FP/FUNCP prototype warnings around its header include. The supplied kernel build remains GNU11; this is not a claim that inherited code has been fully converted to BARR-C.

Static review checked format consistency, serial ordering, task ownership, retry lifetime and no-drive guards. Normal/TEST compile results and artifact hashes are recorded separately. Host boundary/cross-language checks are prepared but unexecuted. Real Pico Wi-Fi/MQTT, reconnects, delivered packet rates and sensor freshness under radio load remain pending. Use [the v0.5 test guide](COMMS_TEST_V0_5.md).
