"""Convert measured straight travel and signed encoder deltas to calibration.

No default wheel diameter, gear ratio, CPR, Wi-Fi settings or motor commands.
"""
import argparse
from decimal import Decimal, InvalidOperation
import json
from pathlib import Path


def calibration_from_measurements(distance_mm, left_counts, right_counts):
    try:
        distance = Decimal(str(distance_mm)) * 1000
    except InvalidOperation as error:
        raise ValueError("Distance must be a finite positive number of millimetres") from error
    if (not distance.is_finite() or distance <= 0 or distance > 1000000000 or
            distance != distance.to_integral_value()):
        raise ValueError("Distance must resolve to1..1000000000 whole micrometres")
    for count in (left_counts, right_counts):
        if type(count) is not int or count == 0 or abs(count) > 2147483647:
            raise ValueError("Each signed encoder delta must have magnitude1..2147483647")

    def wheel(count):
        return dict(reference_um=int(distance), reference_counts=abs(count),
                    forward_sign=1 if count > 0 else -1)

    return dict(schema="AAI05_DISTANCE_CALIBRATION_V1", units="um/count",
                status="USER_INPUTS_NOT_PHYSICALLY_VALIDATED",
                left=wheel(left_counts), right=wheel(right_counts))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--distance-mm", required=True)
    parser.add_argument("--left-counts", type=int, required=True)
    parser.add_argument("--right-counts", type=int, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        calibration = calibration_from_measurements(args.distance_mm,
                                                    args.left_counts, args.right_counts)
        text = json.dumps(calibration, indent=2) + "\n"
        if args.output is not None:
            with args.output.open("x", encoding="utf-8") as stream:
                stream.write(text)
        print(text, end="")
    except (ValueError, OSError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
