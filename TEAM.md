# AAI05 modules and interfaces

The coordinator requested a solo development workflow for all three logical modules. Module numbers identify responsibilities, not separate contributors. Earlier tribunal contribution records outside this checkout are unchanged.

## Modules

| Module | Development owner | Responsibilities | Code |
|---|---|---|---|
| 1 IR | You Yan, with coding-assistant support | ADC acquisition, later calibration/classification | app_program/subsystem1 |
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
| GP2/GP3, GP6, GP28 | Unused by current application | IMU, barcode and third IR omitted |
| GP12-15 | Unused by current application | Servo header disconnected |
| GP18, GP20, GP21, GP22 | Carrier facilities, unused | RGB LED, buttons and buzzer |
| GP19 | Unused | Original example LED removed |

IR task alone opens/reads the existing adca device. The kernel driver implements ADC conversion and interrupts; no other application task uses the ADC. Motion task alone changes motor outputs or samples encoders. Both bridges stay in coast (11) in v0.1; no PWM or movement command exists.

## Tasks and objects

| Task | Priority (1 highest) | Affinity | Nominal activity |
|---|---|---|---|
| Kernel USB service | 3 | Existing port configuration | USB CDC |
| Kernel Wi-Fi service | 4 | Existing port configuration | Radio polling |
| Motion diagnostics | 12 | Any | Manual encoder sample every 1 ms |
| IR acquisition | 14 | Any | Paired ADC read, then 10 ms delay |
| Report | 16 | Any | Status line every 250 ms |

These are configured priorities and delays, not measured deadlines or WCET. The encoder polling diagnostic is for slow manual movement only; qualify an IRQ/PIO decoder before motor-speed control. Each task currently requests 2048 stack bytes; stack high-water measurements are pending.

| Interface | Producer/owner | Consumer | Kernel object/rule |
|---|---|---|---|
| IR snapshot | IR | Report; later Motion | IR-owned priority-inheritance mutex; copied snapshot |
| Motion snapshot | Motion | Report | Motion-owned priority-inheritance mutex; copied snapshot |
| Radio status | Kernel service | Report | cyw43_utk_get_status safe accessor |

Readers use TMO_POL and explicitly report a failed snapshot acquisition. No console output or ADC operation occurs while holding a snapshot mutex. All objects are created before application tasks start. Timestamp ages use unsigned 32-bit subtraction, valid for diagnostic ages across a timer wrap. Counters are signed 32-bit and saturate with a sticky flag; invalid-transition counters also saturate.

## Network and future phases

Current profile: SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=none MQTT=0 TEST=0 CRASH_REPORT=0. RADIO_READY indicates driver state only. No association, broker connection, command processing or MQTT publishing is implemented yet.

Only cyw43_utk_task owns cyw43/lwIP/MQTT calls. Future module3 commands and telemetry must cross through kernel objects into that service. Any shared-library extension must follow the course template's instructor-agreement procedure; this diagnostic milestone changes no shared library code.

The next phases are hardware diagnostic validation, tape calibration, qualified encoder/motor control, and service-owned Wi-Fi/MQTT integration. Minimum-bit storage and binary protocol comparisons must use functionally equivalent variants; the original template baseline is not that comparison.
