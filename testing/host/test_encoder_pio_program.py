"""Execute actual assembled RP2040 instruction words, independent of C decoder."""
from pathlib import Path
import re
import unittest

HEADER = Path(__file__).resolve().parents[2] / "app_program/subsystem2/encoder_counter.pio.h"


class Machine:
    def __init__(self, phase=0, sm=0):
        source = HEADER.read_text()
        body = re.search(r"encoder_counter_program_instructions\[\]\s*=\s*\{(.*?)\};", source, re.S).group(1)
        self.code = [int(word, 16) for word in re.findall(r"0x([0-9a-fA-F]{4})", body)]
        self.bottom = int(re.search(r"#define encoder_counter_wrap_target (\d+)", source).group(1))
        self.top = int(re.search(r"#define encoder_counter_wrap (\d+)", source).group(1))
        self.pc = self.bottom
        self.phase, self.sm = phase, sm
        self.y, self.isr, self.osr, self.flags = 0, 0, phase, 0
        self.fifo = []

    def step(self):
        word = self.code[self.pc]
        major, arg, value = word >> 13, (word >> 5) & 7, word & 31
        next_pc = self.bottom if self.pc == self.top else self.pc + 1
        if major == 0:
            taken = arg == 0
            if arg == 4:
                taken = self.y != 0
                self.y = (self.y - 1) & 0xffffffff
            elif arg != 0:
                raise AssertionError(f"Unsupported JMP condition {arg}")
            if taken:
                next_pc = value
        elif major == 2:  # IN, configured left shift
            assert arg == 0 and value == 2
            self.isr = ((self.isr << 2) | self.phase) & 0xffffffff
        elif major == 3:  # OUT ISR, configured right shift
            assert arg == 6 and value == 2
            self.isr = self.osr & 3
            self.osr >>= 2
        elif major == 4:  # PUSH noblock; ISR clears even with full FIFO
            assert word == 0x8000
            if len(self.fifo) < 4:
                self.fifo.append(self.isr)
            self.isr = 0
        elif major == 5:  # MOV
            src = word & 7
            source = {0: self.phase, 2: self.y, 6: self.isr, 7: self.osr}[src]
            operation = (word >> 3) & 3
            assert operation in (0, 1)
            source = source ^ 0xffffffff if operation == 1 else source
            if arg == 2:
                self.y = source
            elif arg == 5:
                next_pc = source & 31
            elif arg == 6:
                self.isr = source
            elif arg == 7:
                self.osr = source
            else:
                raise AssertionError(f"Unsupported MOV destination {arg}")
        elif major == 6:
            assert word == 0xc010  # IRQ SET 0 REL; never WAIT/CLEAR
            self.flags |= 1 << self.sm
        else:
            raise AssertionError(f"Unsupported instruction {word:04x}")
        self.pc = next_pc

    def hold(self, phase, clocks=32):
        self.phase = phase
        for _ in range(clocks):
            self.step()

    def fresh(self):
        remaining = len(self.fifo) + 1
        for _ in range(256):
            if self.fifo:
                value = self.fifo.pop(0)
                remaining -= 1
                if remaining == 0:
                    return value
            self.step()
        raise AssertionError("No fresh PIO snapshot")


class ProgramTests(unittest.TestCase):
    def test_stationary_all_start_phases_have_no_phantom_edges(self):
        for phase in range(4):
            machine = Machine(phase)
            machine.hold(phase, 100)
            self.assertEqual(machine.y, 0)
            self.assertEqual(machine.flags, 0)
            self.assertEqual(machine.fresh(), 0)

    def test_all_sixteen_transitions_and_relative_invalid_flags(self):
        positive = {(0, 2), (2, 3), (3, 1), (1, 0)}
        negative = {(b, a) for a, b in positive}
        for sm in range(4):
            for old in range(4):
                for new in range(4):
                    machine = Machine(old, sm)
                    machine.hold(old)
                    machine.hold(new)
                    expected = 1 if (old, new) in positive else -1 if (old, new) in negative else 0
                    self.assertEqual(machine.y, expected & 0xffffffff, (old, new))
                    self.assertEqual(machine.flags, (1 << sm) if old ^ new == 3 else 0)

    def test_cpu_pause_and_full_fifo_do_not_lose_count(self):
        left, right = Machine(), Machine()
        for _ in range(1000):
            for reverse, forward in zip((1, 3, 2, 0), (2, 3, 1, 0)):
                left.hold(reverse)
                right.hold(forward)
        self.assertEqual(len(left.fifo), 4)
        self.assertEqual(left.fresh(), (-4000) & 0xffffffff)
        self.assertEqual(right.fresh(), 4000)
        self.assertEqual(left.flags | right.flags, 0)

    def test_reversal_and_counter_wrap(self):
        machine = Machine()
        machine.hold(0)
        machine.y = 0xffffffff
        for phase in (2, 3, 1, 0):
            machine.hold(phase)
        self.assertEqual(machine.fresh(), 3)
        for phase in (1, 3, 2, 0):
            machine.hold(phase)
        self.assertEqual(machine.fresh(), 0xffffffff)

    def test_twelve_clock_state_dwell_with_full_fifo(self):
        machine = Machine()
        machine.hold(0)
        for _ in range(1000):
            for phase in (2, 3, 1, 0):
                machine.hold(phase, 12)
        machine.hold(0)
        self.assertEqual(machine.fresh(), 4000)
        self.assertEqual(machine.flags, 0)

    def test_invalid_flag_stays_latched_while_counting_resumes(self):
        machine = Machine()
        machine.hold(3)
        self.assertEqual(machine.flags, 1)
        machine.hold(1)
        self.assertEqual(machine.fresh(), 1)
        self.assertEqual(machine.flags, 1)


if __name__ == "__main__":
    unittest.main()
