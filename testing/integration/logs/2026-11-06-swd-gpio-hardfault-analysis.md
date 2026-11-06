# Captured GPIO-address HardFault and v0.3.1 correction

## Actual target evidence

The coordinator connected the Debug Probe and powered the car from bank VIN only, with Pico USB absent. Windows detected CMSIS-DAPv2; SWD examined both RP2040 cores. A private read-only flash copy matched every payload block of the original normal v0.3 UF2:281344 bytes,1099 blocks, zero mismatches (2026-11-06-swd-firmware-match.json). Idle motor control registers were SIO/high, PWM4/5 disabled, alarm0 inactive; encoder IRQ masks matched GP4/5 and GP16/17.

After the reviewed capture was armed, the coordinator pressed GP20 once, saw no wheel movement and switched off. The capture completed before power-off. Raw log:2026-11-06-swd-first-request-capture.log. Later SWD errors followed target power-off; the owned process was stopped. They are not the initial failure.

Core0 was in HardFault; core1 was in the idle task. Preserved fault frame: `r1=0x4001400d`, `r2=4`, stacked PC=`0x1000bc10`. Original ELF disassembly at that PC is `str r2,[r1]`; inline location is `out_w` in `motor_pulse_start`, original source line212. Motor status was active1, cutoff0, pulse lock1. The hardware timer IRQ was pending while PRIMASK=1. Encoder IRQ/count/overrun values were0. Motor GPIOs were changed to high/coast by the debug capture and both cores halted; those post-intervention GPIO values are not a firmware stop result.

## Confirmed cause of this capture

The board macro is `#define GPIO_CTRL(n) (IO_BANK0_BASE+0x04+(n*8))`. Passing `direction == 1U ? pin_a : pin_b` as n fails to multiply the selected true-branch pin by8. For M1/D1, the two release writes therefore target0x4001400c (GPIO1) then0x4001400d (unaligned), instead of0x40014044/0x4001404c (GPIO8/9). The second word store HardFaults before releasing the actual motor inputs from coast. This explains the absent wheel movement and, with core0 unavailable to service USB/cutoff, the earlier reporting-stall pattern. It is a proven source-level fault; no claim of measured rail integrity or later powered encoder performance follows.

The old host fixture redefined GPIO_CTRL with safe parentheses, masking the real target expansion. The strengthened fixture now imports the real RP2040 sysdef.h register definitions, suppressing only platform/config selection and supplying the host clock configuration. It rejects unaligned MMIO and writes to any GPIO control outside8..11. It failed on the old first GPIO1 write before the repair (2026-11-06-gpio-macro-regression-red.log).

## Correction and verification

motor_pulse_start now calculates constant_pin and variable_pin first, then passes those simple values to the unchanged board macro. Constant-leg-first release, PWM/duration limits, expiry checks, locks, STOP and encoder handling are unchanged. No shared BSP/kernel source changed. Independent review confirmed both old faulting instruction and new aligned GPIO8..11 stores.

Distinct normal/TEST v0.3.1 firmware builds and all17 host/build steps pass without reported warnings; seven sanitized C programs include the actual-macro normal/probe cases and all four wheel/direction combinations. Evidence:2026-11-06-v0_3_1-fixed-final-build-result.json. Both UF2 hashes were independently matched. Normal SHA256 `30abbae451e37d54b6734c802a2ada3c38902ec979d5d3816ab15b01f288c9a1`; original v0.3 UF2 remains unchanged. No post-fix hardware pass has been recorded yet.

Capture-helper review found armed-error cleanup and alternate-AP stopping gaps before use. Regressions failed first; six host cases and OpenOCD Jim Tcl compatibility checks passed after correction. Debugger stop attempts are not hard physical timing guarantees; the coordinator retained physical power-off control. This instrumentation did not flash or modify target firmware during the captured original failure.
