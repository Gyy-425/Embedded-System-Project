# Cable-supported ground run: closer powered counts and unequal coast

User identifies a confound in the earlier ground tests: the Pico-to-laptop USB
cable pulled the car toward the laptop. Supporting the cable by hand produced
mostly straight travel, with a right turn near the end of the motor sound.
This does not establish the precise instant of visual yaw relative to output
OFF. The earlier ground record remains valid for its tethered setup, not a
calibration of intrinsic motor imbalance.

Computer Use inspected the new retained VS Code session, client terminal14,
broker terminal13 and COM3. No commands, serial writes, firmware upload,
production-code change or robot actuation by the assistant.

## Command evidence

The four most recent visible requests use forward40/turn-2/seconds1, raw
L=-38/R=+42, broker192.168.1.206. Three show fresh output-OFF confirmation;
one intervening run shows `No matching device ACK; command outcome unconfirmed;
STOP not confirmed: No matching device ACK; command outcome unconfirmed`.
Visible mutation sequences progress4->7,7->11,11->12 across the failed run,
then12->16 for the final successful request. The exact runs in which the
user held the cable are not individually labelled in these UI records.
The last request prints final STATUS OK/STOPPED/OFF/REQUEST, duties0/0,
lease0, mutationseq16, followed by fresh off-confirmed message.

## Latest serial run (epoch5)

Selected fields transcribed from the UI; this is not a complete raw export.
All rows below report ENC_READY1, capture/Motion err0, PIO_BAD0/0,
ENC_UNCERTAIN0, sat0, WIFI_LINK3, PicoIP192.168.1.207. RUNNING has lease100,
reason0; STOPPED has duties0/0, lease0, reason1 (REQUEST).

| Uptime ms | Encoder L | Encoder R | State | Raw L/R | Motion seq | Epoch |
| ---: | ---: | ---: | --- | --- | ---: | ---: |
| 102960 | -13641 | 12521 | STOPPED | 0/0 | 33 | 4 |
| 103220 | -13851 | 12775 | RUNNING | -38/+42 | 7 | 5 |
| 103480 | -14748 | 13685 | RUNNING | -38/+42 | 20 | 5 |
| 103740 | -15709 | 14626 | RUNNING | -38/+42 | 33 | 5 |
| 104000 | -16697 | 15565 | RUNNING | -38/+42 | 46 | 5 |
| 104260 | -17514 | 16263 | STOPPED | 0/0 | 49 | 5 |
| 104520 | -17789 | 16313 | STOPPED | 0/0 | 49 | 5 |
| 104780 | -17789 | 16313 | STOPPED | 0/0 | 49 | 5 |
| 105040 | -17789 | 16313 | STOPPED | 0/0 | 49 | 5 |
| 105300 | -17789 | 16313 | STOPPED | 0/0 | 49 | 5 |
| 111800 | -17789 | 16313 | STOPPED | 0/0 | 49 | 5 |
| 112580 | -17789 | 16313 | STOPPED | 0/0 | 49 | 5 |
| 113880 | -17789 | 16313 | STOPPED | 0/0 | 49 | 5 |

After112580, serial reports MQTT connection status256/retry3s while still
STOPPED/OFF. Broker terminal ends with termination; COM3 is closed. This later
loss is not evidence of a moving fault in the final completed request.
At session opening, one DIAG snapshot-busy/error IR=-50/motion=-50 line precedes
healthy records startingt35100. This is a diagnostic snapshot read failure,
not a demonstrated sensor failure/reset; final capture/Motion errors stay0.

## Interpretation

- Entirely powered103480..104000 (520ms): L1949/R1880 ticks, left excess3.67%.
  Latest260ms powered segment: L988/R939, left excess5.22%. These are raw
  tick differences, not calibrated wheel/ground speeds.
- After first observed output OFF104260..104520: L275/R50 additional ticks.
  Forward counts while OFF are consistent with coast; unequal post-OFF motion
  can contribute rightward yaw if encoder/wheel scaling is comparable. The
  records alone do not establish visual turn timing or exclude external forces.
- The earlier two10-11% comparisons were turn0 with cable influence. The latest
  requests are turn-2 with changed cable handling. This is not a controlled
  motor-calibration comparison; do not increase or install a permanent trim
  from it. The latest completed request supports commanded movement and STOP,
  with clean capture diagnostics. Untethered straightness/curve qualification
  remains open; the intervening ACK failure remains unresolved.

## Next experiment: remove the mechanical tether, keep command unchanged

Use the existing v0.9 image and wiring, car powered from bank VIN only, Pico
USB disconnected from the laptop. Keep forward40/turn-2/seconds1 for one trial
so another trim is not added to the comparison. Removing laptop USB also
changes the previous dual-power arrangement to bank-only; this is a practical
untethered operating check, not an isolated measurement of mechanical drag.
Use fresh MQTT status and its client output; after confirmed output OFF and
wheel standstill, USB may be reconnected without power-cycling the bank. Allow
old buffered output to drain, then preserve three fresh records with advancing
timestamps and STOPPED/OFF. Continuous encoder history during that run is
not available through the current v2 MQTT telemetry; do not claim it is.

Read-only source check: usb_console.c appends to a bounded ring, discards on
full and never waits for a USB host; draining is gated on mount status. Thus
no USB logging code change is indicated before this bank-only test. Actual
bank-only load behaviour still needs the physical observation/status check.

If an ACK is missing, use GP21 or the power switch to ensure standstill and
preserve that client error rather than automatically repeating motion. This
is a proposed single trial, not a test already performed. No further static
trim or braking change was made.
