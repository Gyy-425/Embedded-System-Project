# PIO encoder capture v0.9: one focused car check

Latest rerun: **basic powered PIO capture trial passed**. The reconnected run
starts clean, retains PIO_BAD=0/0/UNCERTAIN=0 with no errors/saturation, shows
expected count progression and ends with fresh output OFF and stable totals.
[Clean rerun](../testing/integration/logs/2026-11-08-v0_9-pio-clean-rerun.md).
Do not replay this procedure. The earlier pre-movement flag did not recur;
its cause remains unproven. [Earlier UI result](../testing/integration/logs/2026-11-08-v0_9-pio-ui-result.md).
Instructions below are retained for reproduction, not a new required test.

This candidate replaces CPU-serviced GPIO edge counting with a continuously
running PIO counter for each wheel. The raw direction convention remains
unchanged: forward drive and forward encoder deltas are left negative/right
positive. Existing motor output, GP21 STOP, leases and MQTT commands remain.
Independent count accuracy/scaling has not passed yet.

## Upload the normal candidate

1. Close Serial Monitor, switch the car OFF, disconnect bank VIN and Pico USB,
   and wait for the lights to go out.
2. Hold BOOTSEL while connecting Pico USB to the laptop, then release it.
3. Copy `output/firmware/AAI05_Encoder_PIO_v0_9_REMOTE.uf2` onto RPI-RP2.
   Use the normal image, not the `_TEST` compile variant.
4. Support the chassis securely with both drive wheels clear. Use the existing
   bank-to-VIN and laptop-to-Pico USB connections, with the switch accessible.
   No wiring change is required.

## Check the new capture status

Open Serial Monitor on the actual Pico COM port at 115200. The banner must say
`AAI05 MQTT MOTION v0.9: PIO ENCODER CAPTURE`. Serial records must include:

```text
ENC_CAPTURE=PIO PIO=0 ENC_READY=1 err=0 PIO_BAD=0/0 ENC_UNCERTAIN=0
```

`PIO=0` or `PIO=1` names the allocated hardware block; either is valid.
Require `sat=0`, no capture/Motion error and `DRIVE=OFF` before the request.
If initialization/readiness fails or PIO_BAD is nonzero while stationary,
preserve three full records and stop here. Do not increase duty or rewire.

The old `irq`, `over` and GPIO `bad` counters are deliberately not printed in
this profile because GPIO edge IRQs are not used. Their absence does not by
itself demonstrate that capture is accurate. `PIO_BAD=L/R` instead reports a
sticky diagonal-transition flag for each wheel, not an exact error count.
The encoder AB display is a GPIO level diagnostic, not an atomic phase sample
paired with the returned PIO count.

## Use the existing broker

PowerShell working directory:

```powershell
Set-Location -LiteralPath 'C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware'
```

The candidate uses laptop broker 192.168.1.206. The Pico has its own IP. If the
existing broker is already running, keep it. Otherwise, in terminal A:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\mqtt\Start-Broker.ps1 -BindAddress 192.168.1.206
```

In terminal B, from the same folder:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.206 status
```

Proceed only with a fresh matching status and output OFF. If the laptop IP
has changed since this build, resolve the broker configuration before moving.
No separate watch client or repeated MQTT loss/recovery exercise is needed.

## Capture one forward run

1. Preserve three complete stationary serial records, labelled BEFORE. Leave
   the wheels untouched. Counts need not be zero; PIO_BAD must be 0/0.
2. Run once in terminal B:

```powershell
$runLog = ".\testing\integration\logs\pio-forward-$(Get-Date -Format 'yyyyMMdd-HHmmss').txt"
python .\tools\mqtt\drive_cli.py --host 192.168.1.206 drive --left -50 --right 50 --seconds 2 2>&1 | Tee-Object -FilePath $runLog
```

3. Let the bounded request finish. Preserve every RUNNING serial record.
   GP21 remains available to stop; GP20 is disabled in REMOTE. Do not use the
   carrier's direct motor buttons during the software request. If either
   wheel stalls or reporting freezes, switch OFF and retain the last output.
4. Once output is OFF and the wheels finish coasting, preserve at least three
   consecutive records with unchanged counts, labelled AFTER. If the counts
   continue changing, preserve those records too and report the observation.
5. Send BEFORE, DURING, AFTER and the command output. The command's Tee log
   saves CLI output only; copy/save the serial records separately before
   closing VS Code. Do not power-cycle between the three capture groups.

Acceptance for this focused run: both forward encoder signs remain correct,
PIO_BAD stays 0/0, sat=0, capture/Motion errors=0 and fresh output OFF follows the
request. This would support the new capture path in this setup. An independent
marked-revolution count reference is still needed before claiming accuracy or
using calibrated distance/speed feedback. Do not infer balance from coast totals.

## Implementation limits and reproduction

The Pico SDK allocator reserves all four SM claims in one unused PIO block;
SM0/1 actively decode the wheels. The other block is left to Wi-Fi. Startup
fails visibly if a whole block cannot be claimed. No app PIO reset is issued;
the existing Wi-Fi-enabled BSP already releases PIO0/1 reset. The program is
26 words at origin0, adapted from Raspberry Pi's BSD-3-Clause example:
https://github.com/raspberrypi/pico-examples/tree/master/pio/quadrature_encoder
The license is retained under licenses/.

The state-machine instruction clock is 1 MHz; the longest decoder loop is
10 instructions between GPIO samples. Full RX FIFO discards snapshots
without stalling hardware counting. A depth-plus-one read obtains a newer
snapshot, with a 20 ms per-wheel time bound plus iteration guard. Unexpected SM
disable, missing snapshots or impossible modular deltas latch a capture error;
Motion stops and later commands cannot bypass the fault. A restart after fault
requires resolving it and rebooting; reconnect never automatically moves.

Counters wrap as unsigned 32-bit values in hardware and are accumulated through
int64_t into the existing saturating int32_t view. The caller must read within fewer than
2^31 net edges; a half-range delta is rejected. Sticky flags cannot report the
number of invalid transitions, and arbitrarily fast complete cycles can alias.
Actual maximum reliable motor speed and electrical noise remain measured
properties. No PID, real tape calibration or colour following is added here.

Build in WSL using the existing SDK; no downloads or package install required:

```text
python3 tools/check_encoder_pio.py --label UNIQUE --sdk <SDK2.3.0> --firmware
```

If WSL lacks tkinter, pass `--debug-python <Windows-python.exe>` for the six
existing Tcl debugger tests. Remaining C/Python tests run in WSL, with fatal
ASan/UBSan. The runner checks the generated PIO header against SDK pioasm,
builds normal/TEST PIO and default GPIO compatibility variants, and verifies
that all pre-existing UF2 hashes are unchanged. It never uploads or actuates.
