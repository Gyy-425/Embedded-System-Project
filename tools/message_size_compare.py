"""Compare MQTT binary message sizes with an equivalent compact-JSON text encoding.

Run: python tools/message_size_compare.py   (no network access, no device needed)
Binary sizes come from the firmware wire formats in app_program/subsystem3.
"""
import json

COMMAND = {"op": "FOLLOW", "target": 5, "seq": 4660, "session": 305419896,
           "grant": 2596069104, "left": 0, "right": 0}
ACK = {"op": "KEEPALIVE", "result": 0, "ready": 1, "active": 1, "ir_valid": 1,
       "ir_stale": 0, "tx_drop": 0, "remote_phase": 2, "motion_phase": 1, "reason": 0,
       "echo": 4660, "mseq": 4660, "session": 305419896, "grant": 2596069105,
       "uptime_ms": 123456, "target": 5, "left": -25, "right": 25, "lease_ms": 740}
TELEMETRY = {key: value for key, value in ACK.items() if key not in ("op", "result")}
TELEMETRY.update({"left_raw": 339, "right_raw": 313})
ROUTE = {"phase": 1, "reason": 0, "colour": 5, "status": 0, "finished": 0,
         "session": 305419896, "epoch": 3, "path_um": 1250000, "unassigned_um": 20000,
         "sections": [{"colour": 5, "flags": 1, "length_um": 640000},
                      {"colour": 4, "flags": 0, "length_um": 410000},
                      {"colour": 2, "flags": 2, "length_um": 180000}]}
MESSAGES = (("Command (FOLLOW)", COMMAND, 14), ("ACK", ACK, 23),
            ("Telemetry", TELEMETRY, 26), ("Route, 3 sections", ROUTE, 22 + 6 * 3))


def main():
    for name, fields, binary in MESSAGES:
        text = len(json.dumps(fields, separators=(",", ":")))
        print(f"{name:20s} binary={binary:3d} B  json={text:3d} B  ratio={text / binary:.1f}x")


if __name__ == "__main__":
    main()
