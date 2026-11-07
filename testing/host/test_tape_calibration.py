"""Synthetic protocol fixtures; these tests are not physical tape evidence."""
import csv
from dataclasses import replace
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/mqtt"))
from drive_codec import DriveAck, DriveTelemetry
from tape_calibration import capture_samples, report_captures, write_report


class Clock:
    value = 0.0

    def __call__(self):
        return self.value


def status():
    return DriveAck(0, 0, 7, True, False, True, False, False,
                    0, 2, 1, 0, 0, 123, 100, 1000, 0, 0, 0)


def packet(index, **changes):
    values = vars(replace(status(), sequence=index,
                          uptime_ms=1000 + index * 100))
    return replace(DriveTelemetry(**values, left_raw=100, right_raw=200), **changes)


class Stream:
    """Receive-only stream: capture cannot publish any command through it."""
    def __init__(self, clock, packets):
        self.clock = clock
        self.packets = iter(packets)
        self.calls = 0

    def receive(self, timeout):
        self.calls += 1
        self.clock.value += timeout
        value = next(self.packets, None)
        if isinstance(value, BaseException):
            raise value
        return value


class CalibrationChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def collect(self, directory="capture", packets=None, **options):
        clock = Clock()
        stream = Stream(clock, packets if packets is not None else
                        [packet(i) for i in range(1, 11)])
        arguments = dict(label=0, name="black", setup="fixture board/light",
                         sample_count=10, settle_ms=0)
        arguments.update(options)
        result = capture_samples(stream, status(), self.root / directory,
                                 clock=clock, **arguments)
        return result, stream

    def metadata(self, directory="capture"):
        return json.loads((self.root / directory / "metadata.json").read_text())

    def test_complete_capture_persists_only_fresh_valid_samples(self):
        frames = [packet(0), packet(1, ir_stale=True, ir_valid=False),
                  packet(2, ir_valid=False), packet(3), packet(3)]
        frames += [packet(i, left_raw=0) for i in range(4, 13)]
        result, stream = self.collect(packets=frames)
        self.assertEqual(result["state"], "COMPLETE")
        self.assertEqual(result["accepted"], 10)
        self.assertEqual(result["discarded"]["invalid_ir"], 2)
        self.assertEqual(result["discarded"]["not_fresh"], 2)
        with (self.root / "capture/samples.csv").open(newline="") as file:
            rows = list(csv.DictReader(file))
        self.assertEqual(len(rows), 10)
        self.assertEqual(rows[0]["uptime_ms"], "1300")
        self.assertEqual(rows[1]["left_raw"], "0")
        self.assertEqual(self.metadata(), result)
        before = (self.root / "capture/samples.csv").read_bytes()
        with self.assertRaises(FileExistsError):
            self.collect()
        self.assertEqual((self.root / "capture/samples.csv").read_bytes(), before)

    def test_unsafe_initial_status_does_not_create_capture(self):
        for changed in ({"active": True}, {"remote_phase": 1},
                        {"motion_phase": 3}, {"ready": False}):
            with self.subTest(changed=changed), self.assertRaises(RuntimeError):
                capture_samples(Stream(Clock(), []), replace(status(), **changed),
                                self.root / "unsafe", label=0, name="black", setup="test")
            self.assertFalse((self.root / "unsafe").exists())

    def test_movement_or_session_change_preserves_partial_capture(self):
        for index, changed in enumerate(({"active": True}, {"remote_phase": 1},
                                         {"session": 999}, {"motion_phase": 3})):
            directory = f"partial{index}"
            with self.subTest(changed=changed), self.assertRaises(RuntimeError):
                self.collect(directory, [packet(1), packet(2, **changed)])
            meta = self.metadata(directory)
            self.assertEqual(meta["state"], "ABORTED")
            self.assertEqual(meta["accepted"], 1)
            self.assertEqual(len((self.root / directory / "samples.csv").read_text().splitlines()), 2)

    def test_timeout_and_silence_are_not_complete_data(self):
        with self.assertRaises(TimeoutError):
            self.collect(packets=[packet(i, ir_valid=False) for i in range(1, 30)],
                         timeout=1.0)
        self.assertEqual(self.metadata()["state"], "TIMED_OUT")
        with self.assertRaisesRegex(RuntimeError, "fresh telemetry"):
            self.collect("silent", [])
        self.assertEqual(self.metadata("silent")["state"], "ABORTED")

    def test_interrupt_keeps_flushed_samples(self):
        with self.assertRaises(KeyboardInterrupt):
            self.collect(packets=[packet(1), packet(2), KeyboardInterrupt()])
        self.assertEqual(self.metadata()["state"], "INTERRUPTED")
        self.assertEqual(self.metadata()["accepted"], 2)

    def test_ordered_backlog_cannot_become_fresh_calibration(self):
        class DelayedStream(Stream):
            next_delivery = 1.5

            def receive(self, timeout):
                self.clock.value += timeout
                if self.clock.value + 0.000001 < self.next_delivery:
                    return None
                self.next_delivery += 1.5
                return next(self.packets, None)

        clock = Clock()
        frames = [packet(i, uptime_ms=1000 + i * 250) for i in range(1, 11)]
        with self.assertRaisesRegex(RuntimeError, "elapsed"):
            capture_samples(DelayedStream(clock, frames), status(), self.root / "lag",
                            label=0, name="black", setup="fixture", sample_count=10,
                            settle_ms=0, clock=clock)
        self.assertEqual(self.metadata("lag")["state"], "ABORTED")

    def test_pause_inside_receive_does_not_hide_silence(self):
        class PausedStream(Stream):
            def receive(self, timeout):
                value = super().receive(timeout)
                if self.calls == 1:
                    self.clock.value += 2.1
                return value

        clock = Clock()
        frames = [packet(i, uptime_ms=3200 + (i - 1) * 100) for i in range(1, 11)]
        with self.assertRaisesRegex(RuntimeError, "fresh telemetry"):
            capture_samples(PausedStream(clock, frames), status(), self.root / "pause",
                            label=0, name="black", setup="fixture", sample_count=10,
                            settle_ms=0, clock=clock)
        self.assertEqual(self.metadata("pause")["accepted"], 0)

    def test_last_receive_cannot_complete_after_deadline(self):
        class LastPauseStream(Stream):
            def receive(self, timeout):
                value = super().receive(timeout)
                if self.calls == 10:
                    self.clock.value += 0.2
                return value

        clock = Clock()
        with self.assertRaises(TimeoutError):
            capture_samples(LastPauseStream(clock, [packet(i) for i in range(1, 11)]),
                            status(), self.root / "deadline", label=0, name="black",
                            setup="fixture", sample_count=10, timeout=1.0,
                            settle_ms=0, clock=clock)
        self.assertEqual(self.metadata("deadline")["state"], "TIMED_OUT")
        self.assertEqual(self.metadata("deadline")["accepted"], 9)

    def test_unsigned_wrap_and_settling(self):
        clock = Clock()
        initial = replace(status(), uptime_ms=0xFFFFFF00)
        frames = [packet(i, sequence=(65531 + i) & 65535,
                         uptime_ms=(0xFFFFFF00 + i * 100) & 0xFFFFFFFF)
                  for i in range(1, 12)]
        result = capture_samples(Stream(clock, frames), initial, self.root / "wrap",
                                 label=0, name="black", setup="test", sample_count=10,
                                 settle_ms=200, clock=clock)
        self.assertEqual(result["accepted"], 10)
        self.assertEqual(result["discarded"]["settling"], 1)
        self.assertEqual(result["state"], "COMPLETE")

    def test_invalid_arguments_have_no_output_side_effect(self):
        for index, changed in enumerate(({"sample_count": 9}, {"label": 7},
                                         {"name": ""}, {"setup": ""},
                                         {"height_mm": float("nan")})):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                self.collect(f"invalid{index}", **changed)
            self.assertFalse((self.root / f"invalid{index}").exists())

    def seven(self, overlap=False):
        for label in range(7):
            left = (0 if overlap and label == 1 else label * 100)
            frames = [packet(i, left_raw=left, right_raw=label * 200)
                      for i in range(1, 11)]
            self.collect(f"class{label}", frames, label=label,
                         name=f"tape{label}", height_mm=5.0)

    def test_seven_classes_report_candidates_not_accuracy(self):
        self.seven()
        result = report_captures(self.root)
        self.assertEqual(result["status"], "CANDIDATE_RANGES")
        self.assertEqual(result["missing_classes"], [])
        self.assertEqual(result["overlaps"], {"left": [], "right": []})
        self.assertEqual(result["classes"]["0"]["left"]["rail_hits"], 10)
        self.assertEqual(result["classes"]["6"]["right"]["median"], 1200)
        self.assertNotIn("accuracy", result)
        output = self.root / "report.json"
        write_report(result, output)
        with self.assertRaises(FileExistsError):
            write_report(result, output)

    def test_overlap_is_reported_for_each_sensor(self):
        self.seven(overlap=True)
        result = report_captures(self.root)
        self.assertEqual(result["status"], "OVERLAP")
        self.assertEqual(result["overlaps"]["left"], [{"classes": [0, 1], "range": [0, 0]}])
        self.assertEqual(result["overlaps"]["right"], [])

    def test_incomplete_capture_excluded_and_missing_classes_named(self):
        self.collect()
        with self.assertRaises(TimeoutError):
            self.collect("failed", [packet(i, ir_valid=False) for i in range(1, 30)],
                         timeout=1.0)
        result = report_captures(self.root)
        self.assertEqual(result["status"], "INCOMPLETE")
        self.assertEqual(result["missing_classes"], [1, 2, 3, 4, 5, 6])
        self.assertEqual(result["excluded"], [{"capture": "failed", "state": "TIMED_OUT"}])

    def test_different_setups_or_class_names_are_rejected(self):
        for field, value in (("setup", "different lighting"), ("name", "different tape")):
            with tempfile.TemporaryDirectory() as directory:
                self.root = Path(directory)
                self.collect("a")
                self.collect("b", **{field: value})
                with self.subTest(field=field), self.assertRaises(ValueError):
                    report_captures(self.root)

    def test_malformed_complete_capture_rejected(self):
        self.collect()
        path = self.root / "capture/samples.csv"
        text = path.read_text().replace(",100,200", ",4096,200", 1)
        path.write_text(text)
        with self.assertRaises(ValueError):
            report_captures(self.root)

    def test_corrupt_metadata_state_fails_with_clear_validation(self):
        self.collect()
        path = self.root / "capture/metadata.json"
        meta = self.metadata()
        meta["state"] = []
        path.write_text(json.dumps(meta))
        with self.assertRaises(ValueError):
            report_captures(self.root)

    def test_report_rejects_duplicate_rows(self):
        self.collect()
        path = self.root / "capture/samples.csv"
        with path.open(newline="") as file:
            rows = list(csv.DictReader(file))
        rows[1]["sequence"] = rows[0]["sequence"]
        with path.open("w", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=rows[0])
            writer.writeheader()
            writer.writerows(rows)
        with self.assertRaises(ValueError):
            report_captures(self.root)

    def test_cli_offline_report_and_invalid_capture_arguments(self):
        self.collect()
        cli = Path(__file__).resolve().parents[2] / "tools/mqtt/calibrate_cli.py"
        command = [sys.executable, str(cli), "report", "--root", str(self.root),
                   "--out", str(self.root / "report.json")]
        done = subprocess.run(command, capture_output=True, text=True, timeout=10)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("INCOMPLETE", done.stdout)
        repeated = subprocess.run(command, capture_output=True, text=True, timeout=10)
        self.assertNotEqual(repeated.returncode, 0)
        invalid = subprocess.run([sys.executable, str(cli), "capture", "--samples", "1"],
                                 capture_output=True, text=True, timeout=10)
        self.assertNotEqual(invalid.returncode, 0)


if __name__ == "__main__":
    unittest.main()
