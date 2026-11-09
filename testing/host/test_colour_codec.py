"""Reject corrupt/contradictory colour frames and expose missed transitions."""
from contextlib import redirect_stdout
from dataclasses import replace
import io
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/mqtt"))
from robot_codec import crc8
from colour_codec import decode_colour_packet, format_colour_packet, ColourStream
from colour_cli import watch

GOLDEN = bytes.fromhex("d00102030405060708090a0b0c00000002abc123020f5c")


class ColourCodecTests(unittest.TestCase):
    def test_literal_wire_fields_and_readable_results(self):
        packet = decode_colour_packet(GOLDEN)
        self.assertFalse(packet.is_snapshot)
        self.assertEqual((packet.session, packet.sequence, packet.sampled_ms),
                         (0x01020304, 0x05060708, 0x090A0B0C))
        self.assertEqual((packet.dropped, packet.left_raw, packet.right_raw), (2, 2748, 291))
        self.assertEqual((packet.left_result, packet.right_result), (2, 15))
        rendered = format_colour_packet(packet)
        self.assertIn("colour1/MATCH", rendered)
        self.assertIn("unknown/UNCALIBRATED", rendered)
        self.assertIn("dropped=2", rendered)

    def test_corrupt_frames_and_impossible_result_pairs(self):
        for payload in (b"", GOLDEN[:-1], GOLDEN + b"\0", None,
                        GOLDEN[:-1] + bytes([GOLDEN[-1] ^ 1])):
            with self.subTest(payload=payload), self.assertRaises(ValueError):
                decode_colour_packet(payload)
        for position, value in ((0, 0xD2), (20, 7), (20, 8), (20, 16), (21, 64)):
            data = bytearray(GOLDEN[:-1])
            data[position] = value
            with self.subTest(position=position, value=value), self.assertRaises(ValueError):
                decode_colour_packet(bytes(data) + bytes([crc8(data)]))
        data = bytearray(GOLDEN[:-1]); data[1:5] = bytes(4)
        with self.assertRaises(ValueError):
            decode_colour_packet(bytes(data) + bytes([crc8(data)]))

    def test_sessions_sequence_wrap_duplicates_and_loss(self):
        packet = replace(decode_colour_packet(GOLDEN), sequence=0xFFFFFFFF, dropped=0)
        stream = ColourStream()
        self.assertTrue(any("session" in note.lower() for note in stream.observe(packet)))
        self.assertEqual(stream.observe(replace(packet, sequence=0)), [])
        self.assertEqual(stream.observe(replace(packet, sequence=0, is_snapshot=True)), [])
        self.assertTrue(any("2" in note and "missing" in note.lower()
                            for note in stream.observe(replace(packet, sequence=3))))
        self.assertTrue(any("overflow" in note.lower()
                            for note in stream.observe(replace(packet, sequence=4, dropped=1))))
        self.assertTrue(any("session" in note.lower()
                            for note in stream.observe(replace(packet, session=1, sequence=1, dropped=0))))

    @unittest.skipUnless(os.environ.get("COLOUR_VECTOR_FILE"), "C vectors supplied by full runner")
    def test_c_encoder_golden_vector(self):
        line = Path(os.environ["COLOUR_VECTOR_FILE"]).read_text().splitlines()[0]
        self.assertEqual(bytes.fromhex(line), GOLDEN)
        self.assertEqual(decode_colour_packet(bytes.fromhex(line)).left_raw, 2748)

    def test_watch_reports_loss_and_waiting_without_any_publication(self):
        packet = decode_colour_packet(GOLDEN)

        class ReadOnlyPeer:
            tick = 0.0
            received = 0

            def receive(self, timeout):
                self.tick += 2.0
                self.received += 1
                if self.received == 1: return packet
                if self.received == 2: return replace(packet, sequence=packet.sequence + 3)
                if self.received == 3: return None
                raise KeyboardInterrupt

            def publish(self, *_):
                raise AssertionError("Read-only watch tried to publish a robot command")

        peer = ReadOnlyPeer()
        output = io.StringIO()
        with redirect_stdout(output), self.assertRaises(KeyboardInterrupt):
            watch(peer, 1.0, clock=lambda: peer.tick)
        self.assertIn("missing", output.getvalue().lower())
        self.assertIn("waiting", output.getvalue().lower())


if __name__ == "__main__":
    unittest.main()
