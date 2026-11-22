"""Build the powered MQTT follow image in the existing Ubuntu toolchain."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import struct
import subprocess

from network_config import validate_network_config


# Steering flags match the powered v0.87 D25/T8 home profile.
STEERING = """
CLIPPED EARLY_BEND HOME_TRACK BLUE_STEER_RAMP YELLOW_EXIT_CUE YELLOW_LEAD_CUE
BLUE_REVERSAL_DAMPING BLUE_STEER_WIDE_RANGE STOP_TRACE BLUE_RED_HANDOFF
BLUE_OUTER_EDGE BLUE_OUTER_RECOVERY BLUE_BEND_SLOW BLUE_CONFIRM_GRACE BEND_TRACE
BLUE_EXIT_LEAD BLUE_RED_CONFIRM_GRACE BLUE_EXIT_AFTER_RECOVERY RED_EXIT_LEAD
RED_EXIT_EARLY BLUE_RED_BEND_TRIAL BLUE_RED_BEND_CUES RED_SECOND_PIVOT
RED_SECOND_SLOW RED_BEND_AFTER_HANDOFF RED_FIRST_BEND_BOOST RED_FIRST_BEND_TIGHTER
BLUE_RED_JOIN_REFINEMENT BEND_CUE_REFINEMENT RED_SECOND_EXIT_REFINEMENT
BLUE_DIAGONAL_EXIT RED_HANDOFF_ALIGN RED_EXIT_ALIGN RED_BETWEEN_BENDS
RED_INSIDE_RELEASE RED_EXIT_TURN RED_EXIT_TURN_EARLY RED_EXIT_TURN_TIGHTER
RED_RETURN_GRACE RED_EXIT_TURN_STRONG RED_EXIT_TURN_HOLD BLUE_EXIT_PIVOT
BLUE_RED_LATE_CONFIRM BLUE_RED_BLEND_GRACE BLUE_RED_BLEND_MARGIN BLUE_EXIT_LEFT_BOOST
RED_FIRST_LEFT_BOOST RED_SECOND_ARC_BOOST BLUE_SECOND_LEFT_BOOST RED_FINAL_LEFT_BOOST
BLUE_EXIT_EDGE_CONFIRM BLUE_RED_LATE_APPROACH RED_DIAGONAL_REACQUIRE
RED_FINAL_ARC_EXTEND RED_FINAL_ARC_STRONG BLUE_RED_JOIN_EXTEND RED_PIVOT_INSIDE_RELEASE
RED_OUTSIDE_RECOVERY RED_RIGHT_EDGE_OUTSIDE RED_CORNER_REFINE RED_SPIN_HOLD
""".split()

PROFILE = [
    "SMP=1", "CONSOLE=usb_cdc", "WIFI=cyw43", "NET=lwip", "MQTT=1",
    "CRASH_REPORT=0", "MOTOR_PROBE=0", "COMM_APP=0", "MOTION_APP=1",
    "REMOTE_APP=1", "ENCODER_PIO=1", "RGB_HOME=1", "RGB_ROOM2=0",
    "FOLLOW_MQTT=1", "FOLLOW_APP=1", "FOLLOW_MULTI=1", "FOLLOW_DUTY=25",
    "FOLLOW_TURN=8", "FOLLOW_LOOP_RECOVERY_DIRECTION=1", "FOLLOW_BLUE_RAMP_START=5",
    "FOLLOW_BLUE_EXIT_TURN=22", "TEST=0",
    *("FOLLOW_" + name + "=1" for name in STEERING),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", default=os.environ.get("PICO_SDK_PATH"))
    parser.add_argument("--label", default="follow_mqtt_v1")
    parser.add_argument("--wifi-auth", choices=("default", "wpa3"), default="default")
    args = parser.parse_args()
    if not args.sdk or not re.fullmatch(r"[a-zA-Z0-9_-]+", args.label):
        parser.error("Pass --sdk and use only letters, digits, dashes or underscores in --label")
    repo = Path(__file__).resolve().parents[1]
    sdk = Path(args.sdk).resolve()
    try:
        validate_network_config(repo / "config/wifi_secrets.h")
        parser_source = sdk / "lib/lwip/src/apps/mqtt/mqtt.c"
        if hashlib.sha256(parser_source.read_bytes()).hexdigest() != (
            "71bb51547bf2f81b324d5fa796df367076fb98bc8a0eb687a1450f7eedfcf98a"
        ):
            parser.error("Use the pinned Pico SDK 2.3.0 MQTT parser")
    except (ValueError, OSError) as error:
        parser.error(str(error))
    alias = Path("/tmp") / ("aai05-sdk-" + hashlib.sha256(str(sdk).encode()).hexdigest()[:10])
    if not alias.exists():
        alias.symlink_to(sdk, target_is_directory=True)
    if alias.resolve(strict=True) != sdk:
        parser.error("Existing SDK alias points elsewhere")

    output = repo / "output/firmware" / args.label
    output.mkdir(parents=True, exist_ok=True)
    suffix = "_WPA3" if args.wifi_auth == "wpa3" else ""
    image = output / f"AAI05_MQTT_Follow_Distance_v1{suffix}.uf2"
    if image.exists():
        parser.error("Use a new build label to preserve the existing UF2")
    stem = "aai05_" + args.label
    build_log = output / "build.log"
    with build_log.open("w", encoding="utf-8") as stream:
        built = subprocess.run(
            ["make", "-j8", *PROFILE, "WIFI_AUTH=" + args.wifi_auth, "EXE_FILE=" + stem],
            cwd=repo / "build_make", env=dict(os.environ, LC_ALL="C", PICO_SDK_PATH=str(alias)),
            stdout=stream, stderr=subprocess.STDOUT,
        )
    if built.returncode:
        parser.exit(built.returncode, f"Build failed. See {build_log}\n")
    data = (repo / "build_make" / (stem + ".uf2")).read_bytes()
    if not data or len(data) % 512:
        raise RuntimeError("Malformed UF2 block count")
    for offset in range(0, len(data), 512):
        magic0, magic1, flags, _, size, number, total, family = struct.unpack_from("<8I", data, offset)
        if (magic0, magic1, size, number, total, family) != (
            0x0A324655, 0x9E5D5157, 256, offset // 512, len(data) // 512, 0xE48BFF56
        ) or not flags & 0x2000 or struct.unpack_from("<I", data, offset + 508)[0] != 0x0AB16F30:
            raise RuntimeError(f"Malformed RP2040 UF2 block at {offset}")
    payload = b"".join(data[pos + 32:pos + 288] for pos in range(0, len(data), 512))
    if b"AAI05 MQTT FOLLOW v1" not in payload or b"/route" not in payload:
        raise RuntimeError("Built image is missing the integrated profile signature")
    with image.open("xb") as stream:
        stream.write(data)
    print(f"{image.relative_to(repo)} SHA256 {hashlib.sha256(data).hexdigest()}")


if __name__ == "__main__":
    main()
