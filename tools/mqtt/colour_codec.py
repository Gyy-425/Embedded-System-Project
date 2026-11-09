"""Read-only colour report codec; no network activity on import."""
from dataclasses import dataclass
from robot_codec import COLOUR_NAMES, crc8

REASONS = ("MATCH", "UNCALIBRATED", "NO_MATCH", "AMBIGUOUS",
           "INVALID_SAMPLE", "STALE_SAMPLE", "INVALID_MODEL", "BAD_ARGUMENT")


@dataclass(frozen=True)
class ColourPacket:
    is_snapshot: bool
    session: int
    sequence: int
    sampled_ms: int
    dropped: int
    left_raw: int
    right_raw: int
    left_result: int
    right_result: int


def decode_colour_packet(payload: bytes) -> ColourPacket:
    if (not isinstance(payload, bytes) or len(payload) != 23 or
            payload[0] not in (0xD0, 0xD1) or crc8(payload[:-1]) != payload[-1]):
        raise ValueError("Invalid colour frame kind, length or CRC")
    session = int.from_bytes(payload[1:5], "big")
    if not session:
        raise ValueError("Colour report requires a nonzero connection session")
    for value in payload[20:22]:
        reason, colour = value >> 3, value & 7
        if value & 0xC0 or ((reason == 0) != (colour < 7)):
            raise ValueError("Invalid packed colour/reason")
    return ColourPacket(
        is_snapshot=payload[0] == 0xD1, session=session,
        sequence=int.from_bytes(payload[5:9], "big"),
        sampled_ms=int.from_bytes(payload[9:13], "big"),
        dropped=int.from_bytes(payload[13:17], "big"),
        left_raw=(payload[17] << 4) | (payload[18] >> 4),
        right_raw=((payload[18] & 15) << 8) | payload[19],
        left_result=payload[20], right_result=payload[21])


def format_colour_packet(packet: ColourPacket) -> str:
    def result(value):
        return f"{COLOUR_NAMES[value & 7]}/{REASONS[value >> 3]}"

    kind = "SNAPSHOT" if packet.is_snapshot else "CHANGE"
    return (f"{kind} sample_t={packet.sampled_ms}ms seq={packet.sequence} "
            f"L={result(packet.left_result)} AO={packet.left_raw} "
            f"R={result(packet.right_result)} AO={packet.right_raw} "
            f"dropped={packet.dropped} session={packet.session:08x}")


class ColourStream:
    """Track delivered transitions, without treating snapshots as new colours."""

    def __init__(self):
        self.previous = None

    def observe(self, packet: ColourPacket) -> list[str]:
        notes = []
        prior = self.previous
        if prior is None or prior.session != packet.session:
            notes.append(f"Colour session {packet.session:08x}; continuity starts here.")
        else:
            delta = (packet.sequence - prior.sequence) & 0xFFFFFFFF
            if 1 < delta < 0x80000000:
                notes.append(f"{delta - 1} transition(s) missing before this report.")
            elif delta >= 0x80000000:
                notes.append("Out-of-order colour sequence; continuity uncertain.")
            if packet.dropped > prior.dropped:
                notes.append(f"IR event queue overflow: {packet.dropped} discarded since boot.")
        if packet.dropped and (prior is None or prior.session != packet.session):
            notes.append(f"IR event queue overflow: {packet.dropped} discarded since boot.")
        self.previous = packet
        return notes
