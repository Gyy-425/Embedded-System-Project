"""Build/test standalone route measurement in Ubuntu/WSL; never flashes a car."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]+", args.label):
        parser.error("Use a new alphanumeric label")
    repo = Path(__file__).resolve().parents[1]
    logs = repo / "testing/integration/logs"
    if list(logs.glob(args.label + "-*")):
        parser.error("Label already exists")
    old = {p: hashlib.sha256(p.read_bytes()).hexdigest()
           for p in (repo / "output/firmware").rglob("*.uf2")}
    evidence = dict(status="RUNNING", hardware="NOT RUN", results=[])
    env = dict(os.environ, LC_ALL="C")

    def run(command, name):
        path = logs / (args.label + "-" + name + ".log")
        with path.open("x", encoding="utf-8") as stream:
            result = subprocess.run(command, cwd=repo, env=env, stdout=stream,
                                    stderr=subprocess.STDOUT)
        output = path.read_text(encoding="utf-8", errors="replace")
        warnings = [line for line in output.splitlines() if "warning:" in line.lower()]
        evidence["results"].append(dict(name=name, exit_code=result.returncode,
                                        warnings=warnings, log=str(path.relative_to(repo))))
        if result.returncode or warnings:
            print(output[-4000:], flush=True)
            raise RuntimeError(name + " failed/warned")
        print(name + ": PASS", flush=True)


    try:
        strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wconversion",
                  "-Wshadow", "-Wstrict-prototypes"]
        source = "app_program/subsystem2/route_measurement.c"
        run(["python3", "testing/host/test_calibrate_distance.py"], "calibration-cli")
        with tempfile.TemporaryDirectory(prefix="aai05-distance-") as scratch:
            exe = str(Path(scratch) / "route_measurement_tests")
            run(["gcc", *strict, "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                 "-Iapp_program/subsystem2", "testing/host/test_route_measurement.c",
                 source, "-o", exe], "host-compile")
            run([exe], "behaviour")
            obj = str(Path(scratch) / "route_measurement.o")
            run(["arm-none-eabi-gcc", *strict, "-O2", "-mcpu=cortex-m0plus", "-mthumb",
                 "-ffreestanding", "-c", source, "-o", obj], "arm-compile")
            library = str(Path(scratch) / "libroute_measurement.a")
            run(["arm-none-eabi-ar", "rcs", library, obj], "arm-library")
            target = repo / "output/software" / args.label
            target.mkdir(parents=True, exist_ok=False)
            for src, name in [(exe, "route_measurement_tests_linux"), (library, "libroute_measurement.a"),
                    (repo / source, "route_measurement.c"),
                    (repo / "app_program/subsystem2/route_measurement.h", "route_measurement.h")]:
                shutil.copyfile(src, target / name)
            evidence["software_directory"] = str(target.relative_to(repo))
        evidence["status"] = "PASS"
    except Exception as error:
        evidence.update(status="FAILED", error=str(error))
        raise
    finally:
        evidence["old_uf2s"] = len(old)
        evidence["old_uf2s_unchanged"] = all(p.exists() and hashlib.sha256(p.read_bytes()).hexdigest() == h
                                               for p, h in old.items())
        (logs / (args.label + "-result.json")).write_text(json.dumps(evidence, indent=2) + "\n")
        if not evidence["old_uf2s_unchanged"]:
            raise RuntimeError("Previous firmware changed")
    print("PASS: standalone distance software; hardware NOT RUN; no firmware flashed")


if __name__ == "__main__":
    main()
