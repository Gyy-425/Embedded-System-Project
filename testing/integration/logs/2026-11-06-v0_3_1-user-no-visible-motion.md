# v0.3.1: four software cutoffs completed, physical motion not passed

The coordinator supplied 73 serial reports, t=71500..90220 ms, spaced 260 ms throughout. Raw attachment was saved byte-for-byte as 2026-11-06-v0_3_1-user-no-visible-motion.log; parsed facts are in the matching JSON. The coordinator did not notice wheel movement, heard a beep-like sound on GP20 and confirmed both wheels were raised with bank VIN plus Pico USB connected.

| Request | Duty | Reported elapsed microseconds | Cutoff |
|---|---|---|---|
| M1 D1 | 25% | 400012 | 1 |
| M1 D2 | 25% | 400006 | 2 |
| M2 D1 | 25% | 400006 | 3 |
| M2 D2 | 25% | 400005 | 4 |

The run reached DONE next=0 with continuous output and motor/encoder errors=0. Both encoder counts remained zero, AB stayed2/1, irq=0, bad=0/0, over=0/0 and sat=0 throughout. The first-request reporting freeze did not recur in these four requests. This supports button/controller/cutoff/reporting operation in this run, not measured electrical output timing. With no observed encoder edge or clear visual movement, basic motor functionality and powered encoder qualification are NOT PASSED.

Beep origin, motor terminal voltages/current and actual PWM waveform were not measured. The source configures nominal 10 kHz motor PWM and does not intentionally play a GP20 confirmation tone; motor vibration is a possibility, not an identified sound source. Insufficient startup torque at the conservative25% duty, a poor winding connection, mechanical resistance or inadequate motor supply/output remain hypotheses. Raised wheels and bank attachment do not establish delivered motor current or eliminate those possibilities.

Additional coordinator observation: hand-turning the left wheel clockwise briefly lights M1A, anticlockwise M1B; right clockwise lights M2A, anticlockwise M2B; all observed LEDs red. The power state and viewing direction were not supplied for this observation. This supports basic left-to-M1/right-to-M2 winding connectivity, beyond the earlier encoder-only response. A plausible explanation is generated motor voltage whose polarity reverses with rotation; no terminal waveform was measured. Cytron's ROBO-PICO datasheet identifies these as motor direction/status indicators, not error codes (https://www.farnell.com/datasheets/4248509.pdf, printed pages2-3). LED illumination does not qualify screw-clamp resistance/current capacity or powered driver operation. No rewiring is indicated by this observation; retain the current mappings. CW/ACW observed by hand is not yet the robot's verified powered forward convention.

Next physical check: with board OFF and both power sources disconnected, verify four winding leads are securely clamped and gently confirm neither wheel rubs or binds; do not force the gearboxes. Do not repeat unchanged pulses, increase bank voltage, hand-start a powered wheel or use full-speed hardware buttons. If connections and mechanics are satisfactory, a separately named bounded higher-duty diagnostic can test the low-starting-torque hypothesis while retaining one-wheel operation, explicit GP20 request and400 ms cutoff. No higher-duty image has been prepared or used yet. Wi-Fi/MQTT remains independently prepared and pending actual Pico transport testing.
