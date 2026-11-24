"""Compact v2 Motion codec; import has no network side effects."""
from dataclasses import dataclass
from robot_codec import crc8, target_name

(OP_STATUS, OP_TARGET, OP_CLEAR, OP_START, OP_KEEPALIVE, OP_STOP,
 OP_FOLLOW, OP_STRAIGHT) = range(8)
UNKNOWN = 7
# FOLLOW may select one lane; IDs match the RGB class and route colour IDs.
FOLLOW_TARGETS = {"yellow": 2, "red": 4, "blue": 5}
RESULTS = ("OK", "DUPLICATE", "BAD_FRAME", "BAD_VALUE", "SESSION",
           "STALE_SEQUENCE", "CONFLICT", "NOT_READY", "STALE_GRANT",
           "BAD_STATE", "RETAINED", "MOTION_FAILURE", "BUSY")
REMOTE_PHASES = ("IDLE", "ARMING", "RUNNING", "STOPPED", "FAULT")
MOTION_PHASES = ("READY", "RUNNING", "STOPPED", "FAULT")
REASONS = ("NONE", "REQUEST", "GP21", "EXPIRED", "HARDWARE")
OPERATIONS = ("STATUS", "TARGET", "CLEAR", "START", "KEEPALIVE", "STOP", "FOLLOW",
              "STRAIGHT")


@dataclass(frozen=True)
class DriveAck:
    opcode: int
    result: int
    target: int
    ready: bool
    active: bool
    ir_valid: bool
    ir_stale: bool
    tx_drop: bool
    remote_phase: int
    motion_phase: int
    reason: int
    sequence: int
    mutation_sequence: int
    session: int
    grant: int
    uptime_ms: int
    left_pct: int
    right_pct: int
    lease_ms: int


@dataclass(frozen=True)
class DriveTelemetry(DriveAck):
    left_raw: int
    right_raw: int


def encode_drive_command(opcode: int, target: int = UNKNOWN,
                         sequence: int = 0, session: int = 0, grant: int = 0,
                         left: int = 0, right: int = 0) -> bytes:
    fields = (opcode, target, sequence, session, grant, left, right)
    if any(type(value) is not int for value in fields):
        raise ValueError("Command fields must be integers")
    if not 0 <= opcode <= OP_STRAIGHT or not 0 <= target <= 7:
        raise ValueError("Unsupported opcode/target")
    if not 0 <= sequence <= 0xFFFF or not 0 <= session <= 0xFFFFFFFF:
        raise ValueError("Sequence/session outside wire range")
    if not 0 <= grant <= 0xFFFFFFFF or not -50 <= left <= 50 or not -50 <= right <= 50:
        raise ValueError("Grant/duties outside wire range")
    if opcode == OP_FOLLOW:
        if target != UNKNOWN and target not in FOLLOW_TARGETS.values():
            raise ValueError("FOLLOW target must be unset7, yellow2, red4 or blue5")
    elif (opcode == OP_TARGET and target == UNKNOWN) or (opcode != OP_TARGET and target != UNKNOWN):
        raise ValueError("TARGET requires0..6; other operations require unset7")
    if (opcode == OP_START) == (left == 0 and right == 0):
        raise ValueError("START needs nonzero duty; other operations need zero duties")
    if opcode == OP_STATUS:
        if session or grant:
            raise ValueError("STATUS requires zero session/grant")
    elif not session:
        raise ValueError("Read current status before mutating a device")
    if opcode == OP_STOP:
        if grant:
            raise ValueError("STOP bypasses freshness and requires zero grant")
    elif opcode != OP_STATUS and not grant:
        raise ValueError("Mutation requires a fresh device-issued grant")
    control = ((2 << 20) | (opcode << 17) | (target << 14) |
               ((left & 0x7F) << 7) | (right & 0x7F))
    body = (control.to_bytes(3, "big") + sequence.to_bytes(2, "big") +
            session.to_bytes(4, "big") + grant.to_bytes(4, "big"))
    return body + bytes([crc8(body)])


def decode_drive_packet(payload: bytes) -> DriveAck | DriveTelemetry:
    if len(payload) not in (23, 26) or crc8(payload[:-1]) != payload[-1]:
        raise ValueError("Wrong v2 response length or CRC")
    is_telemetry = len(payload) == 26
    kind = payload[0] >> 4
    opcode = (payload[0] >> 1) & 7
    result = ((payload[0] & 1) << 3) | (payload[1] >> 5)
    if ((is_telemetry and (payload[0] != 0xC0 or result)) or
            (not is_telemetry and (kind != 0xA or result > 12))):
        raise ValueError("Unexpected v2 packet kind/result")
    phase = payload[2] & 7
    motion_phase = (payload[2] >> 3) & 3
    reason = payload[2] >> 5
    flags = payload[1] & 31
    packed = int.from_bytes(payload[19:22], "big")
    left = (packed >> 14) & 127
    right = (packed >> 7) & 127
    left = left - 128 if left >= 64 else left
    right = right - 128 if right >= 64 else right
    lease_ticks = packed & 127
    if phase > 4 or reason > 4 or not -50 <= left <= 50 or not -50 <= right <= 50:
        raise ValueError("Invalid v2 state/duty")
    if lease_ticks > 75 or flags & 12 == 12:
        raise ValueError("Invalid lease or contradictory IR flags")
    values = dict(opcode=opcode, result=result, target=(packed >> 21) & 7,
                  ready=bool(flags & 1), active=bool(flags & 2),
                  ir_valid=bool(flags & 4), ir_stale=bool(flags & 8),
                  tx_drop=bool(flags & 16), remote_phase=phase,
                  motion_phase=motion_phase, reason=reason,
                  sequence=int.from_bytes(payload[3:5], "big"),
                  mutation_sequence=int.from_bytes(payload[5:7], "big"),
                  session=int.from_bytes(payload[7:11], "big"),
                  grant=int.from_bytes(payload[11:15], "big"),
                  uptime_ms=int.from_bytes(payload[15:19], "big"),
                  left_pct=left, right_pct=right, lease_ms=lease_ticks * 10)
    if is_telemetry:
        return DriveTelemetry(**values,
                              left_raw=(payload[22] << 4) | (payload[23] >> 4),
                              right_raw=((payload[23] & 15) << 8) | payload[24])
    return DriveAck(**values)


def format_drive_packet(packet: DriveAck | DriveTelemetry) -> str:
    state = (f"remote={REMOTE_PHASES[packet.remote_phase]} "
             f"motion={MOTION_PHASES[packet.motion_phase]} "
             f"DRIVE={'ON' if packet.active else 'OFF'} "
             f"reason={REASONS[packet.reason]} "
             f"duty L={packet.left_pct:+d}% R={packet.right_pct:+d}% "
             f"lease={packet.lease_ms}ms target={target_name(packet.target)} "
             f"seq={packet.mutation_sequence} tag={packet.session:08x}")
    if isinstance(packet, DriveTelemetry):
        quality = "valid" if packet.ir_valid else "stale" if packet.ir_stale else "invalid"
        return (f"t={packet.uptime_ms:>8} ms IR L={packet.left_raw:4} "
                f"R={packet.right_raw:4} [{quality}] detected=unknown {state}")
    return (f"ACK {OPERATIONS[packet.opcode]} result={RESULTS[packet.result]} "
            f"request={packet.sequence} {state}")
