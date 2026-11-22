"""Bounded MQTT Motion CLI. Import does not connect or actuate."""
import argparse
from collections import deque
import math
from pathlib import Path
import re
import secrets
import subprocess
import sys
import threading
import time
from drive_codec import (DriveAck, DriveTelemetry, OP_STATUS, OP_TARGET,
                         OP_CLEAR, OP_START, OP_KEEPALIVE, OP_STOP, OP_FOLLOW,
                         OP_STRAIGHT, UNKNOWN,
                         decode_drive_packet, encode_drive_command,
                         format_drive_packet)
from robot_cli import Bridge
from robot_codec import parse_colour
from chassis import chassis_duties
from route_codec import (decode_route_packet, format_route_report,
                         format_route_status)

MENU_START_OPCODES = {"f": OP_FOLLOW, "l": OP_STRAIGHT}


def synchronise(bridge, timeout=4.0, *, clock=time.monotonic, check=None):
    identifier = secrets.randbits(16)
    frame = encode_drive_command(OP_STATUS, sequence=identifier)
    deadline = clock() + timeout
    next_send = clock()
    while clock() < deadline:
        if check is not None:
            check()
        if clock() >= next_send:
            bridge.publish(frame, max(0.001, min(0.2 if check else 0.5,
                                                 deadline - clock())))
            next_send = clock() + 0.5
        packet = bridge.receive(max(0.001, min(0.1, deadline - clock())))
        if (type(packet) is DriveAck and packet.opcode == OP_STATUS and
                packet.sequence == identifier and packet.result == 0 and
                packet.session and packet.grant):
            return packet
    raise RuntimeError("No matching v2 device status; broker startup alone "
                       "does not prove the robot is connected")


def request(bridge, status, opcode, *, target=UNKNOWN, left=0, right=0,
            timeout=0.6, clock=time.monotonic, check=None):
    if opcode != OP_STOP and not status.ready:
        raise RuntimeError("Device Motion status is unavailable/not ready")
    sequence = (status.mutation_sequence + 1) & 0xFFFF
    frame = encode_drive_command(opcode, target, sequence, status.session,
                                 0 if opcode == OP_STOP else status.grant,
                                 left, right)
    grant_refreshed = False
    deadline = clock() + timeout
    next_send = clock()
    while clock() < deadline:
        if check is not None:
            check()
        if clock() >= next_send:
            bridge.publish(frame, max(0.001, min(0.2 if check else 0.5,
                                                 deadline - clock())))
            next_send = clock() + 0.2
        packet = bridge.receive(max(0.001, min(0.1, deadline - clock())))
        if packet is None:
            continue
        if packet.session != status.session:
            raise RuntimeError("Device session changed; no automatic restart")
        if (type(packet) is not DriveAck or packet.opcode != opcode or
                packet.sequence != sequence):
            continue
        # STATUS can report a grant just before its expiry. Only a matching
        # explicit rejection proves this idle request was not accepted. Use
        # its new grant once, without changing intent/sequence or the deadline.
        if (packet.result == 8 and not grant_refreshed and
                opcode in (OP_START, OP_FOLLOW, OP_STRAIGHT, OP_TARGET, OP_CLEAR) and
                packet.ready and not packet.active and not status.active and
                packet.remote_phase in (0, 3) and
                packet.remote_phase == status.remote_phase and
                packet.motion_phase in (0, 2) and
                packet.motion_phase == status.motion_phase and
                packet.reason == status.reason and
                packet.mutation_sequence == status.mutation_sequence and
                packet.target == status.target and
                packet.left_pct == status.left_pct and
                packet.right_pct == status.right_pct and
                packet.grant not in (0, status.grant)):
            frame = encode_drive_command(opcode, target, sequence,
                                         status.session, packet.grant,
                                         left, right)
            grant_refreshed = True
            next_send = clock()
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
                   *, clock=time.monotonic, check=None):
    deadline = clock() + timeout
    while clock() < deadline:
        packet = synchronise(bridge, min(0.5, deadline - clock()), clock=clock,
                             check=check)
        if packet.session != status.session:
            raise RuntimeError("Device session changed during confirmation")
        delta = (packet.mutation_sequence - status.mutation_sequence) & 0xFFFF
        if delta >= 0x8000:
            continue
        if is_active and (packet.remote_phase >= 3 or packet.motion_phase == 3):
            raise RuntimeError("Start did not become active: " + format_drive_packet(packet))
        if motion_confirmed(packet, is_active):
            return packet
    raise RuntimeError("Fresh device status did not confirm " +
                       ("active output" if is_active else "output off"))


def motion_confirmed(packet, is_active):
    return (packet.ready and packet.active == is_active and
            packet.motion_phase == (1 if is_active else 2) and
            packet.remote_phase == (2 if is_active else 3))


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


class StopPending(Exception):
    """The terminal requested STOP while a motion command was pending."""


class MenuCommands:
    def __init__(self):
        self._commands = deque()
        self._lock = threading.Lock()

    def add(self, command):
        with self._lock:
            if command in ("s", "q"):
                # Discard queued starts typed before this STOP.
                self._commands.clear()
            self._commands.append(command)

    def take(self):
        with self._lock:
            return self._commands.popleft() if self._commands else None

    def discard_starts(self):
        with self._lock:
            self._commands = deque(command for command in self._commands
                                   if command not in MENU_START_OPCODES)

    def check_stop(self):
        with self._lock:
            if self._commands and self._commands[0] in ("s", "q"):
                raise StopPending

    def read_stdin(self):
        try:
            # The daemon may be waiting for a line when the main thread handles
            # Ctrl+C. Use unbuffered input so it holds no Python stdio lock.
            with open(sys.stdin.fileno(), "rb", buffering=0, closefd=False) as stream:
                for line in stream:
                    command = line.decode("ascii", errors="replace").strip().lower()
                    if command in ("q", "\x1a"):
                        return
                    if command:
                        self.add(command)
        finally:
            self.add("q")  # Includes EOF.


class FollowMenu:
    _transport_errors = (OSError, RuntimeError, subprocess.TimeoutExpired)

    def __init__(self, bridge, timeout, commands, *, clock=time.monotonic,
                 output=print):
        self.bridge = bridge
        self.timeout = timeout
        self.commands = commands
        self.clock = clock
        self.output = output
        self.status = None
        self.seen = float("-inf")
        self.renew_at = None
        self.next_update = 0.0
        self.last_route_state = None

    def _remember(self, status):
        self.status = status
        self.seen = self.clock()

    def current_route(self, *, fresh=True):
        latest = self.bridge.latest_route()
        if latest is None or self.status is None:
            return None
        if self.clock() - self.seen > self.timeout:
            return None
        route, received = latest
        if route.session == self.status.session and (
                not fresh or self.clock() - received <= 1.0):
            return route
        return None

    def show_status(self):
        if self.status is None or self.clock() - self.seen > self.timeout:
            self.output("Robot disconnected or awaiting device status.")
            return
        route = self.current_route()
        detail = format_route_status(route) if route else "Awaiting fresh colour and distance."
        self.output(f"Robot connected | DRIVE {'ON' if self.status.active else 'OFF'} | {detail}")

    def show_report(self):
        route = self.current_route(fresh=False)
        self.output(format_route_report(route) if route else
                    "No route report for the current device session.")

    def start(self, opcode=OP_FOLLOW):
        mode = "Straight track" if opcode == OP_STRAIGHT else "Follow"
        if self.renew_at is not None:
            self.output("A run is already active.")
            return
        attempted = False
        try:
            check = self.commands.check_stop
            self._remember(synchronise(self.bridge, self.timeout, clock=self.clock,
                                       check=check))
            if (not self.status.ready or self.status.active or
                    self.status.remote_phase not in (0, 3)):
                raise RuntimeError("Device must be ready and stopped before starting")
            # A legacy device reports valid v2 Motion status as well. Require
            # fresh integrated telemetry from the same session before arming.
            deadline = self.clock() + min(self.timeout, 0.7)
            while self.current_route() is None and self.clock() < deadline:
                check()
                packet = self.bridge.receive(min(0.1, deadline - self.clock()))
                if isinstance(packet, DriveTelemetry) and packet.session != self.status.session:
                    self._remember(packet)
                    raise RuntimeError("Device session changed. Use f or l to start a new run.")
            if self.current_route() is None:
                raise RuntimeError("No fresh integrated route telemetry for this session")
            check()
            attempted = True
            self._remember(request(self.bridge, self.status, opcode,
                                   clock=self.clock, check=check))
            running = confirm_motion(self.bridge, self.status, True,
                                     min(self.timeout, 0.5), clock=self.clock, check=check)
            self._remember(running)
            self.renew_at = self.clock() + renewal_delay(running)
            self.next_update = self.clock()
            self.output(f"{mode} started (DRIVE ON confirmed by fresh device status).")
        except StopPending:
            return
        except self._transport_errors as error:
            self.commands.discard_starts()
            self.output(f"{mode} start unconfirmed: {error}")
            if attempted:
                self.stop()

    def stop(self):
        self.renew_at = None
        deadline = self.clock() + self.timeout
        try:
            if self.status is None:
                self._remember(synchronise(self.bridge, self.timeout, clock=self.clock))
            initial_session = self.status.session
            confirmed = None
            try:
                stopped = request(self.bridge, self.status, OP_STOP,
                                  timeout=min(0.6, deadline - self.clock()),
                                  clock=self.clock)
            except self._transport_errors:
                # An ACK can be lost after STOP applies. A fresh status read
                # can prove output is off on the same session.
                self._remember(synchronise(self.bridge, deadline - self.clock(),
                                           clock=self.clock))
                if self.status.session == initial_session and motion_confirmed(self.status, False):
                    confirmed = self.status
                else:
                    stopped = request(self.bridge, self.status, OP_STOP,
                                      timeout=min(0.6, deadline - self.clock()),
                                      clock=self.clock)
            if confirmed is None:
                confirmed = confirm_motion(self.bridge, stopped, False,
                                           deadline - self.clock(), clock=self.clock)
            self._remember(confirmed)
            self.output("DRIVE OFF confirmed by fresh device status.")
            return True
        except self._transport_errors as error:
            self.output(f"DRIVE OFF unconfirmed: {error}")
            return False

    def step(self):
        wait = 0.1
        if self.renew_at is not None:
            wait = max(0.001, min(wait, self.renew_at - self.clock()))
        packet = self.bridge.receive(wait)
        if isinstance(packet, DriveTelemetry):
            if self.status is not None and packet.session == self.status.session:
                delta = (packet.mutation_sequence - self.status.mutation_sequence) & 0xFFFF
                if delta >= 0x8000:
                    packet = None
            if packet is not None:
                if self.renew_at is not None:
                    if packet.session != self.status.session:
                        self.commands.discard_starts()
                        self.output("Device session changed. Use f or l to start a new run.")
                        self.renew_at = None
                    elif packet.remote_phase >= 3 or packet.motion_phase == 3:
                        self.renew_at = None
                    else:
                        self.renew_at = min(self.renew_at,
                                            self.clock() + renewal_delay(packet))
                self._remember(packet)
        if self.renew_at is not None and self.clock() >= self.renew_at:
            try:
                check = self.commands.check_stop
                if self.current_route() is None:
                    raise RuntimeError("No fresh route telemetry")
                self._remember(request(self.bridge, self.status, OP_KEEPALIVE,
                                       timeout=0.5, clock=self.clock, check=check))
                if not motion_confirmed(self.status, True):
                    raise RuntimeError(format_drive_packet(self.status))
                self.renew_at = self.clock() + renewal_delay(self.status)
            except StopPending:
                return
            except self._transport_errors as error:
                self.commands.discard_starts()
                self.output(f"Run renewal ended: {error}")
                self.stop()
        route = self.current_route()
        if route is not None:
            state = (route.session, route.epoch, route.phase, route.reason)
            if state != self.last_route_state or (
                    route.phase == 1 and self.clock() >= self.next_update):
                self.output(format_route_status(route))
                self.last_route_state = state
                self.next_update = self.clock() + 1.0


def run_follow_menu(bridge, timeout, *, commands=None, clock=time.monotonic,
                    output=print):
    if commands is None:
        commands = MenuCommands()
        threading.Thread(target=commands.read_stdin, daemon=True).start()
    menu = FollowMenu(bridge, timeout, commands, clock=clock, output=output)
    actions = {"s": menu.stop, "?": menu.show_status, "r": menu.show_report}
    help_line = ("Press a key then Enter   f Follow   l Straight track   "
                 "s Stop   ? Status   r Run report   q Stop and quit")
    try:
        output(help_line)
        while True:
            command = commands.take()
            if command == "q":
                break
            if command in MENU_START_OPCODES:
                menu.start(MENU_START_OPCODES[command])
            elif command in actions:
                actions[command]()
            elif command is not None:
                output(help_line)
            else:
                menu.step()
    except KeyboardInterrupt:
        output("Interrupted. Sending STOP.")
    finally:
        stopped = menu.stop()
    return 0 if stopped else 1


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
    actions.add_parser("menu", help="Interactive follow and straight-track measurement")
    actions.add_parser("target").add_argument("colour")
    run = actions.add_parser("drive", help="Bounded electrical duties; no calibrated speed")
    run.add_argument("--left", type=int, default=50)
    run.add_argument("--right", type=int, default=0)
    run.add_argument("--seconds", type=float, default=2.0)
    move = actions.add_parser("move", help="Forward duty and steering; positive turn is right")
    move.add_argument("--forward", type=int, default=30, help="Forward duty1..50; not calibrated speed")
    move.add_argument("--turn", type=int, default=0, help="Steering duty-50..50; positive is right")
    move.add_argument("--seconds", type=float, default=2.0)
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
    if args.action in ("drive", "move"):
        if not math.isfinite(args.seconds) or not 0 < args.seconds <= 10:
            parser.error("--seconds must be greater than0 and at most10")
    if args.action == "move":
        if args.forward <= 0:
            parser.error("--forward must be1..50; use stop for latched STOP")
        try:
            args.left, args.right = chassis_duties(args.forward, args.turn)
        except ValueError as error:
            parser.error(str(error))
    if args.action == "drive":
        if not -50 <= args.left <= 50 or not -50 <= args.right <= 50 or not (args.left or args.right):
            parser.error("Duties must be -50..50 with at least one nonzero wheel")
    return args


def main():
    args = arguments()
    with Bridge(args, version=2, decoder=decode_drive_packet,
                ack_type=DriveAck,
                route_decoder=decode_route_packet if args.action == "menu" else None) as bridge:
        if args.action == "menu":
            return run_follow_menu(bridge, args.timeout,
                                   output=lambda message: print(message, flush=True))
        status = synchronise(bridge, args.timeout)
        print(format_drive_packet(status), flush=True)
        if args.action in ("drive", "move"):
            if args.action == "move":
                print(f"MOVE forward={args.forward}% turn={args.turn:+}% "
                      f"-> raw L={args.left:+}% R={args.right:+}% "
                      "(positive turn=right)", flush=True)
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
