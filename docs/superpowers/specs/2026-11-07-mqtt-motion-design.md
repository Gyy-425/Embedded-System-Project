# MQTT Motion v0.8 design

The user authorized connecting the passed Motion interface to MQTT with fresh
commands and explicit start/stop. Continue C/micro T-Kernel/BARR-C in the existing
feature checkout. Prior physical results are saved in local0660fdd. No upload,
robot actuation, broker startup, secret disclosure, push or merge is included.

## Boundaries and decisions

Use a separate v2 topic/protocol and opt-in v0.8 REMOTE profile. Preserve v0.5
selection/IR and v0.7 LOCAL images. Reuse the network-owner task, typed Motion
API,100ms internal command/driver lease and ordered paired PWM. Disable the
local GP20 run producer in REMOTE; GP21 remains the local STOP.

One controlling CLI process; passive watch is read-only. Topic base
`aai05/v2/robot01`, command QoS1, ack QoS1, telemetry QoS0, all non-retained.
No closed-loop speed/colour tracking. Duties are signed electrical -50..50%,
not calibrated physical velocity or forward direction. Target selection stays
available when not moving; detected colour remains unknown.

START queues an explicit owner-side rearm, then feeds fresh Motion commands
only after the owner acknowledges the expected epoch in READY/coast. ARM cannot
clear FAULT or a held GP21, and cannot apply drive. Only Motion owns the driver.
Rearm explicitly clears the continuous-driver expiry latch while coasted.
The v0.7 LOCAL profile keeps its original reset-only latch behavior.

Remote authorization lease750ms, renewed by fresh KEEPALIVE at nominal200ms
from the CLI. Device feeds100ms Motion commands at nominal20ms only while
the remote lease/session remains valid. This wider network lease accommodates
the existing Mosquitto subprocess client; internal owner-stall expiry remains
100ms. Loss callback, observed Wi-Fi loss, remote timeout, GP21, Motion failure
or stale Motion status stops feeding and latches STOP. Reconnection never
starts movement. Explicit fresh START is required after stop/reconnection.

## Freshness and transport

Device session and single-use grant are nonzero uint32 values. A grant expires
after1000ms, rotates after a mutation/stop/expiry, and is not renewed by duplicate
requests. STATUS returns the current grant, rotating only an expired one.
START/KEEPALIVE/config changes require current session+grant and forward16-bit
sequence (delta1..32767); identical duplicates return DUPLICATE without renewing
lease or repeating an action. Sequence reuse with different values conflicts.
These are liveness/replay checks on the trusted test LAN, not authentication.
Session generation mixes read-only ROSC random bits/time and excludes the
immediately preceding tag; global uniqueness/cryptographic strength is not claimed.

Valid non-retained STOP for the current session bypasses mutation freshness
and ordering so a client with a lost ACK can stop. It is idempotent, never
starts/rearms, and does not rewind mutation order. Other stale commands reject.
Target/config changes reject while ARMING/RUNNING. STOP ACK means requested;
CLI waits for Motion STOPPED and DRIVE OFF before reporting confirmation.

Snapshot the actual MQTT PUBLISH RETAIN bit at the topic callback, preserving
it across data fragments. Reject retained deliveries before any state change.
Use the SDK2.3.0 lwIP private header/first rx-buffer byte only at this audited
callback; pin mqtt.c SHA256
71bb51547bf2f81b324d5fa796df367076fb98bc8a0eb687a1450f7eedfcf98a.
SDK/parser changes require metadata review; do not edit the external SDK.
MQTT3.1.1 clears RETAIN for established-subscription forwarding, so this detects
retained replays, not every publisher's original retain choice. Single-use grants
and session/lease checks remain necessary. Source: OASIS MQTT3.1.1 section3.3.1.3,
https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html .

## Wire format (big-endian, explicit bytes, CRC-8/SMBUS)

Command14bytes: bytes0..2 are an explicit24-bit word: version2 bits23..20,
opcode19..17, target16..14, signed7-bit left13..7 and right6..0.
Bytes3..4 sequence,5..8 session,9..12 grant,13 CRC.
Opcodes0STATUS,1TARGET,2CLEAR,3START,4KEEPALIVE,
5STOP. STATUS/STOP/CLEAR/KEEPALIVE require target7 and duties0; STATUS also
session/grant0, STOP grant0. TARGET uses0..6, duties0. START requires at least
one nonzero valid duty and target7. KEEPALIVE renews the stored pair only.

ACK23bytes: byte0 high nibbleA, opcode bits3..1, result high bit0;
byte1 result low3 bits7..5, ready0, DRIVE1, IRvalid2, IRstale3, txdrop4;
byte2 remote phase bits0..2, Motion phase3..4, Motion reason5..7;
3..4 request-sequence echo,5..6 last mutation sequence,
7..10 session,11..14 grant,15..18 uptime_ms;
19..21 explicit24-bit word: target23..21, signed7-bit left20..14/right13..7,
remaining lease in10ms units rounded up6..0;22 CRC.
Telemetry26bytes: byte0=C0, result0, same common fields through21,3..4 are
telemetry sequence,22..24 packed12-bit IR pair,25 CRC. Invalid/unavailable IR
uses flags; zero is valid ADC data. No native-structure serialization.
Signed7-bit storage values outside-50..50 reject. No spare whole command byte
is transmitted; packed wire fields are separate from native C structures.

Remote phases0IDLE,1ARMING,2RUNNING (authorized feed, not proof of physical
movement),3STOPPED,4FAULT. Results0OK,1DUPLICATE,2BAD_FRAME,3BAD_VALUE,
4SESSION,5STALE_SEQUENCE,6CONFLICT,7NOT_READY,8STALE_GRANT,9BAD_STATE,
10RETAINED,11MOTION_FAILURE,12BUSY. State/DRIVE explicitly distinguish requested
and observed software output. Existing Motion reasons remain0..4.

## Delivery and evidence

Portable codec/control, synchronized owner rearm and driver rearm guard,
network adapter/fragment metadata, readable bounded CLI (requested budget
max10seconds, default2, beginning before START and including handshake/renewal
waits; always STOP on exit), distinct normal/TEST REMOTE artifacts. Cleanup and
physical/network stopping latency are not a hard duration guarantee. Cover
malformed frames, replay/duplicates/wrap, retained fragments, stale grants,
remote timeout/link loss, held GP21, pending ARM, STOP/renew races, old profiles
and C/Python vectors. One fresh review, fix material issues with regressions,
then focused builds/checks. No physical pass inferred from host tests.
Final task priorities, Wi-Fi WCET, measured timing, powered encoder accuracy,
simultaneous wheel load and colour calibration remain later scoped validation.
