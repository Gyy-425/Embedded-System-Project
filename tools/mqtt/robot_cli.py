"""Readable robot MQTT client using Python 3.10+ and existing Mosquitto tools.

No broker or connection starts on import. Published payloads are binary,
QoS1 and non-retained by this client. This stage has no motion commands.
"""

import argparse
from pathlib import Path
import queue
import re
import shutil
import subprocess
import sys
import threading
import time

from robot_codec import (Ack, Telemetry, OP_CLEAR, OP_STATUS, OP_TARGET,
                         UNKNOWN, decode_packet, encode_command,
                         format_packet, parse_colour)
from mqtt_publisher import MqttPublisher


def executable(name: str, directory: Path | None) -> str:
    suffix = ".exe" if sys.platform == "win32" else ""
    candidate = (directory or Path(__file__).parent /
                 ".runtime/mosquitto-2.1.2") / (name + suffix)
    if candidate.is_file():
        return str(candidate.resolve())
    found = shutil.which(name)
    if found:
        return found
    raise RuntimeError(f"Cannot find {name}; use --tools-dir for Mosquitto")


class Bridge:
    """Own the subscription and any persistent publisher for this invocation."""

    def __init__(self, args: argparse.Namespace, *, version: int = 1,
                 decoder=decode_packet, ack_type=Ack, route_decoder=None,
                 persistent_publish=False):
        self.base = f"aai05/v{version}/{args.device}"
        self.decoder = decoder
        self.ack_type = ack_type
        self.route_decoder = route_decoder
        self._latest_route = None
        self.common = ["-h", args.host, "-p", str(args.port), "-V", "mqttv311"]
        self._publisher = (MqttPublisher(args.host, args.port, self.base + "/command")
                           if persistent_publish else None)
        self.publisher = None if persistent_publish else executable("mosquitto_pub", args.tools_dir)
        self.subscriber = executable("mosquitto_sub", args.tools_dir)
        self.messages = queue.Queue(maxsize=128)
        self.process = None
        self.threads = []

    def __enter__(self):
        command = [self.subscriber, *self.common, "-q", "1", "-R",
                   "-t", self.base + "/ack", "-t", self.base + "/telemetry",
                   "-F", "%t %x"]
        if self.route_decoder is not None:
            command.extend(("-t", self.base + "/route"))
        # Keep the response subscriber alive while the parent handles Ctrl+C
        # and sends/receives its final STOP. __exit__ still owns termination.
        process_options = ({"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP}
                           if sys.platform == "win32" else {"start_new_session": True})
        self.process = subprocess.Popen(command, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, text=True,
                                        encoding="ascii", errors="replace",
                                        **process_options)
        for stream, is_error in ((self.process.stdout, False),
                                 (self.process.stderr, True)):
            thread = threading.Thread(target=self._read,
                                      args=(stream, is_error), daemon=True)
            thread.start()
            self.threads.append(thread)
        return self

    def _read(self, stream, is_error: bool):
        while True:
            line = stream.readline(512)
            if not line:
                return
            if len(line) == 512 and not line.endswith("\n"):
                while line and not line.endswith("\n"):
                    line = stream.readline(512)
                continue
            if is_error:
                clean = "".join(char if char.isprintable() else "?"
                                for char in line.strip())[:200]
                if clean:
                    print(f"broker: {clean}", file=sys.stderr)
                continue
            line = line.strip()
            if self.route_decoder is not None:
                topic, _, encoded = line.partition(" ")
                if topic == self.base + "/route":
                    try:
                        packet = self.route_decoder(bytes.fromhex(encoded))
                        self._latest_route = packet, time.monotonic()
                    except ValueError as error:
                        print(f"ignored route response: {error}", file=sys.stderr)
                    continue
            try:
                self.messages.put_nowait(line)
            except queue.Full:
                # Bounded memory; a missing ACK causes retry/timeout, not success.
                continue

    def latest_route(self):
        """Return the latest decoded route report and its monotonic receipt time."""
        return self._latest_route

    def publish(self, payload: bytes, timeout: float = 3.0):
        if self._publisher is not None:
            self._publisher.publish(payload, timeout)
            return
        command = [self.publisher, *self.common, "-q", "1", "-t",
                   self.base + "/command", "-s"]
        result = subprocess.run(command, input=payload, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=timeout)
        if result.returncode:
            detail = result.stderr.decode("ascii", errors="replace").strip()
            detail = "".join(char if char.isprintable() else "?"
                             for char in detail)[:200]
            raise RuntimeError(f"MQTT publication failed: {detail}")

    def receive(self, timeout: float) -> object | None:
        try:
            line = self.messages.get(timeout=timeout)
        except queue.Empty:
            if self.process.poll() is not None:
                raise RuntimeError("MQTT subscriber exited; check broker/network")
            return None
        try:
            topic, encoded = line.split(" ", 1)
            packet = self.decoder(bytes.fromhex(encoded))
            expected = "/ack" if type(packet) is self.ack_type else "/telemetry"
            if topic != self.base + expected:
                raise ValueError("Packet type/topic mismatch")
            return packet
        except ValueError as error:
            print(f"ignored response: {error}", file=sys.stderr)
            return None

    def __exit__(self, *_):
        if self._publisher is not None:
            self._publisher.close()
        if self.process is not None:
            if self.process.poll() is None:
                self.process.terminate()
                try:
                    self.process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=2)
            for thread in self.threads:
                thread.join(timeout=0.5)
            self.process.stdout.close()
            self.process.stderr.close()


def synchronise(bridge: Bridge, timeout: float) -> Ack:
    deadline = time.monotonic() + timeout
    next_request = 0.0
    request = encode_command(OP_STATUS)
    while time.monotonic() < deadline:
        now = time.monotonic()
        if now >= next_request:
            bridge.publish(request, max(0.1, min(3.0, deadline - now)))
            next_request = time.monotonic() + 1.0
        packet = bridge.receive(0.2)
        if (isinstance(packet, Ack) and packet.opcode == OP_STATUS and
                packet.result == 0 and packet.ready and packet.session):
            return packet
    raise RuntimeError("No device status ACK. A running broker alone is not "
                       "proof that the Pico is connected and subscribed.")


def change_target(bridge: Bridge, status: Ack, opcode: int, target: int,
                  timeout: float) -> Ack:
    sequence = (status.sequence + 1) & 0xFFFF
    request = encode_command(opcode, target, sequence, status.session)
    deadline = time.monotonic() + timeout
    next_request = 0.0
    while time.monotonic() < deadline:
        now = time.monotonic()
        if now >= next_request:
            # Retry identical bytes so a lost ACK cannot repeat a mutation.
            bridge.publish(request, max(0.1, min(3.0, deadline - now)))
            next_request = time.monotonic() + 1.0
        packet = bridge.receive(0.2)
        if not isinstance(packet, Ack) or packet.opcode != opcode:
            continue
        if packet.session != status.session:
            raise RuntimeError("Device reconnected; rerun the command to "
                               "synchronise its new connection tag")
        if packet.result not in (0, 1):
            raise RuntimeError(format_packet(packet))
        if packet.sequence == sequence and packet.target == target:
            return packet
    raise RuntimeError("No matching device ACK; target change is unconfirmed")


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, help="Laptop broker LAN IP")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--device", default="robot01")
    parser.add_argument("--tools-dir", type=Path)
    parser.add_argument("--timeout", type=float, default=8.0)
    commands = parser.add_subparsers(dest="action", required=True)
    commands.add_parser("watch")
    commands.add_parser("status")
    commands.add_parser("clear")
    commands.add_parser("target").add_argument("colour")
    args = parser.parse_args()
    if re.fullmatch(r"[A-Za-z0-9_-]{1,24}", args.device) is None:
        parser.error("--device must be 1..24 letters/digits/underscore/hyphen")
    if not 1 <= args.port <= 65535 or not 1 <= args.timeout <= 60:
        parser.error("Port must be 1..65535 and timeout 1..60 seconds")
    if args.action == "target":
        try:
            args.target = parse_colour(args.colour)
        except ValueError as error:
            parser.error(str(error))
    return args


def main() -> int:
    args = arguments()
    with Bridge(args) as bridge:
        status = synchronise(bridge, args.timeout)
        print(format_packet(status))
        if args.action == "target":
            print(format_packet(change_target(bridge, status, OP_TARGET,
                                               args.target, args.timeout)))
        elif args.action == "clear":
            print(format_packet(change_target(bridge, status, OP_CLEAR,
                                               UNKNOWN, args.timeout)))
        elif args.action == "watch":
            print("Watching raw IR; colour classification/motion unavailable.")
            last_received = time.monotonic()
            while True:
                packet = bridge.receive(0.5)
                if packet is not None:
                    print(format_packet(packet), flush=True)
                    last_received = time.monotonic()
                elif time.monotonic() - last_received > args.timeout:
                    print("No fresh device packets; waiting for reconnection.",
                          flush=True)
                    last_received = time.monotonic()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("Stopped.")
        raise SystemExit(0)
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
