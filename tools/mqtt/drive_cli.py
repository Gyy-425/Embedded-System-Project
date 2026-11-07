"""Bounded MQTT Motion CLI. Import does not connect or actuate."""
import argparse
import math
from pathlib import Path
import re
import secrets
import subprocess
import sys
import time
from drive_codec import (DriveAck, DriveTelemetry, OP_STATUS, OP_TARGET,
                         OP_CLEAR, OP_START, OP_KEEPALIVE, OP_STOP, UNKNOWN,
                         decode_drive_packet, encode_drive_command,
                         format_drive_packet)
from robot_cli import Bridge
from robot_codec import parse_colour


def synchronise(bridge, timeout=4.0, *, clock=time.monotonic):
    identifier = secrets.randbits(16)
    frame = encode_drive_command(OP_STATUS, sequence=identifier)
    deadline = clock() + timeout
    next_send = clock()
    while clock() < deadline:
        if clock() >= next_send:
            bridge.publish(frame, max(0.001, min(0.5, deadline - clock())))
            next_send = clock() + 0.5
        packet = bridge.receive(max(0.001, min(0.1, deadline - clock())))
        if (type(packet) is DriveAck and packet.opcode == OP_STATUS and
                packet.sequence == identifier and packet.result == 0 and
                packet.session and packet.grant):
            return packet
    raise RuntimeError("No matching v2 device status; broker startup alone "
                       "does not prove the robot is connected")


def request(bridge, status, opcode, *, target=UNKNOWN, left=0, right=0,
            timeout=0.6, clock=time.monotonic):
    if opcode != OP_STOP and not status.ready:
        raise RuntimeError("Device Motion status is unavailable/not ready")
    sequence = (status.mutation_sequence + 1) & 0xFFFF
    frame = encode_drive_command(opcode, target, sequence, status.session,
                                 0 if opcode == OP_STOP else status.grant,
                                 left, right)
    deadline = clock() + timeout
    next_send = clock()
    while clock() < deadline:
        if clock() >= next_send:
            bridge.publish(frame, max(0.001, min(0.5, deadline - clock())))
            next_send = clock() + 0.2
        packet = bridge.receive(max(0.001, min(0.1, deadline - clock())))
        if packet is None:
            continue
        if packet.session != status.session:
            raise RuntimeError("Device session changed; no automatic restart")
        if (type(packet) is not DriveAck or packet.opcode != opcode or
                packet.sequence != sequence):
            continue
        if packet.result not in (0, 1):
            raise RuntimeError(format_drive_packet(packet))
        if opcode != OP_STOP and packet.mutation_sequence != sequence:
            continue
        if opcode in (OP_TARGET, OP_CLEAR) and packet.target != target:
            raise RuntimeError("Device acknowledgement target mismatch")
        return packet
    raise RuntimeError("No matching device ACK; command outcome unconfirmed")


def confirm_motion(bridge, status, is_active, timeout=4.0,
                   *, clock=time.monotonic):
    deadline = clock() + timeout
    while clock() < deadline:
        packet = synchronise(bridge, min(0.5, deadline - clock()), clock=clock)
        if packet.session != status.session:
            raise RuntimeError("Device session changed during confirmation")
        delta = (packet.mutation_sequence - status.mutation_sequence) & 0xFFFF
        if delta >= 0x8000:
            continue
        if is_active and (packet.remote_phase >= 3 or packet.motion_phase == 3):
            raise RuntimeError("Start did not become active: " + format_drive_packet(packet))
        if (packet.ready and packet.active == is_active and
                packet.motion_phase == (1 if is_active else 2) and
                packet.remote_phase == (2 if is_active else 3)):
            return packet
    raise RuntimeError("Fresh device status did not confirm " +
                       ("active output" if is_active else "output off"))


def renewal_delay(status):
    # Reserve250ms for the next publication/ACK when feedback is already old.
    return max(0.0, min(0.2, status.lease_ms / 1000.0 - 0.25))


def run_drive(bridge, status, left, right, seconds, timeout=4.0,
              *, clock=time.monotonic):
    if (not isinstance(seconds, (int, float)) or not math.isfinite(seconds) or
            not 0 < seconds <= 10):
        raise ValueError("Drive duration must be greater than0 and at most10 seconds")
    # Validate before any publication or cleanup side effect.
    encode_drive_command(OP_START, sequence=1, session=status.session,
                         grant=status.grant, left=left, right=right)
    if not status.ready or status.remote_phase not in (0, 3) or status.active:
        raise RuntimeError("Device must be ready and stopped before a fresh START")
    failure = None
    current = status
    deadline = clock() + seconds
    try:
        current = request(bridge, current, OP_START, left=left, right=right,
                          timeout=min(timeout, 0.6, deadline - clock()), clock=clock)
        current = confirm_motion(bridge, current, True,
                                 min(timeout, 0.6, deadline - clock()), clock=clock)
        next_renewal = clock() + renewal_delay(current)
        while clock() < deadline:
            wait = max(0.001, min(0.1, deadline - clock(), next_renewal - clock()))
            packet = bridge.receive(wait)
            if packet is not None:
                if packet.session != current.session:
                    raise RuntimeError("Device session changed; no automatic restart")
                delta = (packet.mutation_sequence - current.mutation_sequence) & 0xFFFF
                if delta < 0x8000:
                    if packet.remote_phase >= 3 or packet.motion_phase == 3:
                        raise RuntimeError("Device stopped: " + format_drive_packet(packet))
                    current = packet
                    next_renewal = min(next_renewal, clock() + renewal_delay(current))
            if clock() >= next_renewal and clock() < deadline:
                current = request(bridge, current, OP_KEEPALIVE,
                                  timeout=min(timeout, 0.5, deadline - clock()), clock=clock)
                if (not current.ready or not current.active or
                        current.remote_phase != 2 or current.motion_phase != 1):
                    raise RuntimeError("Motion renewal did not confirm running output: " +
                                       format_drive_packet(current))
                next_renewal = clock() + renewal_delay(current)
    except BaseException as error:
        failure = error
    # A lost START ACK may still have started output. Always request STOP.
    try:
        current = request(bridge, current, OP_STOP,
                          timeout=min(timeout, 0.6), clock=clock)
        stopped = confirm_motion(bridge, current, False, timeout, clock=clock)
    except Exception as stop_error:
        if failure is not None:
            message = str(failure) or type(failure).__name__
            raise RuntimeError(f"{message}; STOP not confirmed: {stop_error}") from failure
        raise
    if failure is not None:
        raise failure
    return stopped


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, help="Laptop broker LAN IP")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--device", default="robot01")
    parser.add_argument("--tools-dir", type=Path)
    parser.add_argument("--timeout", type=float, default=4.0)
    actions = parser.add_subparsers(dest="action", required=True)
    for name in ("status", "watch", "clear", "stop"):
        actions.add_parser(name)
    actions.add_parser("target").add_argument("colour")
    run = actions.add_parser("drive", help="Bounded electrical duties; no calibrated speed")
    run.add_argument("--left", type=int, default=50)
    run.add_argument("--right", type=int, default=0)
    run.add_argument("--seconds", type=float, default=2.0)
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,24}", args.device):
        parser.error("--device must be1..24 letters/digits/underscore/hyphen")
    if not 1 <= args.port <= 65535 or not math.isfinite(args.timeout) or not 1 <= args.timeout <= 30:
        parser.error("Port must be1..65535 and timeout1..30 seconds")
    if args.action == "target":
        try:
            args.target = parse_colour(args.colour)
        except ValueError as error:
            parser.error(str(error))
    if args.action == "drive":
        if not math.isfinite(args.seconds) or not 0 < args.seconds <= 10:
            parser.error("--seconds must be greater than0 and at most10")
        if not -50 <= args.left <= 50 or not -50 <= args.right <= 50 or not (args.left or args.right):
            parser.error("Duties must be -50..50 with at least one nonzero wheel")
    return args


def main():
    args = arguments()
    with Bridge(args, version=2, decoder=decode_drive_packet,
                ack_type=DriveAck) as bridge:
        status = synchronise(bridge, args.timeout)
        print(format_drive_packet(status), flush=True)
        if args.action == "drive":
            print("Bounded electrical-duty request; no automatic restart.", flush=True)
            stopped = run_drive(bridge, status, args.left, args.right,
                                args.seconds, args.timeout)
            print(format_drive_packet(stopped), flush=True)
            print("Motor output OFF confirmed by fresh device status.")
        elif args.action == "stop":
            stopped = request(bridge, status, OP_STOP)
            print(format_drive_packet(confirm_motion(bridge, stopped, False,
                                                      args.timeout)))
        elif args.action in ("target", "clear"):
            opcode = OP_TARGET if args.action == "target" else OP_CLEAR
            target = args.target if args.action == "target" else UNKNOWN
            print(format_drive_packet(request(bridge, status, opcode,
                                               target=target)))
        elif args.action == "watch":
            last_packet = time.monotonic()
            while True:
                packet = bridge.receive(0.5)
                if packet is not None:
                    print(format_drive_packet(packet), flush=True)
                    last_packet = time.monotonic()
                elif time.monotonic() - last_packet > args.timeout:
                    print("No fresh device status; waiting for reconnection.", flush=True)
                    last_packet = time.monotonic()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("Interrupted.")
        raise SystemExit(0)
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
