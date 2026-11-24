#ifndef AAI05_FOLLOW_REMOTE_H
#define AAI05_FOLLOW_REMOTE_H
#include "motion_control.h"

typedef struct
{
    uint32_t issued_ms;
    uint16_t epoch;
    bool armed;
    bool fed;
    bool start;
    bool straight;
    uint8_t target; /* DRIVE_UNKNOWN (7): whole route; else one lane. */
} follow_remote_permission_t;

motion_result_t subsystem2_follow_arm(uint16_t *p_epoch, bool straight, uint8_t target);
/* Motion alone consumes the one-shot start and retains PWM ownership. */
follow_remote_permission_t follow_remote_take(void);

static inline uint32_t follow_remote_remaining(follow_remote_permission_t permission,
                                               uint32_t now_ms)
{
    uint32_t age = now_ms - permission.issued_ms;
    return permission.armed && permission.fed && age < MOTION_COMMAND_TTL_MS ?
           MOTION_COMMAND_TTL_MS - age : 0U;
}
#endif
