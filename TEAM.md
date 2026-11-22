# AAI05 modules and interfaces

The coordinator requested a solo development workflow for all three logical modules. Module numbers identify responsibilities, not separate contributors. Earlier tribunal contribution records outside this checkout are unchanged.

## Modules

| Module | Development owner | Responsibilities | Code |
|---|---|---|---|
| 1 IR/RGB | You Yan, with coding-assistant support | ADC acquisition, TCS34725 acquisition, portable calibrated RGB classifier | app_program/subsystem1 |
| 2 Motion | You Yan, with coding-assistant support | Motor outputs, encoders, later navigation/STOP | app_program/subsystem2 |
| 3 Communications | You Yan, with coding-assistant support | Diagnostic reporting, later command/telemetry encoding | app_program/subsystem3 |

Drivers live with their owning module. The kernel, BSP and built-in radio stack remain shared course infrastructure.

## Pins and peripherals

| Pins | Owner | Purpose |
|---|---|---|
| GP26 / ADC0 | IR | Left AO |
| GP27 / ADC1 | IR | Right AO |
| GP4 / GP5 | Motion | Left encoder A/B |
| GP16 / GP17 | Motion | Right encoder A/B |
| GP8 / GP9 | Motion | Internal M1 driver A/B |
| GP10 / GP11 | Motion | Internal M2 driver A/B |
| GP23, GP24, GP25, GP29 | Kernel radio service | Reserved Pico W nets; never reuse |
| GP0/GP1 | Unused in current profile | Reserved if UART/crash console is selected later |
| GP2/GP3 | RGB owner when RGB_DIAG=1; otherwise unused | TCS34725 I2C1 SDA/SCL; kernel I2C driver stays disabled |
| GP6, GP28 | Unused by current application | Barcode and third IR omitted; Grove5 GP26 duplicates the left IR net |
| GP12-15 | Unused by current application | Servo header disconnected |
| GP20 / GP21 | Motion | Programmable NEXT / latched STOP buttons, input pull-ups |
| GP18, GP22 | Carrier facilities, unused | RGB LED and buzzer |
| GP19 | Unused | Original example LED removed |

IR task alone opens/reads the existing adca device. The kernel driver implements ADC conversion and interrupts; no other application task uses the ADC. Motion owns motor outputs, button control and the encoder dispatcher. v0.3 starts in coast (11), then permits one wheel at fixed 25% PWM/400 ms per fresh GP20 press. Four electrical-polarity steps finish at DONE; GP21 and dependency/driver faults latch STOP until reset. Encoder edges remain decoded in one IO_BANK0 handler, not sampled by the Motion task.

## Tasks and objects

| Task | Priority (1 highest) | Affinity | Nominal activity |
|---|---|---|---|
| Kernel USB service | 3 | Existing port configuration | USB CDC |
| Kernel Wi-Fi service | 4 | Existing port configuration | Radio polling |
| Motion diagnostics | 12 | Processor 1 / core 0 | Handle buttons/faults and motor controller, publish snapshot, then requested 10 ms delay |
| IR acquisition | 14 | Any | Paired ADC read, then 10 ms delay |
| RGB acquisition (RGB_DIAG only) | 15 | Any | Sole I2C1 owner, bounded transfers, fixed-exposure RGBC cycles, requested10ms polling delay |
| Report | 16 | Any | Status line, then requested 250 ms delay |

These are configured priorities and requested delays, not measured deadlines or WCET. Motion's requested 10 ms delay normally yields approximately 20 ms between snapshot publications, and the 250 ms report delay yields approximately 260 ms. v0.1's task-side polling was too coarse for faster hand rotation; its user-run records reached bad=66/60. v0.2 capture runs independently in GPIO IRQ13 at external interrupt priority 2 on core 0 (not task priority 2). The shared port vector table requires rejecting any pre-existing GPIO event owner on either core. Each task requests 2048 stack bytes; stack high-water and maximum encoder edge-rate measurements remain pending.

| Interface | Producer/owner | Consumer | Kernel object/rule |
|---|---|---|---|
| IR snapshot | IR | Report; later Motion | IR-owned priority-inheritance mutex; copied snapshot |
| Motion snapshot | Motion | Report | Motion-owned priority-inheritance mutex; copied snapshot |
| Encoder capture | GPIO IRQ dispatcher | Motion | Interrupt-preserving SMP spinlock; copied state, no blocking mutex or console in ISR |
| Motor pulse status | Motion-owned driver and alarm0 IRQ0 | Motion, then Report through copied snapshot | Separate interrupt-preserving SMP lock; sole runtime motor output writer |
| Radio status | Kernel service | Report | cyw43_utk_get_status safe accessor |

Readers use TMO_POL and explicitly report a failed snapshot acquisition. No console output or ADC operation occurs while holding a snapshot mutex. All objects are created before application tasks start. Timestamp ages use unsigned 32-bit subtraction, valid for diagnostic ages across a timer wrap. Counters are signed 32-bit and saturate with a sticky flag; invalid-transition counters also saturate.

RGB classifier v0.12 adds report-time classification of the copied RGB snapshot
using an immutable black/white/yellow/background model. It introduces no task,
mutex, motor writer or MQTT format change. Unknown, ambiguous and unhealthy
observations do not receive a known label. Report-time labels are not a
transition queue or navigation interface. See docs/RGB_CLASSIFIER_TEST.md.

FOLLOW_APP v0.13 is a mutually exclusive local Motion-owner branch. It reuses
RGB acquisition/I2C1 and PIO encoders, classifies copied RGB at control time,
and remains the single runtime motor writer. Cached successful observations
retain original timestamps; invalid/expired data stops. Report reads copied
follow/Motion status through a copy-only SMP spinlock. Existing legacy snapshot
policies and MQTT formats are unchanged. GP20 starts one nominal1000ms run;
GP21 stops. Later Wi-Fi following is separate. See docs/BLACK_FOLLOW_TEST.md.

## Network and future phases

Original diagnostic profile: SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=none MQTT=0 TEST=0 CRASH_REPORT=0. RADIO_READY indicates driver state only. The separate v0.4 network transport probe enables NET=lwip/MQTT=1 and forces MOTOR_PROBE=1, with a distinct NO_DRIVE artifact. It reuses the existing service's Wi-Fi association, heartbeat publish/subscribe and received-text printing. It is built, but actual Pico/LAN connection evidence is pending; application command processing and sensor telemetry are not implemented by that example.

v0.3 reserves PWM slices 4/5 (configured 10 kHz) and timer alarm0/IRQ0 (external priority 1, core 0). GPIO encoder IRQ13 remains external priority 2. The initial priority-1 task validates the 12 MHz reference/divider and establishes the watchdog-derived 1 MHz timer tick before yielding to the core-0 USB/radio tasks. It never resets the shared timer counter. Motor initialization deasserts only PWM reset with a bounded RESET_DONE check and refuses conflicting alarm/PWM resources. A 400 ms timer cutoff is independent of task publication; a 500 ms task fallback latches a driver fault if it remains active. Actual interrupt latency and pulse timing remain board-validation work. All shared kernel/BSP/radio sources are retained.

Only cyw43_utk_task owns cyw43/lwIP/MQTT calls. Future module3 commands and telemetry must cross through kernel objects into that service. Any shared-library extension must follow the course template's instructor-agreement procedure; this diagnostic milestone changes no shared library code.

The coordinator prioritises visible Wi-Fi/MQTT transport, then service-owned commands/telemetry, before later colour following. A single winding-disconnected motor-output check is the current pending isolation step; communication can proceed while connected-motor validation remains unresolved. Tape calibration, qualified motor/encoder control and integration still follow. Minimum-bit storage and binary protocol comparisons must use functionally equivalent variants; the original template baseline is not that comparison.
