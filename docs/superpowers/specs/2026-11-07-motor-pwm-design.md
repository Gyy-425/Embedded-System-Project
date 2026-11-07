# Focused motor PWM correction

User request: continue development with practical, proportionate testing and less repetition. Work on the established aai05/bring-up checkout; no new worktree or broad reorganisation. This is one motor case, not integrated autonomous driving.

Hardware-button motion is already supported. The GPIO macro fault is fixed. Software50% pulses complete but have not produced observed sustained rotation. Do not repeat the passed basic wiring/IR/manual-encoder/network tests or raise duty without new evidence.

## Decision and alternatives

Use the control waveform in Cytron's example: selected polarity's input receives direct-duty PWM; the other input staysLOW. Maintain50%,10kHz,400ms, GP20 fresh-trigger sequence and GP21 latched STOP. This changes the off state from11 to00 at the same drive fraction. It is a justified hardware comparison, not a proven physical root cause or promised motor cure.

Retaining the previous waveform would repeat an unsuccessful trial. Increasing duty would change another variable before resolving the verified source difference. Replacing the working RTOS/BSP or writing a new mission controller would exceed this focused case.

Board reference:00 brake,10/01 opposite drive polarities,11 coast. Preserve11 for boot/STOP/deadline and all no-drive profiles. Do not transfer generic motor-library decay labels to this carrier.

## Transition contract

With both inputs force-high/coasted, configure PWM and arm the400ms hardware cutoff before releasing outputs. Release the fixed-LOW input first while the PWM input remains force-high, then release the PWM input. Every intermediate state must be coast, brake or requested polarity, never the opposite direction or another wheel.

For STOP/cutoff, latch SIOHIGH, force the active PWM inputHIGH first, then the fixed-LOW inputHIGH. Only then mux both bridges to SIO and disable PWM. This avoids reverse transients when stopping direction2 in the00 off phase. Probe profiles keep both override-highs and remain non-actuating. Clock, alarm ownership, error rejection, wrap handling and locks remain.

## Verification and delivery

Add the missing direct-PWM/brake waveform expectation to the actual-driver MMIO test and observe failure before code changes. Check both polarities/wheels and stop in both carrier phases; preserve deadline/setup-failure/probe invariants. Run only motor pulse/probe host suites for this change, then normal/TEST drive builds. Use BARR-C conventions in the touched driver/header and one fresh read-only review.

Deliver distinct v0.6 drive images and a short conditional hardware procedure: one raised-wheel GP20 request; if it moves, finish the remaining directions in the same session. If it does not, stop unchanged repetitions and retain physical observation plus existing logs for output/power evidence. No automatic upload, power action or motor start. Preserve v0.3.2 and known-working v0.5 communication artifacts.

References checked7 November: Cytron DC_Motor/code.py, Adafruit motor.py default FAST_DECAY branch, supplied ROBO-PICO datasheet truth table. See plan for URLs and implementation evidence.
