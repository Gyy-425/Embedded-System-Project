# Paired motor qualification: observed directions and unresolved encoder quality

Superseding follow-up: the user subsequently supplied forward-run serial
records. They establish raw forward encoder signs left negative/right positive,
requested output OFF and new overrun warnings within the powered interval.
[Current forward-run assessment](2026-11-08-forward-encoder-result.md).
The original observations and evidence gaps below are retained chronologically.

8 November 2026. User ran the raised-wheel paired test and requested inspection
of the retained VS Code terminals and Serial Monitor. The assistant read those
views using the Computer Use skill and read the saved CLI log. No command was
sent to the robot by the assistant.

## Physical directions reported by the user

For the requested raw electrical duties `left=+50, right=+50`, both wheels moved.
The user described both as clockwise, and supplied the unambiguous physical
reference: the left wheel drove toward the rear; the right wheel drove toward
the front. Clockwise alone is not a portable direction convention for mirrored
motors viewed from different sides.

| Wheel | Positive raw duty: observed physical direction | Raw duty for forward: user-confirmed |
|---|---|---|
| Left / M1 | Rearward | Negative |
| Right / M2 | Forward | Positive |

Logical forward control must therefore map the left motor with sign -1 and the
right with sign +1. In the follow-up the user tested `left=-50, right=+50` and
confirmed both wheels moving toward the front. This establishes the physical
forward polarity; do not repeat that direction check. No new serial samples
were supplied or verified for the follow-up, so encoder signs, warning deltas
and count accuracy remain unresolved. Keep the existing wiring and the raw-duty
diagnostic/protocol contract.
Any later logical-forward API must apply the conversion once at a defined
boundary; do not silently redefine existing MQTT electrical duties. Raw encoder
signs for physical forward must be checked independently before normalizing
feedback.

## Saved command result

[Original saved CLI output](paired-drive-20261108-145309.txt) contains:

```text
ACK STATUS result=OK request=29896 remote=STOPPED motion=STOPPED DRIVE=OFF reason=REQUEST duty L=+0% R=+0% lease=0ms target=unset seq=16 tag=267901aa
Bounded electrical-duty request; no automatic restart.
ACK STATUS result=OK request=26530 remote=STOPPED motion=STOPPED DRIVE=OFF reason=REQUEST duty L=+0% R=+0% lease=0ms target=unset seq=24 tag=267901aa
Motor output OFF confirmed by fresh device status.
```

The VS Code command text showed `--host 192.168.1.206 drive --left 50 --right 50
--seconds 2`. The saved CLI output confirms a fresh off-state response after
the request. It does not contain intermediate encoder samples or independently
measure physical stopping time.

## Retained serial observations

The Serial Monitor retained these snapshots on the same increasing uptime:

| Field | Earlier retained sample | Final retained sample |
|---|---|---|
| t, ms | 264160 | 300040 |
| ENC L/R | 10899 / 11439 | 42382 / 44750 |
| IRQ | 22050 | 86039 |
| bad L/R | 0 / 0 | 0 / 0 |
| over L/R | 22 / 22 | 102 / 89 |
| sat | 0 | 0 |
| ENC_READY / capture err | 1 / 0 | 1 / 0 |
| ENC_UNCERTAIN | 1 | 1 |
| Motion | STOPPED, seq98, epoch1, err0 | STOPPED, seq91, epoch4, err0 |
| Output | OFF | OFF |

The Pico reported WIFI_IP=192.168.1.207; this is a separate device from the
laptop broker at .206. At the last view COM3 was closed and the broker terminal
showed termination. Do not infer an unintended disconnect or fault from that
ending alone.

Across this retained interval, counts changed by +31483 / +33311 and overrun
counters increased by 80 / 67. The interval includes different motion epochs;
these deltas cannot be assigned exclusively to the single saved two-second
request. Intermediate RUNNING serial samples were not extracted. There are
new ambiguity warnings in the available record, so powered encoder feedback
and count accuracy remain unqualified. `over` counts ambiguous batches, not
an exact number of missed edges; the record does not establish the underlying
cause or justify estimating speed/PPR from these totals.

## Next focused action

Review the follow-up's stationary, moving and stopped serial samples to isolate
per-run encoder signs and new bad/over deltas. Physical forward direction has
passed by the user's observation; no repeat direction test or rewiring is
needed. Investigate the capture path if ambiguity counters increase; do not use
these counts as qualified PID feedback yet. If the samples were not preserved,
design the next capture-quality run around that missing evidence rather than
replaying the earlier basic motor/MQTT cases.
