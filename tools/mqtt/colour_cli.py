"""Read-only colour stream; never issues robot commands."""
import argparse
import math
from pathlib import Path
import re
import sys
import time

from colour_codec import ColourStream, decode_colour_packet, format_colour_packet
from robot_cli import Bridge


class ColourBridge(Bridge):
    """Reuse bounded subscriber/cleanup, with command publication disabled."""

    def __init__(self, args):
        super().__init__(args, decoder=decode_colour_packet, ack_type=type(None))
        self.base = f"aai05/colour/v1/{args.device}"

    def publish(self, *_args, **_kwargs):
        raise RuntimeError("Colour watch is read-only")


def watch(bridge, timeout: float, *, clock=time.monotonic):
    stream = ColourStream()
    last_arrival = clock()
    while True:
        packet = bridge.receive(0.5)
        if packet is not None:
            for note in stream.observe(packet):
                print(note, flush=True)
            print(format_colour_packet(packet), flush=True)
            last_arrival = clock()
        elif clock() - last_arrival >= timeout:
            print("No fresh colour reports; waiting for connection. Cached output is not live.", flush=True)
            last_arrival = clock()


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, help="Laptop broker LAN IP")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--device", default="robot01")
    parser.add_argument("--tools-dir", type=Path)
    parser.add_argument("--timeout", type=float, default=4.0)
    parser.add_argument("action", choices=("watch",))
    args = parser.parse_args(argv)
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,24}", args.device):
        parser.error("Device must be1..24 letters/digits/underscore/hyphen")
    if not 1 <= args.port <= 65535 or not math.isfinite(args.timeout) or not 1 <= args.timeout <= 30:
        parser.error("Port must be1..65535; timeout1..30 seconds")
    return args


def main():
    args = arguments()
    with ColourBridge(args) as bridge:
        watch(bridge, args.timeout)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("Stopped colour watch; no robot commands sent.")
    except (OSError, RuntimeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
