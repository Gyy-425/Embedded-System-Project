# v0.10 stationary reporting result

9 November 2026. Inspected the user's VS Code colour-watch, status and broker
terminals and Serial Monitor using Windows Computer Use. No robot command,
upload, broker restart or hardware input was issued during inspection.

**Scoped result: basic uncalibrated runtime reporting and concurrent v2 status
passed in the observed setup.** Actual seven-colour recognition, stimulus
attribution, moving segments and final timing remain unverified.

## Serial observations

One exact representative record extracted from the Serial Monitor:

```text
t=311220 IR[L=1235 R=1857 valid=1 err=0 age=10] COLOUR[read=1 L=7/1 R=7/1 t=311210 drop=0] ENC[L=-2 R=-1 AB=2/2 sat=0 age=10] ENC_CAPTURE=PIO PIO=0 ENC_READY=1 err=0 PIO_BAD=0/0 ENC_UNCERTAIN=0 MOTION[state=STOPPED L=0 R=0 lease=0 seq=0 epoch=0 reason=1 cutoff=0 err=0] DRIVE=OFF RADIO_READY=1 NETWORK_MODE=REMOTE_MOTION WIFI_LINK=3 WIFI_IP=192.168.1.211
```

Other extracted records through316160 retain valid1/error0/age10, both7/1,
drop0, capture ready/errors0 and motor output OFF. The visible tail includes
399100..400140, colour sample time399090..400130 with the same healthy state,
ending 'Closed the serial port COM3'. Individual paper/card stimulus times
were not separately labelled or confirmed; do not infer colour separability
or precise stimulus response from ordinary ADC variation.

## MQTT and status observations

- Colour watch shows successive SNAPSHOT reports with advancing sample times,
  seq1, both unknown/UNCALIBRATED, AO readings and dropped0. Stable transition
  sequence is correct while class/reason stay unchanged.
- Status terminal shows a request to broker192.168.1.210 and fresh ACK STATUS
  resultOK, request21731, remoteIDLE, motionSTOPPED, DRIVE OFF, reasonREQUEST,
  duty0/0, lease0ms, targetunset and mutationseq0.
- Broker receives/forwards23-byte colour-v1 reports alongside26-byte raw-v2
  telemetry. Its tail shows 'mosquitto version2.1.2 terminating' and a returned
  PowerShell prompt.
- User explicitly confirmed the ending switch-off/disconnection/broker stop
  was deliberate. Later waiting messages are consistent with planned shutdown,
  not an unexplained failure.
- Earlier 'conda activate base' CommandNotFoundException did not prevent the
  subsequent python status command from succeeding; no environment repair is
  required for this completed check.

## Evidence limits / next action

UI observations and selected serial records are preserved here, not a complete
raw log. The created colour-runtime-20261109-110422.txt file was0bytes when
inspected; its existence is not claimed as saved watch output.
No physical wheel-still observation, calibrated transition, loss-free delivery
or measured timing/accuracy is invented. Output OFF and unchanged encoder
counts are supported by observed records.

Basic runtime reporting needs no replay. Keep the working v0.10 BROKER210
image. Next: actual tape/background capture, separability and calibration,
then moving recognition/target following and integrated timing/stop validation.
