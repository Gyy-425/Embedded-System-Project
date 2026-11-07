import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/mqtt"))
from drive_codec import (DriveAck, DriveTelemetry, OP_START, OP_STOP,
                         decode_drive_packet, encode_drive_command,
                         format_drive_packet)
from robot_codec import crc8

ACK = bytes.fromhex("a6070a1234ffff0102030411223344ffffffffeca74b08")
TELEMETRY = bytes.fromhex("c0070a1234ffff0102030411223344ffffffffeca74babc123e8")


class DriveCodecChecks(unittest.TestCase):
    def test_compact_signed_start_and_stop(self):
        frame = encode_drive_command(OP_START, sequence=65535,
                                     session=0x01020304, grant=0x11223344,
                                     left=50, right=-50)
        self.assertEqual(len(frame), 14)
        self.assertEqual(frame[:-1].hex(), "27d94effff0102030411223344")
        self.assertEqual(frame[-1], crc8(frame[:-1]))
        stop = encode_drive_command(OP_STOP, sequence=1, session=1)
        self.assertEqual(len(stop), 14)

    def test_literal_status_vectors_and_readable_limits(self):
        ack = decode_drive_packet(ACK)
        self.assertIs(type(ack), DriveAck)
        self.assertEqual((ack.opcode, ack.result, ack.sequence), (3, 0, 0x1234))
        self.assertEqual((ack.session, ack.grant), (0x01020304, 0x11223344))
        self.assertEqual((ack.left_pct, ack.right_pct, ack.lease_ms), (50, -50, 750))
        self.assertEqual((ack.remote_phase, ack.motion_phase), (2, 1))
        self.assertTrue(ack.ready and ack.active and ack.ir_valid)
        sample = decode_drive_packet(TELEMETRY)
        self.assertIsInstance(sample, DriveTelemetry)
        self.assertEqual((sample.left_raw, sample.right_raw), (0xABC, 0x123))
        self.assertIn("detected=unknown", format_drive_packet(sample))
        self.assertIn("DRIVE=ON", format_drive_packet(sample))

    def test_invalid_packets_and_requests(self):
        for packet in (b"", ACK[:-1], ACK[:-1] + bytes([ACK[-1] ^ 1])):
            with self.subTest(packet=packet), self.assertRaises(ValueError):
                decode_drive_packet(packet)
        for left, right in ((-51, 0), (0, 51), (0, 0), (1.5, 0)):
            with self.subTest(left=left, right=right), self.assertRaises(ValueError):
                encode_drive_command(OP_START, session=1, grant=1,
                                     left=left, right=right)
        with self.assertRaises(ValueError):
            encode_drive_command(OP_STOP, session=1, grant=1)
        bad = bytearray(ACK[:-1])
        bad[1] |= 0x0C  # Contradictory IR valid and stale flags.
        with self.assertRaises(ValueError):
            decode_drive_packet(bytes(bad) + bytes([crc8(bad)]))

    @unittest.skipUnless(os.environ.get("DRIVE_VECTOR_FILE"),
                         "Set DRIVE_VECTOR_FILE to captured C vectors")
    def test_actual_c_encoder_python_decoder(self):
        lines = Path(os.environ["DRIVE_VECTOR_FILE"]).read_text().splitlines()
        vectors = {line.split()[0]: bytes.fromhex(line.split()[1])
                   for line in lines if line.startswith("DRIVE_" )}
        self.assertEqual(vectors["DRIVE_ACK"], ACK)
        self.assertEqual(vectors["DRIVE_TELEMETRY"], TELEMETRY)
        self.assertEqual(decode_drive_packet(vectors["DRIVE_ACK"]).right_pct, -50)


if __name__ == "__main__":
    unittest.main()
