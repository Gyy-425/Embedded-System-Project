# Wiring and register mapping review, 6 November 2026

**Subsequent authorised correction:** all three recommendations below are implemented for v0.3.2. Independent register/conditional-expression tests pass and the full SDK comparison is now66/66 matches. See [the new test image and procedure](MOTOR_START_TEST_V0_3_2.md). The original review-only findings/evidence below are preserved as history, not a statement that the defects remain in current source. No wiring reassignment was made.

## Verdict and scope

The current two-IR wiring allocation matches the application and Robo Pico's documented connections. No rewiring is indicated by this review. The captured v0.3 HardFault was caused by a C macro argument expansion, not by needing a different motor GPIO allocation. The v0.3.1 call-site repair is present and its four wheel/direction cases pass the focused host driver test.

One additional definite but currently unused BSP register definition is wrong: GPIO_OUT is0xD0000008 instead of0xD0000010. Both inherited GPIO address macros also retain unparenthesised arguments. Current application callers use simple arguments safely; the generic definitions should be corrected before extending low-level GPIO code. These are software findings, not reasons to change the physical pin map. This review does not establish motor starting torque, load current, electrical signal levels or powered wheel direction.

Review only: no production C/header, motor duty/duration, existing UF2 or target firmware was changed. No target connection or motor request was made during this review. New report/evidence files are saved separately.

## Working physical map

Left/right mean the robot's intended travel direction, following the coordinator's identification of the two motor photographs. Contact numbers below are the PH2.0-6P motor connector numbers, not Pico pin numbers. PicoCar.pptx slide2 was checked directly against its embedded connector diagram.

| Motor contact | Function | Left motor | Right motor |
|---|---|---|---|
| 1 | Encoder ground | GND | GND |
| 2 | Encoder A | GP4, Pico physical6, Grove3 signal | GP16, Pico physical21, Grove4 signal |
| 3 | Encoder B | GP5, Pico physical7, Grove3 signal | GP17, Pico physical22, Grove4 signal |
| 4 | Encoder supply | Existing3V3 rail connection | Existing3V3 rail connection |
| 5 | Motor winding + | M1B terminal | M2B terminal |
| 6 | Motor winding - | M1A terminal | M2A terminal |

Contacts5/6 reverse the original proposal's A/B polarity on both wheels. This is a valid polarity choice; it changes which electrical command produces physical forward. Keep it consistent and establish actual forward and encoder sign from later powered observation. Do not connect winding contacts to3V3/GND or directly to a Pico GPIO. The board already connects its driver inputs to GP8..11.

| IR connection | Left active IR | Right active IR |
|---|---|---|
| AO | GP26 / ADC0 / Pico physical31 | GP27 / ADC1 / Pico physical32 |
| VCC | Existing3V3 connection | Existing3V3 connection |
| GND | Common GND | Common GND |
| DO | Disconnected | Disconnected |

The third/side sensor is omitted. GP2/3 remain externally disconnected because IMU is optional/omitted; GP6 and GP28 have no application sensor assignment. Servo/sonar/barcode connections are omitted. No wiring addition is needed for the onboard Wi-Fi radio or USB console.

Grove6 GND/3V3 may power the encoder while GP26/27 carry IR AO: these are separate contacts on shared supply rails and distinct signal nets. GP26 also appears on Grove5; it is the SAME net, so no second output may drive it. Sensor/encoder supply and signal compatibility remain unmeasured; their earlier successful response does not constitute a voltage specification or full-load qualification.

Power mapping remains the existing bank5V lead into VIN+/VIN-, plus Pico USB for serial during the latest test. No voltage or polarity change is proposed. The supported simultaneous USB/VIN arrangement selects its supply automatically; it does not prove bank-only motor current.

## Board controls and peripheral ownership

| Function | Actual GPIO/mode | Peripheral/ownership |
|---|---|---|
| M1A driver input | GP8, PWM when driven / SIO high when coasting | PWM slice4 channelA |
| M1B driver input | GP9, PWM when driven / SIO high when coasting | PWM slice4 channelB |
| M2A driver input | GP10, PWM when driven / SIO high when coasting | PWM slice5 channelA |
| M2B driver input | GP11, PWM when driven / SIO high when coasting | PWM slice5 channelB |
| Encoder left A/B | GP4/5, SIO input | Core0 GPIO IRQ13, bank0 mask0x00CC0000 |
| Encoder right A/B | GP16/17, SIO input | Same dispatcher, bank2 mask0x000000CC |
| IR left/right | GP26/27, FUNCSEL31, input buffer/pulls off | ADC channels0/1; ADC IRQ22 owned by driver |
| NEXT | Onboard GP20 / physical26, input/pull-up | Active-low Motion task input |
| STOP | Onboard GP21 / physical27, input/pull-up | Active-low, firmware latch until reset |
| Pulse cutoff | Timer alarm0, IRQ0 | Core0 priority1; no GPIO allocation |
| Console | Pico USB data connector | Verified build TM_CONSOLE_UART=0, BOARD_UART0_FREE=1, TM_CONSOLE_USB_CDC=1 |
| Pico W radio | Internal GP23/24/25/29 | Do not allocate these to external sensors; ADC3 pin initialization remains disabled |

GP26/27 can alternatively route PWM slice5 A/B, but their muxes are NULL/analogue in this application. Running slice5 on GP10/11 does not drive GP26/27 in that mode. All active application pin allocations are distinct; no extra encoder/motor GPIO jumper is required.

At the current125MHz system clock, the driver sets divider1.0 and TOP12499, giving nominal10kHz PWM. The pin/polarity mapping matches the carrier truth table:11 is coast,10/01 drive opposite ways,00 brake. The existing25% diagnostic uses a constant-high leg and a75%-high/25%-low PWM leg, so drive occurs25% of the cycle, with coast between drive intervals. Nothing in this review establishes that this duty is enough to start the installed geared motors.

## Register/address findings

The one-shot host probe evaluated the REAL course BSP macros and compared66 register addresses with independent constants from the installed official Pico SDK2.3.0 RP2040 headers.65 matched; the only mismatch is the unused GPIO_OUT definition below. Full addresses and source hashes: testing/integration/logs/2026-11-06-pin-audit-result.json. Independent expected values:2026-11-06-pin-audit-sdk-expected.json. Temporary probe source and generation inputs: parent workspace tmp/aai05_pin_audit_2026-11-06. No live target was read by this probe.

| Register | Correct address used by active path |
|---|---|
| GP8 CTRL | 0x40014044 |
| GP9 CTRL | 0x4001404C |
| GP10 CTRL | 0x40014054 |
| GP11 CTRL | 0x4001405C |
| GP26 CTRL | 0x400140D4 |
| GP27 CTRL | 0x400140DC |
| PWM4 CSR/DIV/CTR/CC/TOP | 0x40050050 /54 /58 /5C /60 |
| PWM5 CSR/DIV/CTR/CC/TOP | 0x40050064 /68 /6C /70 /74 |
| SIO GPIO_IN | 0xD0000004 |
| SIO GPIO_OUT_SET / CLR | 0xD0000014 /0xD0000018 |
| SIO GPIO_OE_SET / CLR | 0xD0000024 /0xD0000028 |
| Timer ALARM0 / TIMERAWL | 0x40054010 /0x40054028 |
| Encoder PROC0_INTE0 / INTE2 | 0x40014100 /0x40014108 |

1. **Definite latent address error:** include/sys/sysdepend/cpu/rp2040/sysdef.h, GPIO_OUT at line274, expands to SIO_BASE+0x008. That offset is GPIO_HI_IN (read-only QSPI pin input); GPIO_OUT belongs at+0x010. The current app has no GPIO_OUT reference and motor outputs use the correct SET/OE aliases. The Wi-Fi SDK's GPIO_OUT direction symbol is in its separate SDK GPIO environment, not evidence that the erroneous BSP register is being used there. Do not attribute the present no-motion result to this unused macro.
2. **Remaining macro hazard:** sysdef.h lines211/228 define GPIO_CTRL(n) and GPIO(n) without wrapping n in parentheses. The repaired caller uses constant_pin/variable_pin and all13 active app macro invocations currently pass safe simple values, including pins[i]. A future conditional or arithmetic argument can again compute a different address. Correcting the generic definitions removes that trap rather than relying solely on call-site discipline.

Recommended exact replacements for a later source correction (not applied in this review):

```c
#define GPIO_CTRL(n) (IO_BANK0_BASE + 0x04 + ((n) * 8))
#define GPIO(n)      (PADS_BANK0_BASE + 0x04 + ((n) * 4))
#define GPIO_OUT     (SIO_BASE + 0x010)
```

Add independent SDK/datasheet checks for GPIO_OUT and conditional/arithmetic macro arguments before applying those changes. Preserve the deployed v0.3.1 artifact and build a distinctly named revision. Keep motor duty/duration unchanged for this address correction; evaluate any later higher-duty diagnostic separately.

## Verification and limits

Four focused strict-warning ASan/UBSan host executables were freshly compiled and passed: normal pulse driver using the real register definitions; coast-only probe policy; encoder IRQ ownership/startup; masked motor-coast startup. The normal driver fixture rejects unaligned accesses and motor GPIO-control writes outside8..11, and covers all four wheel/direction combinations. Ten compile/run steps, including register probing, exited0. These host boundaries are not a physical PWM waveform/current measurement.

The current normal UF2 hash remains30abbae451e37d54b6734c802a2ada3c38902ec979d5d3816ab15b01f288c9a1. Production-source diff was empty during this review. The most recent user-run four-pulse record still establishes continuous reporting/software cutoffs with no observed encoder edges or clear wheel movement. That motor-start issue remains under investigation.

## Sources checked

- Supplied PicoCar.pptx slides1/2, including the actual slide2 embedded motor connector image.
- Current app.h, drv_ir.c, encoder_capture.h/.c, encoder_capture_rp2040.c, motor_io_rp2040.c, drv_motor.c, subsystem2.c and motor_pulse.c; used BSP GPIO, ADC and startup implementations and build flags.
- Cytron-authored Robo Pico datasheet, printed pages2-3/5/7: https://www.farnell.com/datasheets/4248509.pdf
- Official RP2040 datasheet, SIO register map (GPIO_HI_IN+0x008, GPIO_OUT+0x010) and GPIO/PWM functions: https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
- Installed official Pico SDK2.3.0 src/rp2040/hardware_regs/include/hardware/regs headers: addressmap, io_bank0, pads_bank0, sio, pwm, timer, adc; hardware PWM slice/channel mapping helpers.
- Coordinator's wiring confirmations, manual encoder tests, motor-generated LED observation and supplied v0.3.1 pulse records. These support the labelled paths but are not a new independent inspection of every hidden connection.
