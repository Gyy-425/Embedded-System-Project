# Open-loop Motion interface

The v0.7 LOCAL profile exercises the reusable Motion owner with a finite GP20
producer. This is open-loop electrical duty control, not autonomous Mission,
physical-speed control, MQTT actuation or an encoder-capture fix.

v0.8 adds a separate REMOTE producer and explicit owner rearm. See
[MQTT v2 contract](MQTT_MOTION_PROTOCOL_V2.md) and
[car test](MQTT_MOTION_V0_8.md). The LOCAL exercise and its recorded pass remain
unchanged. Details below distinguish profile-specific latch behavior.

## Ownership and API

Task-context producer API, declared in subsystem2/subsystem2.h:

```c
motion_result_t subsystem2_motion_submit(const motion_command_t *p_command);
motion_result_t subsystem2_motion_stop(void);
motion_result_t subsystem2_motion_arm(uint16_t *p_epoch); /* REMOTE only */
```

There is one logical producer. LOCAL owns that role through its GP20 exercise;
replace that producer when connecting Mission. Do not run independent sequence
issuers. These APIs return UNAVAILABLE in legacy/NO_DRIVE profiles. REMOTE uses
the network-task producer and disables GP20. ARM queues coast/revalidation;
it cannot itself drive. The producer waits for matching epoch and READY before
submitting. STOP cancels pending ARM. FAULT and held GP21 prevent rearm.

ACCEPTED means stored, not physically applied. Only the Motion task calls
motor_drive_set/motor_pulse_stop. An interrupt-preserving SMP spinlock protects
the controller and request through the short driver operation. STOP uses the
latch directly, outside a queue. Actual status uses the existing snapshot
mutex. The STOP request return alone is not proof of output-off; use the
reported state and DRIVE status. Actual response latency remains unmeasured.

Owner time is resampled under the state lock so a newer other-core submission
cannot falsely appear to come from the future. Driver expiry is latched in
the driver itself, even if the owner holds an older active snapshot.

## Representation and validation

| Field | Range/resolution | Storage and validation |
|---|---|---|
| issued_ms | Device ms modulo2^32 | uint32; age0..99ms accepted; future/stale rejected. |
| sequence |0..65535, unit1 | uint16; forward delta1..32767 accepted modulo65536. |
| left_pct/right_pct | -50..50, unit1 percent | int8 each; positive D1, negative D2, zero coast; others invalid. |
| phase | READY/RUNNING/STOPPED/FAULT | uint8 storage,2 logical bits. |
| reason | NONE/REQUEST/BUTTON/EXPIRED/HARDWARE | uint8 storage,3 logical bits. |
| remaining lease |1..100ms;0 means off | uint8 storage,7 logical bits. |
| flags | true/false | bool storage, one logical bit each; update with whole-state lock. |
| ARM epoch |0..65535, wraps | uint16 status handshake; pending ARM blocks submit. |

Identical duplicates do not renew the original timestamp. Reused sequence with
changed timestamp/duties is CONFLICT. Older/half-range sequences are STALE.
Bad requests do not renew output. A fresh request arriving after a running
command expired latches EXPIRED rather than hiding the gap. Sequence65535->0
and timestamp wrap are tested. The contract assumes short command lifetimes
and less than a half sequence-space gap, not arbitrary multi-wrap replay
protection. This trusted in-process API is not remote authorization.

Host sizes: command8 bytes, control12, output3. Duty needs7 logical bits for101
values and uses addressable int8. Command logical fields total62 bits, fitting
8 bytes; no further whole-byte gain over its native8-byte layout is claimed.
Never send native structures/padding/endianness as MQTT payloads. This feature
LOCAL adds no wire encoding or equivalent-function RAM/message savings claim;
REMOTE's explicit payload is documented separately. Final
Week13 correctness/RAM/message/performance comparisons remain required.

## Output and stopping

Boot is READY/coast. A valid nonzero command becomes RUNNING when applied;
a valid zero pair coasts without clearing a STOP latch. GP21, API STOP and
expiry at age100ms latch STOPPED. LOCAL requires reset; REMOTE permits explicit
owner-side rearm only after coast/revalidation. Hardware/readiness failure
latches FAULT. New or neutral commands cannot clear a latch; first cause wins.

The driver retains10kHz PWM and50% maximum duty. Zero wheels stay SIO-high/11
coast. Other wheels use direct-duty PWM with the other input LOW. Pair changes
coast before reconfiguration. Start drops fixed-LOW before releasing PWM;
STOP raises PWM before the fixed leg, avoiding reverse transients. Same-duty
renewal changes the alarm without resetting PWM or GPIO muxes.

Timer IRQ coasts output if the owner fails to renew. This is independent of
task scheduling, not an external power cutoff; processor/interrupts must still
function. Continuous-output expiry is latched until startup initialization or
REMOTE's explicit guarded owner rearm while coasted.
Renewal checks the previous deadline through rearm so clearing an interrupt
cannot erase expiry. Real clock/IRQ and STOP timing remain measurements.

Encoder warnings remain visible; no counts drive speed/position. Fresh
line/colour data and feedback-quality guards belong to the later integrated
controller. This profile is not a line follower. Existing communication passes
remain supported by their earlier evidence.
