# Debug Probe: connection stage only

Connected v0.3 reporting stalled again with no visible wheel motion after the bank-VIN proposal. Keep the car off and stop unchanged powered retries. The exact CPU state and original cause are still unknown.

The coordinator's kit photo includes the Raspberry Pi Debug Probe, USB cable and three supplied debug leads. The earlier robot photo shows an existing white3-pin **DEBUG** socket on the green Pico W itself, directly above its silver wireless shield. It is separate from the purple carrier's larger Grove sockets.

## Current physical step

1. Keep the car switch OFF. Unplug both the car's laptop USB cable and power-bank input. Leave motor, encoder and IR wiring alone.
2. Remove the Debug Probe from its paper holder and locate the connector marked **D**. Leave **U** empty; this stage uses SWD, not UART.
3. Select the flat cable with a small white3-pin plug at **both** ends. Connect one end to probe D and the other to the Pico W's white DEBUG socket. The plugs are keyed; align them gently and do not force a plug into a Grove port.
4. Plug the Debug Probe's USB socket into the laptop using its supplied USB cable. Keep the car's own USB and bank disconnected for this first adapter-detection stage.
5. Tell the assistant the probe is connected. Do not press GP20, power the car or start another motor test yet. No new firmware upload is needed merely to detect the adapter.

The [official Raspberry Pi guide](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html) specifies probe D for SWD and the supplied matching3-pin connector cable for compatible target sockets. Where a board has only bare holes instead, the loose male ends are not a substitute for a soldered header; stop and verify that physical interface rather than holding unstable contacts. The existing robot photo already shows the suitable socket.

## Host state and next gate

OpenOCD and Arm GDB are already installed under the local .pico-sdk; the CMSIS-DAP and rp2040 scripts are present. Offline original-v0.3 ELF symbol queries resolved the motor entry/status, capture and hard-fault-frame symbols, with a nonblocking host character-conversion warning noted in the incident record. This is not evidence of adapter detection or a live target debug session.

After the probe enumerates, initialise an idle target connection with the car's bank still disconnected and wheels raised. Do not reset/resume/step an active motor request casually: PWM hardware may continue while halted code cannot service the cutoff ISR. Define an output-removal/physical-stop capture sequence before a further GP20 request. No such live trigger is authorised by the connection-only steps above.
