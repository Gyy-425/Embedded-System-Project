# Logical forward and steering motion

This focused addition prepares a C interface for the later IR controller and
a `move` CLI command for practical ground testing. It uses the existing Motion
owner, raw motor driver and MQTT v2 START/renew/STOP flow. The car's tested
v0.9 PIO image can remain installed: the CLI performs coordinate conversion
before sending the existing raw requests. The new on-robot C entry point is
host-tested and included in compile-compatibility builds; no live IR controller
calls it yet. The user confirms three straight neutral powered ground runs
and both requested curve directions, with a small residual stopping movement.
Basic motion bring-up is complete; precise heading/stop performance,
calibrated speed and live IR tracking remain unverified.

## Coordinates and limits

| Input | Range | Meaning |
| --- | --- | --- |
| forward_pct / --forward | C0..50; CLI1..50 | Forward duty request, not cm/s |
| turn_pct / --turn | -50..50 | Positive turns right, negative turns left |
| --seconds | >0..10; default2 | Bounded request duration, including START confirmation |

CLI defaults are forward30, turn0, seconds2. Both wheels are commanded forward
or stopped. Turn magnitude is limited to forward, so this API does not reverse
a wheel or pivot. Logical wheel requests are L=forward+turn and R=forward-turn.
If either exceeds50, both share a scale factor to fit the limit, truncating
nonnegative integer results. Raw polarity is then left-negative/right-positive.
This is duty mixing, not a calibrated turn angle, distance or turning radius.

| Forward | Turn | Raw L | Raw R | Intended movement |
| ---: | ---: | ---: | ---: | --- |
| 40 | 0 | -40 | +40 | Straight forward request |
| 40 | +10 | -50 | +30 | Right curve request |
| 40 | -10 | -30 | +50 | Left curve request |
| 40 | +20 | -50 | +16 | Stronger right request, common scaling |
| 10 | +50 | -20 | 0 | Right request limited to stop inner wheel |
| 0 | +50 | 0 | 0 | Neutral C request; no pivot |

Actual minimum moving duty and curve response depend on the loaded car and
are still to be established. There is no guessed trim, speed feedback or ramp.
Use `stop` for latched STOP. C zero is neutral and allows later fresh movement;
CLI zero-forward requests are rejected before opening the network bridge.
The old `drive --left ... --right ...` continues to mean raw electrical duties.

## C API for the future controller

Include `app_program/subsystem2/motion_runtime.h` and call from the one logical
producer task:

```c
motion_chassis_command_t request = {
    .issued_ms = now_ms,
    .sequence = sequence,
    .forward_pct = 40U,
    .turn_pct = 10};
motion_result_t result = subsystem2_motion_submit_chassis(&request);
```

An accepted request is stored, not immediately applied. The existing Motion
owner applies it. The producer must handle the returned result and issue
fresh commands before the100ms expiry; expired output never silently restarts.
Raw and logical submissions share the same sequence domain and ownership:
switching APIs does not create a second writer or reset sequence checking.
The later normal IR controller must replace/arbitrate the diagnostic remote
producer; it must not run concurrently as another independent producer.

Pure `motion_chassis_convert(request, raw)` performs no IO or synchronization.
Both input coordinates are signed32-bit computation values, range-checked
before narrowing. This rejects296 or negative forward values at the C boundary;
raw/stored motor duties remain signed8-bit. The wider request is temporary
validation input, not a new stored slot or wire representation.
It returns BAD_COMMAND for null/out-of-range input, leaving the caller's output
unchanged; otherwise it preserves timestamp and sequence exactly. The runtime
wrapper then uses the existing readiness, pending-arm, stale/future timestamp,
duplicate/conflict, STOP, fault and rearm checks. Rejected requests do not stop
or renew an earlier valid request. An intentional stop uses
`subsystem2_motion_stop`, not malformed input. The caller owns its request and
output memory; do not mutate it concurrently with conversion/submission.

This typed structure is not a native MQTT payload. No new wire encoding or
minimum-bit saving is claimed. Existing v2 serialization and leases are reused.

## Ground-run procedure and current observations

Latest8November: the user explicitly confirms +10 curves right and -10 curves
left, both correct, with the same small stopping movement. Together with the
three neutral straight trials, this completes basic ground motion bring-up.
Keep turn0 and the current image/wiring; proceed to IR calibration/runtime
integration. Exact stop/heading behaviour remains later measured acceptance.
[Completed scoped result](../testing/integration/logs/2026-11-08-ground-curves-result.md).
The commands below are now reproduction references, not required replays.

Previous neutral observation: three untethered runs travel straight under power,
according to the user, with a short left movement after output switches off.
Current client logs show fresh status/OFF confirmations. Keep turn0 as the
straight baseline; the next checks are one right curve and one left curve.
Precise stopping behaviour remains unmeasured. [Latest result](../testing/integration/logs/2026-11-08-neutral-ground-result.md).

With the same bank-only/USB-disconnected setup and broker running, perform once
each, allowing output OFF and physical standstill between requests:

```powershell
python "C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware\tools\mqtt\drive_cli.py" --host 192.168.1.206 move --forward 40 --turn 10 --seconds 1
python "C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware\tools\mqtt\drive_cli.py" --host 192.168.1.206 move --forward 40 --turn -10 --seconds 1
```

First should curve right, second left. Preserve command output and report
powered direction/stalling separately from post-OFF motion. Earlier connection
failures remain unexplained; a missing ACK requires standstill via GP21/power
switch and preserved output, not an automatic motion retry. The neutral run
below is now historical reproduction context, not another required replay.

Earlier observation: the user reported two bank-powered, laptop-USB-disconnected
runs turning left near the end, then switches OFF. The preceding requested
command was forward40/turn-2/seconds1; exact new arguments/ACK logs are not
independently verified. Assuming that command was used, the provisional left
compensation may be excessive. Next establish the neutral untethered baseline:
forward40/turn0/seconds1, same setup. No further trim or firmware change.
[Untethered report](../testing/integration/logs/2026-11-08-untethered-left-drift-result.md).

Previous cable-supported observation: the user identified USB cable pull.
Supporting it gave mostly straight travel, with an end-of-run right turn.
Latest recorded turn-2 command has3.67% left excess in a powered520ms interval,
then275 left/50 right extra ticks after the first output-OFF row. The last
client confirms STOP; an intervening request has unconfirmed ACK/STOP. These
are not controlled motor-calibration comparisons. Keep further/permanent trim
provisional. [Latest evidence](../testing/integration/logs/2026-11-08-cable-supported-motion-result.md).

The completed neutral baseline used the bank-only, USB-disconnected setup and
zero steering:

1. Start/keep the existing laptop broker running. Close Serial Monitor and
   power the car from the existing bank-to-VIN lead only, with Pico USB
   unplugged from the laptop. Keep the switch/GP21 accessible and floor clear.
2. Obtain fresh MQTT status showing ready and output OFF, then run once:

```powershell
python "C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware\tools\mqtt\drive_cli.py" --host 192.168.1.206 status
python "C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware\tools\mqtt\drive_cli.py" --host 192.168.1.206 move --forward 40 --turn 0 --seconds 1
```

3. Observe forward travel and end-of-run yaw. Wait for fresh output-OFF
   confirmation and wheel standstill. If an ACK is missing, use GP21 or the
   power switch to ensure standstill and retain the error instead of repeating.
4. Once stationary, USB can be reconnected while bank power stays on. Let old
   buffered output drain, then save three fresh serial diagnostic rows with
   advancing timestamps and STOPPED/OFF. USB serial is unavailable during the
   untethered movement; current MQTT telemetry does not include encoder counts.

Removing laptop USB also changes the previous dual-power arrangement to
bank-only. This checks the intended untethered setup rather than isolating
mechanical drag alone. No firmware change is needed. The USB console's bounded ring discards when
full and does not wait for a host. Bank-only movement is user-reported; neutral
straightness and load qualification remain open. Keep curve checks pending. The earlier
tethered procedure below remains a reproduction reference, not another required
straightness rerun with the laptop cable attached.

Keep the current wiring and installed v0.9 image. Use the network and power
arrangement from the clean PIO run, with sufficient Pico USB cable slack and
a clear floor area for a short movement. Keep the power switch reachable;
GP21 remains STOP. Remove temporary wheel supports for this ground check.
Open the actual Pico serial port at115200 and keep its records. Require
`ENC_READY=1`, `PIO_BAD=0/0`, `ENC_UNCERTAIN=0`, errors0/sat0 and output OFF.
If a stationary invalid flag recurs, preserve the log instead of suppressing it.

PowerShell directory:

```powershell
Set-Location -LiteralPath 'C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware'
```

Use the existing laptop broker. Its configured address in the tested image
is192.168.1.206; if the laptop address has changed, resolve that before driving.
If it is not running, start it in terminal A:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\mqtt\Start-Broker.ps1 -BindAddress 192.168.1.206
```

In terminal B, get fresh stopped status, then run the straight request once:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.206 status
python .\tools\mqtt\drive_cli.py --host 192.168.1.206 move --forward 40 --turn 0 --seconds 1
```

Expect the printed mapping raw L=-40/R=+40, forward ground movement and fresh
output-OFF confirmation. Let the wheels settle. If the car only hums, stalls,
turns unexpectedly, resets or stops reporting, switch OFF and preserve the
observations/log; do not automatically raise duty or tune from that failure.

Only after that request works, reposition the stopped car with clear space
and perform one request for each curve, waiting for output OFF and coast after
each. These are new steering/load checks, not repeats of old pulse diagnostics:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.206 move --forward 40 --turn 10 --seconds 1
python .\tools\mqtt\drive_cli.py --host 192.168.1.206 move --forward 40 --turn -10 --seconds 1
```

The first intends a right curve, the second a left curve. Separate STOP:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.206 stop
```

Preserve CLI output and relevant baseline/moving/settled serial rows. Report
whether it starts smoothly, goes roughly straight, curves in the requested
direction and settles after output OFF, with flags/errors still clear. A
small duty correction may be needed after observing the loaded car; neither
equal duty nor total coast counts proves equal wheel speed. A fixed degree
turn, stopping distance or recognition accuracy is not claimed by this check.

Encoder count/revolution calibration is needed before calibrated encoder
speed/distance feedback. It does not block this duty-based ground run or the
separate colour-calibration work. Actual tapes and IR integration follow.
