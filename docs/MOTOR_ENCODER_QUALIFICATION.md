# Motor and encoder qualification — first paired check

Latest result: the user supplied the forward run's serial records. Physical
forward is raw left negative/right positive, and forward encoder deltas have
the same signs. Requested output OFF is observed without reported driver or
capture API errors. Powered capture quality is **not qualified**: overrun
counters rise from 0/0 to 32/30 while moving, then 39/36 during spin-down.
[Forward result and code trace](../testing/integration/logs/2026-11-08-forward-encoder-result.md).
No more basic direction/pulse tests are needed. Address capture quality next
before accepting speed feedback; the procedures below are historical references.

8 November 2026. The current laptop LAN address was read outside the command
sandbox: 192.168.1.206, connected to the existing SINGTEL-A9C0_MLO network.
The previous car image targets broker 192.168.1.199. A separate address-updated
v0.8 image is prepared; only the ignored broker setting changes. The original
images, motor/encoder logic, PWM limit and STOP protections are preserved.

The first paired request was performed: both wheels moved, and a fresh output-off
response was saved. Positive raw duty drove the left wheel rearward and the
right wheel forward. Powered encoder quality remains unresolved because the
retained record contains new overrun warnings. [Observed result and limits](../testing/integration/logs/2026-11-08-paired-motor-qualification-result.md).
Do not repeat the four basic pulses, manual input checks or earlier MQTT tests.

## Confirmed forward mapping; capture quality unresolved

The user tested the following command and confirmed both wheels moving toward
the front. Physical forward polarity is now established: left negative, right
positive. Do not repeat the direction check. The command is retained as the
known forward reference for this raw-duty interface:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.206 drive --left -50 --right 50 --seconds 2
```

The completed run's serial lines have now been reviewed. Forward counts decrease
on the left and increase on the right; new overrun warnings occur in the moving
interval. `bad=0` and `sat=0` do not establish accuracy because ambiguous batches
are skipped. No new run is requested until the capture investigation justifies
a focused validation. GP21 stops. No wiring change is indicated by these logs.
The original first-polarity procedure below is retained as history, not a request
to repeat `+50/+50`. Existing CLI duties remain electrical signs.

## 1. Load the address-updated working image

Support the chassis securely so both wheels are clear and the power switch is
reachable. Disconnect bank power and Pico USB; wait for the lights to go out.
Hold BOOTSEL while connecting laptop USB, then release it. Copy:

`output/firmware/AAI05_Motor_Qualification_v0_8_BROKER206.uf2`

Use the established bank-to-VIN plus laptop-to-Pico USB arrangement. Keep both
wheels raised. Wiring and encoder A/B pins do not change. No new photos,
multimeter or tape samples are required for this first check.

## 2. Start the broker and serial recording

Open PowerShell in `C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware`.
In terminal A:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\mqtt\Start-Broker.ps1 -BindAddress 192.168.1.206
```

Open the Pico's actual COM port in VS Code Serial Monitor at 115200. The old
port was COM3; select the port actually present today. The Pico has its own
Wi-Fi IP; that IP does not have to equal the laptop's broker address.

In terminal B:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.206 status
```

Require a matching STATUS and DRIVE OFF before proceeding. Serial should show
`ENC_READY=1`, capture/motor `err=0`, `sat=0` and a healthy stopped/idle state.
If connection/status fails, retain the error and serial lines; do not request
movement. No extra MQTT watch process is necessary for this case.

Keep three serial lines from before the test with the wheels stationary.
Counters may already be nonzero; we compare changes, not absolute totals.

## 3. One simultaneous two-second request

In terminal B, run once:

```powershell
$runLog = ".\testing\integration\logs\paired-drive-$(Get-Date -Format 'yyyyMMdd-HHmmss').txt"
python .\tools\mqtt\drive_cli.py --host 192.168.1.206 drive --left 50 --right 50 --seconds 2 2>&1 | Tee-Object -FilePath $runLog
```

Positive 50 means electrical duty/polarity, not yet physical forward. Observe
each wheel relative to the robot's intended front, not as clockwise viewed
from an unspecified side: the top tread moving toward the front is the wheel's
forward-driving direction. Report left and right independently. Keep hands and
loose leads clear of the wheels; do not operate the carrier's four direct
motor buttons during software output. GP20 is disabled in REMOTE; GP21 stops.
If a wheel stalls, reporting freezes or behavior is unexpected, switch the car
off and retain the last output rather than increasing duty/repeating the run.

Expect both wheels to turn and the request to end with fresh output-off
confirmation. Preserve all RUNNING serial lines and at least three STOPPED
lines afterward, allowing wheel coast to finish. Keep VS Code open until the
results are reviewed or paste/save those lines before closing it.

## 4. What we will qualify from the evidence

| Observation | Meaning |
|---|---|
| Left/right top tread toward front or rear | Map each physical motor's positive command sign. Correct signs later in software; do not swap encoder supply/A/B wiring based on this alone. |
| Before/after ENC L/R | Signed count changes while each wheel is visibly moving. Opposite raw signs or unequal magnitudes are not automatically wiring faults. |
| Before/after bad and over | New invalid transitions or ambiguous edge batches. `over` is not the exact number of lost edges. Any increase means the counts are not yet qualified feedback. |
| ENC_READY/err/sat and Motion err | Readiness, driver/capture faults or saturation. These must not be hidden by quality-warning handling. |
| Both wheels stop; final DRIVE OFF | Coordinated output/stopping in this raised-wheel setup; not a measured stopping distance. |
| Continuous uptime/reporting | No observed reset/freeze during this case; not a measured power-rail or full-ground-load pass. |

`ENC_UNCERTAIN` is sticky if earlier counters are nonzero; it may stay 1 even
when no new error occurs. Compare the counter deltas for this run. A zero
increase is useful capture-quality evidence but does not prove count accuracy
or counts per wheel revolution. Do not infer speed/distance/PID gains from this
short run. After reviewing it we will choose the next required check, such as
an independent marked-revolution reference or a capture-path fix, without
blindly repeating the motor/MQTT tests already passed.
