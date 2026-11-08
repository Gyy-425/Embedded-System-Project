# v0.9 PIO car run inspected in VS Code: startup flag prevents qualification

The user requested Computer Use inspection of VS Code after the v0.9 run.
Read the retained Serial Monitor and drive-client/broker terminals. No robot
command, serial transmission, power change or firmware upload was performed
by the assistant. This is an observed partial result, not a full capture pass.

## Serial observations

The retained COM3 session begins with zero counts, already-set right invalid
flag and no motor request. Representative full records read from the UI:

```text
t=31980 IR[L=3501 R=3535 valid=1 err=0 age=10] ENC[L=0 R=0 AB=0/3 sat=0 age=10] ENC_CAPTURE=PIO PIO=0 ENC_READY=1 err=0 PIO_BAD=0/1 ENC_UNCERTAIN=1 MOTION[state=STOPPED L=0 R=0 lease=0 seq=0 epoch=0 reason=1 cutoff=0 err=0] DRIVE=OFF RADIO_READY=1 NETWORK_MODE=REMOTE_MOTION WIFI_LINK=3 WIFI_IP=192.168.1.207
t=36140 IR[L=3681 R=3681 valid=1 err=0 age=10] ENC[L=-352 R=351 AB=0/1 sat=0 age=10] ENC_CAPTURE=PIO PIO=0 ENC_READY=1 err=0 PIO_BAD=0/1 ENC_UNCERTAIN=1 MOTION[state=RUNNING L=-50 R=50 lease=100 seq=7 epoch=1 reason=0 cutoff=0 err=0] DRIVE=ON RADIO_READY=1 NETWORK_MODE=REMOTE_MOTION WIFI_LINK=3 WIFI_IP=192.168.1.207
t=36400 IR[L=3732 R=3707 valid=1 err=0 age=10] ENC[L=-1637 R=1624 AB=2/3 sat=0 age=10] ENC_CAPTURE=PIO PIO=0 ENC_READY=1 err=0 PIO_BAD=0/1 ENC_UNCERTAIN=1 MOTION[state=RUNNING L=-50 R=50 lease=100 seq=20 epoch=1 reason=0 cutoff=0 err=0] DRIVE=ON RADIO_READY=1 NETWORK_MODE=REMOTE_MOTION WIFI_LINK=3 WIFI_IP=192.168.1.207
t=36920 IR[L=3743 R=3790 valid=1 err=0 age=10] ENC[L=-4274 R=4252 AB=3/3 sat=0 age=10] ENC_CAPTURE=PIO PIO=0 ENC_READY=1 err=0 PIO_BAD=0/1 ENC_UNCERTAIN=1 MOTION[state=RUNNING L=-50 R=50 lease=100 seq=46 epoch=1 reason=0 cutoff=0 err=0] DRIVE=ON RADIO_READY=1 NETWORK_MODE=REMOTE_MOTION WIFI_LINK=3 WIFI_IP=192.168.1.207
t=42900 IR[L=3749 R=3780 valid=1 err=0 age=10] ENC[L=-10980 R=11440 AB=0/3 sat=0 age=10] ENC_CAPTURE=PIO PIO=0 ENC_READY=1 err=0 PIO_BAD=0/1 ENC_UNCERTAIN=1 MOTION[state=STOPPED L=0 R=0 lease=0 seq=98 epoch=1 reason=1 cutoff=0 err=0] DRIVE=OFF RADIO_READY=1 NETWORK_MODE=REMOTE_MOTION WIFI_LINK=3 WIFI_IP=192.168.1.207
```

Stationary baseline repeats through t35880 with counts0/0 and PIO_BAD0/1.
Final records at t42120,42380,42640 and42900 all retain counts-10980/+11440,
same STOPPED/OFF state, errors0, sat0 and PIO_BAD0/1. Thus the retained tail
has settled count totals. Intermediate moving records also show left-negative
and right-positive count progression, with no reported capture API/driver error.

The user previously reported PIO_BAD0/1 at t81120, with zero counts/epoch0,
and confirmed wheels/leads untouched for that observation. They then reported
a restart clearing the flag and asked to continue. The later retained record
inspected here nevertheless has the flag before its first request. Preserve
that distinction; do not declare all restarts clean or identify a physical
cause from the flag alone.

## Command result

Terminal16 shows a two-second raw left-50/right+50 command to broker206,
initial STATUS OK/IDLE/STOPPED/OFF and final STATUS OK/STOPPED/OFF/REQUEST,
followed by `Motor output OFF confirmed by fresh device status.`
The final CLI mutation sequence is8; the serial Motion feed sequence is98.
These are different counters by design: mqtt_drive_app.c reports the last
remote mutation sequence, while drive_control_poll increments motion_sequence
when it submits internal periodic renewals. They need not match.

A preceding conda activation error did not prevent the later Python drive
command from completing and receiving device acknowledgements. The broker
terminal ends with termination and COM3 is closed in the inspected view;
do not infer an unexpected failure from those endings alone.

## Assessment and next action

Supported: raw forward signs, powered count progression, no reported runtime
capture/driver error or saturation, fresh requested output OFF and a stable
stopped tail. Not supported: clean PIO capture, calibrated count accuracy,
balanced speeds or a measured physical stop latency.

The right flag was already1 before movement. It cannot reveal additional
invalid transitions while latched. This run therefore does not establish
whether powered movement caused new invalid transitions, and is not a clean
capture acceptance pass. Do not replay motion merely to clear a sticky flag
or suppress the diagnostic. The next investigation should isolate initial
phase establishment and the first invalid event while stopped; distinguish
a startup software effect from electrical transitions with evidence before
changing wiring or selecting a fix. No production code changed in this review.
