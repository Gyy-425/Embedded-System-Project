"""Offline PIO/legacy/robot host checks and optional distinct firmware candidates."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shutil
import subprocess
import sys
import tempfile

from network_config import validate_network_config

STRICT = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wconversion",
          "-Wshadow", "-Wstrict-prototypes"]
SANITIZE = ["-fsanitize=undefined,address", "-fno-sanitize-recover=all"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", required=True)
    parser.add_argument("--sdk", required=True)
    parser.add_argument("--firmware", action="store_true")
    parser.add_argument("--debug-python", help="Optional Windows Python exe for Tcl-only tests when WSL lacks tkinter")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]+", args.label):
        parser.error("Use a unique alphanumeric/underscore/hyphen evidence label")
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    repo = Path(__file__).resolve().parents[1]
    sdk = Path(args.sdk).resolve()
    logs = repo / "testing/integration/logs"
    if list(logs.glob(args.label + "-*")):
        parser.error("Evidence label already exists")
    alias = Path("/tmp") / ("aai05-sdk-" + hashlib.sha256(str(sdk).encode()).hexdigest()[:10])
    if alias.is_symlink():
        if alias.resolve() != sdk:
            parser.error("SDK alias collision")
    elif alias.exists():
        parser.error("SDK alias occupied")
    else:
        alias.symlink_to(sdk, target_is_directory=True)
    env = dict(os.environ, LC_ALL="C", PICO_SDK_PATH=str(alias))
    if args.firmware:
        validate_network_config(repo / "config/wifi_secrets.h")
        expected = "71bb51547bf2f81b324d5fa796df367076fb98bc8a0eb687a1450f7eedfcf98a"
        if hashlib.sha256((sdk / "lib/lwip/src/apps/mqtt/mqtt.c").read_bytes()).hexdigest() != expected:
            parser.error("MQTT parser contract changed; review before building")
    artifacts = repo / "output/firmware"
    before = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in artifacts.rglob("*.uf2")}
    summary = dict(status="RUNNING", hardware_tests="NOT RUN", results=[],
                   source_commit=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip())

    def run(command, name, cwd=repo):
        log = logs / (args.label + "-" + name + ".log")
        print(name + ": running", flush=True)
        with log.open("x", encoding="utf-8") as output:
            result = subprocess.run(command, cwd=cwd, env=env, stdout=output, stderr=subprocess.STDOUT, text=True)
        warnings = [line for line in log.read_text(errors="replace").splitlines() if "warning:" in line.lower()]
        summary["results"].append(dict(name=name, command=command, exit_code=result.returncode,
                                       warnings=warnings, log=str(log.relative_to(repo))))
        if result.returncode or warnings:
            raise RuntimeError(name + " failed/warned; inspect " + str(log))
        print(name + ": PASS", flush=True)
        return log

    try:
        run([sys.executable, "tools/assemble_encoder_pio.py", "--sdk", str(sdk), "--check"], "assembler")
        with tempfile.TemporaryDirectory(prefix="aai05-pio-check-") as temporary:
            def c_test(name, sources, extra=(), strict=True, cases=((),)):
                executable = str(Path(temporary) / name)
                flags = STRICT if strict else ["-std=c11", "-Wall", "-Wextra", "-Werror"]
                run(["gcc", *flags, *SANITIZE, "-Iapp_program/subsystem2", "-Iapp_program/subsystem3",
                     "-Iapp_program/subsystem1", "-Itesting/host", *extra, *sources, "-o", executable], name + "-compile")
                last = None
                for index, case in enumerate(cases):
                    last = run([executable, *case], name + "-" + str(index))
                return last

            c_test("pio-count", ["testing/host/test_encoder_pio_count.c", "app_program/subsystem2/encoder_pio_count.c"])
            c_test("pio-io", ["testing/host/test_encoder_pio_io.c", "app_program/subsystem2/encoder_pio_io_rp2040.c"],
                   ["-DAAI05_ENCODER_PIO=1", "-Itesting/host/pio_fixture"],
                   cases=[(case,) for case in ("success", "busy", "programbusy", "unclaimedactive", "initfail", "disabled", "frozen")])
            c_test("pio-adapter", ["testing/host/test_encoder_capture_pio.c", "app_program/subsystem2/encoder_pio_count.c"],
                   ["-DAAI05_ENCODER_PIO=1"], cases=[(case,) for case in ("success", "startfail", "half")])
            c_test("gpio-deferred", ["testing/host/test_encoder_gpio_deferred.c", "app_program/subsystem2/encoder_capture.c", "app_program/subsystem2/encoder_logic.c"])
            c_test("encoder", ["testing/host/test_encoder.c", "app_program/subsystem2/encoder_logic.c"], strict=False)
            c_test("gpio-capture", ["testing/host/test_encoder_capture.c", "app_program/subsystem2/encoder_capture.c", "app_program/subsystem2/encoder_logic.c"], strict=False)
            c_test("gpio-startup", ["testing/host/test_encoder_startup.c"], strict=False)
            c_test("registers", ["testing/host/test_rp2040_registers.c"], strict=False)
            c_test("motor-startup", ["testing/host/test_motor_startup.c", "app_program/subsystem2/drv_motor.c"], strict=False)
            c_test("motor-control", ["testing/host/test_motor_control.c", "app_program/subsystem2/motor_test.c"], strict=False)
            c_test("motor-pulse", ["testing/host/test_motor_pulse.c"], strict=False)
            c_test("motor-probe", ["testing/host/test_motor_pulse.c"], ["-DMOTOR_TEST_PROBE=1"], strict=False)
            c_test("motion", ["testing/host/test_motion_control.c", "app_program/subsystem2/motion_control.c", "app_program/subsystem2/motion_button.c"])
            for remote in (0, 1):
                c_test("motion-runtime" + str(remote), ["testing/host/test_motion_runtime.c", "app_program/subsystem2/motion_control.c", "app_program/subsystem2/motion_button.c"],
                       ["-pthread", "-Iinclude", "-DAAI05_REMOTE_APP=" + str(remote)])
            c_test("drive-capture-fault", ["testing/host/test_drive_capture_fault.c"],
                   ["-DAAI05_REMOTE_APP=1", "-Ilib/libwifi/sysdepend/pico_rp2040", "-Ilib/liblwip/sysdepend/pico_rp2040"])
            vector = c_test("drive-protocol", ["testing/host/test_drive_protocol.c", "app_program/subsystem3/drive_protocol.c", "app_program/subsystem3/comm_protocol.c"])
            env["DRIVE_VECTOR_FILE"] = str(vector)
            c_test("drive-control", ["testing/host/test_drive_control.c", "app_program/subsystem3/drive_control.c"])
            c_test("drive-service", ["testing/host/test_drive_service.c", "app_program/subsystem3/mqtt_drive_app.c", "app_program/subsystem3/drive_control.c",
                                     "app_program/subsystem3/drive_protocol.c", "app_program/subsystem3/comm_protocol.c"], ["-DAAI05_REMOTE_APP=1"])
            vector = c_test("comm-protocol", ["testing/host/test_comm_protocol.c", "app_program/subsystem3/comm_protocol.c"])
            env["COMM_VECTOR_FILE"] = str(vector)
            c_test("colour", ["testing/host/test_colour_classifier.c", "app_program/subsystem1/colour_classifier.c"])
            for remote in (0, 1):
                c_test("mqtt-ingress" + str(remote), ["testing/host/test_mqtt_ingress.c"],
                       ["-DAAI05_REMOTE_APP=" + str(remote), "-DAAI05_COMMS_APP=" + str(1 - remote),
                        "-Itesting/host/mqtt_fixture", "-Ilib/liblwip/sysdepend/pico_rp2040",
                        "-I" + str(alias / "lib/lwip/src/include")], strict=False)
        try:
            import tkinter  # noqa: F401; the existing debug tests require Tcl, not a GUI.
            has_tcl = True
        except ImportError:
            has_tcl = False
        if has_tcl:
            run([sys.executable, "-m", "unittest", "discover", "-s", "testing/host", "-p", "test_*.py", "-v"], "all-python")
        else:
            if not args.debug_python:
                raise RuntimeError("WSL lacks tkinter; supply --debug-python for the six existing Tcl debug tests")
            modules = ["testing.host." + p.stem for p in sorted((repo / "testing/host").glob("test_*.py"))
                       if p.name != "test_debug_capture.py"]
            run([sys.executable, "-m", "unittest", "-v", *modules], "all-python-except-tcl")
            run([args.debug_python, "-m", "unittest", "discover", "-s", "testing/host",
                 "-p", "test_debug_capture.py", "-v"], "windows-tcl-debug")
        if args.firmware:
            candidate = artifacts / ("encoder_pio_" + args.label)
            candidate.mkdir(exist_ok=False)
            profile = ["SMP=1", "CONSOLE=usb_cdc", "WIFI=cyw43", "NET=lwip", "MQTT=1", "CRASH_REPORT=0",
                       "MOTOR_PROBE=0", "COMM_APP=0", "MOTION_APP=1", "REMOTE_APP=1"]
            for pio, test in ((1, 0), (1, 1), (0, 0), (0, 1)):
                stem = "mtk3pico_pio_check_" + args.label + f"_pio{pio}_test{test}"
                run(["make", "-j8", *profile, f"ENCODER_PIO={pio}", f"TEST={test}", "EXE_FILE=" + stem],
                    f"firmware-pio{pio}-test{test}", repo / "build_make")
                name = ("AAI05_Encoder_PIO_v0_9_REMOTE" if pio else "AAI05_GPIO_Compatibility") + ("_TEST" if test else "")
                for extension in ("uf2", "elf", "map"):
                    shutil.copy2(repo / "build_make" / (stem + "." + extension), candidate / (name + "." + extension))
                image = candidate / (name + ".uf2")
                summary["results"][-1].update(uf2=str(image.relative_to(repo)),
                    sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
                    size=subprocess.check_output(["arm-none-eabi-size", str(candidate / (name + ".elf"))], text=True).strip())
        summary["status"] = "PASS"
    except Exception:
        summary["status"] = "FAILED"
        raise
    finally:
        summary["old_artifacts"] = [dict(path=str(p.relative_to(repo)), sha256=digest,
            unchanged=p.exists() and hashlib.sha256(p.read_bytes()).hexdigest() == digest) for p, digest in before.items()]
        if any(not entry["unchanged"] for entry in summary["old_artifacts"]):
            summary["status"] = "FAILED"
        with (logs / (args.label + "-result.json")).open("x", encoding="utf-8") as output:
            json.dump(summary, output, indent=2)
            output.write("\n")
    if summary["status"] != "PASS":
        raise RuntimeError("Artifact preservation failed")
    print(f"PASS: {len(summary['results'])} stages; {len(before)} earlier UF2s unchanged; hardware pending.")


if __name__ == "__main__":
    main()
