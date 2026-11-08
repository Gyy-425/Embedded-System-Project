# v0.9 PIO clean powered rerun observed in VS Code

The user reports reconnecting power and Pico USB and repeating the run. They
now suspect accidental wheel movement during the earlier session. Computer
Use read the new retained COM3 session and both VS Code terminals. No command,
serial write, firmware upload or robot actuation was issued by the assistant.
This is a separate run from the earlier flagged t31980..42900 session.

## Observed serial evidence

The following selected fields were transcribed from the Serial Monitor UI;
this table is not a complete raw serial export. All listed rows report
ENC_CAPTURE=PIO, PIO=0, ENC_READY=1, capture err=0, PIO_BAD=0/0,
ENC_UNCERTAIN=0, sat=0, Motion err=0, RADIO_READY=1 and WIFI_LINK=3.
The Pico address is 192.168.1.207.

| Uptime ms | Encoder L | Encoder R | Motion | Raw duty L/R | Lease ms | Motion seq / epoch | Output |
| --- | ---: | ---: | --- | --- | ---: | --- | --- |
| 75660 | 0 | 0 | STOPPED | 0/0 | 0 | 0 / 0 | OFF |
| 75920 | 0 | 0 | STOPPED | 0/0 | 0 | 0 / 0 | OFF |
| 76180 | 0 | 0 | STOPPED | 0/0 | 0 | 0 / 0 | OFF |
| 80860 | 0 | 0 | STOPPED | 0/0 | 0 | 0 / 0 | OFF |
| 81120 | -58 | 57 | RUNNING | -50/+50 | 100 | 3 / 1 | ON |
| 81380 | -1238 | 1225 | RUNNING | -50/+50 | 100 | 16 / 1 | ON |
| 81640 | -2561 | 2542 | RUNNING | -50/+50 | 100 | 29 / 1 | ON |
| 81900 | -3889 | 3867 | RUNNING | -50/+50 | 100 | 42 / 1 | ON |
| 82160 | -5217 | 5193 | RUNNING | -50/+50 | 100 | 55 / 1 | ON |
| 82420 | -6548 | 6523 | RUNNING | -50/+50 | 100 | 68 / 1 | ON |
| 82940 | -9209 | 9181 | RUNNING | -50/+50 | 100 | 94 / 1 | ON |
| 83200 | -10242 | 10294 | STOPPED | 0/0 | 0 | 94 / 1 | OFF |
| 83460 | -10642 | 10931 | STOPPED | 0/0 | 0 | 94 / 1 | OFF |
| 83720 | -10652 | 11113 | STOPPED | 0/0 | 0 | 94 / 1 | OFF |
| 83980 | -10652 | 11113 | STOPPED | 0/0 | 0 | 94 / 1 | OFF |
| 84240 | -10652 | 11113 | STOPPED | 0/0 | 0 | 94 / 1 | OFF |
| 87620 | -10652 | 11113 | STOPPED | 0/0 | 0 | 94 / 1 | OFF |
| 87880 | -10652 | 11113 | STOPPED | 0/0 | 0 | 94 / 1 | OFF |
| 88140 | -10652 | 11113 | STOPPED | 0/0 | 0 | 94 / 1 | OFF |
| 88920 | -10652 | 11113 | STOPPED | 0/0 | 0 | 94 / 1 | OFF |

Additional baseline records through t80600 are also zero/clean. The final
stable records retain AB=0/1 and reason=1 (REQUEST). Counts still change after
the first observed output-OFF row, consistent with the driver's coast stop.
No exact physical stopping latency is measured by these periodic records.

## Command and broker evidence

Terminal19 shows the command:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.206 drive --left -50 --right 50 --seconds 2
```

Initial STATUS result=OK, remote=IDLE, motion=STOPPED, DRIVE=OFF, seq=0.
Final STATUS result=OK, remote=STOPPED, motion=STOPPED, DRIVE=OFF,
reason=REQUEST, duties0/0, lease0ms, seq=8. The client prints:
`Motor output OFF confirmed by fresh device status.`
CLI mutation seq8 and serial Motion seq94 are separate counters by design.
The preceding conda activation error did not prevent the Python drive command
from completing successfully. Terminal18 shows Pico telemetry publications,
then broker termination; COM3 is closed at inspection. These endings alone
do not establish an unexpected disconnection during the completed command.

## Scoped assessment

PASS for this basic powered PIO capture trial: clean zero-count baseline,
expected raw forward count progression (left negative/right positive), no
reported invalid transition, saturation or capture/Motion error, fresh output
OFF confirmation and more than three unchanged final count records. Sticky
PIO_BAD remains0/0 at the tail, so no invalid transition was reported during
this boot before those records. No firmware or wiring change is required by
this result; do not replay the same basic test.

The earlier flagged result remains valid historical evidence. Accidental
movement is the user's possible explanation, not a demonstrated cause; normal
manual rotation should also be decoded. A reproducible stationary flag should
be investigated if it recurs, rather than suppressed or repeatedly reset.

This trial does not independently prove every edge was captured, counts per
wheel revolution, speed/distance calibration, speed balance, maximum reliable
edge rate, ground-load behaviour or measured rail stability. Use an independent
marked-revolution reference before calibrated encoder feedback. Coast totals
must not be interpreted as a wheel-speed balance measurement. Existing IR,
MQTT direction, GP21 and broker-loss passes need no repetition.
