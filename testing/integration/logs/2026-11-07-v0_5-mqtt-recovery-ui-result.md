# Observed MQTT reconnection result

Source: direct inspection of retained VS Code client/broker terminal output and Serial Monitor after the coordinator followed the requested recovery flow on7 November2026. This is a normalized transcription of visible evidence, not a complete exported raw log. No shell/MQTT test command, service restart, upload, network-setting change or power action was issued during inspection.

## Before the interruption

The client terminal shows SET_TARGET colour1 followed by `watch`, using broker192.168.1.199. ACKs show `result=OK`, `ready=1`, `motion-capability=none` and tag `0baaff02`. SET_TARGET changes unset/seq0 to colour1/seq1; the watch client's initial STATUS ACK confirms colour1/seq1.

Last visible pre-gap telemetry at the inspected boundary: uptime1048720ms, left1180/right1996, target colour1, detected unknown, telemetry seq228, drop0, valid, motion-capability none.

## During and after the interruption

The same running watch invocation displays three `No fresh device packets; waiting for reconnection.` notices, then resumes decoded telemetry without an intervening shell prompt or another watch launch:

| Boundary sample | Uptime ms | Telemetry sequence | Target | Quality |
|---|---:|---:|---|---|
| Last displayed before gap | 1048720 | 228 | colour1 | valid |
| First displayed after gap | 1073320 | 0 | unset | valid |
| Next sample | 1073820 | 1 | unset | valid |

The first resumed sample shows left1221/right1987. The uptime increases across the gap rather than resetting, supporting recovery of the same running Pico rather than a reboot. The target is cleared and telemetry numbering restarts. The24.6s sample-to-sample gap includes the intentional interruption/operator downtime; it is not a measured reconnect latency or WCET.

After stopping watch, a new STATUS returns `ACK result=OK target=unset seq=0 tag=e54d7b2b ready=1 motion-capability=none`. This differs from pre-interruption tag0baaff02. The coordinator then issues SET colour1, STATUS and CLEAR; ACKs show colour1/seq1, colour1/seq1 and unset/seq2, all OK with the new tag. Thus read/write exchange works after reconnection, as well as raw telemetry reception.

The broker is still receiving16-byte publications from `aai05-picow-01` in the inspected tail. Its shutdown/startup lines have rolled out of the retained scrollback, so exact broker process restart timestamps were not archived. Serial Monitor retains only a short later COM3 session beginning around uptime1120860ms; it shows COMMS_APP, WIFI_LINK3/IP192.168.1.200, PROBE/IDLE/pct0/DRIVE OFF, responding IR fields, then COM3 closure. It does not retain the disconnect/reconnect callback messages. The client boundary and ACKs are the primary recovery evidence.

## Quality flags and scope

Occasional zero/raw `stale` frames are visible both before and after interruption: one at uptime1039220ms/seq209 and another at1074320ms/seq2, each followed500ms later by a valid sample. The observations do not establish their cause or a full-session frequency. The flags make these frames distinguishable from valid zero ADC measurements; do not claim every transmitted sensor sample was fresh or infer a sensor electrical fault from these entries. Investigate/measure snapshot availability and timing under the final combined load before using sensor freshness for motion decisions.

Assessment: **MQTT session recovery, target invalidation, automatic watch resumption, uptime continuity and post-recovery status/selection/clearing are supported** by the displayed run. No rebuild or firmware change is needed to pass this observed case.

Limits: no separate actual Wi-Fi association-loss/network-interruption recovery test, exact reconnect-time distribution, retained/replay fault injection, sustained-load acceptance, physical motion or tape classification was observed here. A changed tag and cleared target are not a security/authentication proof. Continue with the network-interruption portion of [COMMS_TEST_V0_5.md](../../../docs/COMMS_TEST_V0_5.md#7-recovery-and-saved-evidence); do not repeat the already demonstrated broker/session recovery without a new reason.
