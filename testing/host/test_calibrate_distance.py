import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

PATH = Path(__file__).resolve().parents[2] / "tools/calibrate_distance.py"
SPEC = importlib.util.spec_from_file_location("calibrate_distance", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class CalibrationTests(unittest.TestCase):
    def test_measured_distance_and_each_forward_sign(self):
        result = MODULE.calibration_from_measurements("500", -10000, 12000)
        self.assertEqual(result.get("left"),
                         dict(reference_um=500000, reference_counts=10000, forward_sign=-1))
        self.assertEqual(result.get("right"),
                         dict(reference_um=500000, reference_counts=12000, forward_sign=1))

    def test_decimal_um_and_reversed_harness_signs(self):
        result = MODULE.calibration_from_measurements("123.456", 999, -1001)
        self.assertEqual(result["left"]["reference_um"], 123456)
        self.assertEqual(result["left"]["forward_sign"], 1)
        self.assertEqual(result["right"]["forward_sign"], -1)

    def test_invalid_measurements(self):
        for distance in ("0", "-1", "NaN", "Infinity", "abc", "0.0001", "1000000.001"):
            with self.subTest(distance=distance), self.assertRaises(ValueError):
                MODULE.calibration_from_measurements(distance, -10, 10)
        for count in (0, 2147483648, -2147483648, 1.5, True):
            with self.subTest(count=count), self.assertRaises(ValueError):
                MODULE.calibration_from_measurements("100", count, 100)

    def test_cli_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "calibration.json"
            command = [sys.executable, str(PATH), "--distance-mm", "500",
                       "--left-counts", "-10000", "--right-counts", "12000",
                       "--output", str(output)]
            first = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(first.returncode, 0, first.stderr)
            original = output.read_bytes()
            self.assertEqual(json.loads(original)["right"]["reference_counts"], 12000)
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(output.read_bytes(), original)

    def test_invalid_input_creates_no_file(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "invalid.json"
            result = subprocess.run([sys.executable, str(PATH), "--distance-mm", "0",
                "--left-counts", "-1", "--right-counts", "1", "--output", str(output)],
                capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
