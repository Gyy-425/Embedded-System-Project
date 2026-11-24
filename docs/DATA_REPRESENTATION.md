# Data representation (Change Requirement 1: minimum-bit)

This document covers the integrated MQTT follow image (`tools/build_follow_mqtt.py`). It records:

1. **Field table:** each application data field, its range, the bits it needs, and how it is stored or sent.
2. **RAM comparison:** RAM before and after packing.
3. **Message-size comparison:** binary messages against an equivalent text encoding.
4. **Concurrency:** how shared packed data is accessed safely.
5. **Correctness and performance:** what has been verified.

All sizes were measured on 24 Nov 2026 with the firmware's own `arm-none-eabi-gcc` command line, Cortex-M0+, `-O2`.

## 1. Field tables

### 1.1 MQTT command, laptop → car: 14 bytes (`app_program/subsystem3/drive_protocol.c`)

| Field | Valid range | Bits needed | Representation |
|---|---|---|---|
| Protocol version | 2 | 4 | Bits 23–20 of the 24-bit control word (bytes 0–2) |
| Opcode | 0–7 (STATUS…STRAIGHT) | 3 | Control bits 19–17 |
| Target lane | 2 yellow, 4 red, 5 blue, 7 unset | 3 | Control bits 16–14 |
| Left / right duty | −50…+50 % | 7 each | Control bits 13–7 and 6–0, two's complement |
| Sequence | 0–65535, wraps | 16 | Bytes 3–4, big-endian |
| Session | 1–2³²−1 | 32 | Bytes 5–8 |
| Grant | 0–2³²−1 | 32 | Bytes 9–12 |
| CRC-8 | — | 8 | Byte 13 |

### 1.2 ACK (23 bytes) and telemetry (26 bytes), car → laptop

| Field | Valid range | Bits needed | Representation |
|---|---|---|---|
| Kind + opcode + result (ACK) | kind 0xA; opcode 0–7; result 0–12 | 4 + 3 + 4 | Byte 0 = kind, opcode, result bit 3; byte 1 holds result bits 2–0 |
| Status flags | READY, ACTIVE, IR_VALID, IR_STALE, TX_DROP | 1 each (5) | One bitmask, byte 1 bits 4–0 |
| Remote phase / motion phase / motion reason | 0–4 / 0–3 / 0–4 | 3 + 2 + 3 | Byte 2 |
| Echo, mutation sequence | 0–65535 | 16 each | Bytes 3–6 |
| Session, grant, uptime | 32-bit | 32 each | Bytes 7–18 |
| Target, left duty, right duty, lease | 0–7; ±50; ±50; 0–750 ms in 10 ms ticks (0–75) | 3 + 7 + 7 + 7 | Packed 24-bit field, bytes 19–21 |
| IR left / right raw (telemetry only) | 0–4095 | 12 each | 3 bytes, bytes 22–24 |
| CRC-8 | — | 8 | Last byte |

### 1.3 Route report: 22 + 6 × n bytes, n ≤ 16 (`mqtt_drive_app.c`, `drive_publish_route`)

| Field | Valid range | Bits needed | Representation |
|---|---|---|---|
| Follow phase | 0–2 | 2 | 1 byte |
| Stop reason | 0–15 | 4 | 1 byte |
| Current colour | 0–7 | 3 | 1 byte |
| Route status | 0–12, or 255 for no run | 4 (+ sentinel) | 1 byte |
| Finished | 0/1 | 1 | 1 byte |
| Session / epoch | 32 / 16 | 32 / 16 | 4 / 2 bytes |
| Path and unassigned length | 0–4 294 967 295 µm | 32 each | 4 bytes each |
| Section count | 0–16 | 5 | 1 byte |
| Per section: colour, flags, length | 0–6; 4 flag bits; µm | 3 + 4 + 32 | 1 + 1 + 4 bytes |

**Deviation:** the route header keeps small fields in whole bytes, so 4 bytes could be saved by packing phase, reason, colour and finished. It is published once every 250 ms, and byte alignment keeps the decoder simple. We record this rather than claim minimum bits for it.

### 1.4 Internal shared data

| Data | Range | Representation |
|---|---|---|
| RGB classification result | class 0–7, reason 0–7 | One byte: class in bits 2–0, reason in bits 5–3 (`rgb_classifier.h`, `rgb_classification_class()` / `_reason()`) |
| Follow status flags: 26 in `black_follow_status_t`, 23 in `black_follow_t`, 2 in `follow_stop_trace_t` | Boolean | **1-bit fields** (`bool name : 1;`) since 24 Nov 2026 |
| Follow phase / reason / corner phase | 0–2 / 0–15 / 0–4 | `uint8_t` fields: they share bytes with neighbouring fields, so packing them further saves no space after alignment |
| Encoder counts | signed 32-bit | `int32_t` (wraps are checked in `route_measurement.c`) |
| Publish counters | 0–65535, saturating | `uint16_t` |

## 2. RAM comparison: packing the follow flags

Each figure is the size of one object of the type, read with `arm-none-eabi-nm -S` from a probe compiled with the firmware flags.

| Type | Before (`bool`) | After (`bool : 1`) | Saving |
|---|---|---|---|
| `black_follow_status_t` | 92 B | 68 B | −24 B (−26 %) |
| `black_follow_t` (policy) | 252 B | 220 B | −32 B (−13 %) |
| `motion_snapshot_t` (shared between tasks) | 324 B | 300 B | −24 B (−7 %) |
| `black_follow_owner_t` (Motion task stack) | 368 B | 336 B | −32 B (−9 %) |

**Whole image** (`size` on the ELF; same feature set, plus the ACK retry buffer):

| Build | text | data | bss |
|---|---|---|---|
| Before: commit `3137627`, label `review_follow_mqtt_20261124` | 368 004 | 248 | 84 608 |
| After: label `follow_mqtt_v2_target_20261124` | 369 948 | 248 | 84 596 |

**Where the bss change comes from** (per-symbol `nm` diff):
- `g_follow_snapshot` −24 B and `g_drive_platform_motion` −24 B: the packed snapshot copies.
- ACK retry buffer and counters: +34 B.
- Net: −12 B.

The policy and owner objects live on the Motion task's stack, so their savings appear as stack headroom rather than bss.

**Other changes in the same build:**
- The lwIP MQTT queue was raised to 8 requests and a 768-byte output buffer. That memory comes from the lwIP heap (`MEM_SIZE` 16 KB), not bss: about +512 B buffer and +4 request slots.
- Text grew by 1.9 KB, from target-lane selection and the ACK retry.

## 3. Message-size comparison: binary against equivalent text

The comparison encodes the same fields as compact JSON with short keys. To reproduce it, run `python tools/message_size_compare.py`.

| Message | Binary | Equivalent JSON | Ratio |
|---|---|---|---|
| Command (FOLLOW) | 14 B | 95 B | 6.8× |
| ACK | 23 B | 263 B | 11.4× |
| Telemetry | 26 B | 266 B | 10.2× |
| Route, 3 sections | 40 B | 266 B | 6.7× |

**Topic overhead** is the same in both cases: about 20–26 B of topic plus 4–6 B of MQTT header per publish.

**Traffic per second while running** (telemetry and route every 250 ms, keepalive ACK about every 200 ms): about (26 + 40) × 4 + 23 × 5 ≈ 380 B/s of payload in binary, against about 3.4 kB/s as text.

## 4. Concurrent access

- **Follow state:**
  - Only the Motion task (core 1) writes `black_follow_t` and its `status`.
  - Other tasks see the state only through `motion_snapshot_t`, copied whole under the `follow_snapshot` lock.
  - The bit-field read-modify-writes therefore happen on a single writer.
- **Motor permission** (`follow_remote.c`) is protected by an SMP spin lock.
- **Publish counters:**
  - Written only by the network-owner task.
  - Read whole by the report task as aligned 16-bit words, which the Cortex-M0+ reads atomically.
  - A report can be one update behind, which is acceptable for a diagnostic.

## 5. Correctness and performance evidence

- **Correctness:**
  - All host suites pass after packing: 77 suite stages, 29 RGB stages and the full follow suite, including `follow-target-owner`. Results are in `testing/integration/logs/req-20261124-*-result.json`.
  - 107 Python tests pass (5 skipped).
  - The firmware builds with `-Werror -Wconversion -Wshadow` and 0 warnings.
- **Performance:**
  - The control code is unchanged apart from bit-field access: a load, mask and store on M0+. The 10 ms Motion loop and the leases are unchanged.
  - **On-car timing after the change has not been measured yet.** Record it with SYS-05 in `testing/system/TEST_LOG.md`.
