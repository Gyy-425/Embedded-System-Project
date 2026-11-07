from dataclasses import replace
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/mqtt"))
from drive_codec import (DriveAck, OP_START, OP_KEEPALIVE, OP_STOP, OP_STATUS)
from drive_cli import request, run_drive


class Clock:
    def __init__(self):
        self.value = 0.0

    def __call__(self):
        return self.value


class Peer:
    """Controlled transport responses, no broker/robot or server-policy tests."""
    def __init__(self, clock, *, drop_start=False, gp21=False,
                 change_session=False, delayed_stop=False, short_lease=False,
                 receive_step=None):
        self.clock = clock
        self.sent = []
        self.publish_times = []
        self.queue = []
        self.drop_start = drop_start
        self.gp21 = gp21
        self.change_session = change_session
        self.delayed_stop = delayed_stop
        self.short_lease = short_lease
        self.receive_step = receive_step
        self.stopping = False
        self.status = DriveAck(0, 0, 7, True, False, True, False, False,
                               0, 2, 1, 0, 0, 123, 100, 0, 0, 0, 0)

    def publish(self, payload, timeout):
        self.sent.append(payload)
        self.publish_times.append(self.clock())
        self.clock.value += 0.01
        opcode = (payload[0] >> 1) & 7
        sequence = int.from_bytes(payload[3:5], "big")
        if opcode in (OP_START, OP_KEEPALIVE):
            self.status = replace(self.status, opcode=opcode,
                                  sequence=sequence, mutation_sequence=sequence,
                                  grant=self.status.grant + 1, remote_phase=2,
                                  motion_phase=1, reason=0, active=True,
                                  left_pct=50, right_pct=0, lease_ms=750)
            if opcode == OP_START and self.drop_start:
                return
            if opcode == OP_KEEPALIVE and self.gp21:
                self.status = replace(self.status, remote_phase=3,
                                      motion_phase=2, active=False, reason=2,
                                      left_pct=0, lease_ms=0)
            if opcode == OP_KEEPALIVE and self.change_session:
                self.status = replace(self.status, session=999)
        elif opcode == OP_STOP:
            self.stopping = True
            self.status = replace(self.status, opcode=opcode, sequence=sequence,
                                  remote_phase=3, motion_phase=2, active=False,
                                  reason=1, left_pct=0, right_pct=0, lease_ms=0)
            if self.delayed_stop:
                self.queue.append(replace(self.status, motion_phase=1,
                                          active=True, left_pct=50))
                return
        elif opcode == OP_STATUS:
            self.status = replace(self.status, opcode=opcode, sequence=sequence)
            if self.short_lease and self.status.active:
                self.status = replace(self.status, lease_ms=100)
        self.queue.append(self.status)

    def receive(self, timeout):
        elapsed = timeout if self.receive_step is None else min(timeout, self.receive_step)
        self.clock.value += max(0.001, elapsed)
        return self.queue.pop(0) if self.queue else None


def operations(peer):
    return [(packet[0] >> 1) & 7 for packet in peer.sent]


class DriveClientChecks(unittest.TestCase):
    def test_bounded_run_renews_and_always_stops(self):
        clock = Clock()
        peer = Peer(clock)
        result = run_drive(peer, peer.status, 50, 0, 0.7, 1.0, clock=clock)
        self.assertEqual(operations(peer).count(OP_START), 1)
        self.assertIn(OP_KEEPALIVE, operations(peer))
        self.assertIn(OP_STOP, operations(peer))
        self.assertFalse(result.active)
        self.assertEqual(result.motion_phase, 2)
        self.assertLess(clock.value, 3.0)

    def test_lost_start_ack_still_requests_stop(self):
        clock = Clock()
        peer = Peer(clock, drop_start=True)
        with self.assertRaises(RuntimeError):
            run_drive(peer, peer.status, 50, 0, 0.7, 0.5, clock=clock)
        starts = [frame for frame in peer.sent if (frame[0] >> 1) & 7 == OP_START]
        self.assertGreaterEqual(len(starts), 2)
        self.assertTrue(all(frame == starts[0] for frame in starts))
        self.assertIn(OP_STOP, operations(peer))
        self.assertNotIn(OP_KEEPALIVE, operations(peer))

    def test_gp21_or_session_change_never_restarts(self):
        for setting in ("gp21", "change_session"):
            with self.subTest(setting=setting):
                clock = Clock()
                peer = Peer(clock, **{setting: True})
                with self.assertRaises(RuntimeError):
                    run_drive(peer, peer.status, 50, 0, 0.7, 0.7, clock=clock)
                self.assertEqual(operations(peer).count(OP_START), 1)
                self.assertIn(OP_STOP, operations(peer))

    def test_stop_ack_is_not_immediate_output_confirmation(self):
        clock = Clock()
        peer = Peer(clock, delayed_stop=True)
        result = run_drive(peer, peer.status, 50, 0, 0.3, 1.0, clock=clock)
        stop_index = operations(peer).index(OP_STOP)
        self.assertIn(OP_STATUS, operations(peer)[stop_index + 1:])
        self.assertTrue(result.ready)
        self.assertFalse(result.active)

    def test_wrong_echo_is_ignored(self):
        clock = Clock()
        peer = Peer(clock)
        peer.queue.append(replace(peer.status, opcode=OP_START, sequence=0))
        result = request(peer, peer.status, OP_START, left=50,
                         timeout=1.0, clock=clock)
        self.assertEqual(result.sequence, 1)
        self.assertEqual(result.mutation_sequence, 1)

    def test_short_remaining_lease_is_renewed_without_fixed_wait(self):
        clock = Clock()
        peer = Peer(clock, short_lease=True, receive_step=0.01)
        run_drive(peer, peer.status, 50, 0, 0.5, 1.0, clock=clock)
        first_keep = operations(peer).index(OP_KEEPALIVE)
        last_status = max(index for index in range(first_keep)
                          if operations(peer)[index] == OP_STATUS)
        self.assertLess(peer.publish_times[first_keep] - peer.publish_times[last_status], 0.12)

    def test_keyboard_interrupt_still_requests_stop(self):
        class InterruptedPeer(Peer):
            interrupted = False

            def receive(self, timeout):
                if not self.stopping and not self.interrupted and self.clock() >= 0.3:
                    self.interrupted = True
                    raise KeyboardInterrupt()
                return super().receive(timeout)

        clock = Clock()
        peer = InterruptedPeer(clock)
        with self.assertRaises(KeyboardInterrupt):
            run_drive(peer, peer.status, 50, 0, 1.0, 1.0, clock=clock)
        self.assertIn(OP_STOP, operations(peer))

    def test_duration_budget_includes_startup(self):
        clock = Clock()
        peer = Peer(clock)
        run_drive(peer, peer.status, 50, 0, 1.0, 1.0, clock=clock)
        stop_index = operations(peer).index(OP_STOP)
        self.assertLessEqual(peer.publish_times[stop_index] - peer.publish_times[0],
                             1.001)

    def test_near_deadline_renewal_cannot_extend_run(self):
        class SlowRenewalPeer(Peer):
            def receive(self, timeout):
                if (self.sent and ((self.sent[-1][0] >> 1) & 7) == OP_KEEPALIVE
                        and not self.stopping):
                    self.clock.value += timeout
                    return self.queue.pop(0) if self.queue else None
                return super().receive(timeout)

        clock = Clock()
        peer = SlowRenewalPeer(clock)
        run_drive(peer, peer.status, 50, 0, 0.75, 1.0, clock=clock)
        stop_index = operations(peer).index(OP_STOP)
        self.assertLessEqual(peer.publish_times[stop_index], 0.751)

    def test_invalid_run_does_not_publish(self):
        for left, seconds in ((51, 1), (50, 0), (50, 11), (50, float("nan"))):
            clock = Clock()
            peer = Peer(clock)
            with self.subTest(left=left, seconds=seconds), self.assertRaises(ValueError):
                run_drive(peer, peer.status, left, 0, seconds, clock=clock)
            self.assertEqual(peer.sent, [])


if __name__ == "__main__":
    unittest.main()
