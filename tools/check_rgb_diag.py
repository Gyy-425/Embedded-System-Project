"""Run in Ubuntu/WSL: RGB host checks and optional distinct firmware builds.

Never flashes hardware, contacts a broker, changes secrets or replaces old UF2s.
"""
import argparse
import csv
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
    parser.add_argument("--firmware", action="store_true")
    parser.add_argument("--home-track", action="store_true",
                        help="Also replay the new home track calibration")
    parser.add_argument("--sdk")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]+", args.label):
        parser.error("Use letters, digits, underscore or hyphen in label")
    repo = Path(__file__).resolve().parents[1]
    logs = repo / "testing/integration/logs"
    logs.mkdir(parents=True, exist_ok=True)
    if list(logs.glob(args.label + "-*")):
        parser.error("Evidence label already exists")
    env = dict(os.environ, LC_ALL="C")
    evidence = {"label": args.label, "hardware_tests": "NOT RUN", "results": []}
    artifacts = repo / "output/firmware"
    before = {p: hashlib.sha256(p.read_bytes()).hexdigest()
              for p in artifacts.rglob("*.uf2")}

    def run(command, name, cwd=repo, expected_failure=False):
        path = logs / (args.label + "-" + name + ".log")
        with path.open("x", encoding="utf-8") as output:
            result = subprocess.run(command, cwd=cwd, env=env, text=True,
                                    stdout=output, stderr=subprocess.STDOUT)
        content = path.read_text(errors="replace")
        warnings = [line for line in content.splitlines() if "warning:" in line.lower()]
        evidence["results"].append(dict(name=name, exit_code=result.returncode,
                                        expected_failure=expected_failure,
                                        warnings=warnings, log=str(path.relative_to(repo))))
        if bool(result.returncode) != expected_failure or (warnings and not expected_failure):
            print(content[-7000:], flush=True)
            raise RuntimeError(name + " failed or warned")
        print(name + (": rejected as intended" if expected_failure else ": PASS"), flush=True)

    try:
        strict = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wconversion",
                  "-Wshadow", "-Wstrict-prototypes"]
        with tempfile.TemporaryDirectory(prefix="aai05-rgb-") as scratch:
            lab_vectors, home_vectors = [], []
            splits = ("calibration", "validation", "unsupported-red", "fresh-cardboard",
                      "track-black-initial", "track-black-repositioned", "track-cardboard", "black-flicker",
                      "yellow-centre", "yellow-robot-left", "yellow-robot-right",
                      "red-centre", "red-robot-left", "red-robot-right",
                      "blue-centre", "blue-robot-left", "blue-robot-right",
                      "home-yellow", "home-blue", "home-red", "home-background")
            for stem in ("rgb_2026_11_16", "rgb_2026_11_17_track", "rgb_2026_11_17_black_flicker",
                         "rgb_2026_11_17_yellow_corridor", "rgb_2026_11_17_red_corridor",
                         "rgb_2026_11_19_blue_corridor", "rgb_2026_11_19_home"):
                vectors = home_vectors if stem == "rgb_2026_11_19_home" else lab_vectors
                fixture = repo / "testing/host/fixtures" / (stem + ".csv")
                provenance = json.loads(fixture.with_name(stem + "_provenance.json").read_text())
                if hashlib.sha256(fixture.read_bytes()).hexdigest() != provenance["fixture_sha256"]:
                    raise RuntimeError("RGB fixture changed without provenance update")
                with fixture.open(newline="", encoding="utf-8") as stream:
                    for row in csv.DictReader(stream):
                        if row["split"] not in splits:
                            raise RuntimeError("Unknown RGB replay split")
                        values = {k: int(v) for k, v in row.items() if k != "split"}
                        fields = ("sampled_ms", "sequence", "red", "green", "blue", "clear",
                                  "valid", "saturated", "status", "device_id", "ready", "has_sample")
                        snapshot = ", ".join("." + k + "=" + str(values[k]) + "U" for k in fields)
                        reason = values.get("expected_reason", 2 if values["expected"] == 7 else 0)
                        vectors.append('{"' + row["split"] + '", ' + str(values["now_ms"]) +
                                       'U, {' + snapshot + '}, ' + str(values["expected"]) + 'U, ' +
                                       str(reason) + 'U, ' + str(values["gain"]) + 'U, ' +
                                       str(values["integration_us"]) + 'U}')
            room2_vectors = []
            room2_fixture = repo / "testing/host/fixtures/rgb_2026_11_22_room2.csv"
            room2_prov = json.loads(room2_fixture.with_name("rgb_2026_11_22_room2_provenance.json").read_text())
            if hashlib.sha256(room2_fixture.read_bytes()).hexdigest() != room2_prov["fixture_sha256"]:
                raise RuntimeError("ROOM2 RGB fixture changed without provenance update")
            with room2_fixture.open(newline="", encoding="utf-8") as stream:
                for row in csv.DictReader(stream):
                    if row["split"] not in ("room2-blue", "room2-background"):
                        raise RuntimeError("Unknown ROOM2 replay split")
                    v = {k: int(x) for k, x in row.items() if k != "split"}
                    room2_vectors.append(
                        '{"%s", %dU, {.sampled_ms=%dU, .sequence=%dU, .red=%dU, .green=%dU, '
                        '.blue=%dU, .clear=%dU, .valid=%dU, .saturated=%dU, .status=%dU, '
                        '.device_id=%dU, .ready=%dU, .has_sample=%dU}, %dU, %dU, %dU, %dU}' % (
                            row["split"], v["now_ms"], v["sampled_ms"], v["sequence"], v["red"],
                            v["green"], v["blue"], v["clear"], v["valid"], v["saturated"],
                            v["status"], v["device_id"], v["ready"], v["has_sample"],
                            v["expected"], v["expected_reason"], v["gain"], v["integration_us"]))
            (Path(scratch) / "rgb_room2_replay_vectors.h").write_text(
                "static const replay_vector_t g_room2_vectors[] = {\n" + ",\n".join(room2_vectors) + "\n};\n")
            for name, vectors in [("rgb_replay_vectors.h", lab_vectors),
                                  ("rgb_home_replay_vectors.h", lab_vectors + home_vectors)]:
                (Path(scratch) / name).write_text(
                    "static const replay_vector_t g_vectors[] = {\n" + ",\n".join(vectors) + "\n};\n")
            classifier = ["app_program/subsystem1/rgb_classifier.c",
                          "app_program/subsystem1/rgb_calibration.c"]
            cases = [
                ("classifier", ["testing/host/test_rgb_classifier.c", *classifier],
                 ["app_program/subsystem1"], []),
                ("classifier-replay", ["testing/host/test_rgb_classifier_replay.c", *classifier],
                 ["app_program/subsystem1", scratch], []),
                ("home-classifier", ["testing/host/test_rgb_classifier.c", *classifier],
                 ["app_program/subsystem1"], ["-DAAI05_RGB_HOME_PROFILE=1"]),
                ("home-replay", ["testing/host/test_rgb_classifier_replay.c", *classifier],
                 ["app_program/subsystem1", scratch], ["-DAAI05_RGB_HOME_PROFILE=1"]),
                ("core", ["testing/host/test_rgb_sensor.c", "app_program/subsystem1/rgb_sensor.c"],
                 ["app_program/subsystem1"], []),
                ("task", ["testing/host/test_rgb_diag.c", "app_program/subsystem1/rgb_sensor.c", *classifier],
                 ["app_program/subsystem1"], []),
                ("adapter", ["testing/host/test_rgb_i2c.c", "app_program/subsystem1/rgb_i2c_rp2040.c"],
                 ["testing/host/rgb_stubs", "app_program/subsystem1"], ["-DAAI05_RGB_DIAG=1"]),
                ("colour-regression", ["testing/host/test_colour_classifier.c", "app_program/subsystem1/colour_classifier.c"],
                 ["app_program/subsystem1", "testing/host"], []),
                ("runtime-regression", ["testing/host/test_colour_runtime.c", "app_program/subsystem1/colour_classifier.c",
                                        "app_program/subsystem1/colour_runtime.c"],
                 ["app_program/subsystem1", "testing/host"], []),
            ]
            if args.home_track:
                track_defines = ["-DAAI05_RGB_HOME_PROFILE=1", "-DFOLLOW_HOME_TRACK=1"]
                cases.extend([
                    ("home-track-classifier", ["testing/host/test_rgb_classifier.c", *classifier],
                     ["app_program/subsystem1"], track_defines),
                    ("home-track-replay", ["testing/host/test_rgb_classifier_replay.c", *classifier],
                     ["app_program/subsystem1", scratch], track_defines),
                    ("room2-replay", ["testing/host/test_rgb_room2_replay.c", *classifier],
                     ["app_program/subsystem1", scratch], [*track_defines, "-DAAI05_RGB_ROOM2=1"]),
                ])
            for name, files, includes, defines in cases:
                exe = str(Path(scratch) / name)
                run(["gcc", *strict, "-fsanitize=undefined,address", "-fno-sanitize-recover=all",
                     *defines, *["-I" + p for p in includes], *files, "-o", exe], name + "-compile")
                run([exe], name)
            for module in ("rgb_sensor", "rgb_classifier", "rgb_calibration"):
                run(["arm-none-eabi-gcc", *strict, "-mcpu=cortex-m0plus", "-mthumb",
                     "-ffreestanding", "-c", "app_program/subsystem1/" + module + ".c",
                     "-o", str(Path(scratch) / (module + "_target.o"))], module + "-target-compile")
            run(["arm-none-eabi-gcc", *strict, "-mcpu=cortex-m0plus", "-mthumb",
                 "-DAAI05_RGB_HOME_PROFILE=1", "-ffreestanding", "-c",
                 "app_program/subsystem1/rgb_calibration.c", "-o",
                 str(Path(scratch) / "rgb_home_calibration_target.o")], "home-calibration-target-compile")
            run(["arm-none-eabi-gcc", *strict, "-mcpu=cortex-m0plus", "-mthumb",
                 "-DAAI05_RGB_HOME_PROFILE=1", "-DFOLLOW_HOME_TRACK=1", "-DAAI05_RGB_ROOM2=1",
                 "-ffreestanding", "-c", "app_program/subsystem1/rgb_calibration.c", "-o",
                 str(Path(scratch) / "rgb_room2_calibration_target.o")], "room2-calibration-target-compile")
        if args.firmware:
            if not args.sdk:
                raise RuntimeError("--firmware requires --sdk")
            sdk = Path(args.sdk).resolve()
            for relative in ("pico_sdk_init.cmake", "src/rp2_common/hardware_i2c/i2c.c",
                             "lib/tinyusb/src/tusb.c", "lib/cyw43-driver/src/cyw43_ctrl.c"):
                if not (sdk / relative).is_file():
                    raise RuntimeError("SDK dependency missing: " + relative)
            alias = Path("/tmp/aai05-rgb-sdk")
            if alias.is_symlink():
                if alias.resolve() != sdk:
                    raise RuntimeError("SDK alias differs")
            elif alias.exists():
                raise RuntimeError("SDK alias occupied")
            else:
                alias.symlink_to(sdk, target_is_directory=True)
            env["PICO_SDK_PATH"] = str(alias)
            common = ["SMP=1", "CONSOLE=usb_cdc", "WIFI=cyw43", "CRASH_REPORT=0"]
            rgb = [*common, "RGB_DIAG=1", "NET=none", "MQTT=0", "MOTOR_PROBE=1",
                   "COMM_APP=0", "MOTION_APP=0", "REMOTE_APP=0", "ENCODER_PIO=0"]
            for name, bad in [("powered", ["MOTOR_PROBE=0"]),
                              ("remote", ["MOTION_APP=1", "REMOTE_APP=1", "NET=lwip", "MQTT=1"]),
                              ("uart", ["CONSOLE=uart"])]:
                run(["make", "-n", *rgb, *bad], "reject-" + name,
                    cwd=repo / "build_make", expected_failure=True)
            candidate = artifacts / ("rgbdiag_" + args.label)
            candidate.mkdir(parents=True, exist_ok=False)
            evidence["artifacts"] = []
            profiles = [("RGB_Classifier_v0_12_1", rgb, 0),
                        ("RGB_Classifier_v0_12_1_TEST", rgb, 1),
                        ("Remote_PIO_Regression", [*common, "RGB_DIAG=0", "NET=lwip", "MQTT=1",
                         "MOTOR_PROBE=0", "COMM_APP=0", "MOTION_APP=1", "REMOTE_APP=1", "ENCODER_PIO=1"], 0)]
            for name, flags, test in profiles:
                stem = "aai05_" + args.label + "_" + name.lower()
                run(["make", "-j8", *flags, "TEST=" + str(test), "EXE_FILE=" + stem],
                    "build-" + name, cwd=repo / "build_make")
                for ext in ("uf2", "elf", "map"):
                    target = candidate / ("AAI05_" + name + "." + ext)
                    if target.exists():
                        raise RuntimeError("Artifact exists")
                    shutil.copyfile(repo / "build_make" / (stem + "." + ext), target)
                    evidence["artifacts"].append(dict(path=str(target.relative_to(repo)),
                        sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
        evidence["status"] = "PASS"
    except Exception as error:
        evidence["status"] = "FAILED"
        evidence["error"] = str(error)
        raise
    finally:
        evidence["old_uf2s"] = len(before)
        evidence["old_uf2s_unchanged"] = all(
            p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest() == digest
            for p, digest in before.items())
        (logs / (args.label + "-result.json")).write_text(json.dumps(evidence, indent=2) + "\n")
        if not evidence["old_uf2s_unchanged"]:
            raise RuntimeError("An existing UF2 changed")
    print("PASS: RGB software checks; hardware NOT RUN; " + str(len(before)) + " prior UF2s unchanged")


if __name__ == "__main__":
    main()
