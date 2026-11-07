# Network-path interruption using laptop Wi-Fi

Observed result7 November: this case is now supported by the saved watch log and inspected serial reconnect/status output. See [the network-path result](../testing/integration/logs/2026-11-07-v0_5-network-path-result.md). The procedure below is retained for reproducibility; no repeat is needed without a new reason.

This is the next practical check with the current home setup because the coordinator is unsure how to access the router's Wi-Fi controls. It tests loss/restoration of the route between Pico and laptop broker while the broker process remains running. It does not prove that the Pico itself lost/reacquired its2.4GHz Wi-Fi association. No test result is claimed by this procedure.

Current laptop address was rechecked as192.168.1.199 when this guide was prepared. The Pico's previously observed address is192.168.1.200. Keep these local instructions open before taking the laptop offline; chat/internet access will pause during the outage.

## 1. Keep the known-working firmware and power

Use the existing v0.5 NO_DRIVE firmware; no upload is needed. Connect the Pico normally by USB if it is currently disconnected, switch the Robo Pico ON as in the successful communication setup, and establish the baseline below. From then until the check is finished, keep the Pico powered and its USB/serial connection open. Do not press a motor button, reset or BOOTSEL.

In VS Code Serial Monitor, select the actual Pico port (previously COM3),115200, and Start Monitoring. Confirm normal repeating reports with NETWORK_MODE=COMMS_APP, WIFI_LINK=3 and DRIVE=OFF. Retain the serial output throughout the test so the disconnect/timeout event is captured.

## 2. Keep the broker running in terminal1

If the existing broker is still running, leave it alone. If it has been stopped, start it before establishing the baseline:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 'C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware\tools\mqtt\Start-Broker.ps1' -BindAddress 192.168.1.199
```

Do not stop/restart this broker during the interruption; that recovery case is already demonstrated. If the port is already occupied, check the existing broker instead of launching a second instance.

## 3. Establish a non-unset target and save watch output in terminal2

```powershell
$client = 'C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware\tools\mqtt\robot_cli.py'
$watchLog = Join-Path 'C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware\testing\integration\logs' ('network-path-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
python $client --host 192.168.1.199 target colour1
Write-Host "Watch log: $watchLog"
python -u $client --host 192.168.1.199 watch 2>&1 | Tee-Object -FilePath $watchLog
```

Check the final SET ACK and the watch client's initial STATUS ACK: target colour1, resultOK, ready1. Note the connection tag and the latest device uptime `t=`. Check that telemetry is arriving before proceeding. Leave watch running. Its output is copied to the log path shown above.

## 4. Interrupt only the laptop's Wi-Fi

In Windows Settings → Network & internet → Wi-Fi, turn Wi-Fi OFF. Leave the router, broker terminal, watch command, Pico power and USB cable alone. Do not use Network reset or forget the saved Wi-Fi profile.

Switch back to Serial Monitor while the laptop is offline. Its USB connection should continue reporting. Expected observations to check:

- Watch stops receiving fresh Pico packets and may print waiting-for-reconnection notices.
- Pico uptime keeps increasing; DRIVE remainsOFF.
- WIFI_LINK may stay3 because the Pico is still associated with the router. That is consistent with this path-loss test.
- Eventually the serial log should report `comm: link unavailable` and an MQTT disconnect/timeout/retry event.

Keep the laptop offline until the MQTT disconnect/timeout is actually observed, allowing up to about2minutes. This is a test observation window, not a project response-time requirement. The current MQTT keepalive is60s; local lwIP code can wait over90s without server activity before its watchdog closes the session. A very brief outage can resume an existing connection instead of exercising a new session. If no disconnect is observed by about2minutes, restore Wi-Fi anyway and report exactly what happened.

## 5. Restore the laptop's original network

Turn Windows Wi-Fi ON and reconnect to the same network. Leave the Pico and broker running and watch the results. A successful new-session recovery should show:

- MQTT connection and command-subscription readiness restored.
- Telemetry resumes; target isunset after the MQTT session reset.
- Device uptime continues instead of restarting.
- Watch resumes automatically if its subscriber process stayed alive.

If watch exited, preserve the error/prompt and report that it required a manual relaunch; do not record automatic watch recovery. Once the laptop is back online, it is fine to rerun watch to determine whether the Pico recovered independently.

If data does not resume, stop only watch with Ctrl+C, run `ipconfig` and check the WLAN IPv4. If it changed from192.168.1.199, save the new address and logs for review before altering firmware/broker settings. If the address is unchanged but recovery fails, preserve the serial/broker messages rather than resetting the Pico first.

## 6. Confirm and preserve the recovered state

After roughly10seconds of resumed output, stop watch with Ctrl+C. In terminal2:

```powershell
python $client --host 192.168.1.199 status
Write-Host "Saved watch log: $watchLog"
```

If a disconnect/new MQTT session was observed, expect the STATUS ACK to be ready, targetunset and tag different from the baseline. Keep the resulting state unchanged until it is recorded. The final status output is not included in the watch-only log; copy it or leave it visible along with the serial disconnect/reconnect lines and broker terminal.

Provide the watch log path, final STATUS ACK and relevant serial lines for review. Record any manual broker/client restart, IP change, reset or power interruption; those affect what the result can demonstrate. After collecting the evidence, the services may be stopped deliberately as before.

Passing this case supports network-path recovery. Actual Pico Wi-Fi association loss/rejoin remains separately unverified until an appropriate controlled access-point interruption can be arranged. Earlier MQTT broker/session recovery and target-selection checks do not need repeating.
