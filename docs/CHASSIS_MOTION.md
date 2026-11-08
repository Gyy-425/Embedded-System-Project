# Logical forward and steering motion

This focused addition prepares a C interface for the later IR controller and
a `move` CLI command for practical ground testing. It uses the existing Motion
owner, raw motor driver and MQTT v2 START/renew/STOP flow. The car's tested
v0.9 PIO image can remain installed: the CLI performs coordinate conversion
before sending the existing raw requests. The new on-robot C entry point is
host-tested and included in compile-compatibility builds; no live IR controller
calls it yet. Ground forward movement is observed with rightward drift;
straight tracking, commanded curves and calibrated speed remain unverified.

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

Update8November: the user performed the straight request twice. Forward travel
works but turns right while powered; comparable encoder intervals show10-11%
more left ticks. PIO flags/errors remain clear and serial shows both stops;
the second client reports a missing ACK. The initial tuning trial is now
`move --forward 40 --turn -2 --seconds 1` (raw-38/+42), with cable slack and
broker/Pico kept on through the return. This is a proposed small correction,
not verified permanent trim. Keep curve tests pending. See the
[observed result](../testing/integration/logs/2026-11-08-ground-straight-drift-result.md).
The original straight/curve procedure below is retained for context.

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
