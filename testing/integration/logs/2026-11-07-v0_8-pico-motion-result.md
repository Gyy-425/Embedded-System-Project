# v0.8 actual MQTT movement result

7 November2026. After the idle-grant client correction, the coordinator reports
that the left wheel moved. The assistant inspected the retained VS Code
command terminal, MQTT watch and serial-monitor output before VS Code closed.
The coordinator separately confirmed pressing GP21 while the left wheel was
moving and observing it stop.

This record contains selected UI observations and user confirmations, not a
full raw-log export. The user subsequently closed VS Code and reports the
terminal results are no longer available. This does not invalidate the
observations below; missing raw data limits later independent re-analysis.
No robot command, upload, power change, serial connection or button press was
performed by the assistant. UI actions only selected/scrolled output views.

## Observed bounded commands and explicit rearm

Two bounded CLI invocations printed
`Motor output OFF confirmed by fresh device status.`
The second visible command was:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.199 drive --left 50 --right 0 --seconds 2
```

Selected fields transcribed from the command output:

| Invocation / point | Remote | Motion | DRIVE | Reason | Mutation sequence | Session |
|---|---|---|---|---|---:|---|
| First initial STATUS | IDLE | STOPPED | OFF | REQUEST |0|df744726|
| First confirmed completion | STOPPED | STOPPED | OFF | REQUEST |7|df744726|
| Second initial STATUS | STOPPED | STOPPED | OFF | REQUEST |7|df744726|
| Second confirmed completion | STOPPED | STOPPED | OFF | REQUEST |13|df744726|

Completed status records show requested duties0/0 and remote lease0ms. The
client's successful path requires fresh RUNNING/active status before cleanup
and fresh STOPPED/output-off status afterward. Therefore the second completion
supports explicit fresh START/rearm after an ordinary remote STOP in the same
session. The physical movement statement is supplied by the coordinator, not
inferred from encoder counts. Separate physical observations for each of the
two invocations and exact rotation duration were not captured.

## GP21 during remote movement

A later visible five-second command ended with this selected ACK content:

```text
ACK KEEPALIVE result=STALE_GRANT request=19
remote=STOPPED motion=STOPPED DRIVE=OFF reason=GP21
duty L=+0% R=+0% lease=0ms target=unset seq=18 tag=df744726
```

The coordinator answered: “Yes—GP21 stopped the moving wheel.” This supports
physical GP21 stopping during MQTT-controlled left-wheel movement. A renewal
arriving after STOP may be rejected because STOP rotates the grant. Here the
GP21 reason and stopped/off state distinguish it from the earlier idle START
grant-expiry bug. The client reported the interruption and did not silently
restart. Exact STOP latency/distance was not measured.

## Intentional availability change afterward

Two subsequent attempts showed `No matching device ACK; command outcome
unconfirmed; STOP not confirmed`. Their initial STATUS records were stopped/off
in the same session (mutation sequences18 and25). Watch later showed stopped/off
REQUEST records with sequence25 through device uptime252820ms, then repeated
`No fresh device status; waiting for reconnection.`, followed by `Interrupted.`

The coordinator confirmed a deliberate broker or power change during those
attempts. Which source was changed and the timing relative to movement are
not established. These lines are consistent with intentional loss of device
availability; they do not establish that a requested command was accepted or
that STOP was acknowledged during the gap. A power shutdown cannot establish
automatic software stopping on broker loss.

The inspected serial view contained only an earlier stopped/off excerpt with
sequence0/epoch0 and had already closed COM3. It is not an active-motion
encoder/timing record. The later closure of VS Code is user-confirmed.

## Result and remaining scope

- Passed within the observed setup: bounded MQTT left-wheel movement with fresh
  output-off confirmation, explicit rearm after ordinary STOP, and GP21 stopping
  the moving left wheel without automatic client restart.
- Pending: broker-only loss while movement is active with Pico power maintained;
  observed stop, broker restoration with no automatic movement, and explicit
  fresh START afterward. Restart after released GP21 can be covered at the
  beginning of that case instead of repeating completed checks.
- Powered encoder accuracy, simultaneous two-wheel load, physical forward signs,
  calibrated speed, measured timing and tape recognition/tracking remain open.

No firmware/source change or rebuild is required to record these results.
The basic bounded-run, ordinary rearm and GP21 cases do not need repeating.
