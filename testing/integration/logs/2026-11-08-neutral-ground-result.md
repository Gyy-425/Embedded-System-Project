# Neutral untethered ground run: straight under power in three user trials

The user reports that the straight command works in three trials, with a very
short leftward movement/tilt after motor output turns off. In the ongoing
bank-only, laptop-USB-disconnected neutral-baseline procedure, the requested
command is forward40/turn0/seconds1, raw L=-40/R=+40. Current VS Code client
records show that command and repeated fresh STATUS resultOK/STOPPED/OFF,
reasonREQUEST, duties0/0/lease0, with off-confirmed messages. The user's physical
observations are kept separate from those command/status records; no measured
heading, drift distance, wheel speed or stopping latency is supplied.

## Connection observation

The preceding request asked why the car and Mosquitto were not connecting.
The user then reported recovery after sending a command. Read-only Computer
Use saw the broker receiving Pico telemetry and the client completing repeated
fresh status/drive/STOP exchanges. Laptop ipconfig remains192.168.1.206.
The original failed error text was not captured in the visible history; the
earlier interruption's cause remains unestablished. No IP/credential/firmware
change was made or proven necessary, and old intermittent ACK failures are
not declared fixed. No movement command was issued by the assistant.

## Scoped assessment

Supported: user-confirmed approximately straight powered travel in three
neutral untethered trials and currently working command/status communication.
The reported end movement occurs after output off; it is not a reason to
install a steady left/right steering correction. Keep turn0 as the straight
baseline. Basic straight-movement bring-up can advance to the planned curve
checks without replaying these three straight runs.

The firmware's motor_pulse_stop_locked implements its configured coast stop.
Residual wheel motion/caster settling are plausible contributors to a short
end movement, but no specific mechanical cause is verified. Stopping-distance,
heading accuracy and final track-end behaviour remain later measured acceptance
work; do not describe the stop as instantaneous physical standstill.

## Next practical checks

Keep the same untethered bank-powered setup, current v0.9 image and fresh
broker connection. Perform each intentional curve once, waiting for fresh
output-OFF confirmation and physical standstill between them:

```powershell
python "C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware\tools\mqtt\drive_cli.py" --host 192.168.1.206 move --forward 40 --turn 10 --seconds 1
python "C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware\tools\mqtt\drive_cli.py" --host 192.168.1.206 move --forward 40 --turn -10 --seconds 1
```

Expected intent: first right curve (raw-50/+30), second left curve
(raw-30/+50). Observe powered curve direction and whether a wheel stalls;
these are duty requests, not calibrated angles. If an ACK/stop confirmation is
missing, ensure standstill with GP21/power switch and retain the error, rather
than automatically repeating. Curves have not yet been reported performed.
No firmware or wiring change, PID calibration or new physical test was made
in recording this result.
