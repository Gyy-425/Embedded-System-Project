"""Persistent MQTT 3.1.1 QoS 1 publisher for the menu's short commands."""

import socket
import time
from uuid import uuid4


def _remaining(deadline):
    seconds = deadline - time.monotonic()
    if seconds <= 0:
        raise TimeoutError
    return seconds


class MqttPublisher:
    def __init__(self, host, port, topic):
        self.address = (host, port)
        topic_bytes = topic.encode("utf-8")
        self.topic = len(topic_bytes).to_bytes(2, "big") + topic_bytes
        self.connection = None
        self.packet_id = 0

    def close(self):
        if self.connection is not None:
            self.connection.close()
            self.connection = None

    def _send(self, header, body, deadline):
        # All menu commands and their topic fit in a single remaining-length byte.
        if len(body) >= 128:
            raise ValueError("MQTT menu packet is too large")
        self.connection.settimeout(_remaining(deadline))
        self.connection.sendall(bytes((header, len(body))) + body)

    def _expect(self, expected, deadline):
        response = bytearray()
        while len(response) < len(expected):
            self.connection.settimeout(_remaining(deadline))
            data = self.connection.recv(len(expected) - len(response))
            if not data:
                raise ConnectionError("MQTT broker closed the publisher connection")
            response.extend(data)
        if response != expected:
            raise RuntimeError(f"Unexpected MQTT broker response ({response.hex()})")

    def publish(self, payload, timeout):
        deadline = time.monotonic() + timeout
        stage = "connection"
        try:
            if self.connection is None:
                self.connection = socket.create_connection(self.address, _remaining(deadline))
                self.connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                client_id = b"aai05-menu-" + uuid4().hex[:12].encode("ascii")
                # Clean session. The firmware lease governs motion during link loss.
                body = b"\x00\x04MQTT\x04\x02\x00\x00" + bytes((0, len(client_id))) + client_id
                self._send(0x10, body, deadline)
                self._expect(b"\x20\x02\x00\x00", deadline)

            stage = "publish acknowledgement"
            self.packet_id = self.packet_id % 65535 + 1
            packet_id = self.packet_id.to_bytes(2, "big")
            self._send(0x32, self.topic + packet_id + payload, deadline)
            self._expect(b"\x40\x02" + packet_id, deadline)
        except TimeoutError as error:
            self.close()
            raise TimeoutError(f"MQTT broker {stage} timed out") from error
        except (OSError, RuntimeError, ValueError):
            self.close()
            raise
