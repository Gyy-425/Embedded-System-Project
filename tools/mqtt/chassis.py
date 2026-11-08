"""Logical forward/steering duties; import never connects or actuates."""


def chassis_duties(forward: int, turn: int) -> tuple[int, int]:
    """Match motion_chassis_convert; positive turn is right, zero is neutral.

    Coordinates are duty units, not speed/angle. No wheel reversal or pivot.
    Apply common scaling before raw left-negative/right-positive polarity.
    """
    if (type(forward) is not int or type(turn) is not int or
            not 0 <= forward <= 50 or not -50 <= turn <= 50):
        raise ValueError("Forward duty must be integer0..50; turn integer-50..50")
    turn = max(-forward, min(forward, turn))
    left, right = forward + turn, forward - turn
    peak = max(left, right)
    if peak > 50:
        left, right = left * 50 // peak, right * 50 // peak
    return -left, right
