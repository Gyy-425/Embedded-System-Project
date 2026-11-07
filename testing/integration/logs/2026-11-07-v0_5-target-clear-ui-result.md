# Pico target-selection and clearing result

Source: direct inspection of the coordinator's retained VS Code terminal and Serial Monitor output on7 November2026. This is a normalized transcription of visible results, not a complete exported raw log. The coordinator explicitly says the terminal was shut down deliberately. No test command, power change or upload was issued during this inspection.

The command terminal uses the project `robot_cli.py` and broker192.168.1.199. The following sequence is visible, with all ACKs showing `result=OK`, `ready=1`, `motion-capability=none` and connection tag `90938cba`:

| User command | Initial status ACK, where applicable | Final command/status ACK |
|---|---|---|
| `status` | — | target unset, seq0 |
| `target colour1` | target unset, seq0 | target colour1, seq1 |
| `status` | — | target colour1, seq1 |
| `clear` | target colour1, seq1 | target unset, seq2 |
| `status` | — | target unset, seq2 |

This supports successful end-to-end target storage, status readback and clearing on the Pico. Read-only STATUS preserves the mutating sequence; SET and CLEAR advance it once each. The tag stays consistent across these commands. The two ACKs printed by a mutating CLI command are its preliminary STATUS and subsequent mutation response, not an error or repeated mutation.

The broker terminal shows repeated16-byte telemetry and an explicit `mosquitto version 2.1.2 terminating` before its PowerShell prompt. The serial tail shows `NETWORK_MODE=COMMS_APP`, `WIFI_LINK=3`, `WIFI_IP=192.168.1.200`, PROBE/IDLE/pct0, `DRIVE=OFF`, responding IR fields and deliberate COM3 closure. Together with the coordinator's statement, treat the ending as intentional. Physical wheel motion was not observed through the computer screen.

A preceding terminal-startup message reports `conda activate base` / CommandNotFoundException. The subsequent Python client commands and ACKs succeed. That startup-environment issue did not invalidate this communication sequence; it was not changed or investigated further in this scoped review.

Assessment: actual Pico **SET → STATUS readback → CLEAR → STATUS readback passes** for this selected target. The earlier actual raw-IR/status exchange and host boundary tests remain separately recorded.

Limits: `colour1` is an assigned protocol identifier, not a demonstrated tape classification. No motor operation, target-path following, hardware duplicate/replay injection, packet-loss rate, full timing distribution or automatic reconnect behavior is established by this sequence. No automatic recovery after broker/network interruption was observed. The next communication case is the controlled recovery procedure in [COMMS_TEST_V0_5.md](../../../docs/COMMS_TEST_V0_5.md#7-recovery-and-saved-evidence), keeping the Pico powered during interruption so boot/reset is not confused with recovery.
