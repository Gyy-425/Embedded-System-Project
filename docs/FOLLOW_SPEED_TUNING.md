# Follower duty presets and ground-weaving check

This records the earlier speed-only comparison. A later moving capture shows
strong alternating corrections at D40/T20; use docs/FOLLOW_STEERING_TUNING.md
for the current D40/T8 test. No physical weaving fix has yet been established.

User reports powered v0.13 moved forward but weaved on the ground. That
demonstrates motion, not accepted tracking. Oscillating corrections suggest
hunting. The later log supports that pattern but does not establish a sole cause.
Both IR heads see cardboard at centre, so guidance reacts when an edge reaches
a head; speed, steering gain, delay and mechanics can all affect overshoot.

v0.13.1 introduces a controlled comparison: selected maximum wheel duty40%,
without changing the steering law, baselines, deadband, run duration or STOP.
Both converted wheel commands are scaled together; merely reducing forward
while retaining a fixed steering duty would increase relative curvature.

| Setting | Centred electrical L/R | Full right correction L/R |
|---|---|---|
| Old50% |−50/+50|−50/+21|
| New40% |−40/+40|−40/+16|
| Optional35% |−35/+35|−35/+14|

Integer rounding slightly changes ratios. These are electrical duties, not
measured cm/s. Lower duty can fail to start a loaded motor; no minimum rolling
duty is claimed. True physical-speed regulation would need encoder feedback
and its separate qualification. No automatic startup boost is added.

## Try one comparison

The earlier comparison used `AAI05_Black_Follow_v0_13_1_D40.uf2` and still
weaved. For the current comparison use the D40_T8 image in the steering guide.
Use the usual power-off/BOOTSEL upload procedure; keep35mm strip, mounting
and power setup.

Check boot/status `limit=40`. GP20 starts one nominal one-second run; GP21
stops. The limits, fresh-RGB/IR checks and absolute hardware deadlines remain.
For someone who already ran v0.13 powered, no repeat of old individual motor
tests or large colour collections is required. Keep clear runway and stop
control reachable; avoid a USB tether pulling the car during comparison.

Report whether it rolls consistently and whether weaving is smaller. If it
buzzes/stalls, report that before choosing a lower duty. If it still weaves,
provide a few FOLLOW RUNNING lines with norm/turn/L/R/limit and the STOP line
when possible without cable drag. Change only one tuning variable at a time;
do not claim this speed experiment fixed the steering before observing it.

## Select a different duty for rebuilding

The current control interface is build/flash presets, not live MQTT or USB
commands. Existing USB driver discards input; no shared driver is changed here.
Valid settings are30,35,40,45,50; larger values are rejected. Default40 is in
`config/follow_config.h`. Make passes `FOLLOW_DUTY` as the compile-time limit
and includes it in the object profile and default image name to avoid stale
objects or images with hidden differing limits.

Example in PowerShell (creates new evidence/artifact files, never flashes):

```powershell
$followDutyLabel = "follow-d35-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
wsl.exe -d Ubuntu -- python3 '/mnt/c/Users/Guo YouYan/Desktop/INF2004-main/AAI05_Firmware/tools/check_black_follow.py' --label $followDutyLabel --duty 35 --turn 20 --firmware --sdk '/mnt/c/Users/Guo YouYan/.pico-sdk/sdk/2.3.0'
```

Do not keep changing speed during a run. This local prototype has no live
speed-selection command; later Wi-Fi integration is a separate milestone.
The explicit --turn20 above preserves the old gain for a speed-only rebuild.
The default is now8 for the current gentler-steering comparison.
