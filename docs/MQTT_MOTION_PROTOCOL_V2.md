# MQTT Motion protocol v2

Implemented in `drive_protocol.[ch]`, `drive_control.[ch]` and
`mqtt_drive_app.c`; laptop codec/client: `tools/mqtt/drive_codec.py` and
`drive_cli.py`. v0.5 v1 topics/client remain separate. This is a trusted-LAN
control protocol; session/grants and CRC provide freshness/error checks, not
authentication or encryption. One logical client controls movement.

## Transport and lifecycle

Default device `robot01` (public `COMM_DEVICE_ID`):

| Topic | QoS | Payload | Retained |
|---|---:|---:|---|
| `aai05/v2/robot01/command` |1|14 bytes|No|
| `aai05/v2/robot01/ack` |1|23 bytes|No|
| `aai05/v2/robot01/telemetry` |0|26 bytes|No|

Wrong topic/length, truncation, bad CRC/version/values and retained replay
deliveries cannot actuate. The PUBLISH RETAIN bit is captured once at the topic
callback and carried through fragments. This uses an audited SDK2.3.0 lwIP
private-header detail: the build pins `mqtt.c` SHA256
`71bb51547bf2f81b324d5fa796df367076fb98bc8a0eb687a1450f7eedfcf98a`.
Re-audit before changing that parser. MQTT3.1.1 clears RETAIN on forwarding to
existing subscriptions, so detection concerns retained replay deliveries,
not every live publisher's original flag. See
[OASIS section3.3.1.3](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html).

STATUS supplies nonzero 32-bit session/grant and last accepted mutation sequence.
Grants expire after 1000ms and rotate on accepted mutations/stop/expiry.
Non-STOP mutations require that grant, current session and a forward 16-bit
sequence: unsigned delta1..32767. Identical duplicates return DUPLICATE without
renewing authorization or repeating ARM; reused sequence with different content
conflicts. A valid current-session STOP bypasses grant and sequence ordering,
never rearms and never rewinds the mutation sequence. Reconnection clears
target/authorization, creates a new session and requires explicit fresh START.
Random/time mixing excludes the immediately previous session; cryptographic
unpredictability or global uniqueness is not claimed.

START queues owner-side ARM. The network layer waits for matching epoch and
READY/coast before feeding Motion. Only Motion writes the driver. A held GP21,
FAULT, unavailable/stale status or rejected submission prevents further feed.
STOP cancels pending ARM; owner rearm can clear ordinary stop/expiry only while
coasted and healthy. GP20 is disabled in REMOTE; v0.7 LOCAL remains reset-only.

Fresh KEEPALIVE renews750ms authorization for the stored duty pair. Duplicate
renewals do not. Motion commands are fed nominally every 20ms with 100ms expiry;
the CLI normally renews every 200ms, earlier when remaining lease is short.
Observed link loss, lease expiry, local STOP or Motion failure stops and never
automatically restarts. Polling also runs when the observed link is down.
No physical latency or scheduler WCET is established by these constants.

## Command,14 bytes

All multi-byte values use explicit big-endian serialization. No native C
structure or compiler bitfield layout is sent. CRC-8/SMBUS: polynomial 0x07,
initial 0, no reflection, final XOR 0, over every preceding byte.

| Bytes / field | Encoding / accepted values |
|---|---|
|0..2 control, bits 23..20|Version 2 (4 bits)|
|control19..17|Opcode 0..5 (3 bits)|
|control16..14|Target 0..6;7 unset (3 bits)|
|control13..7 /6..0|Left/right signed 7-bit two's complement; accepted -50..50 percent,1% resolution|
|3..4|Unsigned 16-bit request sequence|
|5..8|Unsigned 32-bit session|
|9..12|Unsigned 32-bit grant|
|13|CRC8|

Signed7-bit encodings outside -50..50 are invalid, never clamped into a drive
request. Opcode6/7 and other versions reject. Exact operation rules:

| Opcode | Operation | Target / duties | Session / grant |
|---:|---|---|---|
|0|STATUS|7 / both0|Both0; sequence is an echo identifier|
|1|TARGET|0..6 / both0|Current / fresh nonzero|
|2|CLEAR|7 / both0|Current / fresh nonzero|
|3|START|7 / at least one nonzero|Current / fresh nonzero|
|4|KEEPALIVE|7 / both0; keep stored pair|Current / fresh nonzero|
|5|STOP|7 / both0|Current nonzero /0|

TARGET/CLEAR reject while ARMING/RUNNING. Target classes reuse the v1 mapping;
they are selected labels, not evidence that seven tapes are distinguishable.

## ACK23 bytes and telemetry26 bytes

| Bytes / field | Encoding |
|---|---|
|0 ACK|High nibbleA; opcode bits 3..1; result high bit0|
|0 telemetry|C0; result 0|
|1|Result low3 bits 7..5; flags4..0|
|flags0..4|ready, DRIVE active, IR valid, IR stale, synchronous TX enqueue drop|
|2|Remote phase bits 2..0; Motion phase4..3; Motion reason7..5|
|3..4|ACK request sequence echo / telemetry sequence|
|5..6|Last accepted mutation sequence|
|7..10 /11..14|Session / current grant|
|15..18|Unsigned 32-bit device uptime,1ms resolution|
|19..21 word, bits 23..21|Target 0..6 or7 unset|
|word20..14 /13..7|Requested left/right signed 7-bit duties|
|word6..0|Remaining remote lease, rounded up in 10ms units, 0..75|
|22 ACK|CRC8|
|22..24 telemetry|Left ADC bits 23..12, right ADC11..0, each0..4095|
|25 telemetry|CRC8|

IR zero is valid. Flags represent unavailable/stale samples; valid+stale
together is invalid. Uptime/telemetry sequences wrap naturally; compare elapsed
time with unsigned subtraction, not absolute timestamp ordering. Internal
750/1000/100ms windows are far below half the 32-bit timer range. Sequence order
uses the half-range rule above; no sequence saturation. Session/grant increments
skip zero. ACK acceptance and actual Motion/DRIVE are separate: STOP ACK means
requested. The client requires fresh status showing Motion STOPPED and DRIVE
OFF before confirming output-off. It cannot measure physical wheel coast.

Remote phases:0 IDLE,1 ARMING,2 RUNNING,3 STOPPED,4 FAULT. RUNNING describes
authorized command feed, not proof of physical movement. Motion phases:
0 READY,1 RUNNING,2 STOPPED,3 FAULT. Reasons:0 NONE,1 REQUEST,2 GP21,
3 EXPIRED,4 HARDWARE. Remote expiry may request STOP and therefore yield reason1.

Results:0 OK,1 DUPLICATE,2 BAD_FRAME,3 BAD_VALUE,4 SESSION,5 STALE_SEQUENCE,
6 CONFLICT,7 NOT_READY,8 STALE_GRANT,9 BAD_STATE,10 RETAINED,
11 MOTION_FAILURE,12 BUSY. Unsupported result/state values reject in the client.

## Concurrency, client timing and representation evidence

MQTT control state has one network-task writer; callback and periodic service
run in that owner context. Cross-task ARM/STOP/submit/status use the Motion
adapter's spinlock, pending-ARM gate and epoch handshake. Snapshots copy complete
state; a cached Motion snapshot older than 100ms cannot authorize feeding.
Driver rearm requires inactive/coasted outputs, disabled PWM and disarmed timer.

The CLI validates -50..50 duties and a positive duration at most 10s (default 2).
An idle STATUS grant may expire before the next request reaches the device.
For START/TARGET/CLEAR only, the client may use the new grant from one matching
STALE_GRANT rejection, preserving the original sequence, values and timeout.
It requires the same session, unchanged mutation sequence/target/Motion state,
ready status and DRIVE OFF. This is recovery before acceptance, not restarting
stopped movement. A second rejection, lost ACK, running KEEPALIVE failure,
changed state/session or fault aborts. Ambiguous retries keep identical bytes.
The duration budget begins before START, includes handshake time and limits
renewal waits. Exit/interruption requests STOP even after a lost START ACK;
session/lease/STOP failure never triggers an automatic new START. Cleanup may
take longer than the requested budget, and network delivery/device stop latency
are not hard physical duration guarantees. A second controller is unsupported.

Actual codec lengths14/23/26 and independent C/Python byte vectors are checked
in host tests. These are message payload sizes, excluding MQTT/TCP/IP overhead.
Packed fields use sufficient bits for their ranges and explicit invalid states.
Native storage uses fixed-width types/alignment; this does not prove minimum
RAM use or equivalent-function savings. Week13 still needs a measured reference
implementation, comparable maps/message captures and correctness/performance
evidence. Template-vs-new-feature sizes are not representation savings.
