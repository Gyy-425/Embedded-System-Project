"""Exercise the verification runner's actual host flags with controlled UB."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from check_colour_core import HOST_FLAGS


class SanitizerGateCheck(unittest.TestCase):
    @unittest.skipUnless(shutil.which("gcc"), "GCC host toolchain required; run in WSL")
    def test_undefined_behavior_is_fatal_with_runner_flags(self):
        source = """#include <limits.h>
int main(void)
{
    volatile int top = INT_MAX;
    volatile int one = 1;
    volatile int sum = top + one;
    (void)sum;
    return 0;
}
"""
        with tempfile.TemporaryDirectory(prefix="aai05-ubsan-") as directory:
            source_path = Path(directory) / "probe.c"
            binary = Path(directory) / "probe"
            source_path.write_text(source)
            compiled = subprocess.run(["gcc", *HOST_FLAGS, str(source_path), "-o", str(binary)],
                                      capture_output=True, text=True, timeout=30)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            self.assertEqual(compiled.stderr, "")
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10,
                                 env=dict(os.environ, UBSAN_OPTIONS="halt_on_error=0"))
            self.assertIn("runtime error:", ran.stderr)
            self.assertNotEqual(ran.returncode, 0,
                                "Recoverable UB must never yield a passing host step")


if __name__ == "__main__":
    unittest.main()
