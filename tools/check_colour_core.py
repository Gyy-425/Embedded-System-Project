"""Run in Ubuntu/WSL. Host/target checks; optional distinct compile-only firmware."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from network_config import validate_network_config

PARSER_SHA = "71bb51547bf2f81b324d5fa796df367076fb98bc8a0eb687a1450f7eedfcf98a"
STRICT = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wconversion",
          "-Wshadow", "-Wstrict-prototypes"]
HOST_FLAGS = [*STRICT, "-fsanitize=undefined,address", "-fno-sanitize-recover=all"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", required=True, help="Unique evidence prefix")
    parser.add_argument("--firmware", action="store_true", help="Build distinct normal/TEST candidates")
    parser.add_argument("--sdk", help="Existing SDK2.3.0 path, needed for --firmware")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]+", args.label):
        parser.error("Label must contain only letters, digits, underscore or hyphen")
    repo = Path(__file__).resolve().parents[1]
    logs = repo / "testing/integration/logs"
    logs.mkdir(parents=True, exist_ok=True)
    if list(logs.glob(f"{args.label}-*")):
        parser.error("Evidence label already exists; use a new one")
    env = dict(os.environ, LC_ALL="C")
    sdk = None
    if args.firmware:
        if not args.sdk:
            parser.error("--firmware requires --sdk")
        sdk = Path(args.sdk).resolve()
        for name in ("pico_sdk_init.cmake", "lib/tinyusb/src/tusb.c",
                     "lib/cyw43-driver/src/cyw43_ctrl.c", "lib/lwip/src/apps/mqtt/mqtt.c"):
            if not (sdk / name).is_file():
                parser.error(f"SDK dependency missing: {name}")
        if hashlib.sha256((sdk / "lib/lwip/src/apps/mqtt/mqtt.c").read_bytes()).hexdigest() != PARSER_SHA:
            parser.error("SDK MQTT parser changed; review retained-header contract first")
        validate_network_config(repo / "config/wifi_secrets.h")
        alias = Path("/tmp") / ("aai05-sdk-" + hashlib.sha256(str(sdk).encode()).hexdigest()[:10])
        if alias.is_symlink():
            if alias.resolve() != sdk:
                parser.error("SDK alias points elsewhere")
        elif alias.exists():
            parser.error("SDK alias path is occupied")
        else:
            alias.symlink_to(sdk, target_is_directory=True)
        env["PICO_SDK_PATH"] = str(alias)
    artifacts = repo / "output/firmware"
    before = {path: hashlib.sha256(path.read_bytes()).hexdigest()
              for path in artifacts.rglob("*.uf2")}
    summary = dict(label=args.label, hardware_tests="NOT RUN", results=[],
                   source_commit=subprocess.check_output(
                       ["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip(),
                   source_dirty=bool(subprocess.check_output(
                       ["git", "status", "--porcelain"], cwd=repo, text=True).strip()),
                   firmware_runtime="UNCHANGED; core not activated")

    def run(command, name, cwd=repo, input_text=None):
        log = logs / f"{args.label}-{name}.log"
        print(f"{name}: running; {log.name}", flush=True)
        with log.open("x", encoding="utf-8") as output:
            result = subprocess.run(command, cwd=cwd, env=env, input=input_text,
                                    stdout=output, stderr=subprocess.STDOUT, text=True)
        warnings = [line for line in log.read_text(errors="replace").splitlines()
                    if "warning:" in line.lower()]
        summary["results"].append(dict(name=name, command=command,
                                       exit_code=result.returncode, warnings=warnings,
                                       log=str(log.relative_to(repo))))
        if result.returncode or warnings:
            raise RuntimeError(f"{name} failed or warned; inspect {log}")
        print(f"{name}: PASS", flush=True)

    try:
        run([sys.executable, "testing/host/test_colour_sanitizer_gate.py", "-v"],
            "sanitizer-gate")
        with tempfile.TemporaryDirectory(prefix="aai05-colour-") as temporary:
            host = str(Path(temporary) / "colour_host")
            run(["gcc", *HOST_FLAGS,
                 "-Iapp_program/subsystem1", "-Itesting/host",
                 "testing/host/test_colour_classifier.c",
                 "app_program/subsystem1/colour_classifier.c", "-o", host], "host-compile")
            run([host], "host")
            target_flags = ["arm-none-eabi-gcc", *STRICT, "-mcpu=cortex-m0plus",
                            "-mthumb", "-ffreestanding", "-Iapp_program/subsystem1",
                            "-Itesting/host"]
            run([*target_flags, "-c", "app_program/subsystem1/colour_classifier.c",
                 "-o", str(Path(temporary) / "colour_target.o")], "target-compile")
            run([*target_flags, "-x", "c", "-include", "testing/host/colour_reference.h",
                 "-c", "-", "-o", str(Path(temporary) / "layout_target.o")],
                "target-layout", input_text="")
        if args.firmware:
            candidate = artifacts / ("colour_core_" + args.label)
            candidate.mkdir(parents=True, exist_ok=False)
            profile = ["SMP=1", "CONSOLE=usb_cdc", "WIFI=cyw43", "NET=lwip",
                       "MQTT=1", "CRASH_REPORT=0", "MOTOR_PROBE=0", "COMM_APP=0",
                       "MOTION_APP=1", "REMOTE_APP=1"]
            for test in (0, 1):
                stem = "mtk3pico_colour_core_" + args.label + ("_test" if test else "")
                run(["make", "-j8", *profile, f"TEST={test}", f"EXE_FILE={stem}"],
                    f"firmware-test{test}", cwd=repo / "build_make")
                name = "AAI05_Colour_Core_Compile_Check" + ("_TEST" if test else "")
                for extension in ("uf2", "elf", "map"):
                    shutil.copy2(repo / "build_make" / f"{stem}.{extension}",
                                 candidate / f"{name}.{extension}")
                record = summary["results"][-1]
                image = candidate / f"{name}.uf2"
                record["uf2"] = str(image.relative_to(repo))
                record["sha256"] = hashlib.sha256(image.read_bytes()).hexdigest()
                record["size"] = subprocess.check_output(
                    ["arm-none-eabi-size", str(candidate / f"{name}.elf")], text=True).strip()
            (candidate / "README.txt").write_text(
                "Compile-compatibility candidates only. Classifier not active.\n"
                "Keep the previously tested v0.8 image installed; no reflash needed.\n")
        summary["old_artifacts"] = []
        for path, digest in before.items():
            unchanged = path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == digest
            summary["old_artifacts"].append(dict(path=str(path.relative_to(repo)),
                                                  sha256=digest, unchanged=unchanged))
            if not unchanged:
                raise RuntimeError(f"Pre-existing artifact changed: {path.name}")
        summary["status"] = "PASS"
    except Exception:
        summary["status"] = "FAILED"
        raise
    finally:
        with (logs / f"{args.label}-result.json").open("x", encoding="utf-8") as output:
            json.dump(summary, output, indent=2)
            output.write("\n")
    print(f"Complete; {len(before)} pre-existing UF2s unchanged. No upload/device test.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"error: {error}")
