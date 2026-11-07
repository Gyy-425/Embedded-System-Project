from dataclasses import replace
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/mqtt"))
from drive_codec import (DriveAck, OP_START, OP_KEEPALIVE, OP_STOP, OP_STATUS,
                         OP_TARGET, OP_CLEAR)
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
        elif opcode in (OP_TARGET, OP_CLEAR):
            target = (int.from_bytes(payload[:3], "big") >> 14) & 7
            self.status = replace(self.status, opcode=opcode, sequence=sequence,
                                  mutation_sequence=sequence, target=target,
                                  grant=self.status.grant + 1)
        self.queue.append(self.status)

    def receive(self, timeout):
        elapsed = timeout if self.receive_step is None else min(timeout, self.receive_step)
        self.clock.value += max(0.001, elapsed)
        return self.queue.pop(0) if self.queue else None


def operations(peer):
    return [(packet[0] >> 1) & 7 for packet in peer.sent]


class ExpiringGrantPeer(Peer):
    """The device rejects before ARM, returning its rotated grant in the ACK."""
    def __init__(self, clock, *, rejections=1, result=8, changes=None):
        super().__init__(clock)
        self.rejections = rejections
        self.rejection_result = result
        self.changes = changes or {}

    def publish(self, payload, timeout):
        opcode = (payload[0] >> 1) & 7
        if self.rejections and opcode in (OP_START, OP_TARGET, OP_CLEAR, OP_KEEPALIVE):
            self.rejections -= 1
            self.sent.append(payload)
            self.publish_times.append(self.clock())
            self.clock.value += 0.01
            sequence = int.from_bytes(payload[3:5], "big")
            rejected = replace(self.status, opcode=opcode,
                               result=self.rejection_result, sequence=sequence,
                               grant=self.status.grant + 1)
            rejected = replace(rejected, **self.changes)
            self.status = replace(rejected, result=0)
            self.queue.append(rejected)
            return
        super().publish(payload, timeout)


class DriveClientChecks(unittest.TestCase):
    def test_expired_idle_grant_recovers_only_explicitly_rejected_start(self):
        clock = Clock()
        peer = ExpiringGrantPeer(clock)
        try:
            result = run_drive(peer, peer.status, 50, 0, 0.7, 1.0, clock=clock)
        except RuntimeError as error:
            self.fail(f"Unaccepted START must tolerate one idle grant expiry: {error}")
        starts = [frame for frame in peer.sent if (frame[0] >> 1) & 7 == OP_START]
        self.assertEqual(len(starts), 2)
        self.assertEqual([int.from_bytes(frame[9:13], "big") for frame in starts],
                         [100, 101])
        self.assertEqual([int.from_bytes(frame[3:5], "big") for frame in starts],
                         [1, 1])
        self.assertEqual(starts[0][:9], starts[1][:9])
        self.assertFalse(result.active)
        self.assertEqual(operations(peer).count(OP_STOP), 1)
        stop_index = operations(peer).index(OP_STOP)
        self.assertLessEqual(peer.publish_times[stop_index], 0.701)

    def test_idle_configuration_uses_same_bounded_grant_recovery(self):
        for opcode, target in ((OP_TARGET, 3), (OP_CLEAR, 7)):
            clock = Clock()
            peer = ExpiringGrantPeer(clock)
            with self.subTest(opcode=opcode):
                try:
                    result = request(peer, peer.status, opcode, target=target,
                                     timeout=0.6, clock=clock)
                except RuntimeError as error:
                    self.fail(f"Unaccepted idle configuration grant expired: {error}")
                self.assertEqual(result.target, target)
                self.assertEqual(operations(peer), [opcode, opcode])

    def test_grant_recovery_is_limited_to_one_rejection(self):
        clock = Clock()
        peer = ExpiringGrantPeer(clock, rejections=2)
        with self.assertRaisesRegex(RuntimeError, "STALE_GRANT"):
            run_drive(peer, peer.status, 50, 0, 0.7, 1.0, clock=clock)
        self.assertEqual(operations(peer).count(OP_START), 2)
        self.assertIn(OP_STOP, operations(peer))
        self.assertNotIn(OP_KEEPALIVE, operations(peer))

    def test_refreshed_start_with_lost_ack_only_repeats_identical_bytes(self):
        clock = Clock()
        peer = ExpiringGrantPeer(clock)
        peer.drop_start = True
        with self.assertRaises(RuntimeError):
            run_drive(peer, peer.status, 50, 0, 0.7, 0.6, clock=clock)
        starts = [frame for frame in peer.sent if (frame[0] >> 1) & 7 == OP_START]
        self.assertGreaterEqual(len(starts), 3)
        self.assertTrue(all(frame == starts[1] for frame in starts[1:]))
        self.assertEqual(int.from_bytes(starts[1][9:13], "big"), 101)
        self.assertIn(OP_STOP, operations(peer))
        self.assertNotIn(OP_KEEPALIVE, operations(peer))

    def test_changed_device_state_or_other_rejection_cannot_retry_start(self):
        cases = [(7, {}), (9, {}), (11, {}),
                 (8, {"mutation_sequence": 1}), (8, {"session": 999}),
                 (8, {"remote_phase": 3}), (8, {"motion_phase": 3}),
                 (8, {"reason": 2}), (8, {"active": True}),
                 (8, {"ready": False}), (8, {"grant": 100}),
                 (8, {"grant": 0})]
        for result, changes in cases:
            clock = Clock()
            peer = ExpiringGrantPeer(clock, result=result, changes=changes)
            with self.subTest(result=result, changes=changes), self.assertRaises(RuntimeError):
                request(peer, peer.status, OP_START, left=50, clock=clock)
            self.assertEqual(operations(peer), [OP_START])

    def test_expired_grant_during_running_is_not_a_new_start(self):
        clock = Clock()
        peer = ExpiringGrantPeer(clock)
        peer.status = replace(peer.status, remote_phase=2, motion_phase=1,
                              active=True, left_pct=50, lease_ms=750)
        with self.assertRaisesRegex(RuntimeError, "STALE_GRANT"):
            request(peer, peer.status, OP_KEEPALIVE, clock=clock)
        self.assertEqual(operations(peer), [OP_KEEPALIVE])

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
