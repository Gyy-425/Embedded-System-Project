"""Decode integrated follower and route telemetry without network side effects."""
from dataclasses import dataclass
import struct

from robot_codec import crc8

COLOURS = ("black", "white", "yellow", "background", "red", "blue",
           "colour ID 6", "unknown")
FOLLOW_PHASES = ("IDLE", "RUNNING", "STOPPED")
FOLLOW_REASONS = ("NONE", "BUTTON", "IR", "RGB", "HARDWARE", "AMBIGUOUS",
                  "COMPLETE", "DRIVER", "JOIN_TIMEOUT", "CORNER_NOT_SEEN",
                  "CORNER_TIMEOUT", "REFRESH", "MQTT_STOP", "MQTT_TIMEOUT")
ROUTE_STATUSES = ("OK", "DUPLICATE", "CONFIG", "CAPTURE", "ORDER", "JUMP",
                  "REVERSE", "OVERFLOW", "STALE", "HISTORY", "UNKNOWN_TIMEOUT",
                  "QUEUE_FULL", "FINISHED")
NO_RUN = 255
SECTION_FLAGS = ("partial start", "partial end", "uncertain", "aborted")
_HEADER = struct.Struct(">6BIHIIB")
_SECTION = struct.Struct(">BBI")


@dataclass(frozen=True)
class RouteSection:
    colour: int
    flags: int
    length_um: int


@dataclass(frozen=True)
class RouteReport:
    phase: int
    reason: int
    colour: int
    status: int
    finished: bool
    session: int
    epoch: int
    path_um: int
    unassigned_um: int
    sections: tuple[RouteSection, ...]


def decode_route_packet(payload: bytes) -> RouteReport:
    if len(payload) < _HEADER.size + 1:
        raise ValueError("Wrong route telemetry length")
    (kind, phase, reason, colour, status, flags, session, epoch,
     path, unassigned, count) = _HEADER.unpack_from(payload)
    if count > 16 or len(payload) != _HEADER.size + count * _SECTION.size + 1:
        raise ValueError("Wrong route telemetry section count or length")
    if kind != 0xD0 or crc8(payload[:-1]) != payload[-1]:
        raise ValueError("Wrong route telemetry type or CRC")
    if (phase >= len(FOLLOW_PHASES) or reason >= len(FOLLOW_REASONS) or
            colour >= len(COLOURS) or
            (status >= len(ROUTE_STATUSES) and status != NO_RUN) or
            flags & ~1 or not session):
        raise ValueError("Invalid route telemetry state")
    sections = tuple(RouteSection(*entry) for entry in
                     _SECTION.iter_unpack(payload[_HEADER.size:-1]))
    if any(section.colour >= 7 or section.flags & ~15 for section in sections):
        raise ValueError("Invalid route section colour or flags")
    return RouteReport(phase, reason, colour, status, bool(flags & 1),
                       session, epoch, path, unassigned, sections)


def distance(length_um: int) -> str:
    return f"{length_um / 1_000_000:.3f} m"


def format_route_status(report: RouteReport) -> str:
    phase = FOLLOW_PHASES[report.phase]
    reason = f" ({FOLLOW_REASONS[report.reason]})" if report.phase == 2 else ""
    total = "unavailable" if report.status == NO_RUN else distance(report.path_um)
    return f"Route {phase}{reason} | Colour {COLOURS[report.colour]} | Distance estimate {total}"


def format_route_report(report: RouteReport) -> str:
    if report.status == NO_RUN:
        return "No route measurements for this session."
    state = "closed" if report.finished else "in progress"
    reason = FOLLOW_REASONS[report.reason] if report.phase == 2 else "—"
    lines = [f"Run {report.epoch} ({state}) | Estimated total {distance(report.path_um)}"
             f" | {distance(report.unassigned_um)} unassigned",
             f"Route measurement {ROUTE_STATUSES[report.status]} | Stop reason {reason}"]
    for number, section in enumerate(report.sections, 1):
        flags = [name for bit, name in enumerate(SECTION_FLAGS)
                 if section.flags & (1 << bit)]
        note = f" ({', '.join(flags)})" if flags else ""
        lines.append(f"  {number}. {COLOURS[section.colour]} "
                     f"{distance(section.length_um)}{note}")
    if not report.sections:
        lines.append("  No closed colour sections.")
    return "\n".join(lines)
