# Ground straight requests: powered rightward drift observed

User ran the new CLI straight request twice and reports approximately forward
travel followed by a rightward turn. They clarified that turning occurs while
still powered. Computer Use inspected the retained VS Code drive-client,
broker and COM3 Serial Monitor records. No robot command, serial transmission,
firmware upload or production-code change was made by the assistant.

An accidental empty terminal split opened during UI navigation; only that new
empty shell was closed, and both original terminal histories were preserved.

## Command observations

Both runs use broker192.168.1.206:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.206 move --forward 40 --turn 0 --seconds 1
```

Both print requested forward40/turn0 -> raw L=-40/R=+40. The first starts from
IDLE/OFF, mutationseq0, tag74ba92c4, and ends with STATUS OK/STOPPED/OFF,
reasonREQUEST, duties0/0, lease0, mutationseq4 and fresh off-confirmed message.
The second starts STOPPED/OFF, mutationseq4, same tag, then ends:
`error: No matching device ACK; command outcome unconfirmed`.
The user is unsure whether broker/Pico shutdown happened before that return.
The broker terminal ends with termination and COM3 is closed at inspection.
Those endings do not establish why an individual ACK was missing.

## Selected serial fields transcribed from the UI

This is not a complete raw export. Listed rows have captureReady1, capture
err0, PIO_BAD0/0, ENC_UNCERTAIN0, sat0, Motion err0 and WIFI_LINK3; PicoIP207.
RUNNING rows show raw-40/+40, lease100, reason0; STOPPED rows show duties0/0,
lease0, reason1 (REQUEST), output OFF. Uptime continues without an observed reset.

| Uptime ms | Encoder L | Encoder R | Motion | Seq | Epoch |
| ---: | ---: | ---: | --- | ---: | ---: |
| 273260 | -2 | 1 | STOPPED | 0 | 0 |
| 279500 | -1 | 1 | STOPPED | 0 | 0 |
| 280800 | -1 | 1 | STOPPED | 0 | 0 |
| 281060 | -1 | 1 | STOPPED | 0 | 0 |
| 281320 | -353 | 403 | RUNNING | 9 | 1 |
| 281580 | -1308 | 1315 | RUNNING | 22 | 1 |
| 281840 | -2314 | 2183 | RUNNING | 35 | 1 |
| 282100 | -3312 | 3054 | STOPPED | 46 | 1 |
| 282360 | -3517 | 3302 | STOPPED | 46 | 1 |
| 282620 | -3517 | 3302 | STOPPED | 46 | 1 |
| 282880 | -3517 | 3302 | STOPPED | 46 | 1 |
| 283140 | -3512 | 3302 | STOPPED | 46 | 1 |
| 283400 | -3510 | 3302 | STOPPED | 46 | 1 |
| 285740 | -3510 | 3303 | STOPPED | 46 | 1 |
| 286000 | -3510 | 3304 | STOPPED | 46 | 1 |
| 291720 | -3510 | 3303 | STOPPED | 46 | 1 |
| 291980 | -3510 | 3303 | STOPPED | 46 | 1 |
| 292240 | -3510 | 3303 | STOPPED | 46 | 1 |
| 292500 | -3510 | 3303 | RUNNING | 1 | 2 |
| 292760 | -4219 | 4073 | RUNNING | 14 | 2 |
| 293020 | -5198 | 4973 | RUNNING | 27 | 2 |
| 293280 | -6207 | 5857 | RUNNING | 40 | 2 |
| 293540 | -7164 | 6723 | STOPPED | 47 | 2 |
| 293800 | -7428 | 6933 | STOPPED | 47 | 2 |
| 294060 | -7428 | 6933 | STOPPED | 47 | 2 |
| 299260 | -7428 | 6933 | STOPPED | 47 | 2 |
| 299520 | -7428 | 6933 | STOPPED | 47 | 2 |
| 299780 | -7428 | 6933 | STOPPED | 47 | 2 |
| 300040 | -7428 | 6933 | STOPPED | 47 | 2 |

Small count changes while stopped/repositioning are retained without assigning
a cause. Powered comparisons below exclude those and all output-OFF/coast rows.

## Assessment

Two comparable520ms intervals entirely within observed RUNNING output:

| Run | Interval ms | Forward left tick gain | Right tick gain | Left excess relative to right |
| --- | --- | ---: | ---: | ---: |
| 1 | 281320..281840 | 1961 | 1780 | 10.17% |
| 2 | 292760..293280 | 1988 | 1784 | 11.43% |

The late260ms gains are1006/868 in run1 and1009/884 in run2. Equal requested
duty therefore gives unequal raw encoder response in both runs, consistent
with the user's powered rightward drift if wheel/encoder scaling is comparable.
Count/revolution, wheel size/slip, surface, caster resistance and USB cable drag
have not been independently quantified. This is not calibrated ground speed
or proof of an intrinsic motor fault. Coast totals must not be used as speeds.

Ground forward starting/movement and requested output OFF are supported;
straightness remains to be tuned. Clean sticky PIO flags/no reported errors
are retained. The second client transaction is incomplete evidence even though
its serial run reaches REQUEST/OFF and settled counts. The terse error does
not identify which ACK stage failed. No cause or software fix is established.

## One practical next trial

Keep the existing wiring/image, same floor/setup, and ensure the USB cable is
slack and not pulling the car. With broker/Pico left on until command return,
use one small left compensation through the existing interface:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.206 move --forward 40 --turn -2 --seconds 1
```

This requests raw L=-38/R=+42. It is a bounded initial tuning trial, not an
accepted permanent trim or a measured speed-controller gain. It lowers the
left request and raises the right slightly, within the existing50% limit.
Preserve full command output and powered/settled serial rows. Report whether
the powered right drift decreases; keep the curve tests pending until this
trial is understood. If another ACK error occurs, retain it for stage-specific
client/network diagnosis rather than declaring the command successful or
replaying old connectivity/pulse cases. No new firmware or physical rerun was
performed in this inspection task.
