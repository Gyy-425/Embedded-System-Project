from dataclasses import replace
from io import BytesIO, StringIO
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/mqtt"))
from drive_cli import MENU_START_OPCODES, FollowMenu, MenuCommands, run_follow_menu
from drive_codec import (DriveAck, DriveTelemetry, OP_FOLLOW, OP_KEEPALIVE, OP_STATUS, OP_STOP,
                         OP_STRAIGHT, decode_drive_packet, encode_drive_command,
                         format_drive_packet)
from mqtt_publisher import MqttPublisher
from robot_cli import Bridge
from robot_codec import crc8
from route_codec import decode_route_packet, format_route_report, format_route_status
from testing.host.test_drive_cli import Clock, Peer, operations
from testing.host.test_drive_codec import ACK


def with_crc(body):
    return body + bytes([crc8(body)])


ROUTE = with_crc(bytes.fromhex(
    "d0020c050c010000007b00010007a120000186a002"
    "0403000493e00504000186a0"))


class FollowPeer(Peer):
    def __init__(self, clock, *, lose_follow=False, change_session=False):
        super().__init__(clock, change_session=change_session)
        self.route = decode_route_packet(ROUTE)
        self.lose_follow = lose_follow

    def latest_route(self):
        return None if self.route is None else (self.route, self.clock())

    def publish(self, payload, timeout):
        opcode = (payload[0] >> 1) & 7
        if opcode not in (OP_FOLLOW, OP_STRAIGHT):
            return super().publish(payload, timeout)
        self.sent.append(payload)
        sequence = int.from_bytes(payload[3:5], "big")
        self.status = replace(self.status, opcode=opcode, sequence=sequence,
                              mutation_sequence=sequence, remote_phase=2,
                              motion_phase=1, reason=0, active=True,
                              left_pct=30, right_pct=30, lease_ms=750)
        if not self.lose_follow:
            self.queue.append(self.status)


class LanePeer(FollowPeer):
    """Echo the FOLLOW target lane like the firmware status does."""

    def __init__(self, clock, *, echo_target=None):
        super().__init__(clock)
        self.echo_target = echo_target

    def publish(self, payload, timeout):
        opcode = (payload[0] >> 1) & 7
        if opcode == OP_FOLLOW:
            control = int.from_bytes(payload[:3], "big")
            target = (control >> 14) & 7 if self.echo_target is None else self.echo_target
            self.status = replace(self.status, target=target)
        super().publish(payload, timeout)


class TelemetryOnlyPeer(FollowPeer):
    def __init__(self, clock, *, drop_opcode, changes=None):
        super().__init__(clock)
        self.drop_opcode = drop_opcode
        self.changes = changes or {}

    def publish(self, payload, timeout):
        super().publish(payload, timeout)
        opcode = (payload[0] >> 1) & 7
        if self.status.active and opcode == self.drop_opcode:
            ack = self.queue.pop()
            changes = {"uptime_ms": int(self.clock() * 1000), **self.changes}
            values = vars(replace(ack, **changes))
            self.queue.append(DriveTelemetry(**values, left_raw=230, right_raw=260))


class FollowCliChecks(unittest.TestCase):
    def test_route_report_uses_wire_order_distances_and_stop_reason(self):
        route = decode_route_packet(ROUTE)
        self.assertEqual((route.session, route.epoch, route.colour), (123, 1, 5))
        self.assertEqual([item.colour for item in route.sections], [4, 5])
        report = format_route_report(route)
        self.assertIn("0.500 m", report)
        self.assertIn("MQTT_STOP", report)
        self.assertIn("red", report)
        self.assertIn("blue", report)
        self.assertLess(report.index("red"), report.index("blue"))
        self.assertIn("partial start", report)
        self.assertIn("uncertain", report)
        self.assertIn("0.100 m unassigned", report)

    def test_route_rejects_bad_crc_and_wrong_count(self):
        bad_count = bytearray(ROUTE[:-1])
        bad_count[20] = 1
        for payload in (ROUTE[:-1], ROUTE[:-1] + bytes([ROUTE[-1] ^ 1]),
                        with_crc(bytes(bad_count))):
            with self.subTest(payload=payload), self.assertRaises(ValueError):
                decode_route_packet(payload)

    def test_follow_ack_decodes_without_changing_existing_ack(self):
        follow = with_crc(bytes([0xAC]) + ACK[1:-1])
        packet = decode_drive_packet(follow)
        self.assertIs(type(packet), DriveAck)
        self.assertEqual(packet.opcode, OP_FOLLOW)
        self.assertEqual(decode_drive_packet(ACK).opcode, 3)

    def test_straight_command_and_ack_use_opcode_seven(self):
        frame = encode_drive_command(OP_STRAIGHT, sequence=42, session=123, grant=100)
        self.assertEqual(frame, with_crc(bytes.fromhex("2fc000002a0000007b00000064")))
        for invalid in ({"left": 1}, {"right": -1}, {"target": 4}):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                encode_drive_command(OP_STRAIGHT, session=123, grant=100, **invalid)
        straight = with_crc(bytes([0xAE]) + ACK[1:-1])
        packet = decode_drive_packet(straight)
        self.assertEqual(packet.opcode, OP_STRAIGHT)
        self.assertIn("ACK STRAIGHT", format_drive_packet(packet))

    def test_route_traffic_is_cached_without_entering_the_drive_queue(self):
        args = SimpleNamespace(device="test", host="offline", port=1883,
                               tools_dir=None)
        with patch("robot_cli.executable", side_effect=lambda name, _: name):
            bridge = Bridge(args, version=2, decoder=decode_drive_packet,
                            ack_type=DriveAck, route_decoder=decode_route_packet)
        bridge._read(StringIO(f"{bridge.base}/route {ROUTE.hex()}\n"
                              f"{bridge.base}/ack {ACK.hex()}\n"), False)
        self.assertEqual(bridge.latest_route()[0].session, 123)
        self.assertEqual(bridge.receive(0).session, 0x01020304)
        self.assertTrue(bridge.messages.empty())

    def test_menu_publishes_binary_payloads_over_one_broker_connection(self):
        args = SimpleNamespace(device="test", host="offline", port=1883,
                               tools_dir=None)
        responses = BytesIO(bytes.fromhex("200200004002000140020002"))
        peer = Mock()
        peer.recv.side_effect = responses.read
        with patch("robot_cli.executable", return_value="unused"), \
             patch("socket.create_connection", return_value=peer) as connect, \
             patch("robot_cli.subprocess.run") as run:
            bridge = Bridge(args, version=2, persistent_publish=True)
            try:
                bridge.publish(b"\x00\n\xff", 0.2)
                bridge.publish(b"\x0a\x00", 0.2)
            finally:
                bridge.__exit__(None, None, None)
        connect.assert_called_once()
        run.assert_not_called()
        connect_frame, first, second = [call.args[0] for call in peer.sendall.call_args_list]
        self.assertEqual(connect_frame[0], 0x10)
        topic = b"aai05/v2/test/command"
        self.assertEqual(first, bytes([0x32, len(topic) + 7, 0, len(topic)]) +
                         topic + b"\x00\x01\x00\n\xff")
        self.assertEqual(second, bytes([0x32, len(topic) + 6, 0, len(topic)]) +
                         topic + b"\x00\x02\x0a\x00")
        peer.close.assert_called_once()

    def test_missing_broker_puback_closes_connection_without_replaying_command(self):
        connack = bytes.fromhex("20020000")
        lost_peer = Mock()
        lost_peer.recv.side_effect = [connack, TimeoutError()]
        new_peer = Mock()
        new_peer.recv.side_effect = [connack, bytes.fromhex("40020002")]
        publisher = MqttPublisher("offline", 1883, "test/command")
        self.addCleanup(publisher.close)
        with patch("socket.create_connection", side_effect=[lost_peer, new_peer]) as connect:
            with self.assertRaisesRegex(TimeoutError, "broker publish acknowledgement"):
                publisher.publish(b"start", 0.2)
            connect.assert_called_once()
            lost_peer.close.assert_called_once()
            publisher.publish(b"stop", 0.2)
        self.assertEqual(connect.call_count, 2)
        sent = new_peer.sendall.call_args_list
        self.assertEqual(len(sent), 2)
        self.assertTrue(sent[1].args[0].endswith(b"\x00\x02stop"))

    def menu(self, peer, clock, commands=None):
        return FollowMenu(peer, 1.0, commands or MenuCommands(),
                          clock=clock, output=lambda _: None)

    def test_lane_keys_map_to_follow_with_one_target_colour(self):
        self.assertEqual(MENU_START_OPCODES["f"], (OP_FOLLOW, 7))
        self.assertEqual(MENU_START_OPCODES["fb"], (OP_FOLLOW, 5))
        self.assertEqual(MENU_START_OPCODES["fr"], (OP_FOLLOW, 4))
        self.assertEqual(MENU_START_OPCODES["fy"], (OP_FOLLOW, 2))
        self.assertEqual(MENU_START_OPCODES["l"], (OP_STRAIGHT, 7))

    def test_follow_target_must_be_a_supported_lane(self):
        for target in (2, 4, 5, 7):
            frame = encode_drive_command(OP_FOLLOW, target, 1, 123, 100)
            self.assertEqual((int.from_bytes(frame[:3], "big") >> 14) & 7, target)
        for target in (0, 1, 3, 6):
            with self.assertRaises(ValueError):
                encode_drive_command(OP_FOLLOW, target, 1, 123, 100)
        with self.assertRaises(ValueError):
            encode_drive_command(OP_STRAIGHT, 5, 1, 123, 100)

    def test_selected_lane_is_sent_and_confirmed_by_the_ack_target(self):
        clock = Clock()
        peer = LanePeer(clock)
        menu = self.menu(peer, clock)
        menu.start(OP_FOLLOW, 5)
        follow = [frame for frame in peer.sent if (frame[0] >> 1) & 7 == OP_FOLLOW]
        self.assertEqual(len(follow), 1)
        self.assertEqual((int.from_bytes(follow[0][:3], "big") >> 14) & 7, 5)
        self.assertIsNotNone(menu.renew_at)
        self.assertTrue(menu.stop())

    def test_ack_for_a_different_lane_is_not_success_and_stops(self):
        clock = Clock()
        peer = LanePeer(clock, echo_target=4)
        menu = self.menu(peer, clock)
        menu.start(OP_FOLLOW, 5)
        self.assertIsNone(menu.renew_at)
        self.assertIn(OP_STOP, operations(peer))

    def test_route_report_names_lane_end_and_refused_start(self):
        for code, name in ((14, "TARGET_END"), (15, "TARGET_MISMATCH")):
            body = bytearray(ROUTE[:-1])
            body[2] = code
            report = decode_route_packet(with_crc(bytes(body)))
            self.assertIn(name, format_route_status(report))
            self.assertIn(name, format_route_report(report))

    def test_explicit_follow_uses_zero_duty_and_renews_then_fresh_stop(self):
        clock = Clock()
        peer = FollowPeer(clock)
        menu = self.menu(peer, clock)
        menu.step()
        self.assertEqual(peer.sent, [])
        menu.start()
        follow = peer.sent[operations(peer).index(OP_FOLLOW)]
        self.assertEqual(int.from_bytes(follow[:3], "big") & 0x3fff, 0)
        self.assertEqual((int.from_bytes(follow[:3], "big") >> 14) & 7, 7)
        for _ in range(10):
            menu.step()
        self.assertIn(OP_KEEPALIVE, operations(peer))
        self.assertTrue(menu.stop())
        stop_index = operations(peer).index(OP_STOP)
        self.assertIn(OP_STATUS, operations(peer)[stop_index + 1:])
        count = len(peer.sent)
        for _ in range(10):
            menu.step()
        self.assertEqual(len(peer.sent), count)

    def test_menu_confirms_running_after_start_ack_with_telemetry(self):
        clock = Clock()
        peer = TelemetryOnlyPeer(clock, drop_opcode=OP_STATUS)
        menu = self.menu(peer, clock)
        menu.start()
        self.assertIsNotNone(menu.renew_at)
        self.assertEqual(operations(peer), [OP_STATUS, OP_FOLLOW, OP_STATUS])
        self.assertTrue(menu.stop())

    def test_menu_keeps_running_when_telemetry_proves_keepalive_was_accepted(self):
        clock = Clock()
        peer = TelemetryOnlyPeer(clock, drop_opcode=OP_KEEPALIVE)
        menu = self.menu(peer, clock)
        menu.start()
        for _ in range(10):
            menu.step()
        self.assertGreater(operations(peer).count(OP_KEEPALIVE), 1)
        self.assertNotIn(OP_STOP, operations(peer))
        self.assertTrue(menu.stop())

    def test_menu_stops_when_telemetry_cannot_confirm_keepalive(self):
        for changes in ({"mutation_sequence": 1}, {"grant": 100}, {"session": 999},
                        {"uptime_ms": 0xFFFFFFFF},
                        {"lease_ms": 0}, {"remote_phase": 3, "active": False}):
            with self.subTest(changes=changes):
                clock = Clock()
                peer = TelemetryOnlyPeer(clock, drop_opcode=OP_KEEPALIVE, changes=changes)
                menu = self.menu(peer, clock)
                menu.start()
                for _ in range(10):
                    menu.step()
                self.assertIsNone(menu.renew_at)
                self.assertIn(OP_STOP, operations(peer))

    def test_line_key_starts_straight_measurement_and_quits_with_verified_stop(self):
        clock = Clock()
        commands = MenuCommands()
        output = []

        class LinePeer(FollowPeer):
            def publish(self, payload, timeout):
                super().publish(payload, timeout)
                if (payload[0] >> 1) & 7 == OP_KEEPALIVE:
                    commands.add("q")

            def receive(self, timeout):
                if clock() > 3.0 and not self.stopping:
                    commands.add("q")
                return super().receive(timeout)

        peer = LinePeer(clock)
        commands.add("l")
        self.assertEqual(run_follow_menu(peer, 1.0, commands=commands, clock=clock,
                                         output=output.append), 0)
        self.assertEqual(operations(peer),
                         [OP_STATUS, OP_STRAIGHT, OP_STATUS, OP_KEEPALIVE, OP_STOP, OP_STATUS])
        self.assertIn("l Straight track", output[0])
        self.assertTrue(any(line.startswith("Straight track started") for line in output))

    def test_straight_stale_grant_is_retried_with_same_sequence(self):
        clock = Clock()

        class StraightGrantPeer(FollowPeer):
            rejected = False

            def publish(self, payload, timeout):
                if (payload[0] >> 1) & 7 == OP_STRAIGHT and not self.rejected:
                    self.rejected = True
                    self.sent.append(payload)
                    self.status = replace(self.status, grant=101)
                    self.queue.append(replace(
                        self.status, opcode=OP_STRAIGHT, result=8,
                        sequence=int.from_bytes(payload[3:5], "big")))
                else:
                    super().publish(payload, timeout)

        peer = StraightGrantPeer(clock)
        menu = self.menu(peer, clock)
        menu.start(OP_STRAIGHT)
        straight = [frame for frame, opcode in zip(peer.sent, operations(peer))
                    if opcode == OP_STRAIGHT]
        self.assertEqual(len(straight), 2)
        self.assertEqual(straight[0][:9], straight[1][:9])
        self.assertEqual([int.from_bytes(frame[9:13], "big") for frame in straight],
                         [100, 101])
        self.assertTrue(menu.stop())

    def test_lost_follow_ack_stops_without_renewing_or_restarting(self):
        clock = Clock()
        peer = FollowPeer(clock, lose_follow=True)
        menu = self.menu(peer, clock)
        menu.start()
        self.assertIn(OP_STOP, operations(peer))
        self.assertNotIn(OP_KEEPALIVE, operations(peer))
        sent = len(peer.sent)
        for _ in range(10):
            menu.step()
        self.assertEqual(len(peer.sent), sent)

    def test_session_change_suppresses_old_report_and_renewal(self):
        clock = Clock()
        peer = FollowPeer(clock, change_session=True)
        menu = self.menu(peer, clock)
        menu.start()
        menu.commands.add("l")
        menu.commands.add("f")
        for _ in range(12):
            menu.step()
        self.assertEqual(operations(peer).count(OP_FOLLOW), 1)
        self.assertEqual(operations(peer).count(OP_KEEPALIVE), 1)
        self.assertIsNone(menu.current_route())
        self.assertIsNone(menu.commands.take())

    def test_menu_requires_integrated_telemetry_before_sending_follow(self):
        clock = Clock()
        peer = FollowPeer(clock)
        peer.route = None
        menu = self.menu(peer, clock)
        menu.start()
        self.assertEqual(operations(peer), [OP_STATUS])

    def test_fresh_stopped_status_can_confirm_when_stop_ack_is_lost(self):
        class LostStopAckPeer(FollowPeer):
            def publish(self, payload, timeout):
                super().publish(payload, timeout)
                if (payload[0] >> 1) & 7 == OP_STOP:
                    self.queue.clear()

        clock = Clock()
        peer = LostStopAckPeer(clock)
        menu = self.menu(peer, clock)
        menu.start()
        self.assertTrue(menu.stop())
        self.assertEqual(operations(peer)[-1], OP_STATUS)

    def test_stop_preempts_lost_straight_ack_and_discards_prior_starts(self):
        clock = Clock()
        commands = MenuCommands()

        class PreemptedPeer(FollowPeer):
            def publish(self, payload, timeout):
                super().publish(payload, timeout)
                if (payload[0] >> 1) & 7 == OP_STRAIGHT:
                    commands.add("f")
                    commands.add("l")
                    commands.add("s")

        peer = PreemptedPeer(clock, lose_follow=True)
        menu = self.menu(peer, clock, commands)
        menu.start(OP_STRAIGHT)
        self.assertEqual(operations(peer).count(OP_STRAIGHT), 1)
        self.assertEqual(commands.take(), "s")
        self.assertIsNone(commands.take())
        self.assertTrue(menu.stop())
        self.assertLess(clock(), 0.6)
        self.assertNotIn(OP_KEEPALIVE, operations(peer))

    def test_quit_sends_stop_and_verifies_fresh_status(self):
        clock = Clock()
        peer = FollowPeer(clock)
        commands = MenuCommands()
        commands.add("q")
        run_follow_menu(peer, 1.0, commands=commands, clock=clock,
                        output=lambda _: None)
        self.assertIn(OP_STOP, operations(peer))
        self.assertEqual(operations(peer)[-1], OP_STATUS)

    def test_eof_discards_pending_start_and_requests_quit(self):
        commands = MenuCommands()
        with patch("sys.stdin", SimpleNamespace(fileno=lambda: 0)), \
                patch("builtins.open", return_value=BytesIO(b"f\n")):
            commands.read_stdin()
        self.assertEqual(commands.take(), "q")
        self.assertIsNone(commands.take())

    def test_ctrl_c_stops_and_returns_off_confirmation(self):
        class InterruptedPeer(FollowPeer):
            interrupted = False

            def receive(self, timeout):
                if not self.interrupted:
                    self.interrupted = True
                    raise KeyboardInterrupt
                return super().receive(timeout)

        clock = Clock()
        peer = InterruptedPeer(clock)
        result = run_follow_menu(peer, 1.0, commands=MenuCommands(), clock=clock,
                                 output=lambda _: None)
        self.assertEqual(result, 0)
        self.assertIn(OP_STOP, operations(peer))
        self.assertEqual(operations(peer)[-1], OP_STATUS)


if __name__ == "__main__":
    unittest.main()
