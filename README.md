# AAI05 robot firmware

Focused motion addition: logical forward duty and steering C API plus `move`
CLI conversion through the existing MQTT v2 drive path. Positive turn means
right; verified raw motor polarities are handled at the boundary. Keep the
tested v0.9 image installed. Strict/sanitized C checks,71 Python/Tcl tests and
four firmware compatibility builds passed after fixing the review's C input
narrowing finding. **Basic ground motion bring-up is complete** within the
recorded setup: three neutral straight runs and both requested curve directions
are user-confirmed. Keep turn0; a small post-OFF movement remains a stopping
observation. Precise stop/heading performance and earlier intermittent ACK
failures remain open. Next: IR calibration and runtime classifier integration;
actual tape separability and live colour control are not demonstrated yet.
[Ground result and limits](testing/integration/logs/2026-11-08-ground-curves-result.md).
[API, semantics and next ground-run guide](docs/CHASSIS_MOTION.md).
[Software evidence](testing/integration/logs/2026-11-08-chassis-motion-preparation.md).

Latest car observation: **v0.9 basic powered PIO capture trial passed** after
the user reconnected power/USB and reran. Clean baseline, expected raw signs,
PIO_BAD=0/0, no uncertainty/errors/saturation, fresh output OFF and a stable
stopped tail are observed. Count accuracy/scaling and ground-load behaviour
remain unverified. The earlier flag's cause is unproven; no code change was made.
[Clean rerun and limits](testing/integration/logs/2026-11-08-v0_9-pio-clean-rerun.md),
[earlier flagged result](testing/integration/logs/2026-11-08-v0_9-pio-ui-result.md).

Current image, 8 November: **v0.9 PIO encoder capture**. Hardware counters
retain movement across CPU-service gaps; invalid flags and bounded read faults
remain explicit. Strict host checks, 63 Python/Tcl tests, normal/TEST PIO and
GPIO compatibility builds passed; all 32 earlier UF2s were preserved.
The scoped powered trial above passed. Keep
`output/firmware/AAI05_Encoder_PIO_v0_9_REMOTE.uf2`; no repeat upload is needed.
[Upload/test guide](docs/PIO_ENCODER_V0_9.md),
[review and verification evidence](testing/integration/logs/2026-11-08-pio-v0_9-preparation.md).

Earlier GPIO case, 8 November: **paired motor and powered encoder qualification**.
The laptop's broker address is now 192.168.1.206. A separate working v0.8 image,
`output/firmware/AAI05_Motor_Qualification_v0_8_BROKER206.uf2`, is built for that
address; no motor/encoder logic changed. Normal/TEST builds and supporting
software checks passed, and all 28 older images were preserved. The paired
run now has observed movement and saved output-off confirmation:
positive electrical duty drives left rearward and right forward. The user then
confirmed both wheels forward with raw `-50/+50`. The supplied forward-run serial
records establish raw encoder signs left negative/right positive and requested
output OFF. Powered capture is not qualified: new overrun warnings rise to
32/30 while moving, then 39/36 during spin-down. No basic direction replay is
needed; capture quality must be addressed before feedback control.
[Forward-run assessment](testing/integration/logs/2026-11-08-forward-encoder-result.md).
[Observed result](testing/integration/logs/2026-11-08-paired-motor-qualification-result.md),
[next focused check](docs/MOTOR_ENCODER_QUALIFICATION.md),
[preparation evidence](testing/integration/logs/2026-11-08-motor-qualification-preparation.md).

C classifier core prepared,7 November: **configurable per-sensor calibration,
seven classes, unknown/ambiguous handling, validity and timestamp checks**.
[Core API and limits](docs/COLOUR_CLASSIFIER_CORE.md),
[verification evidence](testing/integration/logs/2026-11-07-colour-core-preparation.md).
Strict host/ARM checks and normal/TEST compile-compatibility builds passed;
16,384 ADC/reference comparisons pass. Packed model46 bytes versus equivalent
native60; result1 versus2 bytes. Existing runtime and tested images are unchanged.
No real calibration, live classification, moving accuracy or path following is
claimed. The classifier alone needs no new car image; the broker-address update
above is for today's motor qualification. Actual tape data/model integration
comes later. [Approved scope](docs/PROJECT_SCOPE_REVIEW.md).

Latest actual result,7 November: **broker-loss stopping, recovery without automatic
movement and explicit fresh START passed** for the MQTT left-wheel case.
[Observed result](testing/integration/logs/2026-11-07-v0_8-broker-loss-result.md).
The current MQTT movement bring-up sequence is complete within its recorded
scope; it does not need replaying. The next capture tool is prepared; actual tape samples remain pending.

Actual result,7 November: **v0.8 MQTT left-wheel movement, ordinary STOP/rearm
and GP21 during movement passed** within the recorded setup. Two CLI runs
confirmed fresh output-off status; the coordinator confirms movement and GP21
stopping the moving wheel. The earlier ambiguous shutdown was followed by the dedicated broker-loss
check recorded above. [Observed result and limits](testing/integration/logs/2026-11-07-v0_8-pico-motion-result.md).
Do not repeat the completed basic movement checks.

Next case prepared: [stationary tape calibration capture](docs/TAPE_CALIBRATION.md).
The Python tool saves labelled raw IR samples and metadata, preserves incomplete
captures and reports per-sensor class-range overlap.46 affected host tests
passed after review fixes. Use the current firmware when actual tapes become
available; no tape capture, classifier or recognition accuracy is claimed yet.

Client correction,7 November: fixed an idle grant-expiry race reported on the
first v0.8 car attempt. The Python client safely handles one explicit rejection
before acceptance;28 affected client/codec tests and the sanitized controller
boundary test passed. Firmware files are unchanged; the movement retest above
now passes. [Diagnosis and scope](testing/integration/logs/2026-11-07-v0_8-grant-fix.md).

Prepared on 7 November: **v0.8 MQTT movement control**, with explicit START/STOP,
fresh-command leases, guarded restart and a readable v2 client. All 31 affected
host/build stages passed, including 14 v2 Python tests, 8 legacy client tests
and normal/TEST firmware builds, with zero reported compiler warnings.
[Build evidence](testing/integration/logs/2026-11-07-v0_8-preparation.md),
[protocol](docs/MQTT_MOTION_PROTOCOL_V2.md), [next car test](docs/MQTT_MOTION_V0_8.md).
The earlier idle START rejection was corrected and retested as recorded above.
No tape recognition or path following is implemented by this feature.

The historical `output/firmware/AAI05_MQTT_Motion_v0_8_REMOTE.uf2` targets
192.168.1.199. Use today's separate BROKER206 image and
`tools/mqtt/drive_cli.py` for the paired check above. Verify the laptop's current
LAN address before a later session. GP20 is
disabled in REMOTE; GP21 stops. Ordinary STOP/expiry requires a new explicit
START after release/healthy coast; reconnect never automatically starts.
Session/grants are liveness checks on the trusted test LAN, not authentication.

The preceding **v0.7 LOCAL physical pass** covers renewed left-wheel movement,
automatic expiry, GP21 while moving and restart prevention.
[Recorded result and limits](testing/integration/logs/2026-11-07-v0_7-pico-motion-result.md).
v0.6.1's four software pulse requests and v0.5's target/status/raw-IR and
recorded recovery checks also passed within their scopes. Those tests need
not be repeated. [Motor result](testing/integration/logs/2026-11-07-v0_6_1-pico-motor-result.md),
[communication result](testing/integration/logs/2026-11-07-v0_5-network-path-result.md).
All 22 pre-existing firmware artifacts were hash-checked unchanged.

Powered encoder accuracy/scaling, ground-loaded two-wheel performance,
Wi-Fi timing, actual tape calibration, the two colour demonstrations and final
Week13 evidence remain open. [Remaining work](docs/REMAINING_WORK.md).
The historical motor HardFault was captured and corrected; do not treat old
no-motion diagnostic notes as the current result. Hardware-button, basic IR,
manual encoder and local Motion checks do not need replaying.

Application modules live in `app_program/subsystem1` (IR), `subsystem2`
(Motion/encoders) and `subsystem3` (communication). New application C follows
BARR-C conventions with strict checks; inherited course code was not wholly
restyled. See [bring-up](docs/BRINGUP.md) and [module ownership](TEAM.md).
The original course-template reference follows.

---

# micro T-Kernel 3.0 project for Raspberry Pi Pico W

A starting point for your project: micro T-Kernel 3.0 running on **both cores**
of the Pico W's RP2040, with a USB serial console and the WiFi driver built in.

You need a **Raspberry Pi Pico W**, a USB cable, and a computer running
macOS, Linux, or Windows with **WSL (Ubuntu)**. On Windows, run every
command below inside the WSL Ubuntu terminal.

---

## 1. Set up the Pico C SDK and the compiler

You need two separate things:

| What | Why | Where it comes from |
|---|---|---|
| **Pico C SDK** | USB and WiFi driver source code | `git clone` (step 1.2) |
| **Arm GNU Toolchain** (`arm-none-eabi-gcc`) | the compiler | a separate download (step 1.3) |

> ⚠️ **The SDK does NOT include the compiler.** You must install the compiler
> on its own and put its `bin` folder on your `PATH` (step 1.4).

**1.1 Install the basic tools: git, make and a host C++ compiler**

```sh
# macOS
xcode-select --install

# Linux / WSL (Ubuntu)
sudo apt update && sudo apt install -y git make g++
```

**1.2 Download the Pico C SDK** (any folder works; this guide uses `~/pico`)

```sh
mkdir -p ~/pico && cd ~/pico
git clone --branch 2.2.0 https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init lib/tinyusb lib/cyw43-driver
```

The `submodule` line is required: it downloads the USB and WiFi drivers.
(SDK 2.3.0 has also been tested and works.)

**1.3 Install the Arm GNU Toolchain (the compiler)**

- **macOS / Windows (WSL) / Linux:** download from
  <https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads>.
  Pick the **AArch32 bare-metal target (arm-none-eabi)** package for your
  computer. For WSL, choose the **Linux x86_64** `.tar.xz`.
  Extract it, e.g. to `~/tools/arm-gnu-toolchain-<version>/`.
- **Linux / WSL shortcut:** `sudo apt install -y gcc-arm-none-eabi`
  installs it straight onto your PATH. If you use this, skip the PATH line in 1.4.

Find the folder that contains `arm-none-eabi-gcc`. It is the toolchain's
**`bin`** folder, e.g. `~/tools/arm-gnu-toolchain-15.3.rel1/bin`.

**1.4 Tell your shell where the compiler and SDK are**

Add these two lines to the end of `~/.zshrc` (macOS) or `~/.bashrc` (Linux/WSL),
**changing both paths to match your computer**:

```sh
export PATH="$HOME/tools/arm-gnu-toolchain-15.3.rel1/bin:$PATH"   # folder that contains arm-none-eabi-gcc
export PICO_SDK_PATH="$HOME/pico/pico-sdk"                         # the SDK from step 1.2
```

Open a **new** terminal, then check both:

```sh
arm-none-eabi-gcc --version     # must print a version, not "command not found"
ls $PICO_SDK_PATH               # must list the SDK files
```

If `arm-none-eabi-gcc` is "not found", your `PATH` does not point to the
toolchain's `bin` folder. Fix that before going further.

---

## 2. Clone this repo and build the `.uf2` file

**2.1 Clone your project**

```sh
cd ~/pico
git clone <URL-of-this-repo> my-project
cd my-project
```

**2.2 Build**

```sh
cd build_make
make -j8
```

A successful build ends with a line like
`tools/elf2uf2 mtk3pico_smp1_usb_cdc_wifi.elf mtk3pico_smp1_usb_cdc_wifi.uf2`.
Your firmware is **`build_make/mtk3pico_smp1_usb_cdc_wifi.uf2`**.

If `make` stops with an error, it tells you which one is wrong:
`arm-none-eabi-gcc not found` means fix `PATH`, and
`PICO_SDK_PATH is not set` means fix `PICO_SDK_PATH` (step 1.4).

To build a single-core, UART-only or test version, see section 3.

**2.3 Flash it onto the Pico W**

1. Hold the **BOOTSEL** button and plug the Pico W into USB, then release.
   A drive called **RPI-RP2** appears.
2. Copy the `.uf2` file onto that drive. On WSL, run `explorer.exe .` in
   `build_make` to open the folder in Windows. The Pico reboots by itself.

**2.4 See the output**

Open the Pico's USB serial port at 115200 baud:

```sh
screen /dev/tty.usbmodem* 115200      # macOS   (quit: Ctrl-A then K)
screen /dev/ttyACM0 115200            # Linux
```

On Windows, use PuTTY or the VS Code *Serial Monitor* on the new COM port.
You should see the task reports. Connect an LED with a 330 Ω resistor from
**GP19** to GND to see subsystem 1's LED blink.

**2.5 Write your own code**

Your code lives in **`app_program/`**. Every `.c` file there, and in its
subfolders, is compiled automatically.

- **`app_main.c` → `usermain()` is where the program starts.** It first calls
  every subsystem's `subsystemN_init()` (create kernel objects), then every
  `subsystemN_start()` (start tasks). Read its comments first.
- **One folder per student:** `app_program/subsystem1/` … `subsystem5/`.
  Subsystems 1–4 hold a working example, one task each (LED, producer,
  consumer, monitor). Together they use the main micro T-Kernel APIs and
  pass data between subsystems. Subsystem 5 is an empty template.
  Replace the examples with your own subsystems.
- `subsystemN.h` is that subsystem's public interface: the object IDs and
  data formats the other subsystems may use.
- `app.h` holds the definitions shared by the whole team.
- **Drivers go in your subsystem folder too**, as a `drv_<name>.c` /
  `drv_<name>.h` pair, e.g. `subsystem2/drv_ultrasonic.c`. They are compiled
  automatically. Do not add drivers under `device/`: that folder holds the
  kernel's own serial, I2C and ADC drivers, and adding to it means editing
  kernel and makefile files shared by the whole team.

**Working as a team:** record owners, pins, priorities and the interfaces
between subsystems in **`TEAM.md`**, and record every test in **`testing/`**
(see `testing/README.md`). `make TEST=1` builds a test image that runs all
the subsystem tests at power-up. Short videos of tests and demos are shared
as unlisted YouTube links, listed in **`videos/`**.

Pins: use **GP0–GP22 and GP26–GP28**. GP0/GP1 are free unless your team
builds with `CRASH_REPORT=1` (section 3), which needs them. Never use
GP23/GP24/GP25/GP29: they are wired to the Pico W radio. The Pico W's
on-board LED is also on the radio, so use an external LED instead.

After editing, run `make -j8` again in `build_make` and copy the new `.uf2`.

---

## 3. Build options: `SMP`, `CONSOLE`, `WIFI`, `TEST`, `CRASH_REPORT`

`make` on its own uses the defaults. To build differently, put options
after `make`. They can be combined in any order:

```sh
make -j8                          # defaults: SMP=1 CONSOLE=usb_cdc WIFI=cyw43 TEST=0 CRASH_REPORT=0
make SMP=0 -j8                    # single-core build
make CONSOLE=uart WIFI=none -j8   # no USB console, radio off (no Pico SDK needed)
make TEST=1 -j8                   # firmware with the subsystem self-tests
make CRASH_REPORT=1 -j8           # also see crash reports, on GP0/GP1 (uses those pins)
```

To set several options at once, list them all, separated by spaces. For
example, to track down a crash in the test build while running on one core only:

```sh
make SMP=0 TEST=1 CRASH_REPORT=1 -j8
#  -> build_make/mtk3pico_smp0_usb_cdc_wifi_crash_test.uf2
```

Any option you leave out keeps its default (here `CONSOLE=usb_cdc` and
`WIFI=cyw43`). Options are not remembered: give them on every `make`, as
the next plain `make` goes back to the defaults.

| Option | Values (**default**) | What it does | Image name | C macro your code can test |
|---|---|---|---|---|
| `SMP` | `0`, **`1`** | `1` runs the kernel on both RP2040 cores; `0` uses core 0 only | `smp1` / `smp0` | `TK_SUPPORT_SMP` (1 or 0) |
| `CONSOLE` | **`usb_cdc`**, `uart` | where `tm_printf` output goes. `usb_cdc` = USB cable; `uart` = UART0 only (GP0 TX / GP1 RX, needs a USB-serial adapter) | `_usb_cdc` / `_uart` | `TM_CONSOLE_USB_CDC` |
| `WIFI` | **`cyw43`**, `none` | `cyw43` powers up the Pico W radio and its driver task; `none` leaves the radio off | `_wifi` / *(nothing)* | `TM_WIFI_CYW43` |
| `TEST` | **`0`**, `1` | `1` compiles every `subsystemN_test.c` and runs the tests at power-up | `_test` / *(nothing)* | `APP_TEST` (1 or 0) |
| `CRASH_REPORT` | **`0`**, `1` | `1` also copies the console to UART0 on **GP0 (TX) / GP1 (RX)**, 115200 baud, so you can see the last messages and the kernel's crash report (see below). **Uses GP0/GP1.** `CONSOLE=uart` always does this. | `_crash` / *(nothing)* | `TM_CONSOLE_UART` (1 or 0) |

**Why `CRASH_REPORT` exists.** USB output is not sent the moment you call
`tm_printf`: it waits in a buffer until the kernel's USB task sends it. If the
program crashes, no task runs again, so the last messages and the kernel's
crash report never reach your USB terminal. The terminal just goes quiet.
UART output *is* sent at once, even inside the crash handler. So when you
need to know why the Pico crashed or froze, build with `CRASH_REPORT=1`,
connect a USB-serial adapter (adapter RX → GP0, adapter GND → GND), and read
it at 115200 baud. Normal `tm_printf` output on USB works the same with
either setting. If a subsystem has claimed GP0/GP1 in `TEAM.md`, unplug that
hardware before using `CRASH_REPORT=1`.

The options are part of the image name, e.g.
`mtk3pico_smp1_usb_cdc_wifi_test.uf2`, so you can always tell which build
you are flashing. Switching options needs no `make clean`: the makefile
notices and recompiles everything.

Using an option in C code:

```c
#if TK_SUPPORT_SMP
    ctsk.tskatr |= TA_ASSPRC;   /* core affinity only exists on the dual-core build */
#endif

#if APP_TEST
    /* code that only exists in the test build */
#endif
```

### Where the options live in the makefile

All five are handled in **`build_make/pico_rp2040.mk`**. To find each piece, search the file for the
names in the left column (e.g. `grep -n "TEST" build_make/pico_rp2040.mk`).
Each option goes through the same steps:

| Step | What to look for | Example for `TEST` |
|---|---|---|
| 1. Default value | `NAME ?= value` near the top of the file | `TEST ?= 0` |
| 2. Allowed values | an `$(error Unknown ...)` check just below it | `$(error Unknown TEST '$(TEST)'; use 0 or 1)` |
| 3. Image-name suffix | `NAME_SUFFIX :=` and the `EXE_FILE :=` line | `TEST_SUFFIX := _test` |
| 4. C macro for the code | a `CFLAGS += -D...` line | `CFLAGS += -DAPP_TEST=$(TEST)` |
| 5. Rebuild on change | the `PROFILE_ID :=` line | `...-test$(TEST)` |
| 6. Extra source files (`CONSOLE`, `WIFI` only) | an `ifeq ($(CONSOLE),usb_cdc)` or `ifeq ($(WIFI),cyw43)` block that adds the USB/radio files and SDK include paths | — |

- **Changing a default:** edit the `?=` line from step 1, e.g.
  `CONSOLE ?= uart`. Every `make` without that option then uses the new value.
- **Adding your own option** (say `DEBUG=1` to turn on extra prints): copy
  how `TEST` is done. Add steps 1 to 5 next to the `TEST` lines, then
  use `#if APP_DEBUG` in your code. **Step 5 matters:** without it, switching
  the option can quietly link files that were compiled with the old setting.
- **Kernel settings** such as the maximum number of tasks, semaphores or
  mailboxes are not make options. They are in `config/config.h` (`CNF_MAX_...`).

The makefile also has `NET=lwip` and `MQTT=1` (TCP/IP and MQTT over WiFi).
They need the SDK's `lib/lwip` submodule and a `config/wifi_secrets.h`
(copy `config/wifi_secrets.example.h`). This guide does not cover them.

---

*micro T-Kernel 3.0 © Ken Sakamura / TRON Forum, distributed under the
T-License 2.2. This dual-core RP2040 port is derived from the official
μT-Kernel 3.0 BSP (`tron-forum/mtk3_bsp`, branch `pico_rp2040`).*
