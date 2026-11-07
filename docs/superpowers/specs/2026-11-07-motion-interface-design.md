# Motion command interface and bounded local exercise

The user approved moving from the passed four-pulse test to the normal Motion
interface with controlled running, STOP and expiry. Continue in the existing
feature checkout, using C/micro T-Kernel and BARR-C. The user requests practical
checks and autonomous development, so no repeated planning approval is needed.

## Chosen scope

One task owns motor registers. A typed latest-command interface accepts
independent signed electrical duties -50..50 percent, one-percent resolution,
with uint32 issued milliseconds and uint16 sequence. Positive selects D1;
negative D2. These are electrical polarities, not calibrated physical forward
or wheel speed. Zero coasts that wheel. No encoder feedback is used.

Commands expire at age100ms, including equality. Reject future/stale timestamps,
invalid duties, duplicate conflicts and out-of-order sequences. Identical
duplicates do not renew their original timestamp. Forward sequence delta1..32767
is accepted modulo65536. Never serialize native structures as MQTT payloads.

READY accepts a fresh request; RUNNING applies it. GP21, explicit STOP, command
expiry or an expired hardware lease latch STOPPED until reset. Hardware/startup
failure latches FAULT. Neither new requests nor zero-duty requests clear a
latch. Boot is idle. Producer/consumer access is synchronized; STOP does not
wait behind a queue. A request is not an acknowledgement that outputs are off:
the Motion owner publishes that outcome. The100ms independent hardware lease
bounds output if the owner stalls; actual task/STOP latency needs measurement.

Extend the existing ordered PWM driver for both wheels, preserving old pulse
and no-drive profiles. Refreshing unchanged duties only renews the alarm;
changing duties coasts before reconfiguration. Keep10kHz and the50% limit.
Only the Motion owner calls this API in the new profile.

## Alternatives and delivery

Chosen: portable controller plus synchronized task adapter and a local exercise
image. Extending MQTT actuation now would also require retained/publication,
session/replay and authorization rules, so it is a later separate case.
Replacing the capture backend now would mix the encoder-accuracy investigation
with a command-interface feature. Encoder warnings remain explicit and cannot
qualify closed-loop control.

The new image is opt-in, distinct v0.7 LOCAL firmware. One fresh GP20 press runs
left D1/50%, right coast, while a local producer renews commands for two seconds.
When renewal ceases, expiry stops and latches. A separate reset/run can check
GP21 during the longer window. No boot-held start, automatic restart, wireless
movement or claim of autonomous tracking. Existing v0.6.1/v0.5 images remain.

New C follows Allman braces, four spaces, fixed-width integers, prefixed symbols
and private g_/pointer p_ names. Document native sizes/padding and field ranges;
do not claim measured representation savings or physical results from host tests.
