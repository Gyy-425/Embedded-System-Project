# v0.10 stationary reporting check

Prepared 9 November 2026. This checks the real IR-to-classifier-to-reporting
path with deliberately absent calibration. It does not validate seven colours
or repeat the passed motion tests. Keep the car stationary throughout.

1. Close Serial Monitor and stop old drive/move clients. Switch the car OFF,
   disconnect bank VIN and Pico USB, and wait for lights to go out. Support
   the chassis securely with both drive wheels clear. Keep the existing
   wiring; no GP20, GP21 or direct motor-button press is needed.
2. In VS Code PowerShell, run `ipconfig`. The laptop WLAN address checked for
   this image is192.168.1.210. It must still match when testing; an image built
   for206 cannot be repaired merely by changing the client command to210.
   Laptop and Pico need the same reachable LAN. The laptop may use its MLO
   network while the Pico uses the compatible2.4GHz network. Pico's own IP
   should differ from the laptop's.
3. In terminal A, use the firmware working directory and start one broker:

```powershell
Set-Location -LiteralPath 'C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware'
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\mqtt\Start-Broker.ps1 -BindAddress 192.168.1.210
```

Leave terminal A running. If an old broker occupies the port, stop it with
Ctrl+C in its own terminal before starting this one. No duplicate broker is
needed. A broker listening does not itself prove the Pico has connected.

4. With bank VIN disconnected, hold BOOTSEL on the Pico W while connecting
   its USB cable to the laptop. Release BOOTSEL when RPI-RP2 appears. Copy:

```text
C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware\output\firmware\AAI05_Colour_Runtime_v0_10_BROKER210.uf2
```

Use this normal PIO image, not TEST/GPIO compatibility or the older206 image.
The drive disappearing after copying is the normal reboot. Once copying is
complete, connect the established bank-to-VIN lead and turn the car ON, keeping
Pico USB connected for serial and both wheels raised. No wiring change.

5. Open Serial Monitor on the actual Pico COM port, previously COM3, at115200.
   Allow startup/Wi-Fi association to settle. Preserve three full records.
   Expected steady state:

```text
IR[... valid=1 err=0 ...]
COLOUR[read=1 L=7/1 R=7/1 t=... drop=0]
MOTION[state=STOPPED L=0 R=0 ... err=0] DRIVE=OFF
RADIO_READY=1 ... WIFI_LINK=3 WIFI_IP=...
```

L/R7/1 means unknown/classification reason UNCALIBRATED, not a wiring error.
The sample timestamp should advance. A brief read=0 can mean a busy snapshot;
ignore its placeholder values, but preserve repeated unavailable/error output.
The shared startup banner still says v0.9 PIO capture; the new COLOUR bracket
and the filename distinguish this reporting build. The Pico IP is its own
address; it need not equal192.168.1.210. Wheels should stay still.

6. Open terminal B, change to the same firmware directory, and start the
   read-only colour watch with an automatically named saved log:

```powershell
Set-Location -LiteralPath 'C:\Users\Guo YouYan\Desktop\INF2004-main\AAI05_Firmware'
$colourLog = ".\testing\integration\logs\colour-runtime-$(Get-Date -Format 'yyyyMMdd-HHmmss').txt"
python .\tools\mqtt\colour_cli.py --host 192.168.1.210 watch 2>&1 | Tee-Object -FilePath $colourLog
```

Expected illustrative output, approximately one snapshot per second:

```text
Colour session ...; continuity starts here.
SNAPSHOT sample_t=...ms seq=... L=unknown/UNCALIBRATED AO=... R=unknown/UNCALIBRATED AO=... dropped=0 session=...
```

Observe about10seconds. sample_t should keep advancing and AO values should
reflect current surfaces. seq is a transition sequence, not a sample counter:
it normally stays unchanged while both results remain UNCALIBRATED. No CHANGE
line is required in this uncalibrated test. The session stays constant while
connected. Reports carry sample-time observations; delayed packets are not
proof of fresh sensing merely because they arrived now.

7. Hold a small piece of plain paper/card beneath the left sensor for3seconds,
   remove it, then hold it beneath the right for3seconds. Keep the other sensor
   exposed to its original surface and do not rotate wheels or adjust
   mounting/potentiometers. This is
   one brief display-response check: the corresponding AO value should respond,
   while both class results remain unknown/UNCALIBRATED. No particular ADC
   value, direction of change or colour identification is required. Record
   which side was stimulated. This is not tape calibration.
8. Leave the colour watch running. In terminal C, from the same directory,
   request one ordinary status without a movement command:

```powershell
python .\tools\mqtt\drive_cli.py --host 192.168.1.210 status
```

Require a fresh ACK STATUS result=OK, motion=STOPPED, DRIVE=OFF, duties0/0 and
lease=0ms. Remote phase may be IDLE or STOPPED. This verifies compatibility of
the existing v2 control/status path with concurrent colour reporting. Do not
run drive/move, a repeated recovery exercise or calibration capture now.
9. Preserve three serial records,5–10 colour-watch lines including the brief
   left/right AO changes, and the status ACK. The Tee log contains only watch
   output; serial and terminal C need separate copying/saving before closing
   VS Code. Report any repeated unavailable/stale/error output or missing ACK;
   do not diagnose the cause from the broker window alone.
10. Stop terminal B with Ctrl+C, close Serial Monitor, then stop the broker
    with Ctrl+C if finished. Switch the car OFF and disconnect its power/USB.
    Closing the watch does not issue a robot STOP, but this procedure never
    requests movement. If the car moves or output freezes unexpectedly during
    the check, switch it OFF and retain the last output.

Pass scope: real raw readings and uncalibrated classifier results reach both
serial and MQTT; existing fresh status remains available; no motion occurs.
Calibrated transitions, moving segment capture, accuracy and real timing remain
pending actual tape/background measurements.
