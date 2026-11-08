#ifndef AAI05_MOTION_CHASSIS_H
#define AAI05_MOTION_CHASSIS_H

#include "motion_control.h"

/* Duty coordinates, not calibrated speed/angle. Positive turn means right.
 * Metadata shares the raw Motion producer's sequence and timestamp domain. */
typedef struct
{
    uint32_t issued_ms;
    uint16_t sequence;
    uint8_t forward_pct;
    int8_t turn_pct;
} motion_chassis_command_t;

/* forward0..50, turn-50..50, no wheel reversal/pivot. Excess turn is clipped
 * to forward; wheel duties share a scale factor at the raw duty limit.
 * Zero is neutral, not latched STOP. Rejection leaves *p_command unchanged. */
motion_result_t motion_chassis_convert(const motion_chassis_command_t *p_request,
                                        motion_command_t *p_command);
#endif
