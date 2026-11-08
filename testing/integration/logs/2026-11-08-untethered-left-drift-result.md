# Untethered trials: user reports left turn near the end

The user reports two ground runs with Pico USB disconnected from the laptop;
both turn left near the end. They switched the car OFF afterward. This is
user-reported physical evidence; no UI inspection, encoder records, MQTT ACK
output, angle/stop-time measurement or new capture-diagnostic result is supplied
for these runs. Do not manufacture fresh PIO/error/STOP confirmation from the
earlier cable-supported log. RAM counters cannot be recovered after that
power-off by reconnecting USB.

The preceding instruction was the unchanged forward40/turn-2/seconds1 command,
on bank VIN only. That command is the working assumption for this report;
its exact arguments have not been independently verified from the laptop log.
If different arguments were used, reassess the next-step inference.

## Interpretation and next single check

The existing mapper explicitly requests left steering for turn-2:
forward40/turn-2 -> raw L=-38/R=+42. The tether-free left turn is consistent
with that provisional compensation being too strong for the untethered setup.
It does not prove an exact motor bias, a PID gain, or whether the end turn
starts while powered or during coast. Earlier cable pull and the change from
dual USB/VIN supply to VIN-only confound motor calibration from the old data.

Bank-powered forward movement is user-confirmed. Neutral untethered straightness
has not yet been established. Next change only turn from-2 to0 while retaining
forward40, duration1s, same floor/start setup, bank VIN only and no laptop USB.
Use fresh MQTT stopped/ready status and preserve full client output. This is
the missing neutral baseline, not a reason to choose another guessed permanent
trim. No firmware, wiring or default calibration has been changed.

```powershell
python "C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware\tools\mqtt\drive_cli.py" --host 192.168.1.206 move --forward 40 --turn 0 --seconds 1
```

Keep the existing broker running through command return. Observe powered path
and end/coast direction separately. Wait for fresh output-OFF confirmation and
standstill. If ACK is missing, ensure standstill with GP21/power switch and
preserve the error rather than repeating automatically. Previous ACK failures
remain unresolved. Actual curve following/IR steering and calibrated speed/
distance still await their own implementation/qualification.
