"""Stationary v2 telemetry capture and offline range analysis; no network on import."""
import csv
from datetime import datetime, timezone
import itertools
import json
import math
from pathlib import Path
import re
import statistics
import time

from drive_codec import DriveTelemetry

FIELDS = ("index", "session", "sequence", "uptime_ms", "left_raw", "right_raw")
INCOMPLETE = {"IN_PROGRESS", "TIMED_OUT", "ABORTED", "INTERRUPTED"}


def _text(value, limit):
    return (isinstance(value, str) and 0 < len(value.strip()) <= limit and
            value.isprintable())


def _validate(label, name, setup, device, height_mm, sample_count, timeout, settle_ms):
    if type(label) is not int or not 0 <= label <= 6:
        raise ValueError("Tape class must be ID0..6")
    if not _text(name, 64) or not _text(setup, 300):
        raise ValueError("Provide a printable tape name and physical setup description")
    if not isinstance(device, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,24}", device):
        raise ValueError("Invalid device ID")
    if height_mm is not None and (type(height_mm) not in (int, float) or
                                 not math.isfinite(height_mm) or height_mm <= 0):
        raise ValueError("Measured height must be positive and finite, or omitted")
    if type(sample_count) is not int or not 10 <= sample_count <= 1000:
        raise ValueError("Sample count must be10..1000")
    if type(timeout) not in (int, float) or not math.isfinite(timeout) or not 1 <= timeout <= 300:
        raise ValueError("Capture timeout must be1..300 seconds")
    if type(settle_ms) is not int or not 0 <= settle_ms <= 10000:
        raise ValueError("Settling time must be0..10000 milliseconds")


def _stopped(packet):
    if (not packet.ready or packet.active or packet.remote_phase not in (0, 3) or
            packet.motion_phase not in (0, 2) or packet.left_pct or packet.right_pct or
            packet.lease_ms):
        raise RuntimeError("Capture requires ready, stopped/coasted output; movement/fault detected")


def _forward(new, old, bits):
    delta = (new - old) & ((1 << bits) - 1)
    return 0 < delta < (1 << (bits - 1))


def _json_to_handle(handle, value):
    handle.seek(0)
    json.dump(value, handle, indent=2, allow_nan=False)
    handle.write("\n")
    handle.truncate()
    handle.flush()


def capture_samples(bridge, status, output, *, label, name, setup,
                    device="robot01", height_mm=None, sample_count=40,
                    timeout=30.0, settle_ms=500, clock=time.monotonic):
    """Receive only. Existing paths reject; failures preserve incomplete data."""
    _validate(label, name, setup, device, height_mm, sample_count, timeout, settle_ms)
    _stopped(status)
    if not status.session:
        raise RuntimeError("A current device session is required")
    output = Path(output)
    output.mkdir(parents=True, exist_ok=False)
    summary = dict(schema=1, state="IN_PROGRESS", label=label, name=name.strip(),
                   setup=setup.strip(), device=device, height_mm=height_mm,
                   session=status.session, sample_count=sample_count, accepted=0,
                   settle_ms=settle_ms, timeout_s=timeout,
                   max_gap_s=2.0, max_elapsed_drift_s=1.0,
                   started_utc=datetime.now(timezone.utc).isoformat(),
                   discarded=dict(not_fresh=0, invalid_ir=0, settling=0))
    last_uptime = status.uptime_ms
    last_sequence = None
    began = clock()
    last_fresh = began
    with (output / "metadata.json").open("x", encoding="utf-8") as meta:
        _json_to_handle(meta, summary)
        with (output / "samples.csv").open("x", newline="", encoding="utf-8") as data:
            writer = csv.DictWriter(data, fieldnames=FIELDS)
            writer.writeheader()
            data.flush()
            try:
                while summary["accepted"] < sample_count:
                    now = clock()
                    if now - began >= timeout:
                        raise TimeoutError("Capture deadline reached before enough valid samples")
                    if now - last_fresh >= 2.0:
                        raise RuntimeError("No fresh telemetry for2 seconds; capture aborted")
                    packet = bridge.receive(min(0.1, timeout - (now - began),
                                                2.0 - (now - last_fresh)))
                    received = clock()
                    if received - began >= timeout:
                        raise TimeoutError("Capture deadline reached before accepting sample")
                    if received - last_fresh >= 2.0:
                        raise RuntimeError("No fresh telemetry for2 seconds; capture aborted")
                    if packet is None:
                        continue
                    if packet.session != status.session:
                        raise RuntimeError("Device session changed; keep separate captures")
                    if not _forward(packet.uptime_ms, status.uptime_ms, 32):
                        summary["discarded"]["not_fresh"] += 1
                        continue
                    if type(packet) is not DriveTelemetry:
                        _stopped(packet)
                        continue
                    if (not _forward(packet.uptime_ms, last_uptime, 32) or
                            (last_sequence is not None and
                             not _forward(packet.sequence, last_sequence, 16))):
                        summary["discarded"]["not_fresh"] += 1
                        continue
                    _stopped(packet)
                    device_elapsed = ((packet.uptime_ms - status.uptime_ms) & 0xFFFFFFFF) / 1000.0
                    if abs(device_elapsed - (received - began)) > 1.0:
                        raise RuntimeError("Device/host elapsed times diverged; delayed backlog or clock jump")
                    last_uptime, last_sequence = packet.uptime_ms, packet.sequence
                    last_fresh = received
                    if ((packet.uptime_ms - status.uptime_ms) & 0xFFFFFFFF) < settle_ms:
                        summary["discarded"]["settling"] += 1
                        continue
                    if not packet.ir_valid or packet.ir_stale:
                        summary["discarded"]["invalid_ir"] += 1
                        continue
                    if not 0 <= packet.left_raw <= 4095 or not 0 <= packet.right_raw <= 4095:
                        raise ValueError("Raw ADC sample outside12-bit range")
                    writer.writerow(dict(index=summary["accepted"], session=packet.session,
                                         sequence=packet.sequence, uptime_ms=packet.uptime_ms,
                                         left_raw=packet.left_raw, right_raw=packet.right_raw))
                    data.flush()
                    summary["accepted"] += 1
                summary["state"] = "COMPLETE"
            except KeyboardInterrupt:
                summary["state"] = "INTERRUPTED"
                summary["error"] = "KeyboardInterrupt"
                raise
            except Exception as error:
                summary["state"] = "TIMED_OUT" if isinstance(error, TimeoutError) else "ABORTED"
                summary["error"] = str(error)[:300]
                raise
            finally:
                summary["finished_utc"] = datetime.now(timezone.utc).isoformat()
                _json_to_handle(meta, summary)
    return summary


def _read_complete(path, meta):
    try:
        _validate(meta["label"], meta["name"], meta["setup"], meta["device"],
                  meta["height_mm"], meta["sample_count"], meta["timeout_s"], meta["settle_ms"])
        if type(meta["session"]) is not int or not 1 <= meta["session"] <= 0xFFFFFFFF:
            raise ValueError("Invalid capture session")
        rows = []
        with (path.parent / "samples.csv").open(newline="", encoding="utf-8") as data:
            reader = csv.DictReader(data)
            if reader.fieldnames != list(FIELDS):
                raise ValueError("Unexpected CSV columns")
            for raw in reader:
                if len(rows) >= 1000 or set(raw) != set(FIELDS):
                    raise ValueError("Too many rows or malformed CSV")
                if any(not value or not value.isascii() or not value.isdecimal()
                       for value in raw.values()):
                    raise ValueError("CSV fields must be unsigned integers")
                row = {key: int(value) for key, value in raw.items()}
                if (row["index"] != len(rows) or row["session"] != meta["session"] or
                        not 0 <= row["sequence"] <= 65535 or
                        not 0 <= row["uptime_ms"] <= 0xFFFFFFFF or
                        not 0 <= row["left_raw"] <= 4095 or not 0 <= row["right_raw"] <= 4095):
                    raise ValueError("CSV row outside capture contract")
                if rows and (not _forward(row["uptime_ms"], rows[-1]["uptime_ms"], 32) or
                             not _forward(row["sequence"], rows[-1]["sequence"], 16)):
                    raise ValueError("CSV samples are duplicated/out of order")
                rows.append(row)
        if len(rows) != meta["sample_count"] or meta["accepted"] != len(rows):
            raise ValueError("Complete capture has missing/extra rows")
        return rows
    except (KeyError, TypeError, ValueError) as error:
        raise ValueError(f"Invalid complete capture {path.parent.name}: {error}") from error


def report_captures(root):
    """Observed interval comparison only, never classifier accuracy."""
    root = Path(root)
    if not root.is_dir():
        raise ValueError("Capture root must be an existing directory")
    paths = sorted(root.glob("*/metadata.json"))
    if len(paths) > 256:
        raise ValueError("Use at most256 captures in one report")
    result = dict(schema=1, classes={}, overlaps={"left": [], "right": []},
                  excluded=[], captures=[], setup=None, height_mm=None)
    groups = {}
    condition = None
    for path in paths:
        with path.open(encoding="utf-8") as file:
            meta = json.load(file)
        if not isinstance(meta, dict) or type(meta.get("schema")) is not int or meta["schema"] != 1:
            raise ValueError(f"Unknown metadata schema: {path}")
        state = meta.get("state")
        if not isinstance(state, str):
            raise ValueError(f"Invalid capture state: {path}")
        if state in INCOMPLETE:
            result["excluded"].append(dict(capture=path.parent.name, state=state))
            continue
        if state != "COMPLETE":
            raise ValueError(f"Unknown capture state: {path}")
        rows = _read_complete(path, meta)
        current = (meta["setup"], meta["height_mm"], meta["device"])
        if condition is not None and condition != current:
            raise ValueError("Different setup/height/device; report them separately")
        condition = current
        label = meta["label"]
        group = groups.setdefault(label, dict(name=meta["name"], left=[], right=[]))
        if group["name"] != meta["name"]:
            raise ValueError("One class ID has different tape names")
        for side in ("left", "right"):
            group[side].extend(row[side + "_raw"] for row in rows)
        result["captures"].append(path.parent.name)
    for label, group in sorted(groups.items()):
        summary = dict(name=group["name"])
        for side in ("left", "right"):
            values = group[side]
            summary[side] = dict(n=len(values), min=min(values), max=max(values),
                                 median=statistics.median(values),
                                 rail_hits=sum(value in (0, 4095) for value in values))
        result["classes"][str(label)] = summary
    for left, right in itertools.combinations(sorted(groups), 2):
        for side in ("left", "right"):
            a, b = result["classes"][str(left)][side], result["classes"][str(right)][side]
            low, high = max(a["min"], b["min"]), min(a["max"], b["max"])
            if low <= high:
                result["overlaps"][side].append(dict(classes=[left, right], range=[low, high]))
    result["missing_classes"] = [label for label in range(7) if label not in groups]
    result["status"] = ("INCOMPLETE" if result["missing_classes"] else "OVERLAP"
                        if any(result["overlaps"].values()) else "CANDIDATE_RANGES")
    if condition:
        result["setup"], result["height_mm"], result["device"] = condition
    result["note"] = "Stationary observed ranges only; not recognition accuracy or firmware thresholds."
    return result


def write_report(report, output):
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as file:
        json.dump(report, file, indent=2, allow_nan=False)
        file.write("\n")
