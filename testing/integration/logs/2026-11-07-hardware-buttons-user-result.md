# Coordinator-reported motor hardware-button result

On7 November2026 the coordinator reports all four hardware motor-test buttons clearly turn the motors, with observed stopping on release. This is a basic PASS for motor/driver response through those hardware controls in the reported setup. It is stronger evidence than the earlier hand-generated LED observation. Per-button wheel/direction labels, exact duration, mode/power-source details and electrical measurements were not separately supplied; do not invent them or claim calibrated speed/distance/full-load qualification.

The result does not validate software PWM, encoder performance during powered rotation, firmware STOP/cutoff or operation with Wi-Fi active. The buttons are full-speed requests; the normal software trials were25%/50% PWM. A working DC/full-speed hardware path does not by itself prove a firmware defect or sufficient starting torque at50% PWM.

Process correction: the manufacturer-documented hardware-button check should have been used earlier to establish basic motor response before spending further time on low-duty software trials. The earlier testing was still useful: it exposed the real GPIO macro HardFault, address-definition defect and coarse-polling encoder limitations, and established responsive IR/interrupt encoder input plus software-reported cutoff continuity. Those are not equivalent to a physical software-controlled motion pass.

## What the course documents actually ask

Supplied Project Briefing.pdf page10 calls for repeatable subsystem tests, calibration/measurements and measurable engineering evidence. Supplied Project Write-up.pdf pages1/5/6 require the software platform and integration/system testing, with a motion-control library in the original scope. The revised colour/Wi-Fi scope supplied by the coordinator supersedes omitted mission features. Neither inspected PDF prescribes the exact GP20 four-step app,400ms setting or v0.1-v0.3.2 diagnostic UF2 sequence. Those are engineering choices, not separately mandated robot features. The supplied repository's testing README supports automated assertions and manual evidence; a manual hardware-button pass does not replace program-controlled motion.

The final project still needs motors controlled by its own C/micro T-Kernel code to support the confirmed colour-path/Wi-Fi task, with proportionate repeatable evidence. Diagnostic helpers can remain separate from final operating modes. Do not present every diagnostic version as a mandatory assessment deliverable.

## Concrete source-level lead for the next motion check

Cytron's official example configures GP8/9 and10/11 at10kHz and instantiates adafruit_motor.motor.DCMotor without a decay-mode override. The current upstream DCMotor default pulses one input at requested duty while holding the other LOW. Our driver instead holds one input HIGH and pulses the other with complementary duty. At50%, both produce the requested drive polarity for half the cycle, but the off states differ (00 versus11). The Robo Pico datasheet calls00 brake and11 coast. Treat this as a verifiable source difference and a diagnostic candidate, not a measured root cause. Do not blindly transfer generic chip-specific FAST/SLOW decay labels to this carrier.

Reference sources checked7 November2026:
- https://github.com/CytronTechnologies/Cytron-ROBO-PICO/blob/main/Getting_Started_Guide/CircuitPython/DC_Motor/code.py
- https://github.com/adafruit/Adafruit_CircuitPython_Motor/blob/main/adafruit_motor/motor.py
- https://www.farnell.com/datasheets/4248509.pdf, driver truth table

Next motion work should compare the PWM control/off-state and actual output evidence at unchanged50%/10kHz/400ms, rather than raise duty again or repeat basic wiring/encoder checks. Preserve boot/STOP coast, one-wheel ownership and avoid opposite-direction transients during any mux handoff. In particular, a LOW-fixed-leg PWM change would require safe stop ordering (variable leg high before the fixed-low leg) instead of blindly retaining the old ascending-pin stop loop. Tests must catch that boundary. No implementation or new firmware was made in this question/answer turn.

Actual Pico Wi-Fi/MQTT remains an independent prepared milestone and need not wait for every motion qualification test.
