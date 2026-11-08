#include "motion_chassis.h"
#include <stddef.h>

_Static_assert((MOTION_MAX_DUTY > 0) && (MOTION_MAX_DUTY <= INT8_MAX),
               "Chassis duty must fit the signed raw Motion representation");

motion_result_t motion_chassis_convert(const motion_chassis_command_t *p_request,
                                        motion_command_t *p_command)
{
    if ((p_request == NULL) || (p_command == NULL) ||
        (p_request->forward_pct < 0) ||
        (p_request->forward_pct > MOTION_MAX_DUTY) ||
        (p_request->turn_pct < -MOTION_MAX_DUTY) ||
        (p_request->turn_pct > MOTION_MAX_DUTY))
    {
        return MOTION_BAD_COMMAND;
    }

    int32_t forward = p_request->forward_pct;
    int32_t turn = p_request->turn_pct;
    if (turn > forward)
    {
        turn = forward;
    }
    else if (turn < -forward)
    {
        turn = -forward;
    }
    int32_t left = forward + turn;
    int32_t right = forward - turn;
    int32_t peak = (left > right) ? left : right;
    if (peak > MOTION_MAX_DUTY)
    {
        /* Common scaling preserves the requested ratio to integer precision. */
        left = (left * MOTION_MAX_DUTY) / peak;
        right = (right * MOTION_MAX_DUTY) / peak;
    }
    *p_command = (motion_command_t){
        .issued_ms = p_request->issued_ms,
        .sequence = p_request->sequence,
        .left_pct = (int8_t)-left,
        .right_pct = (int8_t)right};
    return MOTION_ACCEPTED;
}
