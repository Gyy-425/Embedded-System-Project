# MQTT movement test, v0.8

Current result,7 November: bounded MQTT left-wheel movement, ordinary STOP/rearm
and GP21 during movement passed within the recorded setup.
[Result and limits](../testing/integration/logs/2026-11-07-v0_8-pico-motion-result.md).
Those cases do not need repeating; the next focused check is step4. The steps
below remain reproduction instructions. This connects the passed local Motion
interface to MQTT. It does not classify tapes or follow a coloured path yet.
Keep the existing wiring. Basic IR, manual encoder, hardware-button, v0.6.1
pulse and v0.7 local expiry tests do not need repeating.

Client correction,7 November: the first car attempt returned `START
STALE_GRANT` while IDLE/DRIVE OFF. STATUS can report a grant near its expiry.
The Python client now handles one explicitly rejected idle request with the
returned new grant, only while session/sequence/stopped state are unchanged.
Timeouts or lost ACKs never authorize a new grant/START. The original timeout
and movement budget remain in force. Firmware and wiring are unchanged: with
the current v0.8 image and updated client, step3 has now passed as recorded
above. [Diagnosis/tests](../testing/integration/logs/2026-11-07-v0_8-grant-fix.md).

## 1. Load the REMOTE image

Support the chassis with both drive wheels clear and the power switch reachable.
Close Serial Monitor and remove target power before entering BOOTSEL. Hold
BOOTSEL while reconnecting Pico USB, then release it. Copy this normal image
to RPI-RP2:

`output/firmware/AAI05_MQTT_Motion_v0_8_REMOTE.uf2`

Use the established bank-to-VIN power and laptop-to-Pico USB arrangement for
the powered test. Open the actual Pico COM port at 115200. Expect the v0.8
`EXPLICIT REMOTE START` banner, `NETWORK_MODE=REMOTE_MOTION`, healthy encoder
capture/motor initialization and DRIVE OFF. Neither connection nor startup
should turn a wheel. GP20 is disabled in this profile; GP21 remains STOP.

## 2. Start the broker and check the device

Run PowerShell in `C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware`.
The image is configured for broker **192.168.1.199**. Check the laptop's current
LAN IPv4 with `ipconfig`. This must match the image's broker address. The
Pico's own IP can differ; its previously observed `.200` is not the broker.
If the laptop address has changed, update the broker setting locally and
rebuild before this test. Keep the Wi-Fi password out of chat and Git.

Leave the broker terminal open:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\mqtt\Start-Broker.ps1 -BindAddress 192.168.1.199
```

Use the new v2 client in a second terminal. The old `robot_cli.py` controls
the v0.5 no-drive image and does not drive v0.8.

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.199 status
```

Expect a matching STATUS ACK with `result=OK`, `DRIVE=OFF` and remote IDLE or
STOPPED. Motion may already be STOPPED because connection changes request STOP.
The status must be ready internally; if START reports unavailable/not ready,
retain the serial output and resolve that error before requesting motion.

Optional live output in another terminal:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.199 watch
```

`watch` never renews movement. It displays raw IR, requested duty, remote/Motion
states and DRIVE. `detected=unknown` is intentional until tape calibration.

## 3. First MQTT movement and explicit restart

Keep the wheels raised. Send a bounded left-wheel electrical-duty request:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.199 drive --left 50 --right 0 --seconds 2
```

Expect the left wheel to turn, status to reach remote RUNNING/Motion RUNNING,
then STOPPED/DRIVE OFF. The CLI sends periodic KEEPALIVE and ends by requesting
STOP and obtaining fresh output-off status. It prints
`Motor output OFF confirmed by fresh device status.` only after that check.
The two-second budget begins with the first START attempt, so handshake time
is included. Network/cleanup time can extend process completion; this is not
a measured two-second physical rotation guarantee.

Run that same command once more. An explicit fresh START should rearm after
the prior ordinary STOP, without a power cycle or GP20. No automatic restart
is allowed. Positive duty is an electrical direction, not confirmed physical
forward. Do not infer encoder accuracy from counts changing.

Then request five seconds and press GP21 while the left wheel is moving:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.199 drive --left 50 --right 0 --seconds 5
```

Expect stopping with Motion `reason=GP21`/serial reason2; the client must report
the interruption rather than restart. Release GP21. A new explicit two-second
command can rearm; a held GP21 must prevent it. FAULT is not cleared by START.

For an ordinary remote STOP, a second command terminal can send:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.199 stop
```

One `drive` process controls movement at a time. A passive watch and an explicit
STOP terminal may coexist. Target/clear are accepted only when not moving.

## 4. New actuation-loss check

After the first movement case passes, make one five-second request. While the
wheel moves, stop only the broker with Ctrl+C in its terminal. Observe that
the wheel stops and stays stopped. The drive CLI should fail to confirm the
lost connection, not claim success. Device authorization expires after 750ms
without fresh renewal; its internal Motion/driver command lease is100ms.
These are software settings, not measured physical stopping latency.

Restart the broker. Watch should reconnect and show DRIVE OFF; reconnection
must not restart the wheel. A new explicit START is required. Remote loss may
show Motion reason1 (STOP requested by the remote layer) or reason3 (internal
expiry), depending on which path acts first. Neither is automatically a fault.

This checks network loss during actual commanded movement. It does not require
repeating the earlier no-drive Wi-Fi recovery or changing router settings.

Record the command output, relevant serial/watch lines and what the wheel
actually did. Output-off confirmation describes the software drive state;
wheel coast, exact timing, physical forward signs, simultaneous two-wheel load
and powered encoder accuracy remain separate measurements. No new component
photos or wiring changes are required for this software test.
