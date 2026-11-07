"""Capture stationary tape readings or report saved ranges; no dependencies added."""
import argparse
import math
from pathlib import Path
import re
import subprocess
import sys

from drive_cli import synchronise
from drive_codec import DriveAck, decode_drive_packet
from robot_cli import Bridge
from robot_codec import parse_colour
from tape_calibration import capture_samples, report_captures, write_report


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_subparsers(dest="action", required=True)
    capture = actions.add_parser("capture", help="Both sensors over the same tape; no drive client")
    capture.add_argument("--host", required=True)
    capture.add_argument("--port", type=int, default=1883)
    capture.add_argument("--device", default="robot01")
    capture.add_argument("--tools-dir", type=Path)
    capture.add_argument("--label", type=parse_colour, required=True)
    capture.add_argument("--name", required=True)
    capture.add_argument("--setup", required=True, help="Fixed bracket/height, lighting and background")
    capture.add_argument("--height-mm", type=float, help="Actual measured height, if available")
    capture.add_argument("--out", type=Path, required=True, help="New capture directory")
    capture.add_argument("--samples", type=int, default=40)
    capture.add_argument("--timeout", type=float, default=30.0)
    report = actions.add_parser("report", help="Offline; reads immediate capture subdirectories")
    report.add_argument("--root", type=Path, required=True)
    report.add_argument("--out", type=Path, required=True, help="New JSON report file")
    args = parser.parse_args()
    if args.out.exists():
        parser.error("Output already exists; use a new capture/report name")
    if args.action == "capture":
        if not re.fullmatch(r"[A-Za-z0-9_-]{1,24}", args.device) or not 1 <= args.port <= 65535:
            parser.error("Invalid device ID or port")
        if not 10 <= args.samples <= 1000 or not math.isfinite(args.timeout) or not 1 <= args.timeout <= 300:
            parser.error("Use10..1000 samples and a finite timeout1..300 seconds")
        if not args.name.strip() or not args.setup.strip():
            parser.error("Tape name and physical setup must not be empty")
        if args.height_mm is not None and (not math.isfinite(args.height_mm) or args.height_mm <= 0):
            parser.error("Measured height must be positive and finite")
    return args


def main():
    args = arguments()
    if args.action == "report":
        report = report_captures(args.root)
        write_report(report, args.out)
        print(f"{report['status']}: classes={len(report['classes'])}/7; "
              f"excluded incomplete captures={len(report['excluded'])}")
        for label, group in report["classes"].items():
            print(f"{label} {group['name']}: "
                  f"L {group['left']['min']}..{group['left']['max']} "
                  f"R {group['right']['min']}..{group['right']['max']}")
        for side, pairs in report["overlaps"].items():
            for pair in pairs:
                print(f"OVERLAP {side}: classes {pair['classes']}, ADC {pair['range']}")
        print(report["note"])
        print(f"Saved {args.out}")
        return 0
    print("Capture only: keep the car stationary, both sensors over one tape, "
          "and no drive client running.", flush=True)
    try:
        with Bridge(args, version=2, decoder=decode_drive_packet, ack_type=DriveAck) as bridge:
            status = synchronise(bridge, 4.0)
            result = capture_samples(bridge, status, args.out, label=args.label,
                                     name=args.name, setup=args.setup, device=args.device,
                                     height_mm=args.height_mm, sample_count=args.samples,
                                     timeout=args.timeout)
        print(f"COMPLETE: {result['accepted']} valid samples; saved {args.out}")
        return 0
    finally:
        if (args.out / "metadata.json").exists():
            print(f"Capture metadata/partial data: {args.out}", flush=True)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("Capture interrupted; existing partial data is preserved.", file=sys.stderr)
        raise SystemExit(130)
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
