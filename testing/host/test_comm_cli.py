"""Prepared communication checks; no real network or robot is used.

Set COMM_VECTOR_FILE to the C test's captured stdout for cross-language checks.
These tests are not executed by the --build-only profile.
"""

import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/mqtt"))
from robot_codec import (Ack, Telemetry, OP_CLEAR, OP_STATUS, OP_TARGET,
                         crc8, decode_packet, encode_command, format_packet,
                         parse_colour)
from robot_cli import change_target, synchronise


class FakeBridge:
    def __init__(self, responses):
        self.responses = iter(responses)
        self.sent = []

    def publish(self, payload, timeout):
        self.sent.append(payload)

    def receive(self, timeout):
        return next(self.responses)


class CodecChecks(unittest.TestCase):
    def test_crc_reference_and_command_fields(self):
        self.assertEqual(crc8(b"123456789"), 0xF4)
        frame = encode_command(OP_TARGET, 6, 0xFFFF, 0x01020304)
        self.assertEqual(len(frame), 8)
        self.assertEqual(frame[:-1], bytes.fromhex("1cffff01020304"))
        self.assertEqual(crc8(frame[:-1]), frame[-1])

    def test_reject_out_of_range_commands(self):
        invalid = ((OP_TARGET, 7, 1, 1), (OP_CLEAR, 0, 1, 1),
                   (3, 7, 1, 1), (OP_TARGET, 2, 65536, 1),
                   (OP_TARGET, 2, 1, 0), (OP_TARGET, 2, 1, 2**32))
        for values in invalid:
            with self.subTest(values=values), self.assertRaises(ValueError):
                encode_command(*values)
        with self.assertRaises(ValueError):
            parse_colour("red")  # Actual five tape names are not assigned yet.
        self.assertEqual(parse_colour("colour5"), 6)

    def test_reject_bad_response(self):
        body = bytes.fromhex("4e98000001020304")
        ack = body + bytes([crc8(body)])
        self.assertIsInstance(decode_packet(ack), Ack)
        for frame in (b"", ack[:-1], ack[:-1] + bytes([ack[-1] ^ 1])):
            with self.subTest(frame=frame), self.assertRaises(ValueError):
                decode_packet(frame)

    def test_readable_output_matches_capabilities(self):
        ack = Ack(OP_STATUS, 7, 0, 3, True, 0, 1)
        self.assertIn("target=unset", format_packet(ack))
        self.assertIn("motion-capability=none", format_packet(ack))
        enabled = Ack(OP_STATUS, 7, 0, 7, True, 0, 1)
        self.assertIn("motion-capability=present", format_packet(enabled))

    @unittest.skipUnless(os.environ.get("COMM_VECTOR_FILE"),
                         "Set COMM_VECTOR_FILE to captured C output")
    def test_c_encoder_python_decoder(self):
        lines = Path(os.environ["COMM_VECTOR_FILE"]).read_text().splitlines()
        packets = {line.split()[0]: decode_packet(bytes.fromhex(line.split()[1]))
                   for line in lines if line.startswith(("ACK ", "TELEMETRY "))}
        self.assertEqual(packets["ACK"], Ack(OP_STATUS, 7, 0, 3, True,
                                              0, 0x01020304))
        self.assertEqual(packets["TELEMETRY"],
                         Telemetry(2, 7, 3, True, False, False, True,
                                   65535, 0x01020304, 0xFFFFFFFF, 0xABC, 0x123))


class RequestChecks(unittest.TestCase):
    def test_status_waits_for_ready_ack(self):
        ready = Ack(OP_STATUS, 7, 0, 3, True, 65535, 123)
        bridge = FakeBridge([None, ready])
        self.assertEqual(synchronise(bridge, 1), ready)
        self.assertEqual(bridge.sent[0], encode_command(OP_STATUS))

    def test_wrap_and_duplicate_ack(self):
        status = Ack(OP_STATUS, 7, 0, 3, True, 65535, 123)
        duplicate = Ack(OP_TARGET, 2, 1, 3, True, 0, 123)
        bridge = FakeBridge([duplicate])
        self.assertEqual(change_target(bridge, status, OP_TARGET, 2, 1),
                         duplicate)
        self.assertEqual(bridge.sent, [encode_command(OP_TARGET, 2, 0, 123)])

    def test_reconnected_or_rejected_ack_is_not_success(self):
        status = Ack(OP_STATUS, 7, 0, 3, True, 10, 123)
        for packet in (Ack(OP_TARGET, 2, 0, 3, True, 11, 456),
                       Ack(OP_TARGET, 7, 5, 3, True, 10, 123)):
            with self.subTest(packet=packet), self.assertRaises(RuntimeError):
                change_target(FakeBridge([packet]), status, OP_TARGET, 2, 1)


if __name__ == "__main__":
    unittest.main()
