"""Binary v1 codec shared by the laptop CLI; no network side effects."""

from dataclasses import dataclass

COLOUR_NAMES = ("black", "white", "colour1", "colour2", "colour3",
                "colour4", "colour5", "unknown")
RESULT_NAMES = ("OK", "DUPLICATE", "BAD_FRAME", "BAD_TARGET", "SESSION",
                "STALE_SEQUENCE", "SEQUENCE_CONFLICT", "NOT_READY")
OP_STATUS, OP_TARGET, OP_CLEAR = range(3)
UNKNOWN = 7


@dataclass(frozen=True)
class Ack:
    opcode: int
    target: int
    result: int
    capabilities: int
    ready: bool
    sequence: int  # Last accepted mutating sequence, not the GET request ID.
    session: int


@dataclass(frozen=True)
class Telemetry:
    target: int
    detected: int
    capabilities: int
    ir_valid: bool
    ir_stale: bool
    tx_drop: bool
    ready: bool
    sequence: int
    session: int
    uptime_ms: int
    left_raw: int
    right_raw: int


def crc8(payload: bytes) -> int:
    """CRC-8/SMBUS: polynomial 0x07, initial/final XOR 0, not reflected."""
    crc = 0
    for value in payload:
        crc ^= value
        for _ in range(8):
            crc = ((crc << 1) ^ (0x07 if crc & 0x80 else 0)) & 0xFF
    return crc


def encode_command(opcode: int, target: int = UNKNOWN, sequence: int = 0,
                   session: int = 0) -> bytes:
    if opcode not in (OP_STATUS, OP_TARGET, OP_CLEAR):
        raise ValueError("Unsupported opcode")
    if not 0 <= sequence <= 0xFFFF or not 0 <= session <= 0xFFFFFFFF:
        raise ValueError("Sequence/tag outside wire range")
    if opcode == OP_TARGET:
        if not 0 <= target < UNKNOWN:
            raise ValueError("Target must be a colour ID from 0 to 6")
    elif target != UNKNOWN:
        raise ValueError("STATUS/CLEAR require the unset target ID")
    if opcode != OP_STATUS and session == 0:
        raise ValueError("Read current device status before changing a target")
    body = bytes([(opcode << 4) | (target << 1)])
    body += sequence.to_bytes(2, "big") + session.to_bytes(4, "big")
    return body + bytes([crc8(body)])


def decode_packet(payload: bytes) -> Ack | Telemetry:
    if len(payload) not in (9, 16) or crc8(payload[:-1]) != payload[-1]:
        raise ValueError("Wrong packet length or CRC")
    kind = payload[0] >> 6
    if kind == 1 and len(payload) == 9:
        opcode = (payload[0] >> 4) & 3
        if payload[0] & 1 or opcode > OP_CLEAR:
            raise ValueError("Invalid ACK opcode/reserved bit")
        return Ack(opcode, (payload[0] >> 1) & 7, payload[1] & 7,
                   (payload[1] >> 3) & 15, bool(payload[1] & 0x80),
                   int.from_bytes(payload[2:4], "big"),
                   int.from_bytes(payload[4:8], "big"))
    if kind == 2 and len(payload) == 16:
        flags = payload[1]
        if flags & 0x10 and flags & 0x20:
            raise ValueError("IR cannot be valid and stale simultaneously")
        return Telemetry(
            (payload[0] >> 3) & 7, payload[0] & 7, flags & 15,
            bool(flags & 0x10), bool(flags & 0x20), bool(flags & 0x40),
            bool(flags & 0x80), int.from_bytes(payload[2:4], "big"),
            int.from_bytes(payload[4:8], "big"),
            int.from_bytes(payload[8:12], "big"),
            (payload[12] << 4) | (payload[13] >> 4),
            ((payload[13] & 15) << 8) | payload[14])
    raise ValueError("Unexpected packet type on response topic")


def parse_colour(value: str) -> int:
    value = value.strip().lower()
    if value in COLOUR_NAMES[:7]:
        return COLOUR_NAMES.index(value)
    if value.isascii() and value.isdecimal() and 0 <= int(value) <= 6:
        return int(value)
    raise ValueError("Use black, white, colour1..colour5, or ID 0..6")


def target_name(target: int) -> str:
    return "unset" if target == UNKNOWN else COLOUR_NAMES[target]


def format_packet(packet: Ack | Telemetry) -> str:
    motion = "present" if packet.capabilities & 4 else "none"
    if isinstance(packet, Ack):
        return (f"ACK result={RESULT_NAMES[packet.result]} "
                f"target={target_name(packet.target)} seq={packet.sequence} "
                f"tag={packet.session:08x} ready={int(packet.ready)} "
                f"motion-capability={motion}")
    quality = ("valid" if packet.ir_valid else
               "stale" if packet.ir_stale else "invalid")
    return (f"t={packet.uptime_ms:>8} ms  "
            f"IR L={packet.left_raw:4} R={packet.right_raw:4} [{quality}]  "
            f"target={target_name(packet.target):7}  "
            f"detected={COLOUR_NAMES[packet.detected]:7}  "
            f"seq={packet.sequence} drop={int(packet.tx_drop)}  "
            f"motion-capability={motion}")
