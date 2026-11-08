"""Catch polarity/rounding drift and unsafe CLI parsing/publication changes."""
from contextlib import nullcontext, redirect_stderr, redirect_stdout
from dataclasses import replace
import io
import os
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/mqtt"))
from chassis import chassis_duties
import drive_cli
from drive_codec import OP_START, OP_KEEPALIVE, OP_STOP, OP_STATUS
from testing.host.test_drive_cli import Clock, Peer, operations


class ChassisPeer(Peer):
    """Hardware-free transport; report the actual duties in the START frame."""
    def publish(self, payload, timeout):
        opcode = (payload[0] >> 1) & 7
        duties = (self.status.left_pct, self.status.right_pct)
        if opcode == OP_START:
            control = int.from_bytes(payload[:3], "big")
            fields = ((control >> 7) & 127, control & 127)
            duties = tuple(value - 128 if value >= 64 else value for value in fields)
        super().publish(payload, timeout)
        if opcode in (OP_START, OP_KEEPALIVE):
            self.status = replace(self.status, left_pct=duties[0], right_pct=duties[1])
            self.queue[-1] = self.status


class ChassisConversionTests(unittest.TestCase):
    def test_literal_physical_directions_and_scaling(self):
        for forward, turn, expected in (
                (0, 50, (0, 0)), (1, 1, (-2, 0)),
                (10, 50, (-20, 0)), (10, -50, (0, 20)),
                (30, 10, (-40, 20)), (40, 0, (-40, 40)),
                (40, 10, (-50, 30)), (40, -10, (-30, 50)),
                (40, 20, (-50, 16)), (40, -20, (-16, 50)),
                (50, 1, (-50, 48)), (50, 0, (-50, 50))):
            with self.subTest(forward=forward, turn=turn):
                self.assertEqual(chassis_duties(forward, turn), expected)

    def test_reject_narrowing_and_noninteger_values(self):
        for forward, turn in ((-1, 0), (51, 0), (256, 0), (10, -51),
                               (10, 51), (10, 256), (True, 0), (10, False),
                               (10.0, 0), (10, 1.0), ("10", 0), (None, 0)):
            with self.subTest(forward=forward, turn=turn), self.assertRaises(ValueError):
                chassis_duties(forward, turn)

    @unittest.skipUnless(os.environ.get("CHASSIS_VECTOR_FILE"),
                         "C vector output supplied by complete host runner")
    def test_matches_real_c_output_for_all_5151_valid_pairs(self):
        seen = set()
        for line in Path(os.environ["CHASSIS_VECTOR_FILE"]).read_text().splitlines():
            if line.startswith("CHASSIS "):
                forward, turn, left, right = map(int, line.split()[1:])
                self.assertNotIn((forward, turn), seen)
                seen.add((forward, turn))
                self.assertEqual(chassis_duties(forward, turn), (left, right))
        self.assertEqual(seen, {(f, t) for f in range(51) for t in range(-50, 51)})


class ChassisCliTests(unittest.TestCase):
    def parse(self, *words):
        with patch.object(sys, "argv", ["drive_cli.py", "--host", "192.0.2.1", *words]):
            return drive_cli.arguments()

    def test_move_defaults_and_explicit_mapping(self):
        args = self.parse("move")
        self.assertEqual((args.forward, args.turn, args.left, args.right, args.seconds),
                         (30, 0, -30, 30, 2.0))
        args = self.parse("move", "--forward", "40", "--turn", "-10", "--seconds", "1")
        self.assertEqual((args.left, args.right, args.seconds), (-30, 50, 1.0))

    def test_raw_drive_meaning_is_unchanged(self):
        args = self.parse("drive", "--left", "50", "--right", "50")
        self.assertEqual((args.left, args.right), (50, 50))

    def test_script_entrypoint_rejects_zero_forward_before_network(self):
        script = Path(__file__).resolve().parents[2] / "tools/mqtt/drive_cli.py"
        result = subprocess.run([sys.executable, str(script), "--host", "127.0.0.1",
                                 "--port", "1", "move", "--forward", "0"],
                                capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertIn("--forward must be1..50", result.stderr)

    def test_invalid_move_fails_before_bridge_creation(self):
        for words in (("--forward", "0"), ("--forward", "-1"),
                       ("--forward", "51"), ("--turn", "51"),
                       ("--turn", "-51"), ("--forward", "3.5"),
                       ("--seconds", "0"), ("--seconds", "11"),
                       ("--seconds", "nan"), ("--seconds", "inf")):
            with self.subTest(words=words), redirect_stderr(io.StringIO()):
                with patch.object(sys, "argv", ["drive_cli.py", "--host", "192.0.2.1", "move", *words]), \
                        patch.object(drive_cli, "Bridge", side_effect=AssertionError("Network reached")), \
                        self.assertRaises(SystemExit) as stopped:
                    drive_cli.main()
                self.assertEqual(stopped.exception.code, 2)

    def test_move_serializes_mapped_start_renews_and_confirms_stop(self):
        clock = Clock()
        peer = ChassisPeer(clock)
        output = io.StringIO()
        real_run = drive_cli.run_drive

        def bounded_run(bridge, status, left, right, seconds, timeout):
            return real_run(bridge, status, left, right, seconds, timeout, clock=clock)

        with patch.object(sys, "argv", ["drive_cli.py", "--host", "192.0.2.1", "move",
                                        "--forward", "40", "--turn", "10", "--seconds", "0.7"]), \
                patch.object(drive_cli, "Bridge", return_value=nullcontext(peer)), \
                patch.object(drive_cli, "run_drive", side_effect=bounded_run), \
                redirect_stdout(output):
            self.assertEqual(drive_cli.main(), 0)
        starts = [p for p in peer.sent if (p[0] >> 1) & 7 == OP_START]
        self.assertEqual(len(starts), 1)
        control = int.from_bytes(starts[0][:3], "big")
        self.assertEqual(((control >> 7) & 127, control & 127), (78, 30))  # -50/+30 signed7
        ops = operations(peer)
        self.assertIn(OP_KEEPALIVE, ops)
        self.assertEqual(ops.count(OP_STOP), 1)
        self.assertIn(OP_STATUS, ops[ops.index(OP_STOP) + 1:])
        self.assertFalse(peer.status.active)
        self.assertEqual((peer.status.left_pct, peer.status.right_pct), (0, 0))
        self.assertIn("forward=40%", output.getvalue())
        self.assertIn("turn=+10%", output.getvalue())
        self.assertIn("Motor output OFF confirmed", output.getvalue())


if __name__ == "__main__":
    unittest.main()
