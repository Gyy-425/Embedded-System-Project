# v0.8 broker-loss stop and fresh restart

7 November2026. The coordinator reports: the wheel stopped on broker loss and
the fresh command worked afterward. The assistant inspected the retained
VS Code movement, watch and broker terminals. Selected UI observations follow;
this is not a full raw-log export or a measured stopping-latency result.

| Point | Device uptime ms | Remote / Motion | DRIVE | Mutation seq | Session |
|---|---:|---|---|---:|---|
| Before loss |92420|RUNNING / RUNNING|ON|18|c52bdc10|
| Before loss |92680|RUNNING / RUNNING|ON|19|c52bdc10|
| Last visible pre-gap telemetry |92940|RUNNING / RUNNING|ON|20|c52bdc10|
| First visible recovery |126540|IDLE / STOPPED|OFF|0|003d72ce|
| Before explicit START |148380|IDLE / STOPPED|OFF|0|003d72ce|
| Fresh-command movement |150200|RUNNING / RUNNING|ON|7|003d72ce|
| Fresh-command movement |150460|RUNNING / RUNNING|ON|8|003d72ce|
| After command completion |150720|STOPPED / STOPPED|OFF|8|003d72ce|

Pre-gap output requested left+50/right0 and valid remote renewals; a KEEPALIVE
ACK for sequence21 followed. Watch then printed repeated waiting-for-reconnection
messages. After recovery it reported zero duties/lease, unset target and a new
session, remaining off until the explicit START. Increasing device uptime
supports continued operation across the broker interruption.

The interrupted five-second command printed `MQTT publication failed: Error:
Unknown error`. That is expected loss-of-publication feedback, not a successful
STOP ACK during the gap. Physical stopping on broker loss is user-confirmed.

The fresh two-second command's initial STATUS had IDLE/STOPPED/OFF/sequence0
and session003d72ce. START ACK entered ARMING; subsequent fresh status and
telemetry showed RUNNING/ON. STOP ACK briefly still showed Motion RUNNING/ON,
then STATUS request28702 confirmed STOPPED/OFF/REQUEST/sequence8, followed by
`Motor output OFF confirmed by fresh device status.` This demonstrates why
STOP acceptance and actual software output-off are checked separately.

Passed within this setup: broker-loss stopping during MQTT left-wheel motion,
recovery without automatic movement, and explicit fresh START afterward.
Together with the preceding ordinary-rearm and GP21 result, the current MQTT
movement bring-up case is complete; do not repeat it as a diagnostic sequence.
Actual Pico AP association loss, final demo-network operation, simultaneous
two-wheel load, accurate powered encoders and measured timing remain later
integration work. No new firmware or wiring change is implied by this record.

The assistant did not run a broker/device command or actuate the car. Two empty
terminal panes were inadvertently opened while selecting result tabs and were
closed; original test terminals were preserved. No test was rerun.
